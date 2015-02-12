void polyline_for_leg (tdata_t *tdata, leg_t *leg, polyline_t *pl);

/* Produces a polyline connecting a subset of the stops in a route,
 * or connecting two walk path endpoints if route_idx == WALK.
 * sidx0 and sidx1 are global stop indexes, not stop indexes within the route.
 */
void polyline_for_leg (tdata_t *tdata, leg_t *leg, polyline_t *pl) {
    polyline_begin(pl);

    if (leg->journey_pattern == WALK) {
        polyline_latlon (pl, tdata->stop_point_coords[leg->sp_from]);
        polyline_latlon (pl, tdata->stop_point_coords[leg->sp_to]);
    } else {
        jppidx_t i_jpp;
        journey_pattern_t jp = tdata->journey_patterns[leg->journey_pattern];
        spidx_t *stops = tdata_points_for_journey_pattern (tdata, leg->journey_pattern);
        bool output = false;
        for (i_jpp = 0; i_jpp < jp.n_stops; ++i_jpp) {
            spidx_t sidx = stops[i_jpp];
            if (!output && (sidx == leg->sp_from)) output = true;
            if (output) polyline_latlon (pl, tdata->stop_point_coords[sidx]);
            if (sidx == leg->sp_to) break;
        }
    }
    /* printf ("final polyline: %s\n\n", polyline_result ()); */
}

