/* 
 * florence - Florence is a simple virtual keyboard for Gnome.

 * Copyright (C) 2008 François Agrech

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#include "system.h"
#include "florence.h"
#include "trace.h"
#include "trayicon.h"
#include "settings.h"
#include "layoutreader.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cspi/spi.h>

/* Called on destroy event (systray quit or close window) */
void flo_destroy (void)
{
	gtk_exit (0);
}

/* make florence appear in hidden mode */
void flo_show(struct florence *florence, Accessible *object)
{
	AccessibleComponent *component;
	long int x, y, w, h;
	gint screen_width, screen_height;

	/* positionnement intelligent */
	component=Accessible_getComponent(object);
	if (component) {
		screen_height=gdk_screen_get_height(gdk_screen_get_default());
		screen_width=gdk_screen_get_width(gdk_screen_get_default());
		AccessibleComponent_getExtents(component, &x, &y, &w, &h, SPI_COORD_TYPE_SCREEN);
		if (x<0) x=0;
		else if (florence->width<(screen_width-x-w)) x=screen_width-florence->width;
		if (florence->height<(screen_height-y-h)) gtk_window_move(florence->window, x, y+h);
		else if (y>florence->height) gtk_window_move(florence->window, x, y-florence->height);
		else gtk_window_move(florence->window, x, screen_height-florence->height);
	}
	gtk_widget_show(GTK_WIDGET(florence->window));
	/* Some winwow managers forget it */
	gtk_window_set_keep_above(florence->window, TRUE);
}

/* Called when a widget is focused.
 * Check if the widget is editable and show the keyboard or hide if not. */
void flo_focus_event (const AccessibleEvent *event, void *user_data)
{
	struct florence *florence=(struct florence *)user_data;

	if (Accessible_getRole(event->source)==SPI_ROLE_TERMINAL || Accessible_isEditableText(event->source)) {
		if (event->detail1) {
			flo_show(florence, event->source);
		}
		else {
			gtk_widget_hide(GTK_WIDGET(florence->window));
		}
	}
}

/* Shouldn't be used but it seems like we need to traverse accessible widgets when a new window is open to trigger
 * focus events. This is at least the case for gedit. Will need to check how this all work.
 * Can't we get a focus event when the widget is greated focussed? */
void flo_traverse (struct florence *florence, Accessible *obj)
{
	int n_children, i;
	Accessible *child;

	n_children=Accessible_getChildCount (obj);
	if (!Accessible_isTable(obj))
	{
		for (i=0;i<n_children;++i)
		{
			child=Accessible_getChildAtIndex(obj, i);
			if (Accessible_isEditableText(child) &&
				AccessibleStateSet_contains(Accessible_getStateSet(child), SPI_STATE_FOCUSED))
				flo_show(florence, child);
			else flo_traverse(florence, child);
			Accessible_unref(child);
		}
	}
}

/* Called when a window is created */
void flo_window_create_event (const AccessibleEvent *event, gpointer user_data)
{
	/* For some reason, focus state change does happen after traverse 
	 * ==> did I misunderstand? */
	/* TODO: remettre le keyboard au front. Attention: always_on_screen désactive cette fonction */
	flo_traverse((struct florence *)user_data, event->source);
}

/* Switches between always on screen mode and hidden mode.
 * When in hidden mode, the spi events are registered to monitor focus and show on editable widgets.
 * the events are deregistered when always on screen mode is activated */
void flo_switch_mode (struct florence *florence, gboolean on_screen)
{
	static AccessibleEventListener *focus_listener=NULL;
	static AccessibleEventListener *window_listener=NULL;

	if (!on_screen) {
		gtk_widget_hide(GTK_WIDGET(florence->window));
		focus_listener=SPI_createAccessibleEventListener (flo_focus_event, (void*)florence);
		SPI_registerGlobalEventListener(focus_listener, "object:state-changed:focused");
		window_listener=SPI_createAccessibleEventListener (flo_window_create_event, (void*)florence);
		SPI_registerGlobalEventListener(window_listener, "window:activate");
	} else {
		if (focus_listener) {
			SPI_deregisterGlobalEventListenerAll(focus_listener);
			AccessibleEventListener_unref(focus_listener);
			focus_listener=NULL;
		}
		if (window_listener) {
			SPI_deregisterGlobalEventListenerAll(window_listener);
			AccessibleEventListener_unref(window_listener);
			window_listener=NULL;
		}
		gtk_widget_show(GTK_WIDGET(florence->window));
	}
}

