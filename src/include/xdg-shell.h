#ifndef _XDG_SHELL_H
#define _XDG_SHELL_H

#include <wlr/types/wlr_xdg_shell.h>

struct ankkanen_wl_toplevel {
	struct wl_list link;
	struct ankkanen_wl_node *node;
	struct ankkanen_wl_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wlr_foreign_toplevel_handle_v1 *foreign_handle;
	struct wlr_box restore_box;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;

	struct wl_listener foreign_request_maximize;
	struct wl_listener foreign_request_minimize;
	struct wl_listener foreign_request_activate;
	struct wl_listener foreign_request_fullscreen;
	struct wl_listener foreign_request_close;
};

struct ankkanen_wl_toplevel *
desktop_toplevel_at(struct ankkanen_wl_server *server, double lx, double ly,
		    struct wlr_surface **surface, double *sx, double *sy);
void reset_cursor_mode(struct ankkanen_wl_server *server);
void focus_toplevel(struct ankkanen_wl_toplevel *toplevel,
		    struct wlr_surface *surface);
void xdg_toplevel_do_maximize(struct ankkanen_wl_toplevel *toplevel);
void xdg_toplevel_do_unmaximize(struct ankkanen_wl_toplevel *toplevel);
void server_new_xdg_surface(struct wl_listener *listener, void *data);

#endif //_XDG_SHELL_H
