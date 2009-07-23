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

/* Pointer to CAL internal structure. */
typedef struct cal_t * cal;

/*
	Initializes CAL structure.
	If lock_file is not set, then CAL_DEFAULT_LOCK_FILE is used.

	Acquires lock, populates CAL structure with data from CAL storage.

	N.B. You MUST call cal_destroy when you finished working with CAL structure.

	@param path mtd device path.
	@return 0 on success, -1 on error.
*/
cal cal_init(char *path);

int cal_read_block(
	cal c,
	const char *name,
	void **ptr,
	unsigned long *len,
	unsigned long flags);

int cal_write_block(
	cal c,
	const char *name,
	const void *ptr,
	long unsigned int d,
	long unsigned int e);

/*
	Closed/frees/cleanups after cal_init.
	It is ok to pass uninitialized CAL structure (and even NULL).
*/
void cal_destroy(cal c);

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
