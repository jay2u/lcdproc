/** \file server/drivers/parallax.c
 * \c parallax driver for serial LCD devices made by Parallax Inc.
 */

/*-
 * Copyright (C) 2011-2012, Markus Dolze
 *
 * This file is released under the GNU General Public License. Refer to the
 * COPYING file distributed with this package.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "lcd.h"
#include "parallax.h"
#include "report.h"
#include "lcd_lib.h"
#include "adv_bignum.h"

#define DEFAULT_DEVICE		"/dev/ttyS0"
#define DEFAULT_SPEED		19200
#define DEFAULT_MODEL		"27977"
#define NUM_CC			8


/** private data for the \c debug driver */
typedef struct parallax_private_data {
	char *framebuf;		/**< frame buffer */
	int fd;			/**< file descriptor of serial port */
	int width;		/**< display width in characters */
	int height;		/**< display height in characters */
	int cellwidth;		/**< character cell width */
	int cellheight;		/**< character cell height */
	int have_backlight;	/**< flag if the display has a backlight */
	CGmode ccmode;		/**< Custom character mode */
} PrivateData;


/* Vars for the server core */
MODULE_EXPORT char *api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 0;
MODULE_EXPORT int supports_multiple = 1;
MODULE_EXPORT char *symbol_prefix = "parallax_";

/**
 * API: Initialize the driver.
 */
MODULE_EXPORT int
parallax_init(Driver *drvthis)
{
	PrivateData *p;
	char device[256];
	int speed;
	struct termios portset;
	const char *str;
	char out[1];

	report(RPT_INFO, "%s()", __FUNCTION__);

	/* Allocate and store private data */
	p = (PrivateData *) calloc(1, sizeof(PrivateData));
	if (p == NULL)
		return -1;
	if (drvthis->store_private_ptr(drvthis, p))
		return -1;

	/* Initialize private data */
	p->cellwidth = LCD_DEFAULT_CELLWIDTH;
	p->cellheight = LCD_DEFAULT_CELLHEIGHT;
	p->ccmode = standard;

	/* Get configuration from config file */
	strncpy(device, drvthis->config_get_string(drvthis->name, "Device", 0,
		DEFAULT_DEVICE), sizeof(device));
	device[sizeof(device)-1] = '\0';
	report(RPT_INFO, "%s: using Device %s", drvthis->name, device);

	speed = drvthis->config_get_int(drvthis->name, "Speed", 0, DEFAULT_SPEED);
	switch (speed) {
	    case 2400:
		speed = B2400;
		break;
	    case 9600:
		speed = B9600;
		break;
	    case 19200:
		speed = B19200;
		break;
	    default:
		report(RPT_WARNING, "%s: illegal Speed: %d; must be one of 2400, 9600, or 19200; using default %d",
		       drvthis->name, speed, 19200);
		speed = B19200;
	}
	debug(RPT_INFO, "%s: using Speed %d", drvthis->name, speed);

	str = drvthis->config_get_string(drvthis->name, "Model", 0, DEFAULT_MODEL);
	if (strcasecmp(str, "27976") == 0) {
		p->width = 16;
		p->height = 2;
		p->have_backlight = 0;
	}
	else if (strcasecmp(str, "27977") == 0) {
		p->width = 16;
		p->height = 2;
		p->have_backlight = 1;
	}
	else if (strcasecmp(str, "27979") == 0) {
		p->width = 20;
		p->height = 4;
		p->have_backlight = 1;
	}
	else {
		report(RPT_WARNING, "%s: Unknown model; using defaults for 27977",
		       drvthis->name);
		p->width = 16;
		p->height = 2;
		p->have_backlight = 1;
	}
	debug(RPT_INFO, "%s: using Model \"%s\"", drvthis->name, str);


	/* Allocate and clear frame buffer */
	p->framebuf = malloc(p->width * p->height);
	if (p->framebuf == NULL) {
		report(RPT_INFO, "%s: unable to allocate framebuffer", drvthis->name);
		return -1;
	}
	memset(p->framebuf, ' ', p->width * p->height);

	/* Open and configure serial port */
	if ((p->fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY)) == -1) {
		report(RPT_ERR, "%s: open(%s) failed (%s)", drvthis->name, device, strerror(errno));
		return -1;
	}
	debug(RPT_INFO, "%s: opened device %s", drvthis->name, device);

	tcgetattr(p->fd, &portset);
#ifdef HAVE_CFMAKERAW
	cfmakeraw(&portset);
#else
	portset.c_iflag &= ~( IGNBRK | BRKINT | PARMRK | ISTRIP
	                      | INLCR | IGNCR | ICRNL | IXON );
	portset.c_oflag &= ~OPOST;
	portset.c_lflag &= ~( ECHO | ECHONL | ICANON | ISIG | IEXTEN );
	portset.c_cflag &= ~( CSIZE | PARENB | CRTSCTS );
	portset.c_cflag |= CS8 | CREAD | CLOCAL ;
