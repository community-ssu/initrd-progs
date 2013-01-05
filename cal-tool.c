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
#include <popt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <cal.h>
#include "config.h"

int main(const int argc, const char **argv) {
	int version = 0;
	int rd_mode = 0;
	int rd_flags = 0;
	int get_root_device = 0;
	int usb_host_mode = 0;
	char *get_value = NULL;
	char *root_device = NULL;
	const struct poptOption options[] = {
		{"get-rd-mode", 'd', POPT_ARG_NONE, &rd_mode, 0, "Get R&D mode status", NULL},
		{"get-rd-flags", 'f', POPT_ARG_NONE, &rd_flags, 0, "Get R&D mode flags", NULL},
		{"get-root-device", 'r', POPT_ARG_NONE, &get_root_device, 0,
			"Get root device", NULL},
		{"set-root-device", 'R', POPT_ARG_STRING, &root_device, 0,
			"Set root device", NULL},
		{"get-block", 'G', POPT_ARG_STRING, &get_value, 0,
			"Print block data to stdout", NULL},
		{"get-usb-host-mode", 'u', POPT_ARG_NONE, &usb_host_mode, 0,
			"Get USB host mode flag", NULL},
		{"version", 0, POPT_ARG_NONE, &version, 0, "Output version", NULL},
		POPT_TABLEEND
	};
	const struct poptOption popts[] = {
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, &options, 0, "Options:", NULL},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	poptContext ctx = poptGetContext(NULL, argc, argv, popts, POPT_CONTEXT_NO_EXEC);
	poptSetOtherOptionHelp(ctx, "OPTION");
	const int rc = poptGetNextOpt(ctx);
	const int option_sum = version + rd_mode + rd_flags + get_root_device
		+ usb_host_mode + (root_device == NULL ? 0 : 1)
		+ (get_value == NULL ? 0 : 1);

	struct cal * c;
	int ret = EXIT_FAILURE;
	if (rc != -1) {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
	} else if (option_sum > 1) {
		fputs("Only one option can be given\n", stderr);
	} else if (option_sum == 0) {
		/* No action given */
		poptPrintHelp(ctx, stdout, 0);
	} else if (version) {
			printf("open-cal-tool %s\n\n", VERSION);
			puts("Copyright (C) 2009 Marat Radchenko <marat@slonopotamus.org>\n"
				"License GPLv3+: GNU GPL version 3 or later"
					" <http://gnu.org/licenses/gpl.html>\n"
				"This is free software: you are free to change and redistribute it.\n"
				"There is NO WARRANTY, to the extent permitted by law.");
		ret = EXIT_SUCCESS;
	} else if (cal_init(&c) == 0) {
		void *data;
		unsigned long len;
		if (rd_mode && !cal_read_block(c, "r&d_mode", &data, &len, 0)) {
			puts((len >= 1 && ((char *)data)[0]) ? "enabled" : "disabled");
			ret = EXIT_SUCCESS;
		} else if (rd_flags && !cal_read_block(c, "r&d_mode", &data, &len, 0)) {
			char buf[len + 1];
			memcpy(buf, data, len);
			buf[len] = '\0';
			puts(buf);
			ret = EXIT_SUCCESS;
		} else if (get_root_device && !cal_read_block(c, "root_device", &data, &len, 0)) {
			char buf[len + 1];
			memcpy(buf, data, len);
			buf[len] = '\0';
			puts(buf);
			ret = EXIT_SUCCESS;
		} else if (root_device && !cal_write_block(c, "root_device", root_device, strlen(root_device), 0)) {
			ret = EXIT_SUCCESS;
		} else if (usb_host_mode && !cal_read_block(c, "usb_host_mode", &data, &len, 0)) {
			char buf[len + 1];
			memcpy(buf, data, len);
			buf[len] = '\0';
			puts(buf);
			ret = EXIT_SUCCESS;
		} else if (get_value && !cal_read_block(c, get_value, &data, &len, 0)) {
			ret = fwrite(data, 1, len, stdout) == len;
		}
		cal_finish(c);
	}
	poptFreeContext(ctx);
	return ret;
}
