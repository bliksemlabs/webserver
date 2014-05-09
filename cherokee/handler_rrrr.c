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
#include "handler_rrrr.h"
#include "connection-protected.h"
#include "thread.h"
#include "util.h"

#define ENTRIES "rrrr"
#define OUTPUT_LEN 524288

PLUGIN_INFO_HANDLER_EASIEST_INIT (rrrr, http_get | http_post);

static ret_t
latlon_to_idx(cherokee_handler_rrrr_t *hdl, cherokee_buffer_t *value, uint32_t *idx) {
	cherokee_handler_rrrr_props_t *props = HANDLER_RRRR_PROPS(hdl);

	const char *delim = ",";
	char *latlon = strdup(value->buf);
	if (latlon) {
		char *token = strtok (latlon, delim);
		if (token != NULL) {
			double lat = strtod(token, NULL);
			token = strtok(NULL, delim);
			if (token != NULL) {
				double lon = strtod(token, NULL);
				HashGridResult result;
				coord_t qc;
				double radius_meters = 1000.0;
				coord_from_lat_lon (&qc, lat, lon);
				HashGrid_query (&props->hashgrid, &result, qc, radius_meters);
				*idx = HashGridResult_closest (&result);
				free(latlon);
				return ret_ok;
			}
		}
		free(latlon);
	}
	return ret_not_found;
}