#endif
	cfsetospeed(&portset, speed);
	cfsetispeed(&portset, B0);
	tcsetattr(p->fd, TCSANOW, &portset);

	/* Clear display and set cursor home */
	out[0] = 0x0C;
	write(p->fd, out, 1);
	usleep(5000);
	out[0] = 0x16;
	write(p->fd, out, 1);

	return 0;
}


/**
 * API: Close the driver (do necessary clean-up).
 */
MODULE_EXPORT void
parallax_close(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	if (p != NULL) {
		if (p->fd >= 0)
			close(p->fd);

		if (p->framebuf != NULL)
			free(p->framebuf);
		p->framebuf = NULL;

		free(p);
	}
	drvthis->store_private_ptr(drvthis, NULL);
}


/**
 * API: Return the display width in characters.
 */
MODULE_EXPORT int
parallax_width(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	return p->width;
}


/**
 * API: Return the display height in characters.
 */
MODULE_EXPORT int
parallax_height(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	return p->height;
}


/**
 * API: Return the width of a character in pixels.
 */
MODULE_EXPORT int
parallax_cellwidth(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	return p->cellwidth;
}


/**
 * API: Return the height of a character in pixels.
 */
MODULE_EXPORT int
parallax_cellheight(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	return p->cellheight;
}


/**
 * API: Clear the screen.
 */
MODULE_EXPORT void
parallax_clear(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	memset(p->framebuf, ' ', p->width * p->height);
	p->ccmode = standard;
}


/**
 * API: Flush data on screen to the display.
 */
MODULE_EXPORT void
parallax_flush(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	int row;
	unsigned char out[1];

	/* Move cursor to line 0, position 0 */
	out[0] = 0x80;
	write(p->fd, out, 1);

	for (row = 0; row < p->height; row++) {
		write(p->fd, p->framebuf + row * p->width, p->width);
		usleep(1000);
	}
}


/**
 * API: Print a string on the screen at position (x,y).
 * \note  The Parallax serial displays supported by this driver only allow
 *        diplay of visible ASCII characters in the range 32-127. Characters
 *        outside this range are used as control characters. Those are replaced
 *        by '_' (underscore) if they appear as input to this function.
 */
MODULE_EXPORT void
parallax_string(Driver *drvthis, int x, int y, const char string[])
{
	PrivateData *p = drvthis->private_data;
	int i;

	y--;			/* Convert 1-based coords to 0-based... */
	x--;

	if ((y < 0) || (y >= p->height))
		return;

	for (i = 0; (string[i] != '\0') && (x < p->width); i++, x++) {
		if (x >= 0) {
			if (string[i] >= 32 &&  string[i] <= 126)
				p->framebuf[(y * p->width) + x] = string[i];
			else
				p->framebuf[(y * p->width) + x] = '_';
		}
	}
}


/**
 * API: Print a character on the screen at position (x,y).
 * \see parallax_string
 */
MODULE_EXPORT void
parallax_chr(Driver *drvthis, int x, int y, char c)
{
	PrivateData *p = drvthis->private_data;

	x--;			/* Convert 1-based coords to 0-based... */
	y--;

	if (x >= 0 && y >= 0 && x < p->width && y < p->height) {
		if ((c >= 0 && c <= 7) || (c >= 32 && c <= 126))
			p->framebuf[(y * p->width) + x] = c;
		else
			p->framebuf[(y * p->width) + x] = '_';
	}
}


/**
 * API: Draw a vertical bar bottom-up.
 */
MODULE_EXPORT void
parallax_vbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	static unsigned char vbar_char[7][8] = {
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F},
		{0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F},
		{0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
		{0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F},
		{0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}
		/* Full block is done using icon */
	};

	if (p->ccmode != vbar) {
		int i;

		if (p->ccmode != standard) {
			report(RPT_WARNING, "%s: hbar: cannot combine two modes using user-defined characters",
			       drvthis->name);
			return;
		}

		p->ccmode = vbar;
		for (i = 0; i < 7; i++)
			parallax_set_char(drvthis, i + 1, vbar_char[i]);
	}

	lib_vbar_static(drvthis, x, y, len, promille, options, p->cellheight, 0);
}


/**
 * API: Draw a horizontal bar to the right.
 */
