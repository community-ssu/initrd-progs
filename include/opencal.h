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

#ifndef OPENCAL_H
#define OPENCAL_H

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

#define CAL_DEFAULT_PATH "/dev/mtd1"

/* Pointer to CAL internal structure. */
typedef struct cal_t * cal;

/*
	Initializes CAL structure.

	N.B. You MUST call cal_destroy when you finished working with
	CAL structure.
	@return 0 on success, -1 on error. Sets errno.
*/
int cal_init(
	/* Pointer to CAL pointer that'll be set to newly created CAL
		structure. Pointer is set only on successful execution. */
	cal *cal,
	/* mtd device path */
	const char *path);

/*
	Reads CAL block data.
	@return 0 on success, -1 on error. Sets errno.
*/
int cal_read_block(
	cal cal,
	/* Block name */
	const char *name,
	/* Pointer to void * that'll be set to block data */
	void **data,
	/* Pointer to variable that'll be set to block data length. */
	uint32_t *len,
	/* Some mysterious flags. Not used currently. */
	uint16_t flags);

/*
	Writes CAL block data.
	@return 0 on success, -1 on error. Sets errno.
*/
int cal_write_block(
	cal cal,
	/* Block name */
	const char *name,
	/* Pointer to block data */
	const void *data,
	/* Data length */
	const uint32_t len,
	/* Some mysterious flags. Not used currently. */
	const uint16_t flags);

/*
	Closed/frees/cleanups after cal_init.
	It is ok to pass NULL.
*/
void cal_destroy(cal cal);

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

#endif /* OPENCAL_H */
