/*
	Copyright 2009, Marat Radchenko
	Copyright (C) 2012-2013  Pali Rohár <pali.rohar@gmail.com>

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
	along with fb_text2screen.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <popt.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>

#ifndef __user
#define __user
#endif

#ifdef HAVE_LINUX_OMAPFB_H
  #include <linux/omapfb.h>
#else
  #include <asm-arm/arch-omap/omapfb.h>
#endif

struct fb {
	const char *device; /* path to framebuffer device */
	int fd; /* framebuffer file descriptor */
	int width; /* screen width (in px) */
	int height; /* screen height (in px) */
	uint32_t depth; /* screen depth in bytes per pixel */
	void *mem; /* mmaped video memory */
	size_t size; /* mmaped region size */
	uint32_t line_len; /* buffer line length in bytes */
};

/* Converts 24-bit rgb color to 16-bit rgb */
static inline uint16_t rgb_888_to_565(const uint32_t rgb888) {
	return (uint16_t)(
		(((rgb888 & 0xff0000) >> 8) & 0xf800) |
		(((rgb888 & 0x00ff00) >> 5) & 0x07e0) |
		(((rgb888 & 0x0000ff) >> 3) & 0x001f));
}

/* Converts 16-bit rgb color to 24-bit rgb */
static inline uint32_t rgb_565_to_888(const uint16_t rgb565) {
	return (((uint32_t)rgb565 & 0x0000f800) << 8) |
		(((uint32_t)rgb565 & 0x000007e0) << 5) |
		(((uint32_t)rgb565 & 0x0000001f) << 3);
}

/*
 * Fills rectangular area with given color.
 * Expects coordinates and sizes to be validated and normalized.
 */
static void fill(void *out, const struct fb *fb, const uint32_t color,
		const int width, const int height) {
	const uint16_t color16 = rgb_888_to_565(color);
	for (int j = 0; j < height; j++) {
		void *row_out = out;
		for (int i = 0; i < width; i++) {
			if (fb->depth == 2) {
				*(uint16_t *)row_out = color16;
			} else if (fb->depth == 4) {
				*(uint32_t *)row_out = color;
			} else {
			  fprintf(stderr, "Cannot handle bit depth of %u\n", fb->depth);
				abort();
			}
			row_out = (char *)row_out + fb->depth;
		}
		out = (char *)out + fb->line_len;
	}
}

#define NONPRINTABLE 0xffc399bdbd99c3ffL

static const uint64_t alphabet[256] = {
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE,/*\n*/NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	NONPRINTABLE, NONPRINTABLE, NONPRINTABLE, NONPRINTABLE,
	/*space*/0,          0x000c000c1e1e1e0cL, 0x0000000000363636L, 0x0036367f367f3636L,
	0x000c1f301e033e0cL, 0x0063660c18336300L, 0x006e333b6e1c361cL, 0x0000000000030606L,
	0x00180c0606060c18L, 0x00060c1818180c06L, 0x0000663cff3c6600L, 0x00000c0c3f0c0c00L,
	0x060c0c0000000000L, 0x000000003f000000L, 0x000c0c0000000000L, 0x000103060c183060L,
	/*0*/0x3e676f7b73633eL,0x3f0c0c0c0c0e0cL, 0x003f33061c30331eL, 0x001e33301c30331eL,
	0x0078307f33363c38L, 0x001e3330301f033fL, 0x001e33331f03061cL, 0x000c0c0c1830333fL,
	0x001e33331e33331eL, 0x000e18303e33331eL, 0x000c0c00000c0c00L, 0x060c0c00000c0c00L,
	0x00180c0603060c18L, 0x00003f00003f0000L, 0x00060c1830180c06L, 0x000c000c1830331eL,
	0x1e037b7b7b633eL,/*A*/0x33333f33331e0cL, 0x003f66663e66663fL, 0x003c66030303663cL,
	0x001f36666666361fL, 0x007e06061e06067eL, 0x000606061e06067eL, 0x007c66730303663cL,
	0x003333333f333333L, 0x001e0c0c0c0c0c1eL, 0x001e333330303078L, 0x006766361e366667L,
	0x007e060606060606L, 0x0063636b7f7f7763L, 0x006363737b6f6763L, 0x001c36636363361cL,
	0x000f06063e66663fL, 0x00381e3b3333331eL, 0x006766363e66663fL, 0x001e33380e07331eL,
	0x000c0c0c0c0c0c3fL, 0x003f333333333333L, 0x000c1e3333333333L, 0x0063777f6b636363L,
	0x0063361c1c366363L, 0x001e0c0c1e333333L, 0x007f060c1830607fL, 0x001e06060606061eL,
	0x00406030180c0603L, 0x001e18181818181eL, 0x0000000063361c08L, 0xff00000000000000L,
	0x000000180c0cL,/*a*/0x006e333e301e0000L, 0x003b66663e060607L, 0x001e3303331e0000L,
	0x006e33333e303038L, 0x001e033f331e0000L, 0x000f06060f06361cL, 0x1f303e33336e0000L,
	0x006766666e360607L, 0x001e0c0c0c0e000cL, 0x1e33333030300030L, 0x0067361e36660607L,
	0x001e0c0c0c0c0c0eL, 0x00636b7f7f330000L, 0x00333333331f0000L, 0x001e3333331e0000L,
	0x0f063e66663b0000L, 0x78303e33336e0000L, 0x000f06666e3b0000L, 0x001f301e033e0000L,
	0x00182c0c0c3e0c08L, 0x006e333333330000L, 0x000c1e3333330000L, 0x00367f7f6b630000L,
	0x0063361c36630000L, 0x1f303e3333330000L, 0x003f260c193f0000L, 0x00380c0c070c0c38L,
	0x0018181800181818L, 0x00070c0c380c0c07L, 0x0000000000003b6eL, NONPRINTABLE,
	/* TODO: add higher 128 chars? But what encoding? */
};

