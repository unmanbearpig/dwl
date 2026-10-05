/* Exercise the compositor with real Wayland/XWayland clients on headless outputs. */
_Noreturn static void die(const char *fmt, ...);
#define main dwl_main
#include "../dwl.c"
#undef main
#include <assert.h>

#define CHECK(expr) do { if (!(expr)) die("FAIL line %d: %s", __LINE__, #expr); } while (0)
static struct wl_event_source *test_timer;
static int step, attempts;
static Monitor *first, *second;
static Client *a, *b;
static pid_t test_children[4];
static size_t child_count;

static void
launch(const char *id)
{
	pid_t pid = fork();
	CHECK(pid >= 0);
	if (!pid) {
		if (!strcmp(id, "x11"))
			execl("tests/xclient", "tests/xclient", NULL);
		else
			execlp("foot", "foot", "-c", "/dev/null", "--app-id", id,
				"--title", id, "/bin/cat", NULL);
		_exit(127);
	}
	test_children[child_count++] = pid;
}

static Client *
findclient(const char *id)
{
	Client *c;
	wl_list_for_each(c, &clients, link)
		if (!strcmp(client_get_appid(c), id))
			return c;
	return NULL;
}

static void
clickat(double x, double y)
{
	struct wlr_pointer_button_event event = {.button = BTN_LEFT, .time_msec = 1};
	wlr_cursor_warp_closest(cursor, NULL, x, y);
	motionnotify(1, NULL, 0, 0, 0, 0);
	event.state = WL_POINTER_BUTTON_STATE_PRESSED;
	buttonpress(NULL, &event);
	event.state = WL_POINTER_BUTTON_STATE_RELEASED;
	buttonpress(NULL, &event);
}

