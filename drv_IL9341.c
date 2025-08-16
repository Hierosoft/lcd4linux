/* $Id$
 * $URL$
 *
 * IL9341 lcd4linux driver
 *
 * Copyright (C) 2019 Korneliusz Osmenda <korneliuszo@gmail.com>
 * Modified from sample code by:
 * Copyright (C) 2005 Michael Reinelt <michael@reinelt.co.at>
 * Copyright (C) 2005, 2006, 2007 The LCD4Linux Team <lcd4linux-devel@users.sourceforge.net>
 *
 * This file is part of LCD4Linux.
 *
 * LCD4Linux is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * LCD4Linux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 *
 * exported fuctions:
 *
 * struct DRIVER drv_IL9341
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <usb.h>

#include "debug.h"
#include "cfg.h"
#include "qprintf.h"
#include "udelay.h"
#include "plugin.h"
#include "widget.h"
#include "widget_text.h"
#include "widget_icon.h"
#include "widget_bar.h"
#include "drv.h"

/* graphic display? */
#include "drv_generic_graphic.h"

/* vid/pid of IRLCD */
#define LCD_USB_VENDOR 0x03EB	/* for Atmel device */
#define LCD_USB_DEVICE 0x2040

static char Name[] = "IL9341";
static char *device_id = NULL, *bus_id = NULL;

extern int got_signal;

#define _RGB565_0(p) (( ((p.R) & 0xf8)      ) | (((p.G) & 0xe0) >> 5))
#define _RGB565_1(p) (( ((p.G) & 0x1c) << 3 ) | (((p.B) & 0xf8) >> 3))

static usb_dev_handle *lcd;
static int orientation;

/****************************************/
/***  hardware dependant functions    ***/
/****************************************/

static int drv_IL9341_open(const char *section) {
	struct usb_bus *busses, *bus;
	struct usb_device *dev;

	lcd = NULL;

	info("%s: scanning USB for IL9341 interface ...", Name);

	if (bus_id != NULL)
		info("%s: scanning for bus id: %s", Name, bus_id);

	if (device_id != NULL)
		info("%s: scanning for device id: %s", Name, device_id);

	usb_set_debug(0);

	usb_init();
	usb_find_busses();
	usb_find_devices();
	busses = usb_get_busses();

	for (bus = busses; bus; bus = bus->next) {
		/* search this bus if no bus id was given or if this is the given bus id */
		if (!bus_id || (bus_id && !strcasecmp(bus->dirname, bus_id))) {

			for (dev = bus->devices; dev; dev = dev->next) {
				/* search this device if no device id was given or if this is the given device id */
				if (!device_id
						|| (device_id && !strcasecmp(dev->filename, device_id))) {

					if ((dev->descriptor.idVendor == LCD_USB_VENDOR)
							&& (dev->descriptor.idProduct == LCD_USB_DEVICE)) {
						info("%s: found VID/PID on bus %s device %s", Name,
								bus->dirname, dev->filename);
						char name[10];
						usb_dev_handle *candidate = usb_open(dev);
						usb_get_string_simple(candidate,
								dev->descriptor.iProduct, name, 10);
						if (strcmp("IL9341", name) == 0) {
							info(
									"%s: found IL9341 interface on bus %s device %s",
									Name, bus->dirname, dev->filename);

							lcd = candidate;
							if (usb_claim_interface(lcd, 0) < 0) {
								info(
										"%s: WRNING! usb_claim_interface() failed!",
										Name);
								/* try to proceed anyway... */
							}
							return 0;

						} else {
							usb_close(candidate);
						}
					}
				}
			}
		}
	}

	return -1;
}

typedef struct{
	int width;
	int heigth;
} WH;

static WH drv_IL9341_get_dimensions(void)
{
	WH ret;
	uint8_t buff[4];
	if(usb_control_msg(lcd,USB_TYPE_VENDOR|USB_RECIP_INTERFACE|USB_ENDPOINT_IN,2,0,0,buff,4,1000)<0)
	{
		error("%s: USB request failed!", Name);

	    ret.width=-1;
	    ret.heigth=-1;
	    return ret;
	}
	ret.width=buff[0] | (buff[1]<<8);
	ret.heigth=buff[2] | (buff[3]<<8);
	return ret;
}

static int drv_IL9341_close(void) {
	/* close whatever port you've opened */
    usb_release_interface(lcd, 0);
    usb_close(lcd);

	return 0;
}

