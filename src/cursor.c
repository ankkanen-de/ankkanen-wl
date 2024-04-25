#include <ankkanen-wl.h>
#include <cursor.h>
#include <xdg-shell.h>

void server_new_pointer(struct ankkanen_wl_server *server,
			struct wlr_input_device *device)
{
	wlr_cursor_attach_input_device(server->cursor, device);
}

void process_cursor_move(struct ankkanen_wl_server *server, uint32_t time)
{
	struct ankkanen_wl_toplevel *toplevel = server->grabbed_toplevel;
	xdg_toplevel_do_unmaximize(toplevel);
	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
	wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
				    server->cursor->x - server->grab_x,
				    server->cursor->y - server->grab_y);
}

void process_cursor_resize(struct ankkanen_wl_server *server, uint32_t time)
{
	struct ankkanen_wl_toplevel *toplevel = server->grabbed_toplevel;
	xdg_toplevel_do_unmaximize(toplevel);
	wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
	wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo_box);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
				    new_left - geo_box.x, new_top - geo_box.y);

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width,
				  new_height);
}

void process_cursor_motion(struct ankkanen_wl_server *server, uint32_t time)
{
	if (server->cursor_mode == ANKKANEN_WL_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == ANKKANEN_WL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct ankkanen_wl_toplevel *toplevel =
		desktop_toplevel_at(server, server->cursor->x,
				    server->cursor->y, &surface, &sx, &sy);
	if (!toplevel) {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr,
				       "default");
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

void server_cursor_motion(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x,
			event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
				 event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

void server_cursor_button(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	wlr_seat_pointer_notify_button(server->seat, event->time_msec,
				       event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct ankkanen_wl_toplevel *toplevel =
		desktop_toplevel_at(server, server->cursor->x,
				    server->cursor->y, &surface, &sx, &sy);
	if (!toplevel)
		return;
	if (event->state == WLR_BUTTON_RELEASED) {
		reset_cursor_mode(server);
	} else {
		focus_toplevel(toplevel, surface);
	}
}

void server_cursor_axis(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec,
				     event->orientation, event->delta,
				     event->delta_discrete, event->source);
}

void server_cursor_frame(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}
