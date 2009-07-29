/*
	Copyright 2009, Marat Radchenko

	This file is part of opendsme.

	opendsme is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	opendsme is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with opendsme.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include "opencal.h"

/* Magic sequence indicating block header start */
#define CAL_BLOCK_HEADER_MAGIC "ConF"
/* The only known CAL header version. */
#define CAL_HEADER_VERSION 2

/*
	If block has this flag set then next block isn't writesize-aligned,
	but is wordsize (32 bits) aligned and follows current block.
	Empty block or block without this flag set returns alignment back to
	writesize.
*/
#define CAL_BLOCK_FLAG_VARIABLE_LENGTH 1 << 0

/** On-disk CAL block header structure. */
struct conf_block_header {
	/* Magic header. Set to CAL_BLOCK_HEADER_MAGIC. */
	uint32_t magic;
	/* Header version. Set to CAL_HEADER_VERSION. */
	uint8_t hdr_version;
	/*
		Block version. If there are multiple blocks with same name,
		only block with highest version number is considered active.
		Block version starts with 0.
	*/
	uint8_t block_version;
	/*
		Some mysterious flags.
		Possible values: 0, CAL_BLOCK_FLAG_VARIABLE_LENGTH, 1 << 3
	*/
	uint16_t flags;
	/* Block name. */
	char name[16];
	/* Data length. */
	uint32_t len;
	/* CRC(TODO: 32?) for block data. */
	uint32_t data_crc;
	/* CRC(TODO: 32?) for header data. */
	uint32_t hdr_crc;
};

/** Structure describing CAL block. */
struct conf_block {
	/* Header on-disk offset */
	off_t addr;
	/* Block header. */
	struct conf_block_header hdr;
	/* Block data. */
	void *data;
	/* Pointer to next block (NULL if this block is last). */
	struct conf_block *next;
};

/** TODO: what's this? */
struct cal_eb {
	long unsigned int vaddr;
	long unsigned int paddr;
};

struct cal_area {
	const char *name;
	struct cal_eb *ebs;
	int eb_count;
	int valid;
	long unsigned int empty_addr;
	long unsigned int private_data;
};

/** Structure describing CAL storage. */
struct cal_s {
	/* File descriptor. */
	int mtd_fd;
	/* Path to lock file. Set only when lock was sucessfully acquired */
	char *lock_file;
	/* MTD partition info. See <mtd/mtd-abi.h> */
	struct mtd_info_user mtd_info;
	size_t page_size;
	size_t eb_size;
	struct conf_block *main_block_list;
	struct conf_block *user_block_list;
	struct conf_block *wp_block_list;
	struct cal_area config_area[2];
	int active_config_area;
	struct cal_area user_area;
	struct cal_area wp_area;
	int has_wp;
	uint8_t **page_cache;
};

/**
	Aligns offset to next block.
	Doesn't change it if offset is already aligned.
	@offset current offset
	@bs block size to use for alignment
	@return block-aligned offset
*/
static inline off_t align_to_next_block(const off_t offset, const int bs) {
	return (offset & (bs - 1)) ? ((offset & ~(bs - 1)) + bs) : offset;
}