/* for graphic displays only */
static void drv_IL9341_blit(const int row, const int col, const int height,
		const int width) {
	int r, c;
	uint8_t buff[8];
	int rowr;
	switch (orientation)
	{
	case 0:
		buff[0]=col;
		buff[1]=col>>8;
		buff[2]=row;
		buff[3]=row>>8;
		buff[4]=width;
		buff[5]=width>>8;
		buff[6]=height;
		buff[7]=height>>8;
	break;
	case 1:
		rowr=DROWS-row-height;
		buff[0]=rowr;
		buff[1]=rowr>>8;
		buff[2]=col;
		buff[3]=col>>8;
		buff[4]=height;
		buff[5]=height>>8;
		buff[6]=width;
		buff[7]=width>>8;
	break;
	}

	int err;
	if((err=usb_control_msg(lcd,USB_TYPE_VENDOR|USB_RECIP_INTERFACE|USB_ENDPOINT_OUT,1,0,0,buff,8,1000))<0)
	{
		error("%s: USB request control failed! %d: %s", Name, err, usb_strerror());
		if (err == -32)
			return; //Broken pipe sometimes happen, wait for next update

		usb_release_interface(lcd, 0);
		usb_close(lcd);
		lcd=NULL;
	    got_signal = -1;
	    return;
	}
	uint8_t *displaybuff=malloc(width*height*2);
	size_t cntr=0;
	switch (orientation)
	{
	case 0:
		for (r = row; r < row + height; r++) {
			for (c = col; c < col + width; c++) {
				RGBA p =drv_generic_graphic_rgb(r,c);
				displaybuff[cntr++]=_RGB565_0(p);
				displaybuff[cntr++]=_RGB565_1(p);
			}
		}
	break;
	case 1:
		for (c = col; c < col + width; c++) {
			for (r = row+height-1; r >= row; r--) {
				RGBA p =drv_generic_graphic_rgb(r,c);
				displaybuff[cntr++]=_RGB565_0(p);
				displaybuff[cntr++]=_RGB565_1(p);
			}
		}
	break;
	}
	if((err=usb_bulk_write(lcd,1,displaybuff,width*height*2,2000))<0)
	{
		error("%s: USB request failed! %d: %s", Name, err, usb_strerror());

		usb_release_interface(lcd, 0);
		usb_close(lcd);
		lcd=NULL;
	    got_signal = -1;
	    return;
	}
	free(displaybuff);
}

/* start graphic display */
static int drv_IL9341_start(const char *section) {
	char *s;
	char cmd[1];
	int contrast;

	s = cfg_get(section, "Font", "6x8");
	if (s == NULL || *s == '\0') {
		error("%s: no '%s.Font' entry from %s", Name, section, cfg_source());
		return -1;
	}

	XRES = -1;
	YRES = -1;
	if (sscanf(s, "%dx%d", &XRES, &YRES) != 2 || XRES < 1 || YRES < 1) {
		error("%s: bad Font '%s' from %s", Name, s, cfg_source());
		return -1;
	}

	/* Fixme: provider other fonts someday... */
	if (XRES != 6 && YRES != 8) {
		error("%s: bad Font '%s' from %s (only 6x8 at the moment)", Name, s,
				cfg_source());
		return -1;
	}

	int i;
    // Get the logical orientation (0 = landscape, 1 = portrait, 2 = reverse landscape, 3 = reverse portrait)
    if (cfg_number(section, "Orientation", 0, 0, 1, &i) > 0)
	orientation = i;
    else
	orientation = 0;

	/* open communication with the display */
	if (drv_IL9341_open(section) < 0) {
		error("%s: could not find a IL9341 USB LCD", Name);
		return -1;
	}

	WH dimensions = drv_IL9341_get_dimensions();

	if(dimensions.heigth<0 || dimensions.width<0)
	{
		return -1;
	}

	int rotate90 = orientation % 2;
	DROWS = rotate90 ? dimensions.width : dimensions.heigth;
	DCOLS = rotate90 ? dimensions.heigth : dimensions.width;

	return 0;
}

/****************************************/
/***            plugins               ***/
/****************************************/

/****************************************/
/***        widget callbacks          ***/
/****************************************/

/* using drv_generic_text_draw(W) */
/* using drv_generic_text_icon_draw(W) */
/* using drv_generic_text_bar_draw(W) */
/* using drv_generic_gpio_draw(W) */

/****************************************/
/***        exported functions        ***/
/****************************************/

/* list models */
int drv_IL9341_list(void) {
	printf("IL9341 driver");
	return 0;
}

/* initialize driver & display */
/* use this function for a graphic display */
int drv_IL9341_init(const char *section, const int quiet) {
	int ret;

	/* real worker functions */
	drv_generic_graphic_real_blit = drv_IL9341_blit;

	/* start display */
	if ((ret = drv_IL9341_start(section)) != 0)
		return ret;

	/* initialize generic graphic driver */
	if ((ret = drv_generic_graphic_init(section, Name)) != 0)
		return ret;

	if (!quiet) {
		char buffer[40];
		qprintf(buffer, sizeof(buffer), "%s %dx%d", Name, DCOLS, DROWS);
		if (drv_generic_graphic_greet(buffer, NULL)) {
			sleep(3);
			drv_generic_graphic_clear();
		}
	}

	return 0;
}

/* close driver & display */
/* use this function for a graphic display */
int drv_IL9341_quit(const int quiet) {

	info("%s: shutting down.", Name);

	/* clear display */
	drv_generic_graphic_clear();

	/* say goodbye... */
	if (!quiet) {
		drv_generic_graphic_greet("goodbye!", NULL);
	}

	drv_generic_graphic_quit();

	debug("closing connection");
	drv_IL9341_close();

	return (0);
}

/* use this one for a graphic display */
DRIVER drv_IL9341 = { .name = Name, .list = drv_IL9341_list, .init =
		drv_IL9341_init, .quit = drv_IL9341_quit, };
