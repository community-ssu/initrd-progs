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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

const char *default_dsme_path = "/tmp/dsmesock";
const char *default_direct_access_path = "/dev/mtd1";

size_t skip_and_read(const int fd, void *buf, const size_t bytes_read,
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
		Here's mapping table (first column is real code, second - what strace thinks it is):
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

ssize_t write_to(const char *path, const void *value, const size_t len) {
	int f;
	ssize_t ret;
	if ((f = open(path, O_WRONLY)) == -1) {
		fprintf(stderr, "Could not open %s: ", path);
		perror(NULL);
		return -1;
	}
	ret = write(f, value, len);
	if (ret == -1) {
		fprintf(stderr, "Could not write data to %s: ", path);
		perror(NULL);
	} else if ((size_t)ret != len) {
		fprintf(stderr, "Could write only %zd bytes out of %zd: ", ret, len);
		perror(NULL);
	}
	close(f);
	return ret;
}

void print_start(const char *msg) {
	printf(msg);
	fflush(stdout);
}

void print_end(const ssize_t result) {
	if (result != -1) {
		puts("[OK]");
	}
}

/* Country */

void set_default_country() {
	/* const char *default_country_req
		= " \0\0\0\0\22\0\0pp_data\0\0\0\0\0\0\0\0\0\0\0\0\0\10\211\0\0"; */
	/* TODO: at least UK tablets have 0x10 instead of 0x30 */
	/* The problem is that i can't figure out which byte they are using */
	print_start("Pushing default country...");
	print_end(write_to("/sys/devices/platform/wlan-omap/default_country", "0\0\0\0", 4));
}

/* MAC */

ssize_t get_mac_direct(const char *path, void *buf, const size_t len) {
	return get_from_mtd(path, buf, 36, len, 4, MTD_OTP_USER);
}

ssize_t get_mac_from_dsme(const char *path, void *buf, const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-mac\0\0\0\0\0\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 36);
}

void set_mac(const char *path, ssize_t (*get)(const char *, void *, const size_t len)) {
	const size_t mac_len = 6;
	const char *file = "/sys/devices/platform/wlan-omap/cal_mac_address";
	char mac_address[mac_len];
	char input[24];

	print_start("Pushing MAC address...");
	if (get(path, input, sizeof(input)) == sizeof(input)) {
		size_t i;
		for (i = 0; i < mac_len; ++i) {
			const size_t idx = 4 * i;
			assert(idx < sizeof(input));
			mac_address[i] = input[idx];
		}
		print_end(write_to(file, mac_address, mac_len));
	}
}

/* IQ values */

ssize_t get_iq_values_direct(const char *path, char *buf, const size_t len) {
	ssize_t ret;
	/* TODO: is it correct? */
	memcpy(buf, "l\0\0\0", 4);
	ret = get_from_mtd(path, &buf[4], 55332, len - 4, 0, -1);
	return ret == -1 ? ret : ret + 4;
}

ssize_t get_iq_values_from_dsme(const char *path, char *buf, const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-iq-align\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 28);
}

void set_iq_values(const char *path,
		ssize_t (*get)(const char *, char *, const size_t len)) {
	/* 14 (items + 1 empty) * 8 (read_item_len) */
	char input[112];
	print_start("Pushing IQ tuned values...");
	if (get(path, input, sizeof(input)) == sizeof(input)) {
		const size_t read_item_len = 8;
		/* + 2 because two bytes are used for item prefix */
		const size_t item_len = read_item_len + 2;
		/* 10 (prefix + 8 bytes of data) * 13 (items) */
		char iq[130];
		for (size_t i = 0; i < sizeof(iq) / item_len; ++i) {
			/* (i + 1) because there's an empty item in input */
			const size_t read_offset = (i + 1) * read_item_len;
			size_t write_offset = item_len * i;
			if (i == 0) {
				iq[write_offset] = input[0];
			} else {
				iq[write_offset] = iq[write_offset - item_len] + 5;
			}
			write_offset++;
			iq[write_offset++] = '\t';
			memcpy(&iq[write_offset], &input[read_offset], read_item_len);
		}
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_iq", iq, sizeof(iq)));
	}
}

/* TX curve data */

ssize_t get_tx_curve_direct(const char *path, void *buf, const size_t len) {
	return get_from_mtd(path, buf, 57380, len, 14, -1);
}

ssize_t get_tx_curve_from_dsme(const char *path, void *buf, const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-tx-gen2\0\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 46);
}

