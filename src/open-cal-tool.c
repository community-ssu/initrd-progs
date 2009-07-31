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
#include "opencal.h"
#include "config.h"

int main(void) {
	cal c;
	if (cal_init(&c, CAL_DEFAULT_PATH)) {
		return -1;
	}
	void *data;
	uint32_t len;
	if (cal_read_block(c, "r&d_mode", &data, &len, 0)) {
		cal_destroy(c);
		return -1;
	} else {
		assert(len == 1);
		puts(((char *)data)[0] ? "enabled" : "disabled");
	}
	cal_destroy(c);
	return 0;
}
