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

static char vimrc[] = "/tmp/shvim.vimrc.XXXXXX";

extern const char* vi_rc_string();

typedef struct {
  char buf[BUF_SIZE];
  size_t offset;
} ReadBuf;

typedef struct {
  int fd;
  pid_t pid;
  bool selecting;
  bool searching;
} ViState;

#define VI_STATE_INIT \
  {                   \
    -1,               \
    -1,               \
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

#define write0(fd, str) \
  write((fd), (str), strlen((str)))

#define DEBUG(s) s
static void debug(const char* fmt, ...) {
  DEBUG(va_list va);
  DEBUG(va_start(va, fmt));
  DEBUG(vfprintf(stderr, fmt, va));
  DEBUG(fflush(stderr));
  DEBUG(va_end(va));
}

static void vi_atexit() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  unlink(vimrc);
}

static int make_tty_raw() {
  int result;

  result = tcgetattr(STDIN_FILENO, &orig_termios);
  if (result == -1) return result;

  result = atexit(vi_atexit);
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
      (IS_UDLREH(buf, 5) || buf->buf[5] == 0x7e)) {
    // [Ctrl +] Shift + <arrow|home|end>
    result->name =
        ((buf->buf[5] == 0x7e) ? "delete" : UDLREH_TO_NAME(buf->buf[5]));
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
  } else if (buf->offset == 4) {
    result->name = "delete";
    result->shift = false;
    result->ctrl = false;
    result->meta = false;
    return true;
  }

  return false;
}

static bool vi_process_keystroke(ViState* vi, keystroke* k) {
  int row, col;
  bool passThrough = true;

  if ((!strcmp(k->name, "delete") || !strcmp(k->name, "backspace")) &&
      k->shift == false && k->ctrl == false && vi->selecting) {
    vi->selecting = false;
  }

  if (!strcmp(k->name, "up") ||
      !strcmp(k->name, "down") ||
      !strcmp(k->name, "left") ||
      !strcmp(k->name, "right") ||
      !strcmp(k->name, "end")) {
    if (k->shift == true) {
      if (!vi->selecting) {
        vi->selecting = true;
      }
    } else if (vi->selecting) {
      debug("Stop selecting\n");
      // Stop selecting, stop absorbing keystrokes, and return to insert mode.
      vi->selecting = false;
      write0(vi->fd, "\x1bi");
      passThrough = true;
    }
  }

  if (!strcmp(k->name, "home")) {
    if (k->shift == true) {
      keystroke shift_left = { "left", true, false, false };
      vi_process_keystroke(vi, &shift_left);
    } else {
      vi->selecting = false;
      write0(vi->fd, "\x1bi");
    }
  }

  if (!strcmp(k->name, "enter") && vi->searching) {
    passThrough = false;
    vi->searching = false;
    vi->selecting = true;
    write0(vi->fd, "\rmbgn");
  }

  if (k->ctrl == true && k->shift == false) {
    if (!strcmp(k->name, "a")) {
      vi->selecting = true;
    } else if (!strcmp(k->name, "f")) {
      vi->searching = true;
    } else if (!strcmp(k->name, "v")) {
      if (vi->searching) {
        passThrough = false;
        write0(vi->fd, "\x12");
        write0(vi->fd, "0");
      }
    } else if (!strcmp(k->name, "g")) {
      vi->selecting = true;
    } else if (!strcmp(k->name, "q")) {
      passThrough = false;
      write0(vi->fd, "\x1b:q\r");
    } else if (!strcmp(k->name, "s")) {
      passThrough = false;
      write0(vi->fd, "\x1b:w\rli");
    } else if (!strcmp(k->name, "x")) {
      if (vi->selecting) {
        vi->selecting = false;
      }
    }
  }

  return passThrough;
}

static void vi_process_stdin(ViState* vi, ReadBuf* stdin_buf) {
  bool passThrough = true;
  keystroke k;

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

    debug("vi->selecting: %s\n", vi->selecting ? "true" : "false");

    passThrough = vi_process_keystroke(vi, &k);
  } else {
    debug("keystroke not interpreted\n");
    if (vi->selecting) {
      vi->selecting = false;
      write0(vi->fd, "\"_di");
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

static int vi_create_vimrc(char* vimrc) {
  int fd = mkstemp(vimrc);
  if (fd < 0) return fd;

  write0(fd, vi_rc_string());

  if (close(fd) < 0) return -1;
}

int vi_fork(ViState* vi, const char* fname, char* vimrc) {
  struct winsize ws;
  int result; 

  result = ioctl(0, TIOCGWINSZ, &ws);
  if (result == -1) return result;

  if (vi_create_vimrc(vimrc) == -1) return -1;

  vi->pid = forkpty(&vi->fd, NULL, NULL, &ws);
  if (vi->pid == -1) return -1;

  if (vi->pid > 0) {
    return vi->fd;
  }

  char* const argv[] = { "vim", "-S", vimrc, "-c" "startinsert", "-n", fname, NULL };

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

  fds[1].fd = vi_fork(&vi_state, argv[1], vimrc);
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