/**
	Scans CAL storage and fills block list with found blocks.
	@c pointer to CAL struct.
	@select_mode MTD select mode. See OTPSELECT ioctl.
	@list block list to add found blocks to.
*/
static void scan_blocks(
		const cal c,
		const int select_mode,
		struct conf_block **list) {
	/* TODO: handle errors */
	ioctl(c->mtd_fd, OTPSELECT, &select_mode);
	const size_t hdr_len = sizeof(struct conf_block_header);
	struct conf_block *prev = NULL;
	for (off_t offset = 0; (size_t)offset < c->mtd_info.size;) {
		/* TODO: handle errors */
		struct conf_block *block = malloc(sizeof(struct conf_block));
		/* TODO: handle errors */
		lseek(c->mtd_fd, offset, SEEK_SET);
		/* TODO: handle errors */
		read(c->mtd_fd, &block->hdr, hdr_len);
		const size_t magic_len = strlen(CAL_BLOCK_HEADER_MAGIC);
		/* TODO: is it safe to just skip such blocks? */
		if (memcmp(&block->hdr.magic, CAL_BLOCK_HEADER_MAGIC, magic_len) != 0) {
			/* Block should be empty. TODO: check bytes for 0xFF? */
			free(block);
			/* Align to write boundary after empty block */
			offset = align_to_next_block(++offset, c->mtd_info.writesize);
		} else {
			/*
				TODO: check header version. Bail out if it's unknown
				so we don't destroy anything.
			*/
			/* TODO: remove debug output */
			printf("%s v.%d len:%u flags:%u @ %lu\n",
				block->hdr.name, block->hdr.block_version, block->hdr.len,
				block->hdr.flags, offset);
			block->addr = offset;
			if (prev == NULL) {
				*list = block;
			} else {
				prev->next = block;
			}
			prev = block;
			if (block->hdr.flags & CAL_BLOCK_FLAG_VARIABLE_LENGTH) {
				/* We need to align reads to word boundary. */
				offset = align_to_next_block(offset + hdr_len + block->hdr.len,
					sizeof(int));
			} else {
				offset = align_to_next_block(++offset, c->mtd_info.writesize);
			}
		}
	}
}

int cal_init(cal *ptr, const char *path) {
	assert(*ptr == NULL);
	cal c = malloc(sizeof(struct cal_s));
	if (errno == ENOMEM) {
		perror("Could not allocate memory for CAL structure");
		goto cleanup;
	}

	/* TODO: make configurable? */
	const char *lockfile_format = "/tmp/cal.%s.lock";
	assert(path != NULL);
	char *devicename = rindex(path, '/');
	assert(devicename != NULL && strlen(devicename) > 1);
	devicename = &devicename[1];
	const size_t lock_len = strlen(lockfile_format) - 2 + strlen(devicename);
	char *lock = malloc(lock_len);
	if (errno == ENOMEM) {
		goto cleanup;
	}
	assert(sprintf(lock, lockfile_format, devicename) == (int)lock_len);
	const int lock_fd = open(lock, O_WRONLY|O_CREAT|O_EXCL, 0666);
	if (lock_fd == -1) {
		fprintf(stderr, "Could not aquire lock file %s: ", lock);
		perror(NULL);
		free(lock);
		goto cleanup;
	}
	close(lock_fd);
	c->lock_file = lock;

	if ((c->mtd_fd = open(path, O_RDWR)) == -1) {
		fprintf(stderr, "Could not open CAL %s: ", path);
		perror(NULL);
		goto cleanup;
	}
	if (ioctl(c->mtd_fd, MEMGETINFO, &c->mtd_info) == -1) {
		perror("ioctl(MEMGETINFO) failed");
		goto cleanup;
	}

	/* TODO: remove debug output */
	puts("normal:");
	scan_blocks(c, MTD_MODE_NORMAL, &c->main_block_list);
	/* TODO: remove debug output */
	puts("user:");
	scan_blocks(c, MTD_MODE_OTP_USER, &c->user_block_list);
	/* TODO: remove debug output */
	puts("factory:");
	scan_blocks(c, MTD_MODE_OTP_FACTORY, &c->wp_block_list);

	if (0) {
cleanup:
		cal_destroy(c);
		return -1;
	}
	*ptr = c;
	return 0;
}

int cal_read_block(
		cal c,
		const char *name,
		void **data,
		uint32_t *len,
		const uint16_t flags) {
	/* TODO: implement this */
	return -1;
}

static void free_blocks(struct conf_block *block) {
	while (block) {
		struct conf_block *prev = block;
		block = block->next;
		free(prev);
	}
}

void cal_destroy(cal c) {
	if (c) {
		free_blocks(c->main_block_list);
		free_blocks(c->user_block_list);
		free_blocks(c->wp_block_list);
		if (c->mtd_fd) {
			close(c->mtd_fd);
		}
		if (c->lock_file) {
			if (unlink(c->lock_file)) {
				fprintf(stderr, "Could not remove lock file %s!"
					" Please, remove it manually: ", c->lock_file);
				perror(NULL);
			}
			free(c->lock_file);
		}
		free(c);
	}
}

