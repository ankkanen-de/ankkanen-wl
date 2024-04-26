#ifndef _ANKKANEN_WL_H
#define _ANKKANEN_WL_H

#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#include <wlr/util/box.h>

enum ankkanen_wl_cursor_mode {
	ANKKANEN_WL_CURSOR_PASSTHROUGH,
	ANKKANEN_WL_CURSOR_MOVE,
	ANKKANEN_WL_CURSOR_RESIZE,
};

struct ankkanen_wl_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_scene_tree *overlay_tree;
	struct wlr_scene_tree *toplevel_tree;

	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager_v1;
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_layer_shell_v1 *layer_shell_v1;
	struct wl_listener new_xdg_surface;
	struct wl_listener new_layer_surface;
	struct wl_list toplevels;
	struct wl_list layers;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum ankkanen_wl_cursor_mode cursor_mode;
	struct ankkanen_wl_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
	struct ankkanen_wl_output *primary_output;
};

struct ankkanen_wl_node {
	struct ankkanen_wl_toplevel *toplevel;
	struct ankkanen_wl_layer *layer;
};

#endif //_ANKKANEN_WL_H
