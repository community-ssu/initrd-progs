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
#include "opencal.h"
#include "config.h"

int main(const int argc, const char **argv) {
	int version = 0;
	int rd_mode = 0;
	int rd_flags = 0;
	int get_root_device = 0;
	int usb_host_mode = 0;
	char *root_device = NULL;
	const struct poptOption options[] = {
		{"get-rd-mode", 'd', POPT_ARG_NONE, &rd_mode, 0, "Get R&D mode status", NULL},
		{"get-rd-flags", 'f', POPT_ARG_NONE, &rd_flags, 0, "Get R&D mode flags", NULL},
		{"get-root-device", 'r', POPT_ARG_NONE, &get_root_device, 0,
			"Get root device", NULL},
		{"set-root-device", 'R', POPT_ARG_STRING, &root_device, 0,
			"Set root device", NULL},
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
	int ret;
	if (rc != -1) {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		ret = EXIT_FAILURE;
	} else if (version) {
			printf("open-cal-tool %s\n\n", VERSION);
			puts("Copyright (C) 2009 Marat Radchenko <marat@slonopotamus.org>\n"
				"License GPLv3+: GNU GPL version 3 or later"
					" <http://gnu.org/licenses/gpl.html>\n"
				"This is free software: you are free to change and redistribute it.\n"
				"There is NO WARRANTY, to the extent permitted by law.");
		ret = EXIT_SUCCESS;
	} else {
		cal c;
		if ((c = cal_init(CAL_DEFAULT_PATH)) == NULL) {
			ret = EXIT_FAILURE;
		} else {
			void *data;
			uint32_t len;
			/* TODO: produce error if more than one option given */
			if (rd_mode) {
				if (cal_read_block(c, "r&d_mode", &data, &len, 0)) {
					ret = EXIT_FAILURE;
				} else {
					/*
						TODO: r&d flags are stored in same block.
						There might be more that one byte when they're set.
					*/
					assert(len == 1);
					puts(((char *)data)[0] ? "enabled" : "disabled");
				}
			} else if (rd_flags) {
				if (cal_read_block(c, "r&d_mode", &data, &len, 0)) {
					ret = EXIT_FAILURE;
				} else {
					/* TODO: implement this */
					fputs("not implemented yet\n", stderr);
					ret = EXIT_FAILURE;
				}
			} else if (get_root_device) {
				if (cal_read_block(c, "root_device", &data, &len, 0)) {
					ret = EXIT_FAILURE;
				} else {
					puts(data);
					ret = EXIT_SUCCESS;
				}
			} else if (root_device) {
				len = strlen(root_device) + 1;
				if (cal_write_block(c, "root_device", &root_device, len, 0)) {
					ret = EXIT_FAILURE;
				} else {
					puts(data);
					ret = EXIT_SUCCESS;
				}
			} else if (usb_host_mode) {
				if (cal_read_block(c, "usb_host_mode", &data, &len, 0)) {
					ret = EXIT_FAILURE;
				} else {
					/* TODO: implement this */
					fputs("not implemented yet\n", stderr);
					ret = EXIT_FAILURE;
				}
			} else {
				/* No action given */
				poptPrintHelp(ctx, stdout, 0);
				ret = EXIT_FAILURE;
			}
			cal_destroy(c);
			ret = EXIT_SUCCESS;
		}
	}
	poptFreeContext(ctx);
	return ret;
}
