#include <stdlib.h>
#include "debug.h"
#include "vi.h"

#define PARENT_STRUCT_PTR(child_ptr, struct_type, member) \
  ((struct_type*)(((char*)child_ptr) - offsetof(struct_type, member)))

#define UV_CALL(call) \
  do { \
    if ((call) != 0) { \
      debug(#call " failed\n"); \
      abort(); \
    } \
  } while (0)

typedef struct {
  uv_pipe_t pipe;
  ReadBuf buf;
} buffered_pipe_t;

typedef struct {
  ViState vi_state;
  uv_loop_t loop;
  uv_tty_t tty;
  uv_signal_t sigwinch;
  buffered_pipe_t std_in;
  buffered_pipe_t vi;
} app_state_t;

static int uv_tty_get_ws(uv_tty_t* tty, struct winsize* ws) {
  int width, height;
  int result = uv_tty_get_winsize(tty, &width, &height);
  if (result == 0) {
    ws->ws_col = width;
    ws->ws_row = height;
  }
  return result;
}

static void handle_sigwinch(uv_signal_t* handle, int signal) {
  app_state_t* app = PARENT_STRUCT_PTR(handle, app_state_t, sigwinch);
  struct winsize ws;

  debug("sigwinch\n");

  UV_CALL(uv_tty_get_ws(&app->tty, &ws));
  debug("new windows size: %d, %d\n", ws.ws_col, ws.ws_row);
  UV_CALL(ioctl(app->vi_state.fd, TIOCSWINSZ, &ws));
}

static void buffered_pipe_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buffered_pipe_t* uv_buf = (buffered_pipe_t*)handle;
  buf->base = &uv_buf->buf.buf[uv_buf->buf.offset];
  buf->len = BUF_SIZE - uv_buf->buf.offset;
}

static void stdin_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  app_state_t* app = PARENT_STRUCT_PTR(stream, app_state_t, std_in);
  debug("stdin_read_cb: %zd vs. UV_EOF: %zu\n", nread, UV_EOF);
  if (nread > 0) {
    debug("read from stdin: offset %zu + new bytes %zd available\n", app->std_in.buf.offset, nread);
    app->std_in.buf.offset += nread;
    vi_process_stdin(&app->vi_state, &app->std_in.buf);
  }
}

static void clean_up_uv(app_state_t* app) {
  debug("clean_up_uv\n");
  UV_CALL(uv_tty_reset_mode());
  uv_close((uv_handle_t*)&app->tty, NULL);
  uv_close((uv_handle_t*)&app->sigwinch, NULL);
  uv_close((uv_handle_t*)&app->std_in, NULL);
  uv_close((uv_handle_t*)&app->vi, NULL);
  debug("clean_up_uv complete\n");
}

static void vi_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  app_state_t* app = PARENT_STRUCT_PTR(stream, app_state_t, vi);
  debug("read from vi: %zd\n", nread);
  if (nread > 0) {
    write(1, app->vi.buf.buf, nread);
    app->vi.buf.offset = 0;
  } else if (nread < 0) {
    clean_up_uv(app);
  }
}

int main(int argc, char** argv) {
  struct winsize ws;
  app_state_t app = {VI_STATE_INIT};
  app.std_in.buf.offset = 0;
  app.vi.buf.offset = 0;

  if (argc < 2) {
    printf("Need a file name\n");
    return 1;
  }

  debug("\nStarting up\n");

  UV_CALL(uv_loop_init(&app.loop));
  UV_CALL(uv_tty_init(&app.loop, &app.tty, STDIN_FILENO, 0));
  UV_CALL(uv_signal_init(&app.loop, &app.sigwinch));
  UV_CALL(uv_signal_start(&app.sigwinch, handle_sigwinch, SIGWINCH));
  UV_CALL(uv_pipe_init(&app.loop, &app.std_in.pipe, 0));
  UV_CALL(uv_pipe_open(&app.std_in.pipe, 0));
  UV_CALL(uv_read_start((uv_stream_t*)&app.std_in.pipe, buffered_pipe_alloc_cb, stdin_read_cb));
  UV_CALL(uv_tty_set_mode(&app.tty, UV_TTY_MODE_RAW));
  UV_CALL(uv_tty_get_ws(&app.tty, &ws));
  UV_CALL(vi_fork(&app.vi_state, argv[1], &ws));
  UV_CALL(uv_pipe_init(&app.loop, &app.vi.pipe, 0));
  UV_CALL(uv_pipe_open(&app.vi.pipe, app.vi_state.fd));
  UV_CALL(uv_read_start((uv_stream_t*)&app.vi.pipe, buffered_pipe_alloc_cb, vi_read_cb));

  while(uv_run(&app.loop, UV_RUN_ONCE));

  return 0;
}
