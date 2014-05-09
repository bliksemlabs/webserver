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

#include "common-internal.h"
#include "handler_geocoder.h"
#include "connection-protected.h"
#include "thread.h"
#include "util.h"

#define ENTRIES "geocoder"

PLUGIN_INFO_HANDLER_EASIEST_INIT (geocoder, http_get);

static ret_t
connect_to_database (cherokee_handler_geocoder_t *hdl)
{
	MYSQL                             *conn;
	cherokee_handler_geocoder_props_t *props      = HANDLER_GEOCODER_PROPS(hdl);
	cherokee_connection_t             *connection = HANDLER_CONN(hdl);

	conn = mysql_real_connect (hdl->conn,
				   hdl->src_ref->host.buf,
				   NULL,
				   NULL,
				   NULL,
				   hdl->src_ref->port,
				   hdl->src_ref->unix_socket.buf,
				   0);
	if (conn == NULL) {
		cherokee_balancer_report_fail (props->balancer, connection, hdl->src_ref);

		connection->error_code = http_bad_gateway;
		return ret_error;
	}

	return ret_ok;
}

static ret_t
send_query (cherokee_handler_geocoder_t *hdl)
{
    int                    i;
	int                    re;
	cuint_t                len;
	cherokee_connection_t *conn = HANDLER_CONN(hdl);
	cherokee_handler_geocoder_props_t *props = HANDLER_GEOCODER_PROPS(hdl);
	cherokee_buffer_t     *tmp  = &HANDLER_THREAD(hdl)->tmp_buf1;

	/* Extract the SQL query
	 */
	if ((cherokee_buffer_is_empty (&conn->web_directory)) ||
	    (cherokee_buffer_is_ending (&conn->web_directory, '/')))
	{
		len = conn->web_directory.len;
	} else {
		len = conn->web_directory.len + 1;
	}

	cherokee_buffer_clean (tmp);
	cherokee_buffer_add   (tmp,
			       conn->request.buf + len,
			       conn->request.len - len);

	cherokee_buffer_unescape_uri (tmp);

    /* Guard for query injection
     */
    for (i = 0; i < tmp->len; i++) if (tmp->buf[i] == '\'') tmp->buf[i] = ' ';

    /* Build the SphinxQL query
     */
    cherokee_buffer_prepend_buf (tmp, &props->query);
    cherokee_buffer_add_str (tmp, "') ORDER BY w DESC, woonplaatsnaam ASC, provincienaam ASC, openbareruimtenaam ASC, huisnummer ASC OPTION ranker=expr('sum(lcs*user_weight)+bm25');");
    TRACE(ENTRIES, "Geocoder Query: %s\n", tmp->buf);

	/* Send the query
	 */
	re = mysql_real_query (hdl->conn, tmp->buf, tmp->len);
	if (re != 0)
		return ret_error;

	return ret_ok;
}


ret_t
cherokee_handler_geocoder_init (cherokee_handler_geocoder_t *hdl)
{
	ret_t                              ret;
	cherokee_connection_t             *conn  = HANDLER_CONN(hdl);
	cherokee_handler_geocoder_props_t *props = HANDLER_GEOCODER_PROPS(hdl);

	/* Get a reference to the target host
	 */
	if (hdl->src_ref == NULL) {
		ret = cherokee_balancer_dispatch (props->balancer, conn, &hdl->src_ref);
		if (ret != ret_ok)
			return ret;
	}

	/* Connect to the MySQL server
	 */
	ret = connect_to_database(hdl);
	if (unlikely (ret != ret_ok))
		return ret;

	/* Send query:
	 * Do not check whether it failed, ::step() will do
	 * it and send an error message if needed.
	 */
	send_query(hdl);

	return ret_ok;
}


static ret_t
geocoder_add_headers (cherokee_handler_geocoder_t *hdl,
		      cherokee_buffer_t           *buffer)
{
    cherokee_buffer_add_str (buffer, "Content-Type: application/json" CRLF);

	return ret_ok;
}


