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

size_t get_from_mtd(const char *path, void *buf, const off_t seek_to,
		const size_t bytes_read, const size_t bytes_skip, const int select_mode) {
	ssize_t ret;
	int fd;
	if ((fd = open(path, O_RDONLY)) == -1) {
		fprintf(stderr, "Could not open %s: ", path);
		perror(NULL);
		return -1;
	}
	/* Sadly, strace doesn't know mtd ioctl codes and misinterprets them as mtrr codes.
		Here's mapping table
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

ssize_t get_from_dsme(const char *path, const void *request, const size_t bytes_send,
		void *buf, const size_t bytes_read, const size_t bytes_skip) {
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
