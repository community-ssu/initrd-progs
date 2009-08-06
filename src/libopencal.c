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
#include <mtd/mtd-user.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "opencal.h"

/*
	Sadly, strace doesn't know mtd ioctl codes and misinterprets them
	as mtrr codes. Here's mapping table
	(first column is real code, second - what strace thinks it is):
	MEMGETINFO			MTRRIOC_SET_ENTRY
	MEMERASE			MTRRIOC_DEL_ENTRY
	MEMWRITEOOB			MTRRIOC_GET_ENTRY
	MEMREADOOB			MTRRIOC_KILL_ENTRY
	MEMLOCK				MTRRIOC_ADD_PAGE_ENTRY
	MEMUNLOCK			MTRRIOC_SET_PAGE_ENTRY
	MEMGETREGIONCOUNT   MTRRIOC_DEL_PAGE_ENTRY
	MEMGETREGIONINFO	MTRRIOC_GET_PAGE_ENTRY
	MEMSETOOBSEL		MTRRIOC_KILL_PAGE_ENTRY
	MEMGETOOBSEL		0x80c84d0a
	MEMGETBADBLOCK		0x40084d0b
	MEMSETBADBLOCK		0x40084d0c
	OTPSELECT			0x80044d0d
	OTPGETREGIONCOUNT   0x40044d0e
	OTPGETREGIONINFO	0x400c4d0f
	OTPLOCK				0x800c4d10
	ECCGETLAYOUT		0x81484d11
	ECCGETSTATS			0x80104d12
	MTDFILEMODE			0x4d13
*/

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

#define CAL_HEADER_LEN sizeof(struct conf_block_header)

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
	/* CRC32 for block data. */
	uint32_t data_crc;
	/* CRC32 for header data. */
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

/** Structure, describing CAL area eraseblock. */
struct cal_eb {
	/* Eraseblock offset in CAL area */
	uint32_t addr;
	/* Set to 1 if eraseblock doesn't have any conf blocks, 0 otherwise */
	int empty;
	struct cal_eb *next;
};

struct cal_area {
	/* CAL area name ('main' or 'user') */
	const char *name;
	/* Area eraseblocks */
	struct cal_eb *ebs;
};

/** Structure describing CAL storage. */
struct cal_s {
	/* File descriptor. */
	int mtd_fd;
	/* Path to lock file. Set only when lock was sucessfully acquired */
	char *lock_file;
	/* MTD partition info. See <mtd/mtd-abi.h> */
	struct mtd_info_user mtd_info;
	/* Valid configuration block in main CAL area */
	struct conf_block *main_block_list;
	/* Valid configuration block in 'user' CAL area */
	struct conf_block *user_block_list;
	/* Structure, describing main CAL area (MTD_MODE_NORMAL) */
	struct cal_area main_area;
	/* Structure, describing 'user' CAL area (MTD_MODE_OTP_USER) */
	struct cal_area user_area;
};

/**
	Aligns offset to next block.
	Doesn't change it if offset is already aligned.
	@offset current offset
	@bs block size to use for alignment
	@return block-aligned offset
*/
static inline off_t align_to_next_block(const off_t offset, const int bs) {
	return (offset + bs - 1) & ~(bs - 1);
}

