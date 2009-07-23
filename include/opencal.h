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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

#define CAL_DEFAULT_PATH "/dev/mtd1"
#define CAL_BLOCK_HEADER_MAGIC "ConF"
#define CAL_HEADER_VERSION 2
#define CAL_DEFAULT_LOCK_FILE "/tmp/cal.mtd1.lock"

/* On-disk CAL block header structure. */
struct conf_block_header {
	/* Magic header. Set to CAL_BLOCK_HEADER_MAGIC. */
	uint32_t magic;
	/* Header version. Set to CAL_HEADER_VERSION. */
	uint8_t hdr_version;
	/* Block version. If there are multiple blocks with same name,
		only block with highest version number is concidered active.
		Block version starts with 0.  */
	uint8_t block_version;
	/* Some mysterious flags. Seen values: 1, 1 << 2, 1 << 3 */
	uint16_t flags;
	/* Block name. TODO: traling/leading zerofill? */
	char name[16];
	/* Data length. */
	uint32_t len;
	/* CRC(TODO: 32?) for block data. */
	uint32_t data_crc;
	/* CRC(TODO: 32?) for header data. */
	uint32_t hdr_crc;
};

/* Structure describing CAL block. */
struct conf_block {
	/* TODO: Data/header on-disk offset? */
	long unsigned int addr;
	/* Block header. */
	struct conf_block_header hdr;
	/* Block data. */
	void *data;
	/* Pointer to next block (NULL if this block is last). */
	struct conf_block *next;
};

/* TODO: what's this? */
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

/* Structure describing CAL storage. */
struct cal {
	/* File descriptor. */
	int mtd_fd;
	/* Path to lock file. Use CAL_DEFAULT_LOCK_FILE. */
	char *lock_file;
	struct mtd_info_user mtd_info;
	unsigned int page_size;
	unsigned int eb_size;
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

/*
	Initializes CAL structure.
	Required fields:
	- mtd_fd set to open file descriptor.
	If lock_file is not set, then CAL_DEFAULT_LOCK_FILE is used.

	Acquires lock, populates CAL structure with data from CAL storage.

	N.B. You MUST call cal_finish when you finished working with CAL structure.

	@param cal uninitialized CAL structure.
	@return 0 on success, -1 on error.
*/
int cal_init(struct cal **cal);

int cal_lock_otp_area(struct cal *cal, unsigned int b);

int cal_read_block(
	struct cal *cal,
	const char *name,
	void **ptr,
	unsigned long *len,
	unsigned long flags);

int cal_write_block(
	struct cal *cal,
	const char *name,
	const void *c,
	long unsigned int d,
	long unsigned int e);

/*
	Closed/frees/cleanups after cal_init.
	It is ok to pass uninitialized CAL structure (and even NULL).
*/
void cal_finish(struct cal *cal);

/*
	Naive fixed offset based CAL access.
	Do not EVER use it, it'll be deleted as soon as
	stuff above will start to work.
	TODO: delete this.
*/
size_t get_from_mtd(
	const char *path,
	void *buf, const off_t seek_to,
	const size_t bytes_read,
	const size_t bytes_skip,
	const int mode);