static int
tick(void *data)
{
	Client *c, *hit = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_box before;
	const Key *key;
	int fd[2];

	CHECK(++attempts < 150);
	switch (step) {
	case 0:
		CHECK(wl_list_length(&mons) == 2);
		first = wl_container_of(mons.next, first, link);
		second = wl_container_of(first->link.next, second, link);
		selmon = first;
		CHECK(first->w.height == first->m.height - first->b.real_height);
		CHECK(input_method_manager && text_input_manager && foreign_toplevel_list);
		key = keybinding(WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_F1);
		CHECK(key && key->func == chvt && key->arg.ui == 1);
		key = keybinding(MODKEY | WLR_MODIFIER_CAPS, XKB_KEY_b);
		CHECK(key && key->func == togglebar);
		clickat(first->m.x + TEXTW(first, tags[0]) + 4,
			first->m.y + first->b.real_height / 2);
		CHECK(first->tagset[first->seltags] == 2);
		clickat(first->m.x + 300, first->m.y + 300);
		view(&(Arg){.ui = 1});
		setmfact(&(Arg){.f = 1.65f});
		incnmaster(&(Arg){.i = 1});
		setlayout(&(Arg){.v = &layouts[2]});
		view(&(Arg){.ui = 2});
		CHECK(first->lt[first->sellt] == &layouts[0]);
		view(&(Arg){.ui = 1});
		CHECK(first->lt[first->sellt] == &layouts[2]);
		CHECK(fabsf(first->mfact - 0.65f) < 0.001f && first->nmaster == 2);
		view(&(Arg){.ui = ~0u});
		CHECK(first->pertag->curtag == 0);
		toggleview(&(Arg){.ui = 1});
		CHECK(first->pertag->curtag == 2);
		toggleview(&(Arg){.ui = 1});
		CHECK(first->pertag->curtag == 0);
		view(&(Arg){.ui = 1});
		setlayout(&(Arg){.v = &layouts[0]});
		incnmaster(&(Arg){.i = -1});
		CHECK(pipe(fd) == 0);
		CHECK(fd_set_nonblock(fd[0]) == 0);
		statusin(fd[0], WL_EVENT_READABLE, NULL); /* EAGAIN must not index status[-1]. */
		CHECK(write(fd[1], "test status\n", 12) == 12);
		statusin(fd[0], WL_EVENT_READABLE, NULL);
		CHECK(!strcmp(stext, "test status"));
		close(fd[0]); close(fd[1]);
		puts("PASS startup, bar clicks, shortcuts, per-tag settings, status input");
		launch("dwl-test-a");
		break;
	case 1:
		if (!(a = findclient("dwl-test-a"))) goto wait;
		CHECK(a->bw == 0 && a->geom.width == first->w.width);
		CHECK(a->foreign_toplevel_handle && a->image_capture_scene);
		launch("dwl-test-b");
		break;
	case 2:
		if (!(b = findclient("dwl-test-b"))) goto wait;
		CHECK(a->bw == borderpx && b->bw == borderpx);
		CHECK(a->geom.width + b->geom.width == first->w.width);
		focusclient(a, NULL, 1);
		setfullscreen(a, 1);
		CHECK(a->bw == 0 && a->geom.height == first->m.height);
		CHECK(a->scene->node.parent == layers[LyrFS]);
		break;
	case 3:
		setfullscreen(a, 0);
		CHECK(a->bw == borderpx);
		setmon(b, second, 1);
		selmon = first;
		before = b->geom;
		setlayout(&(Arg){.v = &layouts[1]});
		CHECK(!memcmp(&before, &b->geom, sizeof(before)));
		CHECK(a->bw == borderpx);
		break;
	case 4:
		CHECK(a->bw == borderpx); /* client commits must preserve floating-layout borders */
		setlayout(&(Arg){.v = &layouts[0]});
		CHECK(a->bw == 0);
		focusclient(a, NULL, 1);
		togglebar(NULL);
		CHECK(!first->scene_buffer->node.enabled && a->geom.height == first->m.height);
		togglebar(NULL);
		CHECK(first->scene_buffer->node.enabled && a->geom.height == first->w.height);
		setfloating(a, 1);
		CHECK(a->isfloating && a->bw == borderpx);
		key = keybinding(MODKEY | WLR_MODIFIER_SHIFT, XKB_KEY_t);
		if (key && key->func != tag) {
			key->func(&key->arg);
			CHECK(!a->isfloating);
		} else {
			setfloating(a, 0);
		}
		/* Move outputs vertically and exercise upstream monitor navigation. */
		wlr_output_layout_add(output_layout, first->wlr_output, 0, 0);
		wlr_output_layout_add(output_layout, second->wlr_output, 0, first->m.height);
		selmon = first;
		CHECK(dirtomon(WLR_DIRECTION_DOWN) == second);
		selmon = second;
		CHECK(dirtomon(WLR_DIRECTION_DOWN) == first);
		selmon = first;
		puts("PASS real Wayland clients, smart borders, fullscreen, floating, two monitors");
#ifdef XWAYLAND
		launch("x11");
#else
		step = 6;
#endif
		break;
	case 5:
		if (!(c = findclient("dwl-test-x11"))) goto wait;
		CHECK(client_is_x11(c));
		setfullscreen(c, 1);
		break;
	case 6:
#ifdef XWAYLAND
		xytonode(130, 130, &surface, &hit, NULL, NULL, NULL);
		if (!hit || !client_is_unmanaged(hit)) goto wait;
		CHECK(hit->scene->node.parent == layers[LyrUnmanaged]);
		puts("PASS XWayland window and popup above fullscreen");
#endif
		break;
	case 7:
		/* Capture after a frame, while the compositor's event loop keeps running. */
		if (!(test_children[child_count] = fork())) {
			execlp("grim", "grim", "-o", first->wlr_output->name,
				getenv("DWL_TEST_SCREENSHOT"), NULL);
			_exit(127);
		}
		CHECK(test_children[child_count++] > 0);
		break;
	case 8:
		/* dwl's SIGCHLD handler reaps grim; wait for its exit and output. */
		if (kill(test_children[child_count - 1], 0) == 0) goto wait;
		CHECK(errno == ESRCH && access(getenv("DWL_TEST_SCREENSHOT"), R_OK) == 0);
		child_count--;
		/* Disconnect clients before shutdown to exercise unmap/destruction. */
		for (size_t i = 0; i < child_count; i++) kill(test_children[i], SIGTERM);
		break;
	case 9:
		if (!wl_list_empty(&clients)) goto wait;
		puts("PASS screenshot capture and client cleanup");
		wl_display_terminate(dpy);
		return 0;
	}
	step++;
	attempts = 0;
wait:
	wl_event_source_timer_update(test_timer, 100);
	return 0;
}

int
main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setup();
	test_timer = wl_event_loop_add_timer(event_loop, tick, NULL);
	wl_event_source_timer_update(test_timer, 100);
	run(NULL);
	wl_event_source_remove(test_timer);
	cleanup();
	for (size_t i = 0; i < child_count; i++) waitpid(test_children[i], NULL, 0);
	puts("PASS compositor shutdown");
	return 0;
}