/*
 * Writes letters on screen. Each letter is represented as 8x8 bit matrix.
 * Known limitations:
 *  - ASCII only.
 *  - Doesn't handle \n for force line breaks.
 *    Could be done, but complicates limits calculation and wrapping.
 *  - Expects letters to fit square area. Could be fixed to allow rectangular.
 *  - Doesn't accept coordinates and alignment at the same time.
 */
static int fb_write_text(
		struct fb *fb,
		const char *text,
		const int scale,
		bool bg_clear,
		const uint32_t bg_color,
		const uint32_t fg_color,
		int x,
		int y,
		const char *halign,
		const char *valign) {
	if (scale < 1) {
		fputs("Invalid scale\n", stderr);
		return EXIT_FAILURE;
	}
	const unsigned int space_size = scale * 2;
	const unsigned int letter_size = scale * 8;
	const unsigned int max_chars_per_row = fb->width / letter_size;
	const unsigned int row_height = space_size + letter_size;
	const size_t len = strlen(text);

	if (x != 0 && halign != NULL) {
		fputs("You can't specify -H and -x at the same time\n", stderr);
		return EXIT_FAILURE;
	} else if (y != 0 && valign != NULL) {
		fputs("You can't specify -V and -y at the same time\n", stderr);
		return EXIT_FAILURE;
	}

	const unsigned int first_line_len = len > max_chars_per_row ? max_chars_per_row : len;
	if (halign == NULL || strcmp(halign, "left") == 0) {
		/* noop */
	} else if (strcmp(halign, "right") == 0) {
		x = fb->width - first_line_len * letter_size;
	} else if (strcmp(halign, "center") == 0) {
		x = (fb->width - first_line_len * letter_size) / 2;
	} else {
		fputs("Invalid horizontal alignment\n", stderr);
		return EXIT_FAILURE;
	}
	const unsigned int rows_in_text = (len + max_chars_per_row - 1) / max_chars_per_row;
	if (valign == NULL || strcmp(valign, "top") == 0) {
		/* noop */
	} else if (strcmp(valign, "center") == 0) {
		y = (fb->height - row_height * rows_in_text) / 2;
	} else if (strcmp(valign, "bottom") == 0) {
		y = fb->height - row_height * rows_in_text;
	} else {
		fputs("Invalid vertical alignment\n", stderr);
		return EXIT_FAILURE;
	}

	if (x < 0 || x > fb->width || y < 0 || y > fb->width) {
		fputs("Out of screen bounds\n", stderr);
		return EXIT_FAILURE;
	}

	const unsigned int max_rows = (fb->height - y) / row_height;
	if (len > (max_rows - 1) * max_chars_per_row + (fb->width - x) / letter_size) {
		fputs("Text is too long\n", stderr);
		return EXIT_FAILURE;
	}

	uint8_t *screen_out = (uint8_t *)fb->mem;
	/* Pointer to left top letter corner */
	uint8_t *letter_out = screen_out + fb->line_len * y + fb->depth * x;
	unsigned int row = 0;
	/* Iterate over chars in text */
	for (size_t c = 0; c < len; ++c) {
		const uint64_t letter = alphabet[(unsigned char)text[c]];
		/* Pointer to left top pixel corner */
		uint8_t *pxly_out = letter_out;
		/* Vertical letter axis */
		for (int ly = 0; ly < 8; ++ly) {
			uint8_t *pxlx_out = pxly_out;
			/* Horizontal letter axis */
			for (int lx = 0; lx < 8; ++lx) {
				if (letter >> (ly * 8 + lx) & 1) {
					fill(pxlx_out, fb, fg_color, scale, scale);
				} else if (bg_clear) {
					fill(pxlx_out, fb, bg_color, scale, scale);
				}
				/* Advance to next pixel */
				pxlx_out += fb->depth * scale;
			}
			/* Advance to next pixel row */
			pxly_out += fb->line_len * scale;
		}
		/* Advance to next letter in same row */
		letter_out += fb->depth * letter_size;
		const int last_letter_in_row = fb->line_len * (y + row_height * row) +
					       fb->depth * (fb->width - letter_size);
		if (letter_out - screen_out > last_letter_in_row) {
			++row;
			letter_out = screen_out + fb->line_len * (y + row_height * row);
		}
	}
	return EXIT_SUCCESS;
}

