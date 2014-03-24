/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2013 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef CHEROKEE_HANDLER_GEOCODER_H
#define CHEROKEE_HANDLER_GEOCODER_H

#include "handler.h"
#include "balancer.h"
#include "plugin_loader.h"
#include "source.h"

#include <mysql.h>

typedef struct {
	cherokee_handler_props_t  base;
	cherokee_balancer_t      *balancer;
} cherokee_handler_geocoder_props_t;

typedef struct {
	cherokee_handler_t      base;
	cherokee_source_t      *src_ref;
	MYSQL                  *conn;
} cherokee_handler_geocoder_t;

#define HDL_GEOCODER(x)           ((cherokee_handler_geocoder_t *)(x))
#define PROP_GEOCODER(x)          ((cherokee_handler_geocoder_props_t *)(x))
#define HANDLER_GEOCODER_PROPS(x) (PROP_GEOCODER(MODULE(x)->props))

/* Library init function
 */
void PLUGIN_INIT_NAME(geocoder)      (cherokee_plugin_loader_t *loader);

/* Methods
 */
ret_t cherokee_handler_geocoder_new  (cherokee_handler_t **hdl, void *cnt, cherokee_module_props_t *props);
ret_t cherokee_handler_geocoder_free (cherokee_handler_geocoder_t *hdl);
ret_t cherokee_handler_geocoder_init (cherokee_handler_geocoder_t *hdl);

#endif /* CHEROKEE_HANDLER_GEOCODER_H */
