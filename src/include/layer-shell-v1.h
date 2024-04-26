#ifndef _LAYER_SHELL_V1_H
#define _LAYER_SHELL_V1_H

#include <wlr/types/wlr_layer_shell_v1.h>

struct ankkanen_wl_layer {
	struct wl_list link;
	struct ankkanen_wl_node *node;
	struct ankkanen_wl_server *server;
	struct wlr_layer_surface_v1 *surface;
	struct wlr_scene_layer_surface_v1 *scene_tree;
	bool mapped;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
};

void layers_arrange(struct ankkanen_wl_output *output);
void server_new_layer_surface(struct wl_listener *listener, void *data);

#endif //_LAYER_SHELL_V1_H
