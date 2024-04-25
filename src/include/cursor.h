#ifndef _CURSOR_H
#define _CURSOR_H

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_xcursor_manager.h>

void server_new_pointer(struct ankkanen_wl_server *server,
			struct wlr_input_device *device);
void process_cursor_move(struct ankkanen_wl_server *server, uint32_t time);
void process_cursor_resize(struct ankkanen_wl_server *server, uint32_t time);
void process_cursor_motion(struct ankkanen_wl_server *server, uint32_t time);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);

#endif //_CURSOR_H
