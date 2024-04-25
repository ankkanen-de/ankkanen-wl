#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <wlr/types/wlr_keyboard.h>

struct ankkanen_wl_keyboard {
	struct wl_list link;
	struct ankkanen_wl_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

void server_new_keyboard(struct ankkanen_wl_server *server,
			 struct wlr_input_device *device);

#endif //_KEYBOARD_H
