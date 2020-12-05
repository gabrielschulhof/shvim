#ifndef TK_INCLUDE_TK_H
#define TK_INCLUDE_TK_H

#include <stddef.h>
#include <glib.h>
#include <glib-object.h>

typedef enum {
  TK_NONE = 0,
  TK_CTRL = 1,
  TK_ALT = 2,
  TK_SHIFT = 4
} TkModifier;

typedef struct {
  TkModifier modifier;
  const char key;
} TkHotKey;

#define TK_NO_HOT_KEY { TK_NONE, 0 }

typedef struct _TkMenuDescription {
  const char* name;
  TkHotKey key;
  const struct _TkMenuDescription* children[];
} TkMenuDescription;

static TkMenuDescription TK_MENU_SEP =
  { NULL, TK_NO_HOT_KEY, { NULL } };

#endif  // TK_INCLUDE_TK_H