/* Old stuff below, will be removed soon */

static size_t skip_and_read(const int fd, void *buf, const size_t bytes_read,
		const size_t bytes_skip) {
	const size_t total_read = bytes_skip + bytes_read;
	char data[total_read];
	ssize_t ret;
	ret = read(fd, &data, total_read);
	assert((size_t)ret == total_read);
	memcpy(buf, &data[bytes_skip], bytes_read);
	return ret - bytes_skip;
}

size_t get_from_mtd(const char *path,
		void *buf,
		const off_t seek_to,
		const size_t bytes_read,
		const size_t bytes_skip,
		const int select_mode) {
	ssize_t ret;
	int fd;
	if ((fd = open(path, O_RDONLY)) == -1) {
		fprintf(stderr, "Could not open %s: ", path);
		perror(NULL);
		return -1;
	}
	/* Sadly, strace doesn't know mtd ioctl codes and misinterprets them
		as mtrr codes. Here's mapping table
		(first column is real code, second - what strace thinks it is):
		MEMGETINFO				MTRRIOC_SET_ENTRY
		MEMERASE					MTRRIOC_DEL_ENTRY
		MEMWRITEOOB				MTRRIOC_GET_ENTRY
		MEMREADOOB				MTRRIOC_KILL_ENTRY
		MEMLOCK						MTRRIOC_ADD_PAGE_ENTRY
		MEMUNLOCK					MTRRIOC_SET_PAGE_ENTRY
		MEMGETREGIONCOUNT MTRRIOC_DEL_PAGE_ENTRY
		MEMGETREGIONINFO	MTRRIOC_GET_PAGE_ENTRY
		MEMSETOOBSEL			MTRRIOC_KILL_PAGE_ENTRY
		MEMGETOOBSEL			0x80c84d0a
		MEMGETBADBLOCK		0x40084d0b
		MEMSETBADBLOCK		0x40084d0c
		OTPSELECT					0x80044d0d
		OTPGETREGIONCOUNT 0x40044d0e
		OTPGETREGIONINFO	0x400c4d0f
		OTPLOCK						0x800c4d10
		ECCGETLAYOUT			0x81484d11
		ECCGETSTATS				0x80104d12
		MTDFILEMODE				0x4d13
	*/
	if (select_mode != -1) {
		if (ioctl(fd, OTPSELECT, &select_mode) == -1) {
			perror(NULL);
			close(fd);
			return -1;
		}
	}
	if (lseek(fd, seek_to, SEEK_SET) == -1) {
		perror(NULL);
		close(fd);
		return -1;
	}
	ret = skip_and_read(fd, buf, bytes_read, bytes_skip);
	close(fd);
	return ret;
}

ssize_t get_from_dsme(const char *path,
		const void *request,
		const size_t bytes_send,
		void *buf,
		const size_t bytes_read,
		const size_t bytes_skip) {
	int fd;
	ssize_t ret;
	struct sockaddr_un sockaddr;

	/* create socket */
	if ((fd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
		perror("Could not create socket");
		return -1;
	}

	/* connect */
	sockaddr.sun_family = AF_FILE;
	assert(strlen(path) <= sizeof(sockaddr.sun_path));
	/* TODO: test if it works correctly with 108-char path. */
	strcpy(sockaddr.sun_path, path);
	ret = sizeof(sockaddr.sun_path) + sizeof(sockaddr.sun_family);
	if (connect(fd, (struct sockaddr *)&sockaddr, ret) == -1) {
		perror("Could not connect to socket");
		close(fd);
		return -1;
	}

	/* send request */
	ret = write(fd, request, bytes_send);
	if (ret == -1) {
		perror("Could not send dsme request");
		close(fd);
		return -1;
	}
	assert((size_t)ret == bytes_send);

	ret = skip_and_read(fd, buf, bytes_read, bytes_skip);
	close(fd);
	return ret;
}
