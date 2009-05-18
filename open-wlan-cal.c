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

ssize_t get_from_dsme(const char *socket_path, const void *request,
		const size_t bytes_send, void *buff, const size_t bytes_read) {
	int sock;
	ssize_t ret;
	struct sockaddr_un sockaddr;

	/* create socket */
	if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
		perror("Could not create socket");
		return -1;
	}

	/* connect */
	sockaddr.sun_family = AF_FILE;
	strcpy(sockaddr.sun_path, socket_path);
	ret = strlen(sockaddr.sun_path) + sizeof(sockaddr.sun_family);
	if (connect(sock, (struct sockaddr *)&sockaddr, ret) == -1) {
		perror("Could not connect to socket");
		close(sock);
		return -1;
	}

	/* send request */
	ret = write(sock, request, bytes_send);
	if (ret == -1) {
		perror("Could not send dsme request");
		close(sock);
		return -1;
	} else if ((size_t)ret != bytes_send) {
		fprintf(stderr, "Could write only %zd bytes out of %zd", ret, bytes_send);
		perror(NULL);
		close(sock);
		return -1;
	}

	/* read response */
	ret = read(sock, buff, bytes_read);
	if (ret == -1) {
		perror("Could not read dsme response");
	} else if ((size_t)ret != bytes_read) {
		fprintf(stderr, "Could read only %zd bytes out of %zd", ret, bytes_read);
		perror(NULL);
	}

	/* cleanup */
	close(sock);
	return ret;
}

ssize_t write_to(const char *filename, const void *value, const size_t len) {
	int f;
	ssize_t ret;
	if ((f = open(filename, O_WRONLY)) == -1) {
		fprintf(stderr, "Could not open file %s", filename);
		perror(NULL);
		return -1;
	}
	ret = write(f, value, len);
	if (ret == -1) {
		fprintf(stderr, "Could not write data to %s", filename);
		perror(NULL);
	} else if ((size_t)ret != len) {
		fprintf(stderr, "Could write only %zd bytes out of %zd", ret, len);
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

void load_from_dsme(const char *socket_path) {
	/* TODO: use struct for request (and, possibly, for response header)?
		wlan-cal reads first 4 bytes and only then the rest part of response. */
	const char *mac_req
		= " \0\0\0\0\22\0\0wlan-mac\0\0\0\0\0\0\0\0\0\0\0\0\10 \1\0";
	/* const char *default_country_req
		= " \0\0\0\0\22\0\0pp_data\0\0\0\0\0\0\0\0\0\0\0\0\0\10\211\0\0"; */
	const char *iq_req
		= " \0\0\0\0\22\0\0wlan-iq-align\0\0\0\0\0\0\0\10 \1\0";
	const char *curve_req
		= " \0\0\0\0\22\0\0wlan-tx-gen2\0\0\0\0\0\0\0\0\10 \1\0";
	const size_t req_len = 32;

	char iq_resp[140];
	char mac_resp[60];
	char curve_resp[540];
	size_t len;

	/* country */
	/* TODO: at least UK tablets have 0x10 instead of 0x30 */
	print_start("Pushing default country...");
	print_end(write_to("/sys/devices/platform/wlan-omap/default_country", "0\0\0\0", 4));

	/* mac */
	print_start("Pushing MAC address...");
	len = sizeof(mac_resp);
	if (get_from_dsme(socket_path, mac_req, req_len, mac_resp, len) == (ssize_t)len) {
		size_t i;
		char mac_address[6];
		for (i = 0; i < sizeof(mac_address); ++i) {
			mac_address[i] = mac_resp[36 + 4 * i];
		}
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_mac_address",
			mac_address, sizeof(mac_address)));
	}

	/* IQ values */
	print_start("Pushing IQ tuned values...");
	len = sizeof(iq_resp);
	if (get_from_dsme(socket_path, iq_req, req_len, iq_resp, len) == (ssize_t)len) {
		const size_t read_item_len = 8;
		/* + 2 because two bytes are used for item prefix */
		const size_t item_len = read_item_len + 2;
		char iq[130];
		for (size_t i = 0; i < sizeof(iq) / item_len; ++i) {
			const size_t read_start = 28;
			/* (i + 1) because there's an empty item in input */
			const size_t read_offset = read_start + (i + 1) * read_item_len;
			size_t write_offset = item_len * i;
			if (i == 0) {
				iq[write_offset] = iq_resp[read_start];
			} else {
				iq[write_offset] = iq[write_offset - item_len] + 5;
			}
			write_offset++;
			iq[write_offset++] = '\t';
			memcpy(iq + write_offset, iq_resp + read_offset, read_item_len);
		}
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_iq", iq, sizeof(iq)));
	}
	
	/* TX curve data */
	print_start("Pushing TX tuned values...");
	len = sizeof(curve_resp);
	if (get_from_dsme(socket_path, curve_req, req_len, curve_resp, len) == (ssize_t)len) {
		const size_t read_item_len = 38;
		const size_t prefix_len = 4;
		const size_t item_len = prefix_len + read_item_len;
		const char *prefix = "\f\0 \2";
		char tx_curve[552];
		memcpy(tx_curve, "\3\0\6\0l\t", 6);
		for (size_t i = 0; i < (sizeof(tx_curve) - 6) / item_len; ++i) {
			const char *src_addr = curve_resp + 48 + read_item_len * i;
			char *dst_addr = tx_curve + 6 + item_len * i;
			memcpy(dst_addr, prefix, prefix_len);
			memcpy(dst_addr + prefix_len, src_addr, read_item_len);
		}
		/* For some mysterious reason, last two bytes are missing */
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_pa_curve_data",
			tx_curve, sizeof(tx_curve) - 2));
	}

	/* TX limits */
	/* TODO: UK tablets have a bit different value. I think there's a conditional switch
		based on country code, because it doesn't have any additional input. */
	print_start("Pushing TX limits...");
	print_end(write_to("/sys/devices/platform/wlan-omap/cal_output_limits",
		"\1\0\1\0l\t\3\n"
		"\20\1\340\0\340\0\320\0\320\0q\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0v\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0{\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\200\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\205\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\212\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\217\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\224\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\231\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\236\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\243\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0\250\t\3\n"
		"\20\1\360\0\360\0\320\0\320\0",
		186));

	/* RX tuned values */
	print_start("Pushing RX tuned values...Using default values ");
	print_end(write_to("/sys/devices/platform/wlan-omap/cal_rssi",
		"\1\0l\t\n"
		"\1r\376\32\0q\t\n"
		"\1r\376\32\0v\t\n"
		"\1r\376\32\0{\t\n"
		"\1r\376\32\0\200\t\n"
		"\1r\376\32\0\205\t\n"
		"\1r\376\32\0\212\t\n"
		"\1r\376\32\0\217\t\n"
		"\1r\376\32\0\224\t\n"
		"\1r\376\32\0\231\t\n"
		"\1r\376\32\0\236\t\n"
		"\1r\376\32\0\243\t\n"
		"\1r\376\32\0\250\t\n"
		"\1r\376\32\0",
		106));
}

int main() {
	/* Many cool things can be done here. Saving/loading data to/from files,
		commandline args, direct /dev/mtd1 access, etc... */
	load_from_dsme("/tmp/dsmesock");
	return 0;
}