static ret_t
render_geojson_result (cherokee_handler_geocoder_t *hdl,
           cherokee_buffer_t           *buffer,
	       MYSQL_RES                   *result)
{
	cuint_t      i = 0;
	cuint_t      num_fields;
	MYSQL_ROW    row;
	MYSQL_FIELD *fields;

	num_fields = mysql_num_fields (result);
	fields     = mysql_fetch_fields (result);

	/* Types
	 * Blobs: http://www.mysql.org/doc/refman/5.1/en/c-api-datatypes.html
	 */

    cherokee_buffer_add_str (buffer, "{\"type\":\"FeatureCollection\",\"features\":[");
	while (true) {
		row = mysql_fetch_row (result);
		if (! row)
			break;

        if (i > 0)
            cherokee_buffer_add_str(buffer, ",");

        cherokee_buffer_add_va (buffer, "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[%s,%s]},\"properties\":{\"search\":\"%s\"", row[1], row[2], row[3]);
		for(i = 4; i < num_fields; i++) {
			switch(fields[i].type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_INT24:
			case MYSQL_TYPE_DECIMAL:
			case MYSQL_TYPE_NEWDECIMAL:
			case MYSQL_TYPE_DOUBLE:
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_LONGLONG:
                if (row[i] == NULL) {
                    cherokee_buffer_add_va (buffer, ",\"%s\": null", fields[i].name);
                    continue;
                }
                cherokee_buffer_add_va (buffer, ",\"%s\": %s", fields[i].name, row[i]);
				break;

			case MYSQL_TYPE_BIT:
			case MYSQL_TYPE_TIMESTAMP:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_YEAR:
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_VAR_STRING:
			case MYSQL_TYPE_NEWDATE:
			case MYSQL_TYPE_VARCHAR:
                if (row[i] == NULL) {
                    cherokee_buffer_add_va (buffer, ",\"%s\": null", fields[i].name);
                    continue;
                }
                cherokee_buffer_add_va (buffer, ",\"%s\": \"%s\"", fields[i].name, row[i]);
				break;

			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_TINY_BLOB:
			case MYSQL_TYPE_MEDIUM_BLOB:
			case MYSQL_TYPE_LONG_BLOB:
				if ((row[i] == NULL) ||
				    (fields[i].charsetnr != 63))
				{
                    cherokee_buffer_add_va (buffer, ",\"%s\": null", fields[i].name);
					continue;
				}
                cherokee_buffer_add_va (buffer, ",\"%s\": \"%s\"", fields[i].name, row[i]);
				break;

			case MYSQL_TYPE_SET:
			case MYSQL_TYPE_ENUM:
			case MYSQL_TYPE_GEOMETRY:
			case MYSQL_TYPE_NULL:
                cherokee_buffer_add_va (buffer, ",\"%s\": null", fields[i].name);
				break;
			default:
				SHOULDNT_HAPPEN;
			}
		}
        cherokee_buffer_add_str (buffer, "}}");
	}

    cherokee_buffer_add_str (buffer, "]}");

	return ret_ok;
}

static ret_t
geocoder_step (cherokee_handler_geocoder_t *hdl,
	       cherokee_buffer_t           *buffer)
{
	MYSQL_RES *result = mysql_store_result (hdl->conn);

    if (result == NULL) {
        cherokee_buffer_add_str(buffer, "{}");
    } else {
        render_geojson_result(hdl, buffer, result);
	    mysql_free_result (result);
    }

	return ret_eof_have_data;
}


static ret_t
geocoder_free (cherokee_handler_geocoder_t *hdl)
{
	if (hdl->conn)
		mysql_close (hdl->conn);

	return ret_ok;
}

ret_t
cherokee_handler_geocoder_new (cherokee_handler_t     **hdl,
			       void                    *cnt,
			       cherokee_module_props_t *props)
{
	CHEROKEE_NEW_STRUCT (n, handler_geocoder);

	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt, HANDLER_PROPS(props), PLUGIN_INFO_HANDLER_PTR(geocoder));

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_geocoder_init;
	MODULE(n)->free         = (module_func_free_t) geocoder_free;
	HANDLER(n)->step        = (handler_func_step_t) geocoder_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) geocoder_add_headers;

	/* Supported features
	 */
	HANDLER(n)->support     = hsupport_nothing;

	/* Properties
	 */
	n->src_ref  = NULL;

	/* MySQL */
	n->conn = mysql_init (NULL);
	if (unlikely (n->conn == NULL)) {
		cherokee_handler_free (HANDLER(n));
		return ret_nomem;
	}

	*hdl = HANDLER(n);
	return ret_ok;
}


static ret_t
props_free  (cherokee_handler_geocoder_props_t *props)
{
	if (props->balancer)
		cherokee_balancer_free (props->balancer);

    cherokee_buffer_mrproper (&props->query);

	return ret_ok;
}


ret_t
cherokee_handler_geocoder_configure (cherokee_config_node_t  *conf,
				     cherokee_server_t       *srv,
				     cherokee_module_props_t **_props)
{
	ret_t                              ret;
	cherokee_list_t                   *i;
	cherokee_handler_geocoder_props_t *props;

	/* Instance a new property object
	 */
	if (*_props == NULL) {
		CHEROKEE_NEW_STRUCT (n, handler_geocoder_props);

		cherokee_handler_props_init_base (HANDLER_PROPS(n),
						  MODULE_PROPS_FREE(props_free));
		n->balancer = NULL;

        cherokee_buffer_init (&n->query);

		*_props = MODULE_PROPS(n);
	}

	props = PROP_GEOCODER(*_props);

	/* Parse the configuration tree
	 */
	cherokee_config_node_foreach (i, conf) {
		cherokee_config_node_t *subconf = CONFIG_NODE(i);

		if (equal_buf_str (&subconf->key, "balancer")) {
			ret = cherokee_balancer_instance (&subconf->val, subconf, srv, &props->balancer);
			if (ret != ret_ok)
				return ret;
		} else

		if (equal_buf_str (&subconf->key, "indices")) {
            cherokee_buffer_clean(&props->query);
            cherokee_buffer_add_str (&props->query, "SELECT WEIGHT() AS w, lon, lat, search, type FROM ");
            cherokee_buffer_add_buffer (&props->query, &subconf->val);
            cherokee_buffer_add_str (&props->query, " WHERE MATCH('");
		}

	}

	/* Final checks
	 */
	if (props->balancer == NULL) {
		LOG_CRITICAL_S (CHEROKEE_ERROR_HANDLER_GEOCODER_BALANCER);
		return ret_error;
	}

	return ret_ok;
}


