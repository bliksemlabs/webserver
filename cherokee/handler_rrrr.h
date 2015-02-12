/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Stefan de Konink <stefan@konink.de>
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

#ifndef CHEROKEE_HANDLER_RRRR_H
#define CHEROKEE_HANDLER_RRRR_H

#include "rrrr/config.h"
#include "rrrr/router.h"
#include "rrrr/hashgrid.h"
#include "handler.h"
#include "balancer.h"
#include "plugin_loader.h"

typedef struct {
	cherokee_handler_props_t  base;
	cherokee_buffer_t         tdata_file;
	cherokee_buffer_t         metadata;
	tdata_t                   tdata;
	CHEROKEE_RWLOCK_T        (rwlock);
} cherokee_handler_rrrr_props_t;

typedef struct {
	cherokee_handler_t      base;
	router_t                router;
	router_request_t        req;
	cherokee_buffer_t       output;
    cherokee_boolean_t      has_from;
    cherokee_boolean_t      has_to;
} cherokee_handler_rrrr_t;

#define HDL_RRRR(x)           ((cherokee_handler_rrrr_t *)(x))
#define PROP_RRRR(x)          ((cherokee_handler_rrrr_props_t *)(x))
#define HANDLER_RRRR_PROPS(x) (PROP_RRRR(MODULE(x)->props))

/* Library init function
 */
void PLUGIN_INIT_NAME(rrrr)      (cherokee_plugin_loader_t *loader);

/* Methods
 */
ret_t cherokee_handler_rrrr_new  (cherokee_handler_t **hdl, void *cnt, cherokee_module_props_t *props);
ret_t cherokee_handler_rrrr_free (cherokee_handler_rrrr_t *hdl);
ret_t cherokee_handler_rrrr_init (cherokee_handler_rrrr_t *hdl);

#endif /* CHEROKEE_HANDLER_RRRR_H */