static int
arguments_while (cherokee_buffer_t *key, void *val, void *param)
{
	cherokee_buffer_t *value = BUF(val);
	cherokee_handler_rrrr_t *hdl = (cherokee_handler_rrrr_t *) param;
	cherokee_handler_rrrr_props_t *props = HANDLER_RRRR_PROPS(hdl);

	/* Skip key-only entries (with no value)
	 */
	if (value == NULL) {
		return ret_ok;
	}

	cherokee_buffer_unescape_uri(value);

	/* Check the value/
	 */
	switch (key->buf[0]) {
		case 'a': {
			if (cherokee_buffer_cmp_str (key, "arrive") == 0 &&
			    cherokee_buffer_case_cmp_str (value, "true") == 0) {
				hdl->req.arrive_by = true;
			}
			break;
		}

		case 'b': {
			if (cherokee_buffer_cmp_str (key, "banned-trips-idx") == 0) {
				// TODO

			} else if (cherokee_buffer_cmp_str (key, "banned-stops-idx") == 0) {
				if (cherokee_atoi(value->buf, &hdl->req.banned_stop) == ret_ok) {
					hdl->req.n_banned_stops = 1;
				}

			} else if (cherokee_buffer_cmp_str (key, "banned-routes-idx") == 0) {
				if (cherokee_atoi(value->buf, &hdl->req.banned_route) == ret_ok) {
					hdl->req.n_banned_routes = 1;
				}

			} else if (cherokee_buffer_cmp_str (key, "banned-stops-hard-idx") == 0) {
				if (cherokee_atoi(value->buf, &hdl->req.banned_stop_hard) == ret_ok) {
					hdl->req.n_banned_stops_hard = 1;
				}
			}
			break;
		}

		case 'd': {
			if (cherokee_buffer_cmp_str (key, "date") == 0) {
				struct tm ltm;
				memset (&ltm, 0, sizeof(struct tm));
				strptime (value->buf, "%Y-%m-%dT%H:%M:%S", &ltm);
				ltm.tm_isdst = -1;
				router_request_from_epoch (&hdl->req, &props->tdata, mktime(&ltm));

			} else if (cherokee_buffer_cmp_str (key, "depart") == 0 &&
				   cherokee_buffer_case_cmp_str (value, "true") == 0) {
				hdl->req.arrive_by = false;
			}
			break;
		}

		case 'f': {
			if (cherokee_buffer_cmp_str (key, "from-idx") == 0) {
				cherokee_atoi(value->buf, &hdl->req.from);

			} else if (cherokee_buffer_cmp_str (key, "from-latlng") == 0) {
				latlon_to_idx(hdl, value, &hdl->req.from);
			}
			break;
		}

		case 'm': {
			if (hdl->req.mode != m_all && cherokee_buffer_cmp_str (key, "mode") == 0) {
				switch (value->buf[0]) {
					case 'a': {
						if (cherokee_buffer_cmp_str (value, "all") == 0) {
							hdl->req.mode = m_all;
						}
						break;
					}

					case 'b': {
						if (cherokee_buffer_cmp_str (value, "bus") == 0) {
							hdl->req.mode |= m_bus;
						}
						break;
					}

					case 'c': {
						if (cherokee_buffer_cmp_str (value, "cablecar") == 0) {
							hdl->req.mode |= m_cablecar;
						}
						break;
					}

					case 'f': {
						if (cherokee_buffer_cmp_str (value, "ferry") == 0) {
							hdl->req.mode |= m_ferry;
						}
						break;
					}

					case 'g': {
						if (cherokee_buffer_cmp_str (value, "gondola") == 0) {
							hdl->req.mode |= m_gondola;
						}
						break;
					}

					case 'r': {
						if (cherokee_buffer_cmp_str (value, "rail") == 0) {
							hdl->req.mode |= m_rail;
						}
						break;
					}

					case 's': {
						if (cherokee_buffer_cmp_str (value, "subway") == 0) {
							hdl->req.mode |= m_subway;
						}
						break;
					}

					case 't': {
						if (cherokee_buffer_cmp_str (value, "tram") == 0) {
							hdl->req.mode |= m_tram;
						}
						break;
					}
				}
			}
			break;
		}

		case 'o': {
			if (hdl->req.optimise != o_all && cherokee_buffer_cmp_str (key, "optimise") == 0) {
				if (cherokee_buffer_cmp_str (value, "shortest") == 0) {
					hdl->req.optimise |= o_shortest;
				} else if (cherokee_buffer_cmp_str (value, "transfers") == 0) {
					hdl->req.optimise |= o_transfers;
				} else if (cherokee_buffer_cmp_str (value, "all") == 0) {
					hdl->req.optimise = o_all;
				}
			}
			break;
		}

		case 's': {
			if (cherokee_buffer_cmp_str (key, "start-trip-idx") == 0) {

			} else if (cherokee_buffer_cmp_str (key, "showIntermediateStops") == 0 &&
				   cherokee_buffer_case_cmp_str (value, "true") == 0) {
				hdl->req.intermediatestops = true;
			}
			break;
		}

		case 't': {
			if (cherokee_buffer_cmp_str (key, "to-idx") == 0) {
				cherokee_atoi(value->buf, &hdl->req.to);

			} else if (cherokee_buffer_cmp_str (key, "to-latlng") == 0) {
				latlon_to_idx(hdl, value, &hdl->req.to);

			} else if (cherokee_buffer_cmp_str (key, "trip-attributes") == 0) {
				if (cherokee_buffer_cmp_str (value, "accessible") == 0) {
					hdl->req.trip_attributes |= ta_accessible;
				} else if (cherokee_buffer_cmp_str (value, "toilet") == 0) {
					hdl->req.trip_attributes |= ta_toilet;
				} else if (cherokee_buffer_cmp_str (value, "wifi") == 0) {
					hdl->req.trip_attributes |= ta_wifi;
				} else if (cherokee_buffer_cmp_str (value, "none") == 0) {
					hdl->req.trip_attributes = ta_none;
				}
			}
			break;
		}

		case 'v': {
			if (cherokee_buffer_cmp_str (key, "via") == 0) {
				cherokee_atoi(value->buf, &hdl->req.via);

			}
			break;
		}

		case 'w': {
			if (cherokee_buffer_cmp_str (key, "walk-slack") == 0) {
				int walk_slack = 0;
				cherokee_atoi(value->buf, &walk_slack);
				if (walk_slack <= 255)
					hdl->req.walk_slack = walk_slack;

			} else if (cherokee_buffer_cmp_str (key, "walk-speed") == 0) {
				hdl->req.walk_speed = strtod(value->buf, NULL);

			}
			break;
		}
	}

	return 0;
}

