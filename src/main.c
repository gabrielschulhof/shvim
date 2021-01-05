#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <signal.h>

#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>

#define BUF_SIZE 256

static struct termios orig_termios;

typedef struct {
  char buf[BUF_SIZE];
  size_t offset;
} ReadBuf;

typedef struct {
  int fd;
  pid_t pid;
  bool selecting;
  bool searching;
  bool full_line_selection;
} ViState;

#define VI_STATE_INIT \
  {                   \
    -1,               \
    -1,               \
    false,            \
    false,            \
    false,            \
  }

typedef struct {
  char* name;
  bool shift;
  bool ctrl;
  bool meta;
} keystroke;

static const char* ctrl_sequences[] = {
  "`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "return", "k", "l", "enter", "n",
  "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
  "escape", NULL, NULL, "~",
};

static int vi_drain(ViState* vi);

#define DEBUG(s)
static void debug(const char* fmt, ...) {
  DEBUG(va_list va);
  DEBUG(va_start(va, fmt));
  DEBUG(vfprintf(stderr, fmt, va));
  DEBUG(fflush(stderr));
  DEBUG(va_end(va));
}

static void reset_tty() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static int make_tty_raw() {
  int result;

  result = tcgetattr(STDIN_FILENO, &orig_termios);
  if (result == -1) return result;

  result = atexit(reset_tty);
  if (result == -1) return result;

  struct termios raw = orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

#define IS_UDLREH(buf, idx)                                    \
  (((buf)->buf[(idx)] >= 0x41 && (buf)->buf[(idx)] <= 0x44) || \
    (buf)->buf[(idx)] == 0x46 ||                               \
    (buf)->buf[(idx)] == 0x48)

#define UDLREH_TO_NAME(ch)           \
      ((ch) == 0x41 ? "up" :         \
      (ch) == 0x42 ? "down" :        \
      (ch) == 0x43 ? "right" :       \
      (ch) == 0x44 ? "left" :        \
      (ch) == 0x46 ? "end" : "home")

// Produce a keystroke from an input escape sequence. This is not an exhaustive
// interpreter.
static bool interpret_keystroke(ReadBuf* buf, keystroke* result) {
  if (buf->offset == 6 &&
      buf->buf[0] == 0x1b &&
      (buf->buf[4] == 0x32 || buf->buf[4] == 0x35 || buf->buf[4] == 0x36) &&
      IS_UDLREH(buf, 5)) {
    // [Ctrl +] Shift + <arrow|home|end>
    result->name = UDLREH_TO_NAME(buf->buf[5]);
    result->shift = (buf->buf[4] != 0x35);
    result->ctrl = (buf->buf[4] == 0x36 || buf->buf[4] == 0x35);
    result->meta = false;
    return true;
  } else if (buf->offset == 3 &&
      buf->buf[0] == 0x1b &&
      buf->buf[1] == 0x4f &&
      IS_UDLREH(buf, 2)) {
    // <arrow|home|end>
    result->name = UDLREH_TO_NAME(buf->buf[2]);
    result->shift = false;
    result->ctrl = false;
    result->meta = false;
    return true;
  } else if (buf->offset == 1 &&
      buf->buf[0] < 0x1e &&
      ctrl_sequences[buf->buf[0]] != NULL) {
    result->name = ctrl_sequences[buf->buf[0]];
    result->shift = false;
    result->ctrl =
        (!(buf->buf[0] == 0x1b || buf->buf[0] == 0x0a || buf->buf[0] == 0x0d));
    result->meta = false;
    return true;
  }

  return false;
}

static int get_cursor_pos(int* row, int* col) {
  char response[256] = "";
  char* end;
  int idx;

  write(1, "\x1b[6n", 4);

  // We expect <ESC>[??;???R back, so let's keep reading until we get a letter R.
  for (idx = 0; idx < 256; idx++) {
    read(0, &response[idx], 1);
    if (response[idx] == 'R') break;
  }

  // This assumes the tty does indeed produce the correct sequence and that the
  // terminal does not have so many rows/columns that the resulting decimal
  // representation of the numbers doesn't fit in the buffer ;)
  *row = strtol(&response[2], &end, 10);
  end++;
  *col = strtol(end, NULL, 10);
  return 0;
}

static void vi_process_stdin(ViState* vi, ReadBuf* stdin_buf) {
  bool passThrough = true;
  keystroke k;
  int row, col;

  for (int idx = 0; idx < stdin_buf->offset; idx++)
    debug("%02x%s",
        stdin_buf->buf[idx],
        ((idx == stdin_buf->offset - 1) ? "" : " "));
  debug("\n");

  if (interpret_keystroke(stdin_buf, &k)) {
    debug("keystroke: %s%s%s%s\n",
      k.ctrl ? "Ctrl+" : "",
      k.meta ? "Alt+" : "",
      k.shift ? "Shift+" : "",
      k.name);
    if (!strcmp(k.name, "up") ||
        !strcmp(k.name, "down") ||
        !strcmp(k.name, "left") ||
        !strcmp(k.name, "right") ||
        !strcmp(k.name, "home") ||
        !strcmp(k.name, "end")) {
      const char* direction =
        !strcmp(k.name, "up") ? "\x1bOA" :
        !strcmp(k.name, "down") ? "\x1bOB" :
        !strcmp(k.name, "right") ? "\x1bOC" :
        !strcmp(k.name, "left") ? "\x1bOD" :
        !strcmp(k.name, "home") ? "0" :
        !strcmp(k.name, "end") ? "$" : NULL;
      bool forwards =
          (!strcmp(k.name, "down") ||
          !strcmp(k.name, "right") ||
          !strcmp(k.name, "end"));
      if (k.shift == true) {
        passThrough = false;
        if (!vi->selecting) {
          vi->selecting = true;
          get_cursor_pos(&row, &col);
          write(vi->fd, "\x1b", 1);
          if (col > 1 && forwards) write(vi->fd, "\x1bOC", 3);
          write (vi->fd, "mbv", 3);
        }
        if (k.ctrl) {
          debug("Ctrl+Shift+%s\n", k.name);
          if (!strcmp(k.name, "down")) write(vi->fd, "}", 1);
          else if (!strcmp(k.name, "up")) write(vi->fd, "}", 1);
          else if (!strcmp(k.name, "left")) write(vi->fd, "b", 1);
          else if (!strcmp(k.name, "right")) write(vi->fd, "w", 1);
          else if (!strcmp(k.name, "home")) write(vi->fd, "1G0", 3);
          else if (!strcmp(k.name, "end")) write(vi->fd, "G$", 2);
        } else {
          write(vi->fd, direction, strlen(direction));
        }
      } else if (vi->selecting) {
        debug("Stop selecting\n");
        // Stop selecting, stop absorbing keystrokes, and return to insert mode.
        vi->selecting = false;
        write(vi->fd, "\x1bi", 2);
        passThrough = true;
      }
    }

    if (!strcmp(k.name, "enter") && vi->searching) {
      passThrough = false;
      vi->searching = false;
      vi->selecting = true;
      write(vi->fd, "\rmbgn", 5);
    }

    if (k.ctrl == true && k.shift == false) {
      if (!strcmp(k.name, "a")) {
        passThrough = false;
        vi->selecting = true;
        // Gotta write these separately, because "\x1b1" is not what we mean.
        write(vi->fd, "\x1b", 1);
        write(vi->fd, "1G0mbvG$", 8);
      } else if (!strcmp(k.name, "c")) {
        passThrough = false;
        if (vi->selecting) {
          write(vi->fd, "m`ybv``", 7);
          get_cursor_pos(&row, &col);
          vi->full_line_selection = (col == 1);
        }
      } else if (!strcmp(k.name, "f")) {
        passThrough = false;
        vi->searching = true;
        write(vi->fd, "\x1bl/", 3);
      } else if (!strcmp(k.name, "g")) {
        passThrough = false;
        vi->selecting = true;
        write(vi->fd, "\x1bnmbgn", 6);
      } else if (!strcmp(k.name, "q")) {
        passThrough = false;
        write(vi->fd, "\x1b:q\r", 4);
      } else if (!strcmp(k.name, "s")) {
        passThrough = false;
        write(vi->fd, "\x1b:w\ri", 5);
        // We need to move to the right by one after saving to maintain cursor
        // position, but we cannot do so before entering insert mode because
        // outside of insert mode it refuses to move past the last character.
        get_cursor_pos(&row, &col);
        if (col > 1) write(vi->fd, "\x1bOC", 3);
      } else if (!strcmp(k.name, "v")) {
        passThrough = false;
        if (vi->selecting) {
          vi->selecting = false;
          write(vi->fd, "\"_di", 4);
        }
        // For some reason we have to move to the right by one before pasting
        // ^o^
        write(vi->fd, "\x1bOC\x1bP`]", 7);
        if (!vi->full_line_selection) {
          // We also have to move past what we just pasted, unless it's a full
          // line ^o^
          write(vi->fd, "\x1bOC", 3);
        }
        write(vi->fd, "i", 1);
      } else if (!strcmp(k.name, "x")) {
        passThrough = false;
        if (vi->selecting) {
          write(vi->fd, "di", 2);
          vi->selecting = false;
        }
      } else if (!strcmp(k.name, "z")) {
        passThrough = false;
        write(vi->fd, "\x1bui", 3);
      } else if (!strcmp(k.name, "down")) {
        passThrough = false;
        write(vi->fd, "\x1b}i", 3);
      } else if (!strcmp(k.name, "up")) {
        passThrough = false;
        write(vi->fd, "\x1b{i", 3);
      }
    }
  }

  if (passThrough) {
    write(vi->fd, stdin_buf->buf, stdin_buf->offset);
  }
  stdin_buf->offset = 0;
}

static int vi_drain_stdin(ViState* vi, ReadBuf* stdin_buf) {
  int result;
  int stdin_avail = -1;
  ssize_t stdin_read = -1;

  result = ioctl(0, FIONREAD, &stdin_avail);
  if (result == -1) return result;

  while (stdin_avail > 0) {
    stdin_read = read(0, &stdin_buf->buf[stdin_buf->offset],
      BUF_SIZE - stdin_buf->offset);
    if (stdin_read < 0) return stdin_read;
    stdin_avail -= stdin_read;
    stdin_buf->offset += stdin_read;
    vi_process_stdin(vi, stdin_buf);
  }
}

int vi_fork(ViState* vi, const char* fname) {
  struct winsize ws;
  int result;

  result = ioctl(0, TIOCGWINSZ, &ws);
  if (result == -1) return result;

  vi->pid = forkpty(&vi->fd, NULL, NULL, &ws);
  if (vi->pid == -1) return -1;

  if (vi->pid > 0) {
    return vi->fd;
  }

  char* const argv[] = {
    "-n", "+star",
    "-c", ":0",
    "-c", ":set tabstop=2",
    "-c", ":set expandtab",
    "-c", ":set whichwrap+=<,>,[,]",
    "-c", ":set nohlsearch",
    fname, NULL
  };

  execvp("vim", argv);
}

static int vi_drain(ViState* vi) {
  char buf[BUF_SIZE] = "";
  int result;
  int vi_avail = -1;
  ssize_t vi_read = -1;

  result = ioctl(vi->fd, FIONREAD, &vi_avail);
  if (result == -1) return result;

  // EOF from vi, meaning it exited.
  if (vi_avail == 0) exit(0);

  while(vi_avail > 0) {
    vi_read = read(vi->fd, buf, BUF_SIZE);
    if (vi_read < 0) return vi_read;
    if (write(1, buf, vi_read) < 0) {
      return -1;
    }
    vi_avail -= vi_read;
  }

  return 0;
}

int main(int argc, char** argv) {
  ViState vi_state = VI_STATE_INIT;

  struct pollfd fds[2] = {
    { 0, POLLIN, 0 },
    { -1, POLLIN, 0 },
  };

  ReadBuf stdin_buf = { "", 0 };

  DEBUG(freopen("/home/nix/editor/log", "a", stderr));

  debug("\nStarting up\n");

  if (argc < 2) {
    printf("Need a file name\n");
    return 1;
  }

  if (make_tty_raw() == -1) return 1;

  fds[1].fd = vi_fork(&vi_state, argv[1]);
  if (fds[1].fd == -1) {
    printf("Failed to spawn vi\r\n");
    return 2;
  }

  while (ppoll(fds, sizeof(fds) / sizeof(*fds), NULL, NULL) >= 0) {
    if (fds[0].revents != 0) {
      fds[0].revents = 0;
      vi_drain_stdin(&vi_state, &stdin_buf);
    } else if (fds[1].revents != 0) {
      fds[1].revents = 0;
      vi_drain(&vi_state);
    }
  }

  return 0;
}
