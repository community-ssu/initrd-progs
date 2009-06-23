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
	along with fb_text2screen.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <popt.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <asm-arm/arch-omap/omapfb.h>
#include <limits.h>

typedef struct {
	const char *device; /* path to framebuffer device */
	int fd; /* framebuffer file descriptor */
	int width; /* screen width (in px) */
	int height; /* screen height (in px) */
	unsigned int depth; /* screen depth in bytes per pixel */
	void *mem; /* mmaped video memory */
	size_t size; /* mmaped region size */
} fb_t;

/* Converts 24-bit rgb color to 16-bit rgb */
static inline uint16_t rgb_888_to_565(const uint32_t rgb888) {
	return
		(((rgb888 & 0xff0000) >> 8) & 0xf800) |
		(((rgb888 & 0x00ff00) >> 5) & 0x07e0) |
		(((rgb888 & 0x0000ff) >> 3) & 0x001f);
}

static const uint64_t alphabet[CHAR_MAX] = {
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
	0xFFFFFFFFFFFFFFFF, /*a*/0x6e333e301e0000L, 0x3b66663e060607L, 0x001e3303331e0000L,
	0x006e33333e303038L, 0x001e033f331e0000L, 0x000f06060f06361cL, 0x1f303e33336e0000L,
	0x006766666e360607L, 0x001e0c0c0c0e000cL, 0x1e33333030300030L, 0x0067361e36660607L,
	0x001e0c0c0c0c0c0eL, 0x00636b7f7f330000L, 0x00333333331f0000L, 0x001e3333331e0000L,
	0x0f063e66663b0000L, 0x78303e33336e0000L, 0x000f06666e3b0000L, 0x001f301e033e0000L,
	0x00182c0c0c3e0c08L, 0x006e333333330000L, 0x000c1e3333330000L, 0x00367f7f6b630000L,
	0x0063361c36630000L, 0x1f303e3333330000L, /*z*/0x3f260c193f0000L, 0x380c0c070c0c38L,
	0x0018181800181818L, 0x00070c0c380c0c07L, 0x0000000000003b6eL,
};

/*
 * Writes letters on screen. Each letter is represented as 8x8 bit matrix.
 * Known limitations:
 *  - ASCII only.
 *  - Doesn't have space (2px * scale) between lines.
 *    This requires changing of limits calculation.
 *  - Doesn't handle \n for force line breaks.
 *    Could be done, but complicates limits calculation and wrapping.
 *  - Doesn't put text at left screen border after line breaks
 *    caused by overly long lines. Original text2screen does this.
 *    However, it is arguable, which behavior is better.
 *  - Expects letters to fit square area. Could be fixed to allow rectangular.
 *  - Doesn't accept coordinates and alignment at the same time.
 */
int fb_write_text(fb_t *fb, const char *text, const int scale, const uint32_t color,
		int x, int y) {
	if (x < 0 || x > fb->width || y < 0 || y > fb->width) {
		fputs("Out of screen bounds\n", stderr);
		return EXIT_FAILURE;
	} else if (scale < 1) {
		fputs("Invalid scale\n", stderr);
		return EXIT_FAILURE;
	}
	const int letter_size = scale * 8;
	const int max_chars_per_row = (fb->width - x) / letter_size;
	const int max_rows = (fb->height - y) / letter_size;
	const int len = strlen(text);
	if (len == 0) {
		fputs("No text specified\n", stderr);
		return EXIT_FAILURE;
	} else if (len > max_chars_per_row * max_rows) {
		fputs("Text is too long\n", stderr);
		return EXIT_FAILURE;
	}
	const uint16_t color16 = rgb_888_to_565(color);

	/* Pointer to left top row corner */
	uint8_t *row_out = (uint8_t *)fb->mem + fb->depth * (fb->width * y + x);
	/* Pointer to left top letter corner */
	uint8_t *letter_out = row_out;
	/* Iterate over chars in text */
	for (int c = 0; c < len; ++c) {
		// TODO: this'll break if char is signed
		const uint64_t letter = alphabet[(unsigned int)text[c]];
		/* Pointer to left top pixel corner */
		uint8_t *pxly_out = letter_out;
		/* Vertical letter axis */
		for (int ly = 0; ly < 8; ++ly) {
			uint8_t *pxlx_out = pxly_out;
			/* Horizontal letter axis */
			for (int lx = 0; lx < 8; ++lx) {
				if (letter >> (ly * 8 + lx) & 1) {
					uint8_t *zy_out = pxlx_out;
					/* Vertical zoom axis */
					for (int zy = 0; zy < scale; ++zy) {
						uint8_t *zx_out = zy_out;
						/* Horizontal zoom axis */
						for (int zx = 0; zx < scale; ++zx) {
							if (fb->depth == 2) {
								*(uint16_t *)zx_out = color16;
							} else if (fb->depth == 4) {
								*(uint32_t *)zx_out = color;
							} else {
								assert(0);
							}
							zx_out += fb->depth;
						}
						zy_out += fb->depth * fb->width;
					}
				}
				/* Advance to next pixel */
				pxlx_out += fb->depth * scale;
			}
			/* Advance to next pixel row */
			pxly_out += fb->depth * fb->width * scale;
		}
		if ((c + 1) % max_chars_per_row == 0) {
			/* Advance to new row */
			row_out += fb->depth * fb->width * letter_size;
			letter_out = row_out;
		} else {
			/* Advance to next letter in same row */
			letter_out += fb->depth * letter_size;
		}
	}
	return EXIT_SUCCESS;
}