ret_t
cherokee_handler_rrrr_init (cherokee_handler_rrrr_t *hdl)
{
	ret_t                          ret;
	cherokee_connection_t         *conn  = HANDLER_CONN(hdl);
	cherokee_handler_rrrr_props_t *props = HANDLER_RRRR_PROPS(hdl);

	if (conn->post.has_info) {
		return ret_ok;
    }

	/* Parse HTTP arguments
	 */
	ret = cherokee_connection_parse_args (conn);
	if (ret != ret_ok || AVL_GENERIC(conn->arguments)->root == NULL) {
        conn->error_code = http_not_found;
		return ret_error;
	}

	hdl->req.mode = 0;
	hdl->req.optimise = 0;

	/* Check all arguments
	 */
	ret = cherokee_avl_while (AVL_GENERIC(conn->arguments),
	                          (cherokee_avl_while_func_t) arguments_while,
	                          hdl, NULL, NULL);

	if (hdl->req.from == NONE || hdl->req.to == NONE) {
        conn->error_code = http_not_found;
		return ret_error;
	}

	if (hdl->req.mode == 0)
		hdl->req.mode = m_all;

	if (hdl->req.optimise == 0)
		hdl->req.optimise = o_all;

	if (hdl->req.time == UNREACHED) {
		struct tm ltm;
		memset (&ltm, 0, sizeof(struct tm));
		time_t now = time(0);
		ltm = *localtime(&now);
		router_request_from_epoch (&hdl->req, &props->tdata, mktime(&ltm));
	}

	router_request_t req = hdl->req;

	CHEROKEE_RWLOCK_READER (&props->rwlock);

	router_setup (&hdl->router, &props->tdata);

	router_route (&hdl->router, &req);

	// repeat search in reverse to compact transfers
	uint32_t n_reversals = req.arrive_by ? 1 : 2;

	// but do not reverse requests starting on board (they cannot be compressed, earliest arrival is good enough)
	if (req.start_trip_trip != NONE) n_reversals = 0;

	for (uint32_t i = 0; i < n_reversals; ++i) {
		// handle case where route is not reversed
		router_request_reverse (&hdl->router, &req);
		router_route (&hdl->router, &req);
	}

	cherokee_buffer_ensure_size (&hdl->output, OUTPUT_LEN);
	struct plan plan;
	router_result_to_plan (&plan, &hdl->router, &req);
	plan.req.time = hdl->req.time; // restore the original request time
	hdl->output.len = render_plan_json(&plan, &props->tdata, hdl->output.buf, OUTPUT_LEN);

	CHEROKEE_RWLOCK_UNLOCK(&props->rwlock);

	return ret_ok;
}


static ret_t
rrrr_read_post (cherokee_handler_rrrr_t *hdl)
{
	ret_t                  ret;
	cherokee_connection_t *conn    = HANDLER_CONN(hdl);
	cherokee_buffer_t     *post    = &HANDLER_THREAD(hdl)->tmp_buf1;
	cherokee_handler_rrrr_props_t *props = HANDLER_RRRR_PROPS(hdl);

	/* Check for the post info
	 */
	if (! conn->post.has_info) {
		conn->error_code = http_bad_request;
		return ret_error;
	}

	cherokee_buffer_clean (post);
	ret = cherokee_post_read (&conn->post, &conn->socket, post);
	switch (ret) {
	case ret_ok:
		cherokee_connection_update_timeout (conn);
		break;
	case ret_eagain:
		ret = cherokee_thread_deactive_to_polling (HANDLER_THREAD(hdl),
		                                           HANDLER_CONN(hdl),
		                                           conn->socket.socket,
		                                           FDPOLL_MODE_READ, false);
		if (ret != ret_ok) {
			return ret_error;
		} else {
			return ret_eagain;
		}
	default:
		conn->error_code = http_bad_request;
		return ret_error;
	}

	cherokee_buffer_add_buffer(&hdl->output, post);
	if (! cherokee_post_read_finished (&conn->post)) {
		return ret_eagain;
	} else {
		CHEROKEE_RWLOCK_WRITER(&props->rwlock);
		tdata_apply_gtfsrt (&props->tdata, hdl->output.buf, hdl->output.len);
		CHEROKEE_RWLOCK_UNLOCK(&props->rwlock);
	}

	return ret_ok;
}