/**
	Scans CAL storage and fills block list with found blocks.
	Doesn't free successfully read blocks on error.
	@c pointer to CAL struct.
	@select_mode MTD select mode. See OTPSELECT ioctl.
	@list block list to add found blocks to.
	@return 0 on success, other value on error.
*/
static int scan_blocks(
		const cal c,
		const int select_mode,
		struct conf_block **list) {
	if (ioctl(c->mtd_fd, OTPSELECT, &select_mode)) {
		perror("ioctl(OTPSELECT)");
		return -1;
	}
	size_t size;
	if (select_mode == MTD_MODE_NORMAL) {
		size = c->mtd_info.size;
	} else {
		/*
			All this crap is required to determine OTP area size.
			Have a better way?
		*/
		int reg_count;
		if (ioctl(c->mtd_fd, OTPGETREGIONCOUNT, &reg_count) < 0) {
			perror("ioctl(OTPGETREGIONCOUNT)");
			return -1;
		}
		if (reg_count <= 0) {
			fputs("No regions\n", stderr);
			return 0;
		}
		struct otp_info info[reg_count];
		if (ioctl(c->mtd_fd, OTPGETREGIONINFO, &info)	< 0) {
			perror("ioctl(OTPGETREGIONCOUNT)");
			return -1;
		}
		size = info[reg_count - 1].start + info[reg_count - 1].length;

	}
	struct conf_block *prev = NULL;
	for (off_t offset = 0; (size_t)offset < size;) {
		struct conf_block *block = malloc(sizeof(struct conf_block));
		if (errno == ENOMEM) {
			perror("malloc failed");
			return -1;
		}
		if (lseek(c->mtd_fd, offset, SEEK_SET) != offset) {
			fprintf(stderr,  "Could not seek to %u: ", (uint32_t)offset);
			perror(NULL);
			free(block);
			return -1;
		}
		const ssize_t ret = read(c->mtd_fd, &block->hdr, CAL_HEADER_LEN);
		if (ret == -1) {
			perror("CAL read error");
			free(block);
			return -1;
		} else if ((size_t)ret != CAL_HEADER_LEN) {
			fputs("Could not fully read CAL block header\n", stderr);
			free(block);
			return -1;
		}
		const size_t magic_len = strlen(CAL_BLOCK_HEADER_MAGIC);
		if (memcmp(&block->hdr.magic, CAL_BLOCK_HEADER_MAGIC, magic_len) != 0) {
			/* Block should be empty. */
			free(block);
			if (offset % c->mtd_info.erasesize == 0) {
				/*
					If first conf block in eraseblock is empty, we assume whole
					eraseblock to be empty.
				*/
				offset = align_to_next_block(++offset, c->mtd_info.erasesize);
			} else {
				/* Align to writesize boundary after empty block */
				offset = align_to_next_block(++offset, c->mtd_info.writesize);
			}
		} else {
			if (block->hdr.hdr_version != CAL_HEADER_VERSION) {
				free(block);
				fprintf(stderr, "Unknown CAL block version %u at offset %u\n",
					block->hdr.hdr_version, (uint32_t)offset);
				return -1;
			}
			block->addr = offset;
			if (prev == NULL) {
				*list = block;
			} else {
				prev->next = block;
			}
			prev = block;
			if (block->hdr.flags & CAL_BLOCK_FLAG_VARIABLE_LENGTH) {
				/*
					We align reads to word boundary if block has
					CAL_BLOCK_FLAG_VARIABLE_LENGTH flag set.
				*/
				offset = align_to_next_block(
					offset + CAL_HEADER_LEN + block->hdr.len,
					sizeof(void *));
			} else {
				offset = align_to_next_block(++offset, c->mtd_info.writesize);
			}
		}
	}
	return 0;
}

/** See cal_init in opencal.h for documentation. */
cal cal_init(const char *path) {
	cal c = malloc(sizeof(struct cal_s));
	if (errno == ENOMEM) {
		perror("Could not allocate memory for CAL structure");
		goto cleanup;
	}

	/* TODO: make configurable? */
	const char *lockfile_format = "/tmp/cal.%s.lock";
	assert(path != NULL);
	char *devicename = rindex(path, '/');
	/* TODO: replace assert with check and return */
	assert(devicename != NULL && strlen(devicename) > 1);
	devicename = &devicename[1];
	/* -2 because of '%s' placeholder. */
	const size_t lock_len = strlen(lockfile_format) - 2 + strlen(devicename);
	/* +1 for trailing \0. */
	char *lock = malloc(lock_len + 1);
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
	/*
		There's one more area, accessible via MTD_MODE_OTP_FACTORY. But for some
		reason, OTPGETREGIONINFO reports bigger size for it than I can read().
		It starts returning 0 bytes read after some offset.
		Luckily, 'factory' area doesn't contain any data (dsme doesn't look at it
		at all, so it is safe to just skip MTD_MODE_OTP_FACTORY.
	*/
	if (scan_blocks(c, MTD_MODE_NORMAL, &c->main_block_list)
			|| scan_blocks(c, MTD_MODE_OTP_USER, &c->user_block_list)) {
		goto cleanup;
	}

	/* TODO: this if (0) is kinda weird */
	if (0) {
cleanup:
		cal_destroy(c);
		return NULL;
	}
	return c;
}

