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
#include <errno.h>
#include <popt.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include "opendsme.h"
#include "opencal.h"
#include "config.h"

static ssize_t write_to(const char *path, const void *value, const size_t len) {
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

static void print_start(const char *msg) {
	printf(msg);
	fflush(stdout);
}

static void print_end(const ssize_t result) {
	if (result != -1) {
		puts("[OK]");
	}
}

/* Country */

static void set_default_country() {
	/* const char *default_country_req
		= " \0\0\0\0\22\0\0pp_data\0\0\0\0\0\0\0\0\0\0\0\0\0\10\211\0\0"; */
	/* TODO: at least UK tablets have 0x10 instead of 0x30 */
	/* The problem is that i can't figure out which byte they are using */
	print_start("Pushing default country...");
	print_end(write_to("/sys/devices/platform/wlan-omap/default_country", "0\0\0\0", 4));
}

/* MAC */

static ssize_t get_mac_direct(const char *path, void *buf, const size_t len) {
	return get_from_mtd(path, buf, 36, len, 4, MTD_OTP_USER);
}

static ssize_t get_mac_from_dsme(const char *path, void *buf, const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-mac\0\0\0\0\0\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 36);
}

static void set_mac(const char *path,
		ssize_t (*get)(const char *, void *, const size_t len)) {
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

static ssize_t get_iq_values_direct(const char *path, char *buf,
		const size_t len) {
	ssize_t ret;
	/* TODO: is it correct? */
	memcpy(buf, "l\0\0\0", 4);
	ret = get_from_mtd(path, &buf[4], 55332, len - 4, 0, -1);
	return ret == -1 ? ret : ret + 4;
}

static ssize_t get_iq_values_from_dsme(const char *path, char *buf,
		const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-iq-align\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 28);
}

static void set_iq_values(const char *path,
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

static ssize_t get_tx_curve_direct(const char *path, void *buf,
		const size_t len) {
	return get_from_mtd(path, buf, 57380, len, 14, -1);
}

static ssize_t get_tx_curve_from_dsme(const char *path, void *buf,
		const size_t len) {
	const char *req = " \0\0\0\0\22\0\0wlan-tx-gen2\0\0\0\0\0\0\0\0\10 \1\0";
	return get_from_dsme(path, req, 32, buf, len, 46);
}

static void set_tx_curve(const char *path,
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
static void set_tx_limits() {
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

static void set_rx_values() {
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

int main(const int argc, const char **argv) {
	int direct = 0;
	int version = 0;
	const struct poptOption options[] = {
		{"direct-mode", 'd', POPT_ARG_NONE, &direct, 0,
			"If specified, data is read directly from mtd partition"
			" instead of dsme server socket.", NULL},
		{"version", 0, POPT_ARG_NONE, &version, 0, "Output version", NULL},
		POPT_TABLEEND
	};
	const struct poptOption popts[] = {
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, &options, 0, "Options:", NULL},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	poptContext ctx = poptGetContext(NULL, argc, argv, popts, POPT_CONTEXT_NO_EXEC);
	poptSetOtherOptionHelp(ctx, "[OPTION...] [PATH]\n"
		"  PATH\tSpecifies where path to mtd partition if -d option is used or to dsme"
		" server socket path otherwise. If no value is specified, "
		CAL_DEFAULT_PATH  " is used in direct access mode and "
		DEFAULT_DSME_PATH " in dsme.");
	const int rc = poptGetNextOpt(ctx);
	int ret;
	if (rc != -1) {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		ret = EXIT_FAILURE;
	} else if (version) {
			printf("open-wlan-cal %s\n\n", VERSION);
			puts("Copyright (C) 2009 Marat Radchenko <marat@slonopotamus.org>\n"
				"License GPLv3+: GNU GPL version 3 or later"
				" <http://gnu.org/licenses/gpl.html>\n"
				"This is free software: you are free to change and redistribute it.\n"
				"There is NO WARRANTY, to the extent permitted by law.");
		ret = EXIT_SUCCESS;
	} else {
		const char *path;
		if (poptPeekArg(ctx) != NULL) {
			path = poptGetArg(ctx);
		} else if (direct) {
			path = CAL_DEFAULT_PATH;
		} else {
			path = DEFAULT_DSME_PATH;
		}

		set_default_country();
		set_mac(path, direct ? get_mac_direct : get_mac_from_dsme);
		set_iq_values(path, direct ? get_iq_values_direct : get_iq_values_from_dsme);
		set_tx_curve(path, direct ? get_tx_curve_direct : get_tx_curve_from_dsme);
		set_tx_limits();
		set_rx_values();
		ret = EXIT_SUCCESS;
	}
	poptFreeContext(ctx);
	return ret;
}
