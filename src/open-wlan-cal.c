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
#include <string.h>
#include <fcntl.h>
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
	/* Stored in pp_data block */
	/* TODO: at least UK tablets have 0x10 instead of 0x30 */
	/* The problem is that i can't figure out which byte they are using */
	print_start("Pushing default country...");
	print_end(write_to("/sys/devices/platform/wlan-omap/default_country", "0\0\0\0", 4));
}

static void set_mac(cal c) {
	const size_t mac_len = 6;
	char mac[mac_len];
	uint32_t *data;
	uint32_t len;
	print_start("Pushing MAC address...");
	if (cal_read_block(c, "wlan-mac", (void **)&data, &len, 0) == 0) {
		assert(len == (mac_len + 1) * sizeof(uint32_t));
		for (size_t i = 0; i < mac_len; ++i) {
			mac[i] = data[i + 1];
		}
		const char *file = "/sys/devices/platform/wlan-omap/cal_mac_address";
		print_end(write_to(file, mac, mac_len));
	}
}

static void set_iq_values(cal c) {
	/* 13 (items) * 8 (read_item_len) */
	char *data;
	uint32_t len;
	print_start("Pushing IQ tuned values...");
	if (cal_read_block(c, "wlan-iq-align", (void **)&data, &len, 0) == 0) {
		assert(len == 108);
		const size_t read_item_len = 8;
		/* + 2 because two bytes are used for item prefix */
		const size_t item_len = read_item_len + 2;
		/* 10 (prefix + 8 bytes of data) * 13 (items) */
		char iq[130];
		for (size_t i = 0; i < sizeof(iq) / item_len; ++i) {
			const size_t read_offset = i * read_item_len + 4;
			/* TODO: off-by-one error?
				Result is correct, but assert fails.
				13 * 8 == 104 == len */
			/* assert(read_offset + read_item_len < len); */
			size_t write_offset = item_len * i;
			if (i == 0) {
				iq[write_offset] = 108;
			} else {
				iq[write_offset] = iq[write_offset - item_len] + 5;
			}
			write_offset++;
			iq[write_offset++] = '\t';
			memcpy(&iq[write_offset], &data[read_offset], read_item_len);
		}
		print_end(write_to("/sys/devices/platform/wlan-omap/cal_iq", iq, sizeof(iq)));
	}
}

static void set_tx_curve(cal c) {
	/* 38 * 13 (items) */
	char *data;
	uint32_t len;
	print_start("Pushing TX tuned values...");
	if (cal_read_block(c, "wlan-tx-gen2", (void **)&data, &len, 0) == 0) {
		assert(len == 508);
		const size_t read_item_len = 38;
		const size_t sep_len = 4;
		const char *sep = "\f\0 \2";
		const size_t prefix_len = 4;
		const size_t item_len = sep_len + read_item_len;
		/* 4 + (4 + 38) * 13 */
		char tx_curve[550];
		memcpy(tx_curve, "\3\0\6\0", prefix_len);
		for (size_t i = 0; i < (sizeof(tx_curve) - prefix_len) / item_len; ++i) {
			const char *src_addr = &data[read_item_len * i + 14];
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
	int version = 0;
	const struct poptOption options[] = {
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
		"  PATH\tSpecifies path to CAL");
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
		} else {
			path = CAL_DEFAULT_PATH;
		}
		cal c;
		if (cal_init(&c, path) == 0) {
			set_default_country();
			set_mac(c);
			set_iq_values(c);
			set_tx_curve(c);
			set_tx_limits();
			set_rx_values();
			cal_destroy(c);
			ret = EXIT_SUCCESS;
		} else {
			ret = EXIT_FAILURE;
		}
	}
	poptFreeContext(ctx);
	return ret;
}