/**
	Searches for block with given name and reads its data if found.
	TODO: return different values when nothing found or error occurred?
	@c pointer to CAL structure.
	@name block name
	@data pointer to pointer that'll be set to read data.
	@len pointer to var that'll be set to data length.
	@existing pointer to list of blocks that'll be searched
		for block with given name.
	@select_mode MTD select mode (see OTPSELECT ioctl).
	@return 0 if block was successfully found and read, other value on error.
*/
static int read_block(
		cal c,
		const char *name,
		void **data,
		uint32_t *len,
		struct conf_block *existing,
		int select_mode) {
	struct conf_block *block = NULL;
	while (existing) {
		if (strcmp(name, existing->hdr.name) == 0
				&& (block == NULL
				/* Only block with highest version is considered active */
				|| existing->hdr.block_version > block->hdr.block_version)) {
			block = existing;
		}
		existing = existing->next;
	}
	if (block) {
		if (block->data == NULL) {
			if (ioctl(c->mtd_fd, OTPSELECT, &select_mode)) {
				perror("ioctl(OTPSELECT)");
				return -1;
			}
			if (lseek(c->mtd_fd, block->addr + CAL_HEADER_LEN, SEEK_SET) == -1) {
				perror("lseek");
				return -1;
			}
			block->data = malloc(block->hdr.len);
			if (errno == ENOMEM) {
				perror("malloc");
				return -1;
			}
			const ssize_t ret = read(c->mtd_fd, block->data, block->hdr.len);
			if (ret == -1 || (size_t)ret != block->hdr.len) {
				perror("read error");
				free(block->data);
				return -1;
			}
		}
		/* TODO: is it ok that our user can modify data via his pointer? */
		*data = block->data;
		*len = block->hdr.len;
		return 0;
	} else {
		return -1;
	}
}

/** See cal_read_block in opencal.h for documentation. */
int cal_read_block(
		cal c,
		const char *name,
		void **data,
		uint32_t *len,
		const uint16_t flags) {
	assert(c != NULL);
	assert(name != NULL);
	assert(data != NULL);
	assert(len != NULL);
	if (read_block(c,name,data,len,c->main_block_list,MTD_MODE_NORMAL)
		&& read_block(c,name,data,len,c->user_block_list,MTD_MODE_OTP_USER)) {
		fprintf(stderr, "No block %s found\n", name);
		return -1;
	} else {
		return 0;
	}
}

/** See cal_read_block in opencal.h for documentation. */
int cal_write_block(
		cal c,
		const char *name,
		const void *data,
		const uint32_t len,
		const uint16_t flags) {
	assert(c != NULL);
	assert(name != NULL);
	assert(data != NULL);
	const uint32_t max_data_len = c->mtd_info.writesize - CAL_HEADER_LEN;
	if (len > max_data_len) {
		fprintf(stderr, "Can't write data longer than %u bytes\n", max_data_len);
		return -1;
	}
	/*
		TODO: implement this.
		Plan:
		1. When scanning for blocks, remember empty space. Sort them by size.
		2. First, try to find smallest empty space that given data fits.
		3. If found, write to it. Return success.
		4. If not found, search for eraseblock with largest (free + filled
		with inactive blocks) space that is large enough to store given data.
		5. If not found, return error.
		6. Read all active blocks from found eraseblock into mem.
		7. Erase that eraseblock and write all its saved active blocks.
		8. Restart from 1. Assert that we stop at step 3 to prevent endless loop.
	*/
	fputs("not implemented yet\n", stderr);
	return -1;
}

/**
	Frees all blocks in given list.
	@block pointer to first block in a list. Can be NULL.
*/
static void free_blocks(struct conf_block *block) {
	while (block) {
		struct conf_block *prev = block;
		block = block->next;
		free(prev->data);
		free(prev);
	}
}

/** See cal_destroy in opencal.h for documentation. */
void cal_destroy(cal c) {
	if (c) {
		free_blocks(c->main_block_list);
		free_blocks(c->user_block_list);
		if (c->mtd_fd) {
			/* TODO: check return code? And what to do? */
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
