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

#ifndef OPENDSME_H
#define OPENDSME_H

#define DEFAULT_DSME_PATH "/tmp/dsmesock"

ssize_t get_from_dsme(const char *path,
	const void *request,
	const size_t bytes_send,
	void *buf,
	const size_t bytes_read,
	const size_t bytes_skip);

#endif /* OPENDSME_H */