int fb_init(fb_t *fb) {
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

void fb_destroy(fb_t *fb) {
	if (fb->fd) {
		munmap(fb->mem, fb->size);
		close(fb->fd);
		fb->mem = NULL;
	}
}

void fb_flush(fb_t *fb) {
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
			perror("Could not ioctl(OMAPFB_UPDATE_WINDOW)");
		}
	}
}

/* Normalizes coordinates (fixes negative width/height) */
void normalize(int *x, int *y, int *width, int *height) {
	if (*width < 0) {
		*width = -*width;
		*x -= *width;
	}
	if (*height < 0) {
		*height = -*height;
		*y -= *height;
	}
}

int fb_clear(fb_t *fb, const uint32_t color, int x, int y, int width, int height) {
	if (width == 0) {
		width = fb->width - x;
	}
	if (height == 0) {
		height = fb->height - y;
	}
	normalize(&x, &y, &width, &height);
	if (x < 0 || x + width > fb->width || y < 0 || y + height > fb->height) {
		fputs("Boundaries out of range", stderr);
		return EXIT_FAILURE;
	}
	uint16_t color16 = rgb_888_to_565(color);
	uint8_t *out = (uint8_t *)fb->mem + fb->depth * (fb->width * y + x);
	for (int j = 0; j < height; j++) {
		uint8_t *row_out = out;
		for (int i = 0; i < width; i++) {
			if (fb->depth == 2) {
				*(uint16_t *)row_out = color16;
			} else if (fb->depth == 4) {
				*(uint32_t *)row_out = color;
			} else {
				assert(0);
			}
			row_out += fb->depth;
		}
		out += fb->depth * fb->width;
	}
	return EXIT_SUCCESS;
}

int main(const int argc, const char **argv) {
	char *text = NULL;
	int clear = 0;
	const struct poptOption actions[] = {
		{"set-text", 't', POPT_ARG_STRING, &text, 0, "Write text on screen", "<text>"},
		{"clear", 'c', POPT_ARG_NONE, &clear, 0, "Clear screen or its part", NULL},
		POPT_TABLEEND
	};

	char *text_color = "0x00FF00";
	char *bg_color = "0xFFFFFF";
	int scale = 1;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	char *halign = NULL;
	char *valign = NULL;

	fb_t fb = {"/dev/fb0", 0, 0, 0, 0, NULL, 0};
	const struct poptOption options[] = {
		{"set-text-color", 'T', POPT_ARG_STRING, &text_color, 0,
			"Use specified color for text. Default is green.", "<color>"},
		{"set-bg-color", 'B', POPT_ARG_STRING, &bg_color, 0,
			"Use specified color for background. Default is 0xFFFFFF (white).", "<color>"},
		{"set-scale", 's', POPT_ARG_INT, &scale, 0,
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
	poptContext ctx = poptGetContext(NULL, argc, argv, popts, POPT_CONTEXT_NO_EXEC);
	poptSetOtherOptionHelp(ctx, "[OPTION...] ACTION [DEVICE]");
	int rc = poptGetNextOpt(ctx);
	int ret;
	if (rc != -1) {
		/* Invalid option */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		ret = EXIT_FAILURE;
	} else {
		/* TODO: fail if more than one non-option arg is given. */
		if (poptPeekArg(ctx) != NULL) {
			fb.device = poptGetArg(ctx);
		}
		if (text && clear) {
			/* More than one action at a time */
			puts("Only one action can be given");
			ret = EXIT_FAILURE;
		} else if (!text && !clear) {
			/* No action given */
			poptPrintHelp(ctx, stdout, 0);
			ret = EXIT_FAILURE;
		} else {
			ret = fb_init(&fb);
			if (ret == EXIT_SUCCESS) {
				if (clear) {
					/* Clear mode */
					uint32_t color32 = strtol(bg_color, NULL, 16);
					ret = fb_clear(&fb, color32, x, y, width, height);
				} else {
					/* Text mode */
					uint32_t color32 = strtol(text_color, NULL, 16);
					ret = fb_write_text(&fb, text, scale, color32, x, y);
				}
				fb_flush(&fb);
			}
			fb_destroy(&fb);
		}
	}
	poptFreeContext(ctx);
	return ret;
}
