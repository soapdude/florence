/* 
   Florence - Florence is a simple virtual keyboard for Gnome.

   Copyright (C) 2008, 2009, 2010 François Agrech

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#ifndef FLO_KEYBOARD
#define FLO_KEYBOARD

#include "status.h"
#include <glib.h>
#ifdef ENABLE_XKB
#include <X11/XKBlib.h>
#endif
#include "key.h"
#include "layoutreader.h"
#include "xkeyboard.h"

/* A keyboard is a set of keys logically grouped together */
/* Examples: the main keyboard, the numpad or the function keys */
struct keyboard {
	gchar *id; /* identification of the keyboard: NULL for main keyboard */
	gchar *name; /* name of the keyboard: NULL for main keyboard */
	gdouble xpos, ypos; /* logical position of the keyboard (may change according to which keyboards are activated) */
	gdouble width, height; /* logical width and height of the keyboard */
	enum layout_placement placement; /* position of the kekboard relative to main (VOID placement) */
	gboolean under; /* TRUE if the keyboard is under another one */
	GSList *keys; /* list of the keys of the keyboard */
};

/* This structure contains data from hardware keyboard as well as global florence data
 * Used to initialize the keyboard */
struct keyboard_globaldata {
	struct style *style; /* style of florence  */
	struct status *status; /* status of the keybaord to update */
};

/* create a keyboard: the layout is passed as a text reader */
struct keyboard *keyboard_new (struct layout *layout, struct style *style, gchar *id, gchar *name,
	enum layout_placement placement, struct keyboard_globaldata *data);
/* delete a keyboard */
void keyboard_free (struct keyboard *keyboard);

/* Returns Keyboard width in pixels */
gdouble keyboard_get_width(struct keyboard *keyboard);
/* Returns Keyboard height in pixels */
gdouble keyboard_get_height(struct keyboard *keyboard);
/* Returns Keyboard position relative to the main keybord, as defined in the layout file */
enum layout_placement keyboard_get_placement(struct keyboard *keyboard);
/* Checks gconf for the activation of the keyboard. */
gboolean keyboard_activated(struct keyboard *keyboard);
/* Get the key at position (x,y) */
struct key *keyboard_hit_get(struct keyboard *keyboard, gint x, gint y, gdouble z);
/* returns a rectangle containing the key */
/* WARNING: not thread safe */
GdkRectangle *keyboard_key_getrect(struct keyboard *keyboard, struct key *key,
	gdouble zoom, gboolean focus_zoom);

/* update the relative position of the keyboard to the view */
void keyboard_set_pos(struct keyboard *keyboard, gdouble x, gdouble y);
/* tell the keyboard that it is under another one */
void keyboard_set_under(struct keyboard *keyboard);
/* tell the keyboard that it is above other keyboards */
void keyboard_set_over(struct keyboard *keyboard);
/* draw the keyboard background to cairo surface */
void keyboard_background_draw (struct keyboard *keyboard, cairo_t *cairoctx,
	struct style *style, struct status *status);
/* draw the keyboard symbols  to cairo surface */
void keyboard_symbols_draw (struct keyboard *keyboard, cairo_t *cairoctx,
	struct style *style, struct status *status);
/* clear the focus key from surface */
void keyboard_shape_clear (struct keyboard *keyboard, cairo_surface_t *surface,
	struct style *style, struct key *key, gdouble zoom);
/* add the focus key to surface */
void keyboard_shape_draw (struct keyboard *keyboard, cairo_surface_t *surface,
	struct style *style, struct key *key, gdouble zoom);
/* draw the focus indicator on a key */
void keyboard_focus_draw (struct keyboard *keyboard, cairo_t *cairoctx, gdouble w, gdouble h,
	struct style *style, struct key *key, struct status *status);
/* draw the pressed indicator on a key */
void keyboard_press_draw (struct keyboard *keyboard, cairo_t *cairoctx,
	struct style *style, struct key *key, struct status *status);

#endif

