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
#include <stdbool.h>

/* Dirty hack to make it build with vanilla kernel headers. */
#ifndef __u32
    #define __u32 uint32_t
#endif
#ifndef __u8
    #define __u8 uint8_t
#endif

#include <mtd/mtd-user.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "crc32.h"
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
#define CAL_HEADER_LEN sizeof(struct conf_block_header)
#define CAL_BLOCK_NAME_LEN 16

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
	char name[CAL_BLOCK_NAME_LEN];
	/* Data length. */
	uint32_t len;
	/* CRC32 for block data. */
	uint32_t data_crc;
	/* CRC32 for header data. */
	uint32_t hdr_crc;
} __attribute__((packed));

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
};

/**
	Aligns offset to next block.
	Doesn't change it if offset is already aligned.
	@offset current offset
	@bs block size to use for alignment
	@return block-aligned offset
*/
static inline off_t __attribute__((const))
		align_to_next_block(const off_t offset, const size_t bs) {
	return (offset + (off_t)bs - 1) & ~((off_t)bs - 1);
}

/*
	Calculates CRC32 value for header data of given block.
	@block CAL conf block.
	@return header crc.
*/
static inline uint32_t __attribute__((nonnull,warn_unused_result))
		conf_block_header_crc(const struct conf_block *block) {
	const size_t len = CAL_HEADER_LEN - sizeof(block->hdr.hdr_crc);
	return crc32(0L, &block->hdr, len);
}

/**
	Scans CAL storage and fills block list with found blocks.
	Doesn't free successfully read blocks on error.
	@c pointer to CAL struct.
	@select_mode MTD select mode. See OTPSELECT ioctl.
	@list block list to add found blocks to.
	@return 0 on success, other value on error.
*/
static int __attribute__((nonnull(1),warn_unused_result))
		scan_blocks(
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
		if (!reg_count) {
			fputs("No regions\n", stderr);
			return -1;
		}
		struct otp_info info[reg_count];
		if (ioctl(c->mtd_fd, OTPGETREGIONINFO, &info) < 0) {
			perror("ioctl(OTPGETREGIONCOUNT)");
			return -1;
		}
		size = info[reg_count - 1].start + info[reg_count - 1].length;

	}
	struct conf_block *prev = NULL;
	for (off_t offs = 0; (size_t)offs < size;) {
		struct conf_block *block = malloc(sizeof(struct conf_block));
		if (!block) {
			perror("malloc failed");
			return -1;
		}
		const ssize_t ret = pread(c->mtd_fd, &block->hdr, CAL_HEADER_LEN, offs);
		if (ret == -1 || (size_t)ret != CAL_HEADER_LEN) {
			perror("CAL read error");
			free(block);
			return -1;
		}
		size_t align_to;
		if (memcmp(&block->hdr.magic, CAL_BLOCK_HEADER_MAGIC, 4)) {
			/* Block should be empty. */
			free(block);
			if (offs % (int)c->mtd_info.erasesize == 0) {
				/*
					If first conf block in eraseblock is empty, we assume whole
					eraseblock to be empty.

					TODO: is the same true if we encounter _any_ empty 2kb block?
					It should work because writes are performed sequentally.
				*/
				align_to = c->mtd_info.erasesize;
			} else {
				align_to = c->mtd_info.writesize;
			}
		} else {
			if (block->hdr.hdr_version != CAL_HEADER_VERSION) {
				fprintf(stderr, "Unknown CAL block version %u at offset 0x%x\n",
					block->hdr.hdr_version, (int)offs);
				free(block);
				return -1;
			}
			const uint32_t crc = conf_block_header_crc(block);
			if (crc == block->hdr.hdr_crc) {
				block->addr = offs;
				if (prev) {
					prev->next = block;
				} else {
					*list = block;
				}
				/*
				printf("block %s version %u at 0x%x flags %u\n",
					block->hdr.name, block->hdr.block_version, (int)block->addr,
					block->hdr.flags);
				*/
				prev = block;
				align_to = c->mtd_info.writesize;
			} else {
				fprintf(stderr, "Skipped block with invalid header crc at"
					" offset 0x%x. Expected 0x%x but got 0x%x.\n",
					(int)offs, crc, block->hdr.hdr_crc);
				free(block);
				align_to = c->mtd_info.writesize;
			}
		}
		offs = align_to_next_block(++offs, align_to);
	}
	return 0;
}

