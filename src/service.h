/* 
 * florence - Florence is a simple virtual keyboard for Gnome.

 * Copyright (C) 2012 François Agrech

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

#include <gio/gio.h>
#include "view.h"

#if GTK_CHECK_VERSION(2,26,0)

/* Service object */
struct service {
	guint owner_id;
	GDBusNodeInfo *introspection_data;
	struct view *view;
};

/* Create a service object */
struct service *service_new(struct view *view);
/* Destroy a service object */
void service_free(struct service *service);

#endif