static int fb_init(struct fb *fb) {
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	if ((fb->fd = open(fb->device, O_RDWR)) < 0) {
		perror("Could not open device");
		return EXIT_FAILURE;
	}
	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Could not ioctl(FBIOGET_FSCREENINFO)");
		close(fb->fd);
		return EXIT_FAILURE;
	}
	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo)){
		perror("Could not ioctl(FBIOGET_VSCREENINFO)");
		close(fb->fd);
		return EXIT_FAILURE;
	}
	fb->size = finfo.line_length * vinfo.yres;
	fb->width = vinfo.xres;
	fb->height = vinfo.yres;
	fb->depth = ((vinfo.bits_per_pixel) >> 3);
	fb->line_len = finfo.line_length;
	/* move viewport to upper left corner */
	if (vinfo.xoffset != 0 || vinfo.yoffset != 0) {
		vinfo.xoffset = 0;
		vinfo.yoffset = 0;
		if (ioctl(fb->fd, FBIOPAN_DISPLAY, &vinfo)) {
			perror("Could not ioctl(FBIOPAN_DISPLAY)");
			close(fb->fd);
			return EXIT_FAILURE;
		}
	}
	fb->mem = mmap(0, fb->size, PROT_WRITE, MAP_SHARED, fb->fd, 0);
	if (fb->mem == MAP_FAILED) {
		perror("Could not mmap device");
		close(fb->fd);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static void fb_destroy(struct fb *fb) {
	if (fb->fd) {
		munmap(fb->mem, fb->size);
		close(fb->fd);
		fb->mem = NULL;
	}
}

static void fb_flush(struct fb *fb) {
	struct omapfb_update_window update;
	if (fb->mem) {
		update.x = 0;
		update.y = 0;
		update.width = fb->width;
		update.height = fb->height;
		/* TODO: there ain't OMAPFB_COLOR_RGB888 for 24-bit color */
		update.format = OMAPFB_COLOR_RGB565;
		update.out_x = 0;
		update.out_y = 0;
		update.out_width = fb->width;
		update.out_height = fb->height;
		if (ioctl(fb->fd, OMAPFB_UPDATE_WINDOW, &update) < 0) {
//			perror("Could not ioctl(OMAPFB_UPDATE_WINDOW)");
		}
	}
}

/* Normalizes coordinates (fixes negative width/height) */
static void normalize(int *x, int *y, int *width, int *height) {
	if (*width < 0) {
		*width = -*width;
		*x -= *width;
	}
	if (*height < 0) {
		*height = -*height;
		*y -= *height;
	}
}

static int fb_clear(struct fb *fb, const uint32_t color, int x, int y,
		int width, int height) {
	if (width == 0) {
		width = fb->width - x;
	}
	if (height == 0) {
		height = fb->height - y;
	}
	normalize(&x, &y, &width, &height);
	if (x < 0 || x + width > fb->width || y < 0 || y + height > fb->height) {
		fputs("Boundaries out of range\n", stderr);
		return EXIT_FAILURE;
	}
	uint8_t *out = (uint8_t *)fb->mem + (ptrdiff_t)(fb->line_len * y + fb->depth * x);

	fill(out, fb, color, width, height);
	return EXIT_SUCCESS;
}

int main(int argc, const char *argv[]) {
	char *text = NULL;
	int version = 0;
	int clear = 0;
	const struct poptOption actions[] = {
		{"set-text", 't', POPT_ARG_STRING, &text, 0, "Write text on screen", "<text>"},
		{"clear", 'c', POPT_ARG_NONE, &clear, 0, "Clear screen or its part", NULL},
		{"version", 0, POPT_ARG_NONE, &version, 0, "Output version", NULL},
		POPT_TABLEEND
	};

	const char *text_color = "0x4E02";
	const char *bg_color = "";
	int scale = 1;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	char *halign = NULL;
	char *valign = NULL;
	const struct poptOption options[] = {
		{"set-text-color", 'T', POPT_ARG_STRING, &text_color, 0,
			"Use specified 24bit or 32bit RGB color for text. Default is 0x4E02 (green).", "<color>"},
		{"set-bg-color", 'B', POPT_ARG_STRING, &bg_color, 0,
			"Use specified 24bit or 32bit RGB color for background. Default is 0xFFFF (white).", "<color>"},
		{"set-scale", 's', POPT_ARG_INT, &scale, 0,
			"Set text size", "{1-10}"},
		{"set-x", 'x', POPT_ARG_INT, &x, 0, "Text/clear area x-coordinate", "<int>"},
		{"set-y", 'y', POPT_ARG_INT, &y, 0, "Text/clear area y-coordinate", "<int>"},
		{"set-width", 'w', POPT_ARG_INT, &width, 0, "Clear area width", "<int>"},
		{"set-height", 'h', POPT_ARG_INT, &height, 0, "Clear area height", "<int>"},
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
	poptContext ctx = poptGetContext(NULL, argc, argv, popts, POPT_CONTEXT_NO_EXEC);
	poptSetOtherOptionHelp(ctx, "[OPTION...] ACTION [DEVICE]");
	int rc = poptGetNextOpt(ctx);
	int ret = EXIT_FAILURE;
	if (rc == -1) {
		struct fb fb = {"/dev/fb0", 0, 0, 0, 0, NULL, 0, 0};
		/* TODO: fail if more than one non-option arg is given. */
		if (poptPeekArg(ctx) != NULL) {
			fb.device = poptGetArg(ctx);
		}
		const int action_sum = (text == NULL ? 0 : 1) + clear + version;
		if (action_sum > 1) {
			/* More than one action at a time */
			fputs("Only one action can be given\n", stderr);
		} else if (action_sum == 0) {
			/* No action given */
			poptPrintHelp(ctx, stdout, 0);
		} else if (version) {
			printf("fb_text2screen %s\n\n", VERSION);
			puts("Copyright (C) 2009 Marat Radchenko <marat@slonopotamus.org>\n"
				"Copyright (C) 2012-2013 Pali Rohár <pali.rohar@gmail.com>\n"
				"License GPLv3+: GNU GPL version 3 or later"
					" <http://gnu.org/licenses/gpl.html>\n"
				"This is free software: you are free to change and redistribute it.\n"
				"There is NO WARRANTY, to the extent permitted by law.");
			ret = EXIT_SUCCESS;
		} else {
			ret = fb_init(&fb);
			if (ret == EXIT_SUCCESS) {
				const char *bg_colorstr = bg_color[0] ? bg_color : "0xFFFF";
				uint32_t bg_color32 = strtoul(bg_colorstr, NULL, 16);
				uint32_t fg_color32 = strtoul(text_color, NULL, 16);
				const char *ptr = bg_colorstr;
				if (strncmp(ptr, "0x", 2) == 0) {
					ptr += 2;
				}
				if (strlen(ptr) == 4) {
					bg_color32 = rgb_565_to_888(bg_color32);
				}
				ptr = text_color;
				if (strncmp(ptr, "0x", 2) == 0) {
					ptr += 2;
				}
				if (strlen(ptr) == 4) {
					fg_color32 = rgb_565_to_888(fg_color32);
				}
				if (clear) {
					/* Clear mode */
					ret = fb_clear(&fb, bg_color32, x, y, width, height);
				} else {
					/* Text mode */
					ret = fb_write_text(&fb, text, scale, bg_color[0], bg_color32, fg_color32, x, y, halign, valign);
				}
				fb_flush(&fb);
			}
			fb_destroy(&fb);
		}
	} else {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
	}
	poptFreeContext(ctx);
	return ret;
}
