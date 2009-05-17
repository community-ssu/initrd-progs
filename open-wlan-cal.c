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

ssize_t set_mac(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_mac_address", value, len);
}

ssize_t set_default_country(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/default_country", value, len);
}

ssize_t set_tx_limits(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_output_limits",
		value, len);
}

ssize_t set_iq_values(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_iq", value, len);
}

ssize_t set_tx_curve_data(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_pa_curve_data",
		value, len);
}

ssize_t set_rx_tuned_data(const void *value, const size_t len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_rssi", value, len);
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
	/* TODO: use struct for request (and, possibly, for response header)? */
	const char *mac_request
		= " \0\0\0\0\22\0\0wlan-mac\0\0\0\0\0\0\0\0\0\0\0\0\10 \1\0";
	/* const char *default_country_request
		= " \0\0\0\0\22\0\0pp_data\0\0\0\0\0\0\0\0\0\0\0\0\0\10\211\0\0"; */
	const char *iq_request
		= " \0\0\0\0\22\0\0wlan-iq-align\0\0\0\0\0\0\0\10 \1\0";
	const char *rx_tx_data_request
		= " \0\0\0\0\22\0\0wlan-tx-gen2\0\0\0\0\0\0\0\0\10 \1\0";

	char iq_data[140];
	char mac_address_data[60];
	size_t len;

	/* country */
	/* TODO: at least UK tablets have 0x10 instead of 0x30 */
	print_start("Pushing default country...");
	print_end(set_default_country("0\0\0\0", 4));

	/* mac */
	print_start("Pushing MAC address...");
	/* TODO: request size is hardcoded here */
	len = sizeof(mac_address_data);
	if (get_from_dsme(socket_path, mac_request, 32, mac_address_data, len) != -1) {
		size_t i;
		char mac_address[6];
		for (i = 0; i < sizeof(mac_address); ++i) {
			mac_address[i] = mac_address_data[36 + 4 * i];
		}
		print_end(set_mac(mac_address, sizeof(mac_address)));
	}

	/* IQ values */
	print_start("Pushing IQ tuned values...");
	len = sizeof(iq_data);
	if (get_from_dsme(socket_path, iq_request, 32, iq_data, len) != -1) {
		char iq[130];
		const size_t iq_len = sizeof(iq);
		const size_t read_offset = 28;
		const size_t item_len = 10;
		for (size_t i = 0; i < iq_len / item_len; ++i) {
			const size_t offset = item_len * i;
			if (i == 0) {
				iq[offset] = iq_data[read_offset];
			} else {
				// It's a kind of magic
				iq[offset] = iq[offset - item_len] + 5;
			}
			iq[offset + 1] = '\t';
			/* (item_len - 2) because 2 symbols already set */
			for (size_t j = 0; j < (item_len - 2); ++j) {
				/* offset + 2 because 2 symbols already set */
				/* (i + 1) because there's an empty item in input */
				iq[offset + 2 + j] = iq_data[read_offset + (i + 1) * (item_len - 2) + j];
			}
		}
		print_end(set_iq_values(iq, iq_len));
	}
}

int main() {
	/* Many cool things can be done here. Saving/loading data to/from files,
	commandline args, direct /dev/mtd1 access, etc... */
	load_from_dsme("/tmp/dsmesock");
	return 0;
}