/* Callback called by the layour reader while parsing the layout file. Provides informations about the layout */
void flo_layout_infos(char *name, char *version)
{
	flo_info("Layout name: \"%s\"", name);
	if (strcmp(version, VERSION)) {
		flo_warn(_("Layout version %s is different from program version %s"), version, VERSION);
	}
}

/* load the keyboards from the layout file into the keyboards member of florence */
void flo_keyboards_load(struct florence *florence, xmlTextReaderPtr layout)
{
	int maj = XkbMajorVersion;
	int min = XkbMinorVersion;
	int opcode_rtrn=0, event_rtrn=0, error_rtrn=0;
	struct keyboard *keyboard;
	struct keyboard_globaldata global;

	/* Check XKB Version */
	if (!XkbLibraryVersion(&maj, &min) ||
		!XkbQueryExtension((Display *)gdk_x11_drawable_get_xdisplay(gdk_get_default_root_window()),
		&opcode_rtrn, &event_rtrn, &error_rtrn, &maj, &min)) {
		flo_fatal(_("XKB version mismatch"));
	}
	/* get the modifier map from xkb */
	global.xkb_desc=XkbGetMap((Display *)gdk_x11_drawable_get_xdisplay(gdk_get_default_root_window()),
	XkbKeyActionsMask|XkbModifierMapMask, XkbUseCoreKbd);
	/* get global modifiers state */
	XkbGetState((Display *)gdk_x11_drawable_get_xdisplay(gdk_get_default_root_window()), XkbUseCoreKbd, &(global.xkb_state));
	global.pressedkeys=&(florence->pressedkeys);

	/* initialize global data */
	global.key_table=(struct key **)&(florence->keys);
	global.style=florence->style;

	/* read the layout file and create the extensions */
	florence->keyboards=g_slist_append(florence->keyboards,
		(gpointer)keyboard_new(layout, 1, NULL, LAYOUT_VOID, &global));
	while ((keyboard=(struct keyboard *)layoutreader_readextension(layout,
		(layoutreader_keyboardprocess)keyboard_new, (void *)&global))) {
		florence->keyboards=g_slist_append(florence->keyboards, keyboard);
	}
	layoutreader_free(layout);

	/* Free the modifiers map */
	XkbFreeClientMap(global.xkb_desc, XkbKeyActionsMask|XkbModifierMapMask, True);
}

