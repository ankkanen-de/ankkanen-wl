#include <ankkanen-wl.h>
#include <output.h>
#include <xdg-shell.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/util/log.h>

struct ankkanen_wl_output *
output_for_toplevel(struct ankkanen_wl_toplevel *toplevel)
{
	struct wlr_scene_node *node = &toplevel->scene_tree->node;
	int lx, ly;
	wlr_scene_node_coords(node, &lx, &ly);
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		toplevel->server->output_layout, lx, ly);
	struct wlr_surface *surf = toplevel->xdg_toplevel->base->surface;
	if (!wlr_output)
		wlr_output = wlr_output_layout_output_at(
			toplevel->server->output_layout,
			lx + surf->current.width, ly);
	if (!wlr_output)
		wlr_output = wlr_output_layout_output_at(
			toplevel->server->output_layout, lx,
			ly + surf->current.height);
	if (!wlr_output)
		wlr_output = wlr_output_layout_output_at(
			toplevel->server->output_layout,
			lx + surf->current.width, ly + surf->current.height);
	return wlr_output ? wlr_output->data : NULL;
}

struct ankkanen_wl_toplevel *
desktop_toplevel_at(struct ankkanen_wl_server *server, double lx, double ly,
		    struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}

	struct ankkanen_wl_node *n = tree->node.data;
	return n ? n->toplevel : NULL;
}

void reset_cursor_mode(struct ankkanen_wl_server *server)
{
	server->cursor_mode = ANKKANEN_WL_CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
}

void focus_toplevel(struct ankkanen_wl_toplevel *toplevel,
		    struct wlr_surface *surface)
{
	if (toplevel == NULL)
		return;

	struct ankkanen_wl_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		return;
	}
	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel != NULL) {
			struct wlr_scene_tree *prev_tree =
				prev_toplevel->base->data;
			struct ankkanen_wl_node *prev_tl = prev_tree->node.data;
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
			wlr_foreign_toplevel_handle_v1_set_activated(
				prev_tl->toplevel->foreign_handle, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	wlr_foreign_toplevel_handle_v1_set_activated(toplevel->foreign_handle,
						     true);

	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(
			seat, toplevel->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, map);

	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, unmap);

	if (toplevel == toplevel->server->grabbed_toplevel) {
		reset_cursor_mode(toplevel->server);
	}

	wl_list_remove(&toplevel->link);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);

	wlr_foreign_toplevel_handle_v1_destroy(toplevel->foreign_handle);

	free(toplevel->node);
	free(toplevel);
}

static void begin_interactive(struct ankkanen_wl_toplevel *toplevel,
			      enum ankkanen_wl_cursor_mode mode, uint32_t edges)
{
	struct ankkanen_wl_server *server = toplevel->server;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (toplevel->xdg_toplevel->base->surface !=
	    wlr_surface_get_root_surface(focused_surface)) {
		return;
	}
	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == ANKKANEN_WL_CURSOR_MOVE) {
		server->grab_x =
			server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y =
			server->cursor->y - toplevel->scene_tree->node.y;
	} else {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base,
					     &geo_box);

		double border_x =
			(toplevel->scene_tree->node.x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y =
			(toplevel->scene_tree->node.y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y;

		server->resize_edges = edges;
	}
}

void xdg_toplevel_do_maximize(struct ankkanen_wl_toplevel *toplevel)
{
	struct ankkanen_wl_output *output = output_for_toplevel(toplevel);
	assert(output != NULL);
	toplevel->restore_box.x = toplevel->scene_tree->node.x;
	toplevel->restore_box.y = toplevel->scene_tree->node.y;
	toplevel->restore_box.width = toplevel->xdg_toplevel->current.width;
	toplevel->restore_box.height = toplevel->xdg_toplevel->current.height;
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
				  output->usable_area.width,
				  output->usable_area.height);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
				    output->usable_area.x,
				    output->usable_area.y);
}

void xdg_toplevel_do_unmaximize(struct ankkanen_wl_toplevel *toplevel)
{
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
				  toplevel->restore_box.width,
				  toplevel->restore_box.height);
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
				    toplevel->restore_box.x,
				    toplevel->restore_box.y);
}

void xdg_toplevel_do_minimize(struct ankkanen_wl_toplevel *toplevel)
{
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
	wlr_foreign_toplevel_handle_v1_set_activated(toplevel->foreign_handle,
						     false);
	if (toplevel->server->seat->keyboard_state.focused_surface ==
	    toplevel->xdg_toplevel->base->surface)
		wlr_seat_keyboard_clear_focus(toplevel->server->seat);
	wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
}

