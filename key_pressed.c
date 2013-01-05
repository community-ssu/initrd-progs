/*
    key_pressed.c - check key is pressed within the given time
    Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>

int main(int argc, char * argv[]) {

	fd_set rfds;
	char buf[1024];
	struct termios termios_save;
	struct termios termios_p;
	struct timeval tv;
	int timeout = -1;
	int ret;

	if ( argc == 2 )
		timeout = strtol(argv[1], NULL, 10);

	if ( timeout < 0 ) {
		fprintf(stderr, "Usage: %s TIME\n" \
			"\treturn 1, if any key is pressed within the given time\n" \
			"\treturn 0, if timeout occurs and no key is pressed\n", argv[0]);
		return -1;
	}

	tcgetattr(0, &termios_save);

	memcpy(&termios_p, &termios_save, sizeof(struct termios));
	termios_p.c_lflag &= ~ECHO;
	termios_p.c_lflag &= ~ICANON;
	tcsetattr(0, 0, &termios_p);

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	ret = select(1, &rfds, NULL, NULL, &tv);

	if (ret == 1 && FD_ISSET(0, &rfds))
		read(0, buf, sizeof(buf));

	tcsetattr(0, 0, &termios_save);
	return ret;

}
