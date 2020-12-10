#include <stdlib.h>

#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>

#include <glib.h>

#define ASSERT(app, cond)                    \
  if (!(cond)) {                             \
    endwin();                                \
    fprintf(stderr, "(" #cond ") failed\n"); \
    fflush(stderr);                          \
    abort();                                 \
  }

typedef struct {
  WINDOW* screen;
  WINDOW* menu;
  WINDOW* tabs;
} App;

static void enter_menu_mode(App* app) {
  int ch;

  while (TRUE) {
    ch = wgetch(app->screen);
    if (ch == 27) break;
  }
}

int main(int argc, char** argv) {
  App the_app;
  App* app = &the_app;

  g_set_application_name("Editor");
	initscr();
	typeahead(-1);
	noecho();
  refresh();
  raw();

  int maxy, maxx, ch;

  app->screen = newwin(0, 0, 0, 0);
  ASSERT(app, app->screen != NULL);
  getmaxyx(app->screen, maxy, maxx);
  keypad(app->screen, TRUE);

  app->menu = subwin(app->screen, 1, maxx, 0, 0);
  ASSERT(app, app->menu != NULL);
  mvwaddch(app->menu, 0, 0, 'F' | A_UNDERLINE);
  wprintw(app->menu, "ile ");
  waddch(app->menu, 'E' | A_UNDERLINE);
  wprintw(app->menu, "dit");

  app->tabs = subwin(app->screen, 1, maxx, 1, 0);
  ASSERT(app, app->tabs != NULL);

  wrefresh(app->screen);

  while (TRUE) {
    ch = wgetch(app->screen);
    wprintw(app->screen, "%d", ch);

    // Ctrl+Q
    if (ch == 17) break;

    // ESC
    if (ch == 27) enter_menu_mode(app);
  }

  delwin(app->menu);
  delwin(app->tabs);
  delwin(app->screen);
  endwin();

  return 0;
}
