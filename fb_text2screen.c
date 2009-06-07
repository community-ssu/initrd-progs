/*
	Copyright 2009, Marat Radchenko

	This file is part of fb_text2screen.

	fb_text2screen is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	fb_text2screen is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with opendsme.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <popt.h>
#include <stdlib.h>

int main(const int argc, const char **argv) {
	char *text = NULL;
	int clear = 0;
	/* TODO: default values? */
	char *text_color;
	char *bg_color;
	int size;
	int x;
	int y;
	int width;
	int height;
	char *halign;
	char *valign;
	const char *device = "/dev/fb0";

	poptContext ctx;
	int rc;
	int ret;

	const struct poptOption actions[] = {
		{"set-text", 't', POPT_ARG_STRING, &text, 0, "Write text on screen", "<text>"},
		{"clear", 'c', POPT_ARG_NONE, &clear, 0, "Clear screen or its part", NULL},
		POPT_TABLEEND
	};
	const struct poptOption options[] = {
		{"set-text-color", 'T', POPT_ARG_STRING, &text_color, 0,
			"Use specified color for text. Default is green.", "<color>"},
		{"set-bg-color", 'B', POPT_ARG_STRING, &bg_color, 0,
			"Use specified color for background. Default is white.", "<color>"},
		{"set-scale", 's', POPT_ARG_INT, &size, 0,
			"Set text size", "{1-10}"},
		{"set-x", 'x', POPT_ARG_INT, &x, 0, "Text x-coordinate", "<int>"},
		{"set-y", 'y', POPT_ARG_INT, &y, 0, "Text y-coordinate", "<int>"},
		{"set-width", 'w', POPT_ARG_INT, &width, 0, "Clear area width", "<int>"},
		{"set-height", 'h', POPT_ARG_INT, &height, 0, "Clear area height", "<int>"},
		/* TODO: can these be used with --clear? */
		{"set-halign", 'H', POPT_ARG_STRING, &halign, 0,
			"Horizontal aligment", "{left|center|right}"},
		{"set-valign", 'V', POPT_ARG_STRING, &valign, 0,
			"Vertical aligment", "{top|center|bottom}"},
		POPT_TABLEEND
	};
	const struct poptOption popts[] = {
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, &actions, 0, "Actions:", NULL},
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, &options, 0, "Options:", NULL},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	ctx = poptGetContext(NULL, argc, argv, popts, POPT_CONTEXT_NO_EXEC);
	poptSetOtherOptionHelp(ctx, "[OPTION...] ACTION [DEVICE]");
	rc = poptGetNextOpt(ctx);
	if (rc != -1) {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		ret = EXIT_FAILURE;
	} else {
		/* TODO: fail if more than one non-option arg is given. */
		if (poptPeekArg(ctx) != NULL) {
			device = poptGetArg(ctx);
		}
		if (clear && !text) {
			/* Clear mode */
			ret = EXIT_SUCCESS;
		} else if (text && !clear) {
			/* Text mode */
			ret = EXIT_SUCCESS;
		} else if (text) {
			/* More than one action at a time */
			puts("Only one action can be given");
			ret = EXIT_FAILURE;
		} else {
			/* No action given */
			poptPrintHelp(ctx, stdout, 0);
			ret = EXIT_FAILURE;
		}
	}
	poptFreeContext(ctx);
	return ret;
}