void set_tx_curve(const char *path,
		ssize_t (*get)(const char *, void *, const size_t len)) {
	/* 38 * 13 (items) */
	char input[494];
	print_start("Pushing TX tuned values...");
	if (get(path, input, sizeof(input)) == sizeof(input)) {
		const size_t read_item_len = 38;
		const size_t sep_len = 4;
		const char *sep = "\f\0 \2";
		const size_t prefix_len = 4;
		const size_t item_len = sep_len + read_item_len;
		/* 4 + (4 + 38) * 13 */
		char tx_curve[550];
		memcpy(tx_curve, "\3\0\6\0", prefix_len);
		for (size_t i = 0; i < (sizeof(tx_curve) - prefix_len) / item_len; ++i) {
			const char *src_addr = &input[read_item_len * i];
			char *dst_addr = &tx_curve[4 + item_len * i];
			memcpy(dst_addr, src_addr, 2);
			memcpy(&dst_addr[2], sep, sep_len);
			memcpy(&dst_addr[2 + sep_len], &src_addr[2], read_item_len - 2);
		}
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_pa_curve_data",
			tx_curve, sizeof(tx_curve)));
	}
}

/* TX limits */
void set_tx_limits() {
	/* TODO: UK tablets have a bit different value. I think there's a conditional switch
		based on country code, because it doesn't have any additional input. */
	print_start("Pushing TX limits...");
	print_end(write_to("/sys/devices/platform/wlan-omap/cal_output_limits",
		"\1\0\1\0"
		"l\t\3\n\20\1\340\0\340\0\320\0\320\0"
		"q\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"v\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"{\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\200\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\205\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\212\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\217\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\224\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\231\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\236\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\243\t\3\n\20\1\360\0\360\0\320\0\320\0"
		"\250\t\3\n\20\1\360\0\360\0\320\0\320\0",
		/* 4 + 14 * 13 */
		186));
}

/* RX tuned values */

void set_rx_values() {
	print_start("Pushing RX tuned values...Using default values ");
	print_end(write_to("/sys/devices/platform/wlan-omap/cal_rssi",
		"\1\0"
		"l\t\n\1r\376\32\0"
		"q\t\n\1r\376\32\0"
		"v\t\n\1r\376\32\0"
		"{\t\n\1r\376\32\0"
		"\200\t\n\1r\376\32\0"
		"\205\t\n\1r\376\32\0"
		"\212\t\n\1r\376\32\0"
		"\217\t\n\1r\376\32\0"
		"\224\t\n\1r\376\32\0"
		"\231\t\n\1r\376\32\0"
		"\236\t\n\1r\376\32\0"
		"\243\t\n\1r\376\32\0"
		"\250\t\n\1r\376\32\0",
		/* 2 + 8 * 13 */
		106));
}

int usage(const char *progname) {
	fprintf(stderr, "Usage: %s [-d] [PATH]\n"
		"  -d\tIf specified, data is read directly from mtd partition"
		" instead of dsme server socket.\n"
		"  PATH\tSpecifies where path to mtd partition if -d option is used"
		" or to dsme server socket path otherwise.\n"
		"\tIf no value is specified, %s is used in direct access mode and %s in dsme.\n",
		progname, default_direct_access_path, default_dsme_path);
	return EXIT_FAILURE;
}

int main(const int argc, char *argv[]) {
	const char *progname = argv[0];
	const char *path;
	int direct = 0;
	int verbose = 0;

	while (1) {
		int opt;
		if ((opt = getopt(argc, argv, "dvh?")) == -1) {
			break;
		}
		switch(opt) {
			case 'd':
				direct = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				return usage(progname);
		}
	}

	assert(optind <= argc);
	if (optind == argc) {
		/* No path given */
		if (direct) {
			path = default_direct_access_path;
		} else {
			path = default_dsme_path;
		}
	} else if (optind + 1 == argc) {
		/* Path was given */
		path = argv[optind];
	} else {
		/* More than one non-opt arg */
		return usage(progname);
	}

	if (verbose) {
		const char *modestr = direct ? "direct" : "dsme";
		printf("Using %s mode, reading from %s\n", modestr, path);
	}

	set_default_country();
	set_mac(path, direct ? get_mac_direct : get_mac_from_dsme);
	set_iq_values(path, direct ? get_iq_values_direct : get_iq_values_from_dsme);
	set_tx_curve(path, direct ? get_tx_curve_direct : get_tx_curve_from_dsme);
	set_tx_limits();
	set_rx_values();

	return EXIT_SUCCESS;
}
