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

#include <stdint.h>

#define CAL_DEFAULT_PATH "/dev/mtd1"

/* Pointer to CAL internal structure. */
typedef struct cal_s * cal;

/**
	Initializes CAL structure.
	N.B. You MUST call cal_destroy when you finished working with CAL.
	It is guaranteed that no other proccess with modify CAL data by using lock
	file, which is removed when cal_destroy is called.
	Try to minimize time when you hold a lock as other processes may need CAL too.
	@path mtd device path
	@return pointer to CAL structure on success, NULL on error.
*/
cal cal_init(const char *path);

/**
	Reads CAL block data.
	@return 0 on success, other value on error (do not rely on -1).
*/
int cal_read_block(
	cal cal,
	/* Block name */
	const char *name,
	/*
		Pointer to void * that'll be set to block data. You must not modify data.
		Automatically freed when cal_destroy is called.
	*/
	void **data,
	/* Pointer to variable that'll be set to block data length. */
	uint32_t *len,
	/* Some mysterious flags. Not used currently. */
	const uint16_t flags);

/**
	Writes CAL block data.
	@return 0 on success, other value on error (do not rely on -1).
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

/** Closes/frees/cleanups after cal_init. */
void cal_destroy(cal cal);

#endif /* OPENCAL_H */
