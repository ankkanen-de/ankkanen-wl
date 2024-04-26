#include <ankkanen-wl.h>
#include <layer-shell-v1.h>
#include <output.h>

void layers_arrange(struct ankkanen_wl_output *output) {
	struct ankkanen_wl_server *server = output->server;

	output->usable_area = output->full_area;

	struct ankkanen_wl_layer *l;
	wl_list_for_each(l, &server->layers, link)
	{
		if (l->surface->current.exclusive_zone > 0)
			wlr_scene_layer_surface_v1_configure(
				l->scene_tree,
				&output->full_area,
				&output->usable_area);
	}
	wl_list_for_each(l, &server->layers, link)
	{
		if (l->surface->current.exclusive_zone <= 0)
			wlr_scene_layer_surface_v1_configure(
				l->scene_tree,
				&output->full_area,
				&output->usable_area);
	}
} 

static void layer_surface_do_position(struct ankkanen_wl_layer *layer)
{
	struct wlr_layer_surface_v1 *layer_surface = layer->surface;
	struct ankkanen_wl_server *server = layer->server;

	struct wlr_box area;
	if (layer_surface->current.exclusive_zone == 0)
		area = server->primary_output->usable_area;
	else
		area = server->primary_output->full_area;

	int x = 0, y = 0;
	if (layer_surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		x = area.x;
	} else if (layer_surface->current.anchor &
		   ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		x = area.width - layer_surface->current.desired_width;
	} else {
		x = (area.width - layer_surface->current.desired_width) / 2;
	}

	if (layer_surface->current.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
		y = area.y;
	} else if (layer_surface->current.anchor &
		   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		y = area.height - layer_surface->current.desired_height;
	} else {
		y = (area.height - layer_surface->current.desired_height) / 2;
	}

	wlr_scene_node_set_position(&layer->scene_tree->tree->node, x, y);
}

static void layer_surface_map(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_layer *layer = wl_container_of(listener, layer, map);

	wl_list_insert(&layer->server->layers, &layer->link);
}

static void layer_surface_unmap(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_layer *layer =
		wl_container_of(listener, layer, unmap);

	wl_list_remove(&layer->link);
}

static void layer_surface_commit(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_layer *layer =
		wl_container_of(listener, layer, commit);
	uint32_t committed = layer->surface->current.committed;
	if (committed || layer->mapped != layer->surface->surface->mapped) {
		layer->mapped = layer->surface->surface->mapped;
		output_update_usable_area(layer->server->primary_output);
		layer_surface_do_position(layer);
	}
}

static void layer_surface_destroy(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_layer *layer =
		wl_container_of(listener, layer, destroy);

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->destroy.link);

	free(layer->node);
	free(layer);
}

void server_new_layer_surface(struct wl_listener *listener, void *data)
{
	struct ankkanen_wl_server *server =
		wl_container_of(listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *layer_surface = data;

	struct ankkanen_wl_node *node = calloc(1, sizeof(*node));
	struct ankkanen_wl_layer *layer = calloc(1, sizeof(*layer));
	layer->server = server;
	layer->surface = layer_surface;
	layer->scene_tree = wlr_scene_layer_surface_v1_create(
		layer->server->overlay_tree, layer_surface);
	layer->scene_tree->tree->node.data = node;
	layer->node = node;
	layer_surface->data = layer->scene_tree;
	node->layer = layer;

	layer->map.notify = layer_surface_map;
	wl_signal_add(&layer_surface->surface->events.map, &layer->map);
	layer->unmap.notify = layer_surface_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &layer->unmap);
	layer->commit.notify = layer_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &layer->commit);
	layer->destroy.notify = layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy, &layer->destroy);

	wlr_scene_layer_surface_v1_configure(
		layer->scene_tree, &server->primary_output->full_area,
		&server->primary_output->usable_area);
}
