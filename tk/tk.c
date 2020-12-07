#include <tk/tk.h>

struct _TkApp {
  GMainLoop* loop;
};

TkApp* tk_app_new() {
  TkApp* ret = g_new(TkApp, 1);
  ret->loop = g_main_loop_new(NULL, FALSE);
	initscr();
	typeahead(-1);
	noecho();
  refresh();
  return ret;
}

void tk_app_run(TkApp* app) {
  g_main_loop_run(app->loop);
}

void tk_app_quit(TkApp* app) {
  g_main_loop_quit(app->loop);
}

void tk_app_destroy(TkApp* app) {
  g_main_loop_unref(app->loop);
  g_free(app);
}
