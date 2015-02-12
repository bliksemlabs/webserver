/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Stefan de Konink <stefan@konink.de>
 *
 * Copyright (C) 2001-2015 Alvaro Lopez Ortega
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
#include "rrrr/config.h"
#include "rrrr/router.h"
#include "rrrr/api.h"
#include "rrrr/set.h"
#include "rrrr/plan_render_otp.h"
#include "rrrr/tdata_realtime_expanded.h"

#define ENTRIES "rrrr"
#define OUTPUT_LEN 524288

PLUGIN_INFO_HANDLER_EASIEST_INIT (rrrr, http_get | http_post);

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
#if 0
		case 'b': {
			if (cherokee_buffer_cmp_str (key, "banned-trips-idx") == 0) {
				// TODO

			} else if (cherokee_buffer_cmp_str (key, "banned-stops-idx") == 0) {
				if (cherokee_atoi(value->buf, &hdl->req.banned_stop) == ret_ok) {
					hdl->req.n_banned_stops = 1;
				}

			} else if (cherokee_buffer_cmp_str (key, "banned-routes-idx") == 0) {
                i
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
#endif
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
                int stop_idx;
				cherokee_atoi(value->buf, &stop_idx);
                if (stop_idx >= 0 && stop_idx < props->tdata.n_stop_points) {
                    hdl->req.from_stop_point = (spidx_t) stop_idx;
                    hdl->has_from = true;
                }
			} else if (cherokee_buffer_cmp_str (key, "from-latlng") == 0) {
				hdl->has_from = strtolatlon(value->buf, &hdl->req.from_latlon);
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
                int stop_idx;
				cherokee_atoi(value->buf, &stop_idx);
                if (stop_idx >= 0 && stop_idx < props->tdata.n_stop_points) {
                    hdl->req.to_stop_point = (spidx_t) stop_idx;
                    hdl->has_to = true;
                }

			} else if (cherokee_buffer_cmp_str (key, "to-latlng") == 0) {
				hdl->has_to = strtolatlon(value->buf, &hdl->req.to_latlon);

			} else if (cherokee_buffer_cmp_str (key, "trip-attributes") == 0) {
				if (cherokee_buffer_cmp_str (value, "accessible") == 0) {
					hdl->req.vj_attributes |= vja_accessible;
				} else if (cherokee_buffer_cmp_str (value, "toilet") == 0) {
					hdl->req.vj_attributes |= vja_toilet;
				} else if (cherokee_buffer_cmp_str (value, "wifi") == 0) {
					hdl->req.vj_attributes |= vja_wifi;
				} else if (cherokee_buffer_cmp_str (value, "none") == 0) {
					hdl->req.vj_attributes = vja_none;
				}
			}
			break;
		}
#if 0
		case 'v': {
			if (cherokee_buffer_cmp_str (key, "via") == 0) {
				cherokee_atoi(value->buf, &hdl->req.via_stop_point);

			}
			break;
		}
#endif
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

    cherokee_connection_set_pathinfo (conn);

    if (conn->post.has_info) {
		return ret_ok;
    }

    if (cherokee_buffer_cmp_str (&conn->pathinfo, "/metadata") == 0) {
        cherokee_buffer_add_buffer (&hdl->output, &props->metadata);
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

    if (!hdl->has_from || !hdl->has_to) {
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

    if (hdl->req.time_rounded && ! (hdl->req.arrive_by)) {
        hdl->req.time++;
    }
    hdl->req.time_rounded = false;

    if (hdl->req.arrive_by) {
        hdl->req.time_cutoff = 0;
    } else {
        hdl->req.time_cutoff = UNREACHED;
    }

	CHEROKEE_RWLOCK_READER (&props->rwlock);

    plan_t plan;
    memset (&plan, 0, sizeof(plan_t));
    router_route_full_reversal (&hdl->router, &hdl->req, &plan);
    plan.req.time = hdl->req.time; /* restore the original request time */

	router_result_sort (&plan);
	cherokee_buffer_ensure_size (&hdl->output, OUTPUT_LEN);
    hdl->output.len = plan_render_otp (&plan, &props->tdata, hdl->output.buf, OUTPUT_LEN);


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
        tdata_apply_gtfsrt_tripupdates (&props->tdata, hdl->output.buf, hdl->output.len);
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
    memset (&n->router, 0, sizeof(router_t));
	router_setup (&n->router, &(PROP_RRRR(props)->tdata));

	cherokee_buffer_init (&n->output);
    n->has_from = false;
    n->has_to = false;

	*hdl = HANDLER(n);
	return ret_ok;
}


static ret_t
props_free  (cherokee_handler_rrrr_props_t *props)
{
	CHEROKEE_RWLOCK_DESTROY (&props->rwlock);
    tdata_close(&props->tdata);
	cherokee_buffer_mrproper (&props->metadata);
	cherokee_buffer_mrproper (&props->tdata_file);

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
		cherokee_buffer_init (&n->metadata);
	    cherokee_buffer_ensure_size (&n->metadata, 512);

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

    memset (&(props->tdata), 0, sizeof(tdata_t));
	tdata_load(&(props->tdata), props->tdata_file.buf);
    tdata_hashgrid_setup(&(props->tdata));
    props->metadata.len = metadata_render_otp (&(props->tdata), props->metadata.buf, props->metadata.size);

#if 0
    tdata_realtime_setup (&(props->tdata));
#endif

	CHEROKEE_RWLOCK_INIT (&props->rwlock, NULL);

	return ret_ok;
}