/* draws the background of florence */
void flo_draw (struct florence *florence, cairo_t *cairoctx,
	cairo_surface_t **surface, enum style_class class)
{
	GSList *list=florence->keyboards;
	struct keyboard *keyboard;
	cairo_t *offscreen;

	/* create the surface */
	if (!*surface) *surface=cairo_surface_create_similar(cairo_get_target(cairoctx),
		CAIRO_CONTENT_COLOR_ALPHA, florence->width, florence->height);
	offscreen=cairo_create(*surface);
	cairo_set_source_rgba(offscreen, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(offscreen, CAIRO_OPERATOR_SOURCE);
	cairo_paint(offscreen);

	/* browse the keyboards */
	cairo_save(offscreen);
	cairo_scale(offscreen, florence->zoom, florence->zoom);
	while (list)
	{
		keyboard=(struct keyboard *)list->data;
		if (keyboard_activated(keyboard)) {
			/* actual draw */
			switch(class) {
				case STYLE_SHAPE:
					keyboard_background_draw(keyboard, offscreen, florence->style);
					break;
				case STYLE_SYMBOL:
					keyboard_symbols_draw(keyboard, offscreen, florence->style,
						florence->globalmod);
					break;
			}
		}
		list=list->next;
	}
	cairo_destroy(offscreen);
}

/* draws the background of florence */
void flo_background_draw (struct florence *florence, cairo_t *cairoctx)
{
	flo_draw(florence, cairoctx, &(florence->background), STYLE_SHAPE);
}

/* draws the symbols */
void flo_symbols_draw (struct florence *florence, cairo_t *cairoctx) {
	flo_draw(florence, cairoctx, &(florence->symbols), STYLE_SYMBOL);
}

/* set global modifiers status (executed at startup) */
void flo_set_modifiers (struct florence *florence)
{
	guint i;
	for (i=0;i<256;i++) {
		if(florence->keys[i]) {
			if (key_is_pressed(florence->keys[i]))
				florence->globalmod|=key_get_modifier(florence->keys[i]);
		}
	}
}

/* calculate the dimensions of Florence */
void flo_set_dimensions(struct florence *florence)
{
	GSList *list=florence->keyboards;
	struct keyboard *keyboard;
	gdouble w, h;

	while (list)
	{
		keyboard=(struct keyboard *)list->data;
		if (keyboard_activated(keyboard)) {
			switch (keyboard_get_placement(keyboard)) {
				case LAYOUT_VOID:
					w=keyboard_get_width(keyboard);
					h=keyboard_get_height(keyboard);
					florence->xoffset=florence->yoffset=0;
					break;
				case LAYOUT_TOP:
					h+=(florence->yoffset+=keyboard_get_height(keyboard));
					break;
				case LAYOUT_BOTTOM:
					h+=keyboard_get_height(keyboard);
					break;
				case LAYOUT_LEFT:
					w+=(florence->xoffset+=keyboard_get_width(keyboard));
					break;
				case LAYOUT_RIGHT:
					w+=keyboard_get_width(keyboard);
					break;
			}
		}
		list = list->next;
	}
	florence->width=(guint)(w*florence->zoom);
	florence->height=(guint)(h*florence->zoom);
}

/* Create a hitmap for florence */
void flo_hitmap_create(struct florence *florence)
{
	GSList *list=florence->keyboards;
	struct keyboard *keyboard;
	gdouble width, height, xoffset, yoffset;
	gdouble x, y;

	florence->hitmap=g_malloc(florence->width*florence->height);
	memset(florence->hitmap, 0, florence->width*florence->height);
	/* browse the keyboards */
	while (list)
	{
		keyboard=(struct keyboard *)list->data;
		if (keyboard_activated(keyboard)) {
			/* get the position to draw the keyboard */
			switch (keyboard_get_placement(keyboard)) {
				case LAYOUT_VOID:
					width=keyboard_get_width(keyboard);
					height=keyboard_get_height(keyboard);
					xoffset=yoffset=0;
					x=y=0.0;
					break;
				case LAYOUT_TOP:
					yoffset+=keyboard_get_height(keyboard);
					x=0.0; y=-yoffset;
					break;
				case LAYOUT_BOTTOM:
					x=0.0; y=height;
					height+=keyboard_get_height(keyboard);
					break;
				case LAYOUT_LEFT:
					xoffset+=keyboard_get_width(keyboard);
					x=-xoffset; y=0.0;
					break;
				case LAYOUT_RIGHT:
					x=width; y=0.0;
					width+=keyboard_get_width(keyboard);
					break;
			}
			keyboard_hitmap_draw(keyboard, florence->hitmap, florence->width, florence->height,
				x+florence->xoffset, y+florence->yoffset, florence->zoom);
		}
		list = list->next;
	}
}

/* Create a window mask for stransparent window in shaped mode for non-composited screen */
/* For composited screen, this function is useless, use alpha channel instead. */
void flo_create_window_mask(struct florence *florence)
{
	GdkBitmap *mask=NULL;
	cairo_t *cairoctx=NULL;

	if (!florence->composite && settings_get_bool("window/shaped")) {
		if (!(mask=(GdkBitmap*)gdk_pixmap_new(NULL, florence->width, florence->height, 1)))
			flo_fatal(_("Unable to create mask"));
		cairoctx=gdk_cairo_create(mask);
		flo_background_draw(florence, cairoctx);
		cairo_set_source_rgba(cairoctx, 0.0, 0.0, 0.0, 0.0);
		cairo_set_operator(cairoctx, CAIRO_OPERATOR_SOURCE);
		cairo_paint(cairoctx);
		cairo_set_source_surface(cairoctx, florence->background, 0, 0);
		cairo_paint(cairoctx);
		gdk_window_shape_combine_mask(GTK_WIDGET(florence->window)->window, mask, 0, 0);
		cairo_destroy(cairoctx);
		cairo_surface_destroy(florence->background);
		florence->background=NULL;
		g_object_unref(G_OBJECT(mask));
	} else {
		gdk_window_shape_combine_mask(GTK_WIDGET(florence->window)->window, NULL, 0, 0);
	}
}

/* Triggered by gconf when the "shaped" parameter is changed. Calls flo_create_window_mask */
void flo_set_shaped(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	gboolean shown=GTK_WIDGET_VISIBLE(GTK_WINDOW(florence->window));
	gtk_widget_show(GTK_WIDGET(florence->window));
	flo_create_window_mask(florence);
	if (!shown) gtk_widget_hide(GTK_WIDGET(florence->window));
}

/* Triggered by gconf when the "decorated" parameter is changed. Decorates or undecorate the window. */
void flo_set_decorated(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	gtk_window_set_decorated(florence->window, gconf_value_get_bool(gconf_entry_get_value(entry)));
}

/* Triggered by gconf when the "always_on_screen" parameter is changed. */
void flo_set_show_on_focus(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	flo_switch_mode(florence, gconf_value_get_bool(gconf_entry_get_value(entry)));
}

/* Triggered by gconf when the "extensions" parameter is changed. */
void flo_update_extensions(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	flo_set_dimensions(florence);
	if (florence->hitmap) g_free(florence->hitmap);
	flo_hitmap_create(florence);
	if (florence->background) cairo_surface_destroy(florence->background);
	florence->background=NULL;
	if (florence->symbols) cairo_surface_destroy(florence->symbols);
	florence->symbols=NULL;
	gtk_widget_set_size_request(GTK_WIDGET(florence->window), florence->width, florence->height);
	flo_create_window_mask(florence);
	gtk_widget_queue_draw(GTK_WIDGET(florence->window));
}

/* Triggered by gconf when the "zoom" parameter is changed. */
void flo_set_zoom(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	florence->zoom=gconf_value_get_float(gconf_entry_get_value(entry));
	flo_update_extensions(client, xnxn_id, entry, user_data);
}

/* Triggered by gconf when the "zoom" parameter is changed. */
void flo_redraw(GConfClient *client, guint xnxn_id, GConfEntry *entry, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	char *key;
	key=strrchr(entry->key, '/');
	key+=1;
	style_update_colors(florence->style);
	if ((!strcmp(key, "key")) || (!strcmp(key, "outline"))) {
		if (florence->background) cairo_surface_destroy(florence->background);
		florence->background=NULL;
	} else if (!strcmp(key, "label")) {
		if (florence->symbols) cairo_surface_destroy(florence->symbols);
		florence->symbols=NULL;
	}
	gtk_widget_queue_draw(GTK_WIDGET(florence->window));
}

/* Registers callbacks to gconf changes */
void flo_register_settings_cb(struct florence *florence)
{
	settings_changecb_register("window/shaped", flo_set_shaped, florence);
	settings_changecb_register("window/decorated", flo_set_decorated, florence);
	settings_changecb_register("behaviour/always_on_screen", flo_set_show_on_focus, florence);
	settings_changecb_register("window/zoom", flo_set_zoom, florence);
	settings_changecb_register("layout/extensions", flo_update_extensions, florence);
	settings_changecb_register("colours/key", flo_redraw, florence);
	settings_changecb_register("colours/outline", flo_redraw, florence);
	settings_changecb_register("colours/label", flo_redraw, florence);
	settings_changecb_register("colours/activated", flo_redraw, florence);
}

/* Redraw the key to the window */
void flo_update_keys(struct florence *florence, GtkWidget *window, struct key *key, gboolean statechange)
{
	GdkRectangle rect;
	gdouble x, y, w, h;
	GList *found;

	if (key) {
		if (key_is_pressed(key)) {
			if (!g_list_find(florence->pressedkeys, key))
				florence->pressedkeys=g_list_prepend(florence->pressedkeys, key);
		} else {
			if ((found=g_list_find(florence->pressedkeys, key)))
				florence->pressedkeys=g_list_delete_link(florence->pressedkeys, found);
		}
		if (statechange) {
			florence->redrawsymbols=TRUE;
			gtk_widget_queue_draw(GTK_WIDGET(florence->window));
		} else {
			keyboard_key_getrect((struct keyboard *)key_get_userdata(key), key, &x, &y, &w, &h);
			rect.x=x*florence->zoom-5.0; rect.y=y*florence->zoom-5.0;
			rect.width=w*florence->zoom+10.0; rect.height=h*florence->zoom+10.0;
			gdk_window_invalidate_rect(window->window, &rect, TRUE);
		}
	}
}

/* update the timer representation: to be called when idle */
gboolean flo_timer_update(gpointer data)
{
	struct florence *florence=(struct florence *)data;
	if (florence->timer && florence->current) {
		flo_update_keys(florence, GTK_WIDGET(florence->window), florence->current, FALSE);
		return TRUE;
	} else return FALSE;
}


/* handles mouse motion events 
 * update the keyboard key under the mouse */
gboolean flo_mouse_move_event(GtkWidget *window, GdkEvent *event, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	struct key *current;
	guint code;
	gint x, y;
	y=(gint)((GdkEventMotion*)event)->y;
	x=(gint)((GdkEventMotion*)event)->x;
	if (x>=0 && x<florence->width && y>=0 && y<florence->height)
		code=florence->hitmap[(y*florence->width)+x];
	else code=0;
	if (florence->current!=florence->keys[code]) {
		if (florence->keys[code] && settings_get_double("behaviour/auto_click")>0.0) {
			if (florence->timer) g_timer_start(florence->timer);
			else {
				florence->timer=g_timer_new();
				g_idle_add(flo_timer_update, (gpointer)florence);
			}
		} else if (florence->timer) {
			g_timer_destroy(florence->timer);
			florence->timer=NULL;
		}
		current=florence->current;
		florence->current=florence->keys[code];
		flo_update_keys(florence, window, current, FALSE);
		flo_update_keys(florence, window, florence->current, FALSE);
	}
	return FALSE;
}

/* handles mouse leave events */
gboolean flo_mouse_leave_event (GtkWidget *window, GdkEvent *event, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	flo_update_keys(florence, window, florence->current, FALSE);
	florence->current=NULL;
	if (florence->timer) {
		g_timer_destroy(florence->timer);
		florence->timer=NULL;
	}
	/* As we don't support multitouch yet, and we no longer get button events when the mouse is outside,
	 * we just release any pressed key when the mouse leaves. */
	if (florence->pressed) {
		key_release(florence->pressed);
		flo_update_keys(florence, window, florence->pressed , FALSE);
	}
	florence->pressed=NULL;
	return FALSE;
}

/* handles button press events */
gboolean flo_button_press_event (GtkWidget *window, GdkEventButton *event, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	gboolean redrawall=FALSE;

	/* we don't want double and triple click events */
	if (event && ((event->type==GDK_2BUTTON_PRESS) || (event->type==GDK_3BUTTON_PRESS))) return FALSE;

	/* means 2 consecutive button press and no release, but we don't support multi-touch, yet. */
	/* so we just release any pressed key */
	if (florence->pressed) {
		key_release(florence->pressed);
		flo_update_keys(florence, window, florence->pressed, FALSE);
	}
	if (florence->current) {
		redrawall=key_get_modifier(florence->current) || florence->globalmod;
		key_press(florence->current, &(florence->pressedkeys), &(florence->globalmod));
		flo_update_keys(florence, window, florence->current, redrawall);
	}
	florence->pressed=florence->current;
	if (florence->timer) {
		g_timer_destroy(florence->timer);
		florence->timer=NULL;
	}
	return FALSE;
}

/* handles button release events */
gboolean flo_button_release_event (GtkWidget *window, GdkEvent *event, gpointer user_data)
{
	struct florence *florence=(struct florence *)user_data;
	if (florence->pressed) {
		key_release(florence->pressed);
		flo_update_keys(florence, window, florence->pressed, FALSE);
	}
	florence->pressed=NULL;
	if (florence->timer) {
		g_timer_destroy(florence->timer);
		florence->timer=NULL;
	}
	return FALSE;
}

/* on screen change event: check for composite extension */
void flo_screen_changed (GtkWidget *widget, GdkScreen *old_screen, struct florence *florence)
{
	GdkScreen *screen = NULL;
	GdkColormap *colormap = NULL;

	screen = gtk_widget_get_screen (widget);
	if (colormap == NULL) {
		florence->composite = FALSE;
		colormap = gdk_screen_get_rgb_colormap(screen);
	} else { 
		florence->composite = TRUE;
		colormap = gdk_screen_get_rgba_colormap (screen);
	}

	gtk_widget_set_colormap (widget, colormap);
}

/* on expose event: draws the keyboards to the window */
void flo_expose (GtkWidget *window, GdkEventExpose* pExpose, struct florence *florence)
{
	cairo_t *context;
	GList *list=florence->pressedkeys;
	struct keyboard *keyboard;
	struct key *key;
	gdouble timer=0.0;

	/* Don't need to redraw several times in one chunk */
	if (pExpose->count>0) return;

	/* create the context */
	context=gdk_cairo_create(window->window);

	/* prepare the background */
	if (!florence->background) {
		flo_background_draw(florence, context);
	}
	cairo_set_operator(context, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface(context, florence->background, 0, 0);
	cairo_paint(context);

	/* draw highlights (pressed keys) */
	cairo_save(context);
	cairo_scale(context, florence->zoom, florence->zoom);
	if (list) {
		while (list) {
			key=(struct key *)list->data;
			keyboard=(struct keyboard *)key_get_userdata(key);
			keyboard_press_draw(keyboard, context, florence->zoom, florence->style, key);
			list=list->next;
		}
	}

	/* focused key */
	if (florence->current) {
		key=florence->current;
		keyboard=(struct keyboard *)key_get_userdata(key);
		if (florence->timer) {
			timer=g_timer_elapsed(florence->timer, NULL)*1000./
				settings_get_double("behaviour/auto_click");
			keyboard_focus_draw(keyboard, context, florence->zoom, florence->style, key, timer);
			if (timer>=1.0) {
				flo_button_press_event(window, NULL, (void *)florence);
				flo_button_release_event(window, NULL, (void *)florence);
			}
		} else keyboard_focus_draw(keyboard, context, florence->zoom, florence->style, key, 0.0);
	}
	cairo_restore(context);

	/* draw the symbols */
	if (florence->redrawsymbols || (!florence->symbols)) {
		flo_symbols_draw(florence, context);
		florence->redrawsymbols=FALSE;
	}
	cairo_set_source_surface(context, florence->symbols, 0, 0);
	cairo_paint(context);

	/* and free up drawing memory */
	cairo_destroy(context);
}

/* liberate all the memory used by florence */
void flo_free(struct florence *florence)
{
	GSList *list=florence->keyboards;
	struct keyboard *keyboard;
	int i;
	while (list) {
		keyboard=(struct keyboard *)list->data;
		keyboard_free(keyboard);
		list=list->next;
	}
	for (i=0;i<256;i++) {
		if (florence->keys[i]) key_free(florence->keys[i]);
	}
	if (florence->pressedkeys) g_list_free(florence->pressedkeys);
	if (florence->hitmap) g_free(florence->hitmap);
	if (florence->style) style_free(florence->style);
	if (florence->background) cairo_surface_destroy(florence->background);
	if (florence->symbols) cairo_surface_destroy(florence->symbols);
	g_free(florence);
}

/* This is the main function.
 * Creates the hitmap, the window, combine the mask and draw the keyboard
 * Registers the event callbacks.
 * Call the event loop.
 * Cleans up at exit. */
int florence (void)
{
	xmlTextReaderPtr layout;
	struct florence *florence;
	const char *modules;

	settings_init(FALSE);
	/*flo_register_settings_cb(window);*/

	florence=g_malloc(sizeof(struct florence));
	if (!florence) flo_fatal(_("Unable to allocate memory for florence"));
	memset(florence, 0, sizeof(struct florence));

	layout=layoutreader_new();
	layoutreader_readinfos(layout, flo_layout_infos);
	florence->style=style_new(layout);
	florence->zoom=settings_get_double("window/zoom");
	flo_keyboards_load(florence, layout);

	flo_set_modifiers(florence);
	flo_set_dimensions(florence);
	flo_hitmap_create(florence);

	florence->window=GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_keep_above(florence->window, TRUE);
	gtk_window_set_accept_focus(florence->window, FALSE);
	gtk_window_set_skip_taskbar_hint(florence->window, TRUE);
	gtk_window_set_resizable(florence->window, FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(florence->window), florence->width, florence->height);
	gtk_container_set_border_width(GTK_CONTAINER(florence->window), 0);
	gtk_widget_set_events(GTK_WIDGET(florence->window),
		GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK);
	/*gtk_widget_realize(window);
	gdk_window_set_back_pixmap(window->window, NULL, FALSE);*/
	gtk_widget_set_app_paintable (GTK_WIDGET(florence->window), TRUE);
	gtk_window_set_decorated(florence->window, settings_get_bool("window/decorated"));

	g_signal_connect(G_OBJECT(florence->window), "destroy", G_CALLBACK(flo_destroy), NULL);
	g_signal_connect(G_OBJECT(florence->window), "screen-changed", G_CALLBACK(flo_screen_changed), florence);
	g_signal_connect(G_OBJECT(florence->window), "expose-event", G_CALLBACK(flo_expose), florence);
	g_signal_connect(G_OBJECT(florence->window), "motion-notify-event", G_CALLBACK(flo_mouse_move_event), florence);
	g_signal_connect(G_OBJECT(florence->window), "leave-notify-event", G_CALLBACK(flo_mouse_leave_event), florence);
	g_signal_connect(G_OBJECT(florence->window), "button-press-event", G_CALLBACK(flo_button_press_event), florence);
	g_signal_connect(G_OBJECT(florence->window), "button-release-event", G_CALLBACK(flo_button_release_event), florence);
	flo_screen_changed(GTK_WIDGET(florence->window), NULL, florence);
	gtk_widget_show(GTK_WIDGET(florence->window));
	flo_create_window_mask(florence);

        modules = g_getenv("GTK_MODULES");
	if (!modules||modules[0]=='\0')
		putenv("GTK_MODULES=gail:atk-bridge");
	SPI_init();
	flo_switch_mode(florence, settings_get_bool("behaviour/always_on_screen"));

	trayicon_create(GTK_WIDGET(florence->window), G_CALLBACK(flo_destroy));
	flo_register_settings_cb(florence);
	gtk_main();

	settings_exit();

	flo_free(florence);
	SPI_exit();
	putenv("AT_BRIDGE_SHUTDOWN=1");
	return 0;
}

