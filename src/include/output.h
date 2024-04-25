#ifndef _OUTPUT_H
#define _OUTPUT_H

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>

struct ankkanen_wl_output {
	struct wl_list link;
	struct ankkanen_wl_server *server;
	struct wlr_output *wlr_output;
	struct wlr_box full_area;
	struct wlr_box usable_area;

	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

void server_new_output(struct wl_listener *listener, void *data);

#endif //_OUTPUT_H
