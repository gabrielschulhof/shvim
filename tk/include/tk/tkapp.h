#ifndef TK_INCLUDE_TKAPP_H
#define TK_INCLUDE_TKAPP_H

typedef struct _TkApp TkApp;

G_BEGIN_DECLS

TkApp* tk_app_new(const char* name);

void tk_app_run(TkApp* app);

void tk_app_quit(TkApp* app);

void tk_app_destroy(TkApp* app);

G_END_DECLS

#endif  // TK_INCLUDE_TKAPP_H