static char * __attribute__((nonnull,warn_unused_result))
		acquire_lock(const char *device_path) {
	assert(device_path);
	char *devicename = rindex(device_path, '/');
	if (!devicename || !devicename[0] || !devicename[1]) {
		return NULL;
	}
	char *lock = NULL;
	/* TODO: make format configurable? */
	/* ++ to skip leading slash */
	if (asprintf(&lock, "/tmp/cal.%s.lock", ++devicename) == -1) {
		perror("Could not build lock filename");
		return NULL;
	}
	const int lock_fd = open(lock, O_WRONLY|O_CREAT|O_EXCL, 0666);
	if (lock_fd == -1) {
		fprintf(stderr, "Could not acquire lock file %s: ", lock);
		perror(NULL);
		free(lock);
		return NULL;
	}
	if (close(lock_fd)) {
		fprintf(stderr, "Could not close lock file %s: ", lock);
		perror(NULL);
		free(lock);
		return NULL;
	}
	return lock;
}

/** See cal_init in opencal.h for documentation. */
cal cal_init(const char *path) {
	cal c = malloc(sizeof(struct cal_s));
	if (!c) {
		perror("malloc failed");
		goto cleanup;
	}
	if ((c->lock_file = acquire_lock(path)) == NULL) goto cleanup;
	if ((c->mtd_fd = open(path, O_RDWR)) == -1) {
		fprintf(stderr, "Could not open CAL %s: ", path);
		perror(NULL);
		goto cleanup;
	}
	if (ioctl(c->mtd_fd, MEMGETINFO, &c->mtd_info) < 0) {
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

  return c;

cleanup:
	cal_destroy(c);
	return NULL;
}

/*
	Searches for active block with given name in given block list.
	@name block name to search.
	@block list of existing blocks.
	@return active block with given name or NULL if no such block found.
*/
static struct conf_block * __attribute__((nonnull(1),warn_unused_result))
		find_block(const char *name, struct conf_block *block) {
	struct conf_block *result = NULL;
	while (block) {
		if (!strncmp(name, block->hdr.name, CAL_BLOCK_NAME_LEN)
				&& (!result
					/* Only block with highest version is considered active */
					|| block->hdr.block_version > result->hdr.block_version)) {
			result = block;
		}
		block = block->next;
	}
	return result;
}

/*
	Checks block name validity.
	@name block name to be validated.
	@return 0 if block name is valid, otherwise -1.
*/
static bool __attribute__((nonnull,warn_unused_result))
		validate_block_name(const char *name) {
	if (!name) {
		fputs("Block name cannot be NULL\n", stderr);
		return false;
	}
	if (!name[0]) {
		fputs("Empty name is not allowed\n", stderr);
		return false;
	}
	if (strlen(name) > CAL_BLOCK_NAME_LEN) {
		fputs("Too long block name\n", stderr);
		return false;
	}
	return true;
}

/**
	Reads block data from MTD into block->data.
	@c pointer to CAL structure.
	@block block, whose data is to be read name
	@select_mode MTD select mode (see OTPSELECT ioctl).
	@return 0 if block was successfully read, other value on error.
*/
static int __attribute__((nonnull,warn_unused_result))
		read_block_data(
		const cal c,
		struct conf_block *block,
		const int select_mode) {
	if (!block->data) {
		if (ioctl(c->mtd_fd, OTPSELECT, &select_mode)) {
			perror("ioctl(OTPSELECT)");
			return -1;
		}
		block->data = malloc(block->hdr.len);
		if (!block->data) {
			perror("malloc failed");
			return -1;
		}
		const off_t offset = block->addr + (int)CAL_HEADER_LEN;
		const ssize_t ret = pread(c->mtd_fd, block->data, block->hdr.len, offset);
		if (ret == -1 || (size_t)ret != block->hdr.len) {
			perror("read error");
			free(block->data);
			block->data = NULL;
			return -1;
		}
		const uint32_t crc = crc32(0L, block->data, block->hdr.len);
		if (crc != block->hdr.data_crc) {
			fprintf(stderr, "Invalid data crc at offset 0x%x."
				" Expected 0x%x but got 0x%x\n",
				(size_t)block->addr, crc, block->hdr.data_crc);
			free(block->data);
			block->data = NULL;
			return -1;
		}
	}
	return 0;
}

/** See cal_read_block in opencal.h for documentation. */
int cal_read_block(
		cal c,
		const char *name,
		void **data,
		uint32_t *len,
		const uint16_t flags __attribute__((unused))) {
	assert(c);
	if (!validate_block_name(name)) return -1;
	assert(data);
	assert(len);
	struct conf_block *block;
	if ((block = find_block(name, c->user_block_list)) != NULL) {
		if (read_block_data(c, block, MTD_MODE_OTP_USER)) return -1;
	} else if ((block = find_block(name, c->main_block_list)) != NULL) {
		if (read_block_data(c, block, MTD_MODE_NORMAL)) return -1;
	} else {
		fprintf(stderr, "No block %s found\n", name);
		return -1;
	}
	/* TODO: is it ok that our user can modify data via his pointer? */
	*data = block->data;
	*len = block->hdr.len;
	return 0;
}

/** See cal_read_block in opencal.h for documentation. */
int cal_write_block(
		cal c,
		const char *name,
		const void *data,
		const uint32_t len,
		const uint16_t flags __attribute__((unused))) {
	assert(c);
	if (!validate_block_name(name)) return -1;
	assert(data);
	assert(c->mtd_info.writesize >= CAL_HEADER_LEN);
	char buf[c->mtd_info.writesize];
	const size_t max_data_len = sizeof(buf) - CAL_HEADER_LEN;
	if (len > max_data_len) {
		fprintf(stderr, "Can't write data longer than %u bytes\n", max_data_len);
		return -1;
	}
	/*
		Check that 'user' area doesn't contain block with given name.
		It is important because if we write such block, there is no way to
		return things to previous state 'cause version number is only increased.
	*/
	if (find_block(name, c->user_block_list)) {
		fprintf(stderr, "Tried to overwrite block '%s' from 'user' area.\n",
			name);
		return -1;
	}
	struct conf_block *prev = find_block(name, c->main_block_list);
	if (prev
			&& prev->hdr.len == len
			&& !read_block_data(c, prev, MTD_MODE_NORMAL)
			&& !memcmp(data, prev->data, len)) {
		/* Active block already has same data. */
		return 0;
	} else if (!prev) {
		fputs("No previous block, don't know where to write", stderr);
		return -1;
	}
	/* New block is written right after previous one by default. */
	const off_t offset = align_to_next_block(prev->addr + 1, sizeof(buf));
	const off_t end = offset + (off_t)sizeof(buf);
	if ((prev->next == NULL && end > (off_t)c->mtd_info.size)
			|| (prev->next != NULL && end > (off_t)prev->next->addr)) {
		fputs("Block doesn't fit", stderr);
		return -1;
	}

	const int select_mode = MTD_MODE_NORMAL;
	if (ioctl(c->mtd_fd, OTPSELECT, &select_mode)) {
		perror("ioctl(OTPSELECT)");
		return -1;
	}
	/* Found empty space, write to it. */
	struct conf_block *block = malloc(sizeof(struct conf_block));
	if (!block) {
		perror("malloc failed");
		return -1;
	}
	memcpy(&block->hdr.magic, CAL_BLOCK_HEADER_MAGIC, 4);
	block->hdr.hdr_version = CAL_HEADER_VERSION;
	/*
		If active block with same name found, set new block
		version to old block version + 1. Otherwise, set version to 0.
	*/
	block->hdr.block_version = (uint8_t)(prev ? prev->hdr.block_version + 1 : 0);
	block->hdr.flags = prev->hdr.flags;
	memcpy(block->hdr.name, name, strlen(name));
	memset(&block->hdr.name[strlen(name)], 0, CAL_BLOCK_NAME_LEN - strlen(name));
	block->hdr.len = len;
	block->hdr.data_crc = crc32(0L, data, len);
	block->hdr.hdr_crc = conf_block_header_crc(block);
	block->addr = offset;
	block->data = malloc(len);
	if (!block->data) {
		perror("malloc failed");
		free(block);
		return -1;
	}
	memcpy(block->data, data, len);
	memcpy(buf, &block->hdr, CAL_HEADER_LEN);
	memcpy(&buf[CAL_HEADER_LEN], data, len);
	memset(&buf[CAL_HEADER_LEN + len], 0xFF, sizeof(buf) - CAL_HEADER_LEN + len);
	fprintf(stderr, "Writing new block at offset 0x%x\n", (int)offset);
	const ssize_t ret = pwrite(c->mtd_fd, buf, sizeof(buf), offset);
	if (ret == -1 || (size_t)ret != sizeof(buf)) {
		perror("write failed");
		free(block->data);
		free(block);
		return -1;
	}
	block->next = prev->next;
	prev->next = block;

	/*
		TODO: implement this.
		4. If not found, search for eraseblock with largest free + filled
		with inactive blocks block count.
		5. If not found, return error (no space left).
		6. Read all active blocks data from found eraseblock into mem.
		7. Erase that eraseblock.
		8. Iterate over blocks from erased area; free() inactive,
		write active and fix their stored on-disk addr.
		9. Write given data to following block after last written,
		add it to in-mem block list.
	*/
	return 0;
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
	assert(c);
	free_blocks(c->main_block_list);
	free_blocks(c->user_block_list);
	if (c->mtd_fd && close(c->mtd_fd)) perror("Could not close CAL file");
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
