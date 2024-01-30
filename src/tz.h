/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Generic timezone utilities.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 * 
 * Largely based on Michael Fulbright's work on Anaconda.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _E_TZ_H
#define _E_TZ_H

#include <glib.h>

#include "cc-timezone-location.h"

# define TZ_DATA_FILE "/usr/share/libtimezonemap/citiesInfo.txt"

# define ADMIN1_FILE "/usr/share/libtimezonemap/admin1Codes.txt"
# define COUNTRY_FILE "/usr/share/libtimezonemap/countryInfo.txt"

G_BEGIN_DECLS

typedef struct _TzDB TzDB;

struct _TzDB
{
	GPtrArray *locations;
};

TzDB      *tz_load_db                 (void);
void       tz_db_free                 (TzDB *db);
GPtrArray *tz_get_locations           (TzDB *db);

G_END_DECLS

#endif