void xdg_toplevel_do_unminimize(struct ankkanen_wl_toplevel *toplevel)
{
	wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, ANKKANEN_WL_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener,
					void *data)
{
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, ANKKANEN_WL_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener,
					  void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->current.maximized) {
		if (!toplevel->xdg_toplevel->current.fullscreen)
			xdg_toplevel_do_unmaximize(toplevel);
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, false);
	} else {
		if (!toplevel->xdg_toplevel->current.fullscreen)
			xdg_toplevel_do_maximize(toplevel);
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, true);
	}
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener,
					    void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->current.fullscreen) {
		if (!toplevel->xdg_toplevel->current.maximized)
			xdg_toplevel_do_unmaximize(toplevel);
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
	} else {
		if (!toplevel->xdg_toplevel->current.maximized)
			xdg_toplevel_do_maximize(toplevel);
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
	}
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void foreign_request_maximize(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, foreign_request_maximize);

	if (!toplevel->xdg_toplevel->current.maximized) {
		if (!toplevel->xdg_toplevel->current.fullscreen)
			xdg_toplevel_do_maximize(toplevel);
		wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, true);
	}
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void foreign_request_minimize(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, foreign_request_minimize);

	xdg_toplevel_do_minimize(toplevel);
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void foreign_request_activate(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, foreign_request_activate);

	xdg_toplevel_do_unminimize(toplevel);
	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void foreign_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, foreign_request_fullscreen);

	if (!toplevel->xdg_toplevel->current.fullscreen) {
		if (!toplevel->xdg_toplevel->current.maximized)
			xdg_toplevel_do_maximize(toplevel);
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
	}
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void foreign_request_close(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_toplevel *toplevel =
		wl_container_of(listener, toplevel, foreign_request_close);

	wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
}

void server_new_xdg_surface(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent =
			wlr_xdg_surface_try_from_wlr_surface(
				xdg_surface->popup->parent);
		assert(parent != NULL);
		struct wlr_scene_tree *parent_tree = parent->data;
		xdg_surface->data =
			wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
		return;
	}
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	struct ankkanen_wl_node *node = calloc(1, sizeof(*node));
	struct ankkanen_wl_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_surface->toplevel;
	toplevel->scene_tree = wlr_scene_xdg_surface_create(
		toplevel->server->toplevel_tree, toplevel->xdg_toplevel->base);
	toplevel->scene_tree->node.data = node;
	toplevel->node = node;

	toplevel->foreign_handle = wlr_foreign_toplevel_handle_v1_create(
		server->foreign_toplevel_manager_v1);
	wlr_foreign_toplevel_handle_v1_set_app_id(
		toplevel->foreign_handle, toplevel->xdg_toplevel->app_id);
	wlr_foreign_toplevel_handle_v1_set_title(toplevel->foreign_handle,
						 toplevel->xdg_toplevel->title);
	struct wlr_scene_node *parent_node =
		&toplevel->scene_tree->node.parent->node;
	struct ankkanen_wl_node *parent_tl = parent_node->data;
	if (parent_tl)
		wlr_foreign_toplevel_handle_v1_set_parent(
			toplevel->foreign_handle,
			parent_tl->toplevel->foreign_handle);

	toplevel->foreign_request_maximize.notify = foreign_request_maximize;
	wl_signal_add(&toplevel->foreign_handle->events.request_maximize,
		      &toplevel->foreign_request_maximize);
	toplevel->foreign_request_minimize.notify = foreign_request_minimize;
	wl_signal_add(&toplevel->foreign_handle->events.request_minimize,
		      &toplevel->foreign_request_minimize);
	toplevel->foreign_request_activate.notify = foreign_request_activate;
	wl_signal_add(&toplevel->foreign_handle->events.request_activate,
		      &toplevel->foreign_request_activate);
	toplevel->foreign_request_fullscreen.notify =
		foreign_request_fullscreen;
	wl_signal_add(&toplevel->foreign_handle->events.request_fullscreen,
		      &toplevel->foreign_request_fullscreen);
	toplevel->foreign_request_close.notify = foreign_request_close;
	wl_signal_add(&toplevel->foreign_handle->events.request_close,
		      &toplevel->foreign_request_close);

	xdg_surface->data = toplevel->scene_tree;
	node->toplevel = toplevel;

	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &toplevel->unmap);
	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &toplevel->destroy);

	struct wlr_xdg_toplevel *xdg_toplevel = xdg_surface->toplevel;
	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move,
		      &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize,
		      &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		      &toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		      &toplevel->request_fullscreen);
}