MODULE_EXPORT void
parallax_hbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	static unsigned char hbar_char[4][8] = {
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
		{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
		{0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C},
		{0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E}
		/* Full block is done using icon */
	};

	if (p->ccmode != hbar) {
		int i;

		if (p->ccmode != standard) {
			report(RPT_WARNING, "%s: hbar: cannot combine two modes using user-defined characters",
			       drvthis->name);
			return;
		}

		p->ccmode = hbar;
		for (i = 0; i < 4; i++)
			parallax_set_char(drvthis, i + 1, hbar_char[i]);
	}

	lib_hbar_static(drvthis, x, y, len, promille, options, p->cellwidth, 0);
}


/**
 * API: Write a big number to the screen.
 */
MODULE_EXPORT void
parallax_num(Driver *drvthis, int x, int num)
{
	PrivateData *p = drvthis->private_data;
	int do_init = 0;

	if ((num < 0) || (num > 10))
		return;

	if (p->ccmode != bignum) {
		if (p->ccmode != standard) {
			report(RPT_WARNING, "%s: num: cannot combine two modes using user-defined characters",
			       drvthis->name);
			return;
		}
		p->ccmode = bignum;
		do_init = 1;
	}

	lib_adv_bignum(drvthis, x, num, 0, do_init);
}


/**
 * API: Place an icon on the screen.
 * \note  Parallax serial displays to not allow access to the full block
 *        character as its code (usually 0xFF) is used by a control command.
 *        Thus the full block is implemented using a custom character.
 */
MODULE_EXPORT int
parallax_icon(Driver *drvthis, int x, int y, int icon)
{
	PrivateData *p = drvthis->private_data;

	static unsigned char heart_open[] =
		{ b__XXXXX,
		  b__X_X_X,
		  b_______,
		  b_______,
		  b_______,
		  b__X___X,
		  b__XX_XX,
		  b__XXXXX };
	static unsigned char heart_filled[] =
		{ b__XXXXX,
		  b__X_X_X,
		  b___X_X_,
		  b___XXX_,
		  b___XXX_,
		  b__X_X_X,
		  b__XX_XX,
		  b__XXXXX };
	static unsigned char block_filled[] =
		{ b__XXXXX,
		  b__XXXXX,
		  b__XXXXX,
		  b__XXXXX,
		  b__XXXXX,
		  b__XXXXX,
		  b__XXXXX,
		  b__XXXXX };

	/* The full block works except if ccmode=bignum */
	if (icon == ICON_BLOCK_FILLED) {
		if (p->ccmode != bignum) {
			parallax_set_char(drvthis, 0, block_filled);
			parallax_chr(drvthis, x, y, 0);
			return 0;
		}
		else {
			return -1;
		}
	}

	/* The heartbeat icons do not work in bignum and vbar mode */
	if ((icon == ICON_HEART_FILLED) || (icon == ICON_HEART_OPEN)) {
		if ((p->ccmode != bignum) && (p->ccmode != vbar)) {
			switch (icon) {
			    case ICON_HEART_FILLED:
				parallax_set_char(drvthis, 7, heart_filled);
				parallax_chr(drvthis, x, y, 7);
				return 0;
			    case ICON_HEART_OPEN:
				parallax_set_char(drvthis, 7, heart_open);
				parallax_chr(drvthis, x, y, 7);
				return 0;
			}
		}
		else {
			return -1;
		}
	}

	return -1;
}


/**
 * API: Define a custom character and write it to the LCD.
 * \note  Parallax serial displays can use custom characters at values 0x00 -
 *        0x07. To set those characters the corresponding 'define custom
 *        character' command has to be sent (value 0xF8 - 0xFF).
 */
MODULE_EXPORT void
parallax_set_char(Driver *drvthis, int n, unsigned char *dat)
{
	PrivateData *p = drvthis->private_data;
	char out[9];
	int row;

	if ((n < 0) || (n > 7) || (!dat))
		return;

	memset(out, '\0', 9);
	out[0] = 0xF8 + n;

	for (row = 0; row < p->cellheight && row < 8; row++)
		out[1 + row] = dat[row] & 0x1F;

	write(p->fd, out, 9);
}


/**
 * API: Get total number of custom characters available.
 */
MODULE_EXPORT int
parallax_get_free_chars(Driver *drvthis)
{
	return NUM_CC;
}


/**
 * API: Turn the backlight on or off.
 */
MODULE_EXPORT void
parallax_backlight(Driver *drvthis, int state)
{
	PrivateData *p = drvthis->private_data;

	if (p->have_backlight) {
		char out[1];
		if (state == BACKLIGHT_ON)
			out[0] = 0x11;
		else
			out[0] = 0x12;
		write(p->fd, out, 1);
	}
}


/**
 * API: Provide some information about this driver.
 */
MODULE_EXPORT const char *
parallax_get_info(Driver *drvthis)
{
	static char *info_string = "Parallax Inc. serial LCD driver";
	return info_string;
}
