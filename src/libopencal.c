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
#include "opencal.h"

#define CAL_BLOCK_HEADER_MAGIC "ConF"
#define CAL_HEADER_VERSION 2
/* TODO: Remove it. Use "/tmp/cal.<devicename>.lock" */
#define CAL_DEFAULT_LOCK_FILE "/tmp/cal.mtd1.lock"

/* On-disk CAL block header structure. */
struct conf_block_header {
	/* Magic header. Set to CAL_BLOCK_HEADER_MAGIC. */
	uint32_t magic;
	/* Header version. Set to CAL_HEADER_VERSION. */
	uint8_t hdr_version;
	/* Block version. If there are multiple blocks with same name,
		only block with highest version number is considered active.
		Block version starts with 0.  */
	uint8_t block_version;
	/* Some mysterious flags. Seen values: 0, 1, 1 << 2, 1 << 3 */
	uint16_t flags;
	/* Block name. TODO: trailing/leading zerofill? */
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
struct cal_s {
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
