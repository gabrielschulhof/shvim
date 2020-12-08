#include <tk/tk.h>

#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>

static TkMenuDescription file_new_menu =
  { "&New", { TK_CTRL, 'n' }, { NULL } };

static TkMenuDescription file_open_menu =
  { "&Open", { TK_CTRL, 'o' }, { NULL } };

static TkMenuDescription file_save_menu =
  { "&Save", { TK_CTRL, 's' }, { NULL } };

static TkMenuDescription file_save_as_menu =
  { "Save &As", { TK_CTRL | TK_SHIFT, 's' }, { NULL } };

static TkMenuDescription file_close_menu =
  { "&Close", { TK_CTRL, 'w' }, { NULL } };

static TkMenuDescription file_exit_menu =
  { "E&xit", { TK_CTRL, 'q' }, { NULL } };

static TkMenuDescription file_menu = {
  "&File", { TK_ALT, 'f' }, {
    &file_new_menu,
    &file_open_menu,
    &TK_MENU_SEP,
    &file_save_menu,
    &file_save_as_menu,
    &TK_MENU_SEP,
    &file_close_menu,
    &file_exit_menu,
    NULL
  }
};

static TkMenuDescription edit_undo_menu =
  { "&Undo", { TK_CTRL, 'z' }, { NULL } };

static TkMenuDescription edit_redo_menu =
  { "&Redo", { TK_CTRL, 'y' }, { NULL } };

static TkMenuDescription edit_cut_menu =
  { "Cu&t", { TK_CTRL, 'x' }, { NULL } };

static TkMenuDescription edit_copy_menu =
  { "&Copy", { TK_CTRL, 'c' }, { NULL } };

static TkMenuDescription edit_paste_menu =
  { "&Paste", { TK_CTRL, 'v' }, { NULL } };

static TkMenuDescription edit_select_all_menu =
  { "Select &All", { TK_CTRL, 'a' }, { NULL } };

static TkMenuDescription edit_menu = {
  "&Edit", { TK_ALT, 'e' }, {
    &edit_undo_menu,
    &edit_redo_menu,
    &TK_MENU_SEP,
    &edit_cut_menu,
    &edit_copy_menu,
    &edit_paste_menu,
    &TK_MENU_SEP,
    &edit_select_all_menu,
    NULL
  }
};

static TkMenuDescription menu = {
  NULL, { TK_ALT, 'm' }, {
    &file_menu,
    &edit_menu,
    NULL
  }
};

int main(int argc, char** argv) {
  TkApp* app = tk_app_new("Editor");

  tk_app_run(app);

  tk_app_destroy(app);

  return 0;
}