static ret_t
rrrr_add_headers (cherokee_handler_rrrr_t *hdl,
                  cherokee_buffer_t *buffer)
{
	cherokee_buffer_add_str (buffer, "Content-Type: application/json" CRLF);
	if (HANDLER_SUPPORTS (hdl, hsupport_length)) {
		cherokee_buffer_add_str (buffer, "Content-Length: ");
		cherokee_buffer_add_ulong10 (buffer, (culong_t) hdl->output.len);
		cherokee_buffer_add_str (buffer, CRLF);
	}

	return ret_ok;
}


static ret_t
rrrr_step (cherokee_handler_rrrr_t *hdl,
           cherokee_buffer_t *buffer)
{
	cherokee_buffer_add_buffer (buffer, &hdl->output);
	return ret_eof_have_data;
}


static ret_t
rrrr_free (cherokee_handler_rrrr_t *hdl)
{
	router_teardown(&hdl->router);
	cherokee_buffer_mrproper(&hdl->output);

	return ret_ok;
}

ret_t
cherokee_handler_rrrr_new (cherokee_handler_t  **hdl,
                           void *cnt,
                           cherokee_module_props_t *props)
{
	CHEROKEE_NEW_STRUCT (n, handler_rrrr);

	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt, HANDLER_PROPS(props), PLUGIN_INFO_HANDLER_PTR(rrrr));

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_rrrr_init;
	MODULE(n)->free         = (module_func_free_t) rrrr_free;
	HANDLER(n)->step        = (handler_func_step_t) rrrr_step;
	HANDLER(n)->read_post   = (handler_func_read_post_t) rrrr_read_post;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) rrrr_add_headers;

	/* Supported features
	 */
	HANDLER(n)->support = hsupport_nothing;

	/* Properties
	 */
	router_request_initialize (&n->req);
	router_setup (&n->router, &(PROP_RRRR(props)->tdata));

	cherokee_buffer_init (&n->output);

	*hdl = HANDLER(n);
	return ret_ok;
}


static ret_t
props_free  (cherokee_handler_rrrr_props_t *props)
{
	tdata_close(&props->tdata);
	cherokee_buffer_mrproper (&props->tdata_file);
	HashGrid_teardown(&props->hashgrid);
	free(props->coords);
	CHEROKEE_RWLOCK_DESTROY (&props->rwlock);

	return ret_ok;
}


ret_t
cherokee_handler_rrrr_configure (cherokee_config_node_t   *conf,
                                 cherokee_server_t        *srv,
                                 cherokee_module_props_t **_props)
{
	ret_t                          ret;
	cherokee_list_t               *i;
	cherokee_handler_rrrr_props_t *props;

	/* Instance a new property object
	 */
	if (*_props == NULL) {
		CHEROKEE_NEW_STRUCT (n, handler_rrrr_props);

		cherokee_handler_props_init_base (HANDLER_PROPS(n),
		                                  MODULE_PROPS_FREE(props_free));

		cherokee_buffer_init (&n->tdata_file);

		*_props = MODULE_PROPS(n);
	}

	props = PROP_RRRR(*_props);

	/* Parse the configuration tree
	 */
	cherokee_config_node_foreach (i, conf) {
		cherokee_config_node_t *subconf = CONFIG_NODE(i);

		if (equal_buf_str (&subconf->key, "tdatafile")) {
			cherokee_buffer_add_buffer (&props->tdata_file, &subconf->val);
		}
	}

	tdata_load_dynamic(props->tdata_file.buf, &(props->tdata));
	props->coords = malloc(sizeof(coord_t) * props->tdata.n_stops);
	for (uint32_t c = 0; c < props->tdata.n_stops; ++c) {
		coord_from_latlon(props->coords + c, props->tdata.stop_coords + c);
	}
	HashGrid_init (&props->hashgrid, 100, 500.0, props->coords, props->tdata.n_stops);

	props->tdata.stopid_index  = rxt_load_strings_from_tdata (props->tdata.stop_ids, props->tdata.stop_ids_width, props->tdata.n_stops);
	props->tdata.tripid_index  = rxt_load_strings_from_tdata (props->tdata.trip_ids, props->tdata.trip_ids_width, props->tdata.n_trips);
	props->tdata.routeid_index = rxt_load_strings_from_tdata (props->tdata.route_ids, props->tdata.route_ids_width, props->tdata.n_routes);


	CHEROKEE_RWLOCK_INIT (&props->rwlock, NULL);

	return ret_ok;
}

