/* A managed X11 window followed by an override-redirect popup. */
#include <stdlib.h>
#include <time.h>
#include <xcb/xcb.h>

int
main(void)
{
	xcb_connection_t *c = xcb_connect(NULL, NULL);
	xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	xcb_window_t window = xcb_generate_id(c), popup = xcb_generate_id(c);
	uint32_t values[] = {0xcc4433, 1};
	const char name[] = "dwl-test-x11\0dwl-test-x11";
	struct timespec delay = {.tv_sec = 1};
	xcb_generic_event_t *event;
	if (xcb_connection_has_error(c)) return 1;
	xcb_create_window(c, XCB_COPY_FROM_PARENT, window, s->root,
		50, 50, 500, 300, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual,
		XCB_CW_BACK_PIXEL, values);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS,
		XCB_ATOM_STRING, 8, sizeof(name), name);
	xcb_map_window(c, window);
	xcb_flush(c);
	nanosleep(&delay, NULL);
	xcb_create_window(c, XCB_COPY_FROM_PARENT, popup, s->root,
		120, 120, 220, 120, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual,
		XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT, values);
	xcb_map_window(c, popup);
	xcb_flush(c);
	while ((event = xcb_wait_for_event(c))) free(event);
	xcb_disconnect(c);
	return 0;
}
