#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>

#define BUF_SIZE 256

static struct termios orig_termios;

static void debug(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  fflush(stderr);
  va_end(va);
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

typedef struct {
  char buf[BUF_SIZE];
  size_t offset;
} ReadBuf;

static struct {
  bool selecting;
} vi_state = {
  false
};

typedef struct {
  char* name;
  bool shift;
  bool ctrl;
  bool meta;
} keystroke;

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

static bool interpret_keystroke(ReadBuf* buf, keystroke* result) {
  if (buf->offset == 6 &&
      buf->buf[0] == 0x1b &&
      (buf->buf[4] == 0x32 || buf->buf[4] == 0x36) &&
      IS_UDLREH(buf, 5)) {
    // [Ctrl +] Shift + <arrow|home|end>
    result->name = UDLREH_TO_NAME(buf->buf[5]);
    result->shift = (buf->buf[4] == 0x32);
    result->ctrl = (buf->buf[4] == 0x36);
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
  }

  return false;
}

static void process_stdin(ReadBuf* stdin_buf, int vi_fd) {
  bool passThrough = true;
  for (int idx = 0; idx < stdin_buf->offset; idx++)
    debug("%02x%s", stdin_buf->buf[idx], ((idx == stdin_buf->offset - 1) ? "" : " "));
  debug("\n");

  keystroke k;
  if (interpret_keystroke(stdin_buf, &k)) {
    if (!strcmp(k.name, "up") ||
        !strcmp(k.name, "down") ||
        !strcmp(k.name, "left") ||
        !strcmp(k.name, "right") ||
        !strcmp(k.name, "home") ||
        !strcmp(k.name, "end")) {
      const char* direction =
        !strcmp(k.name, "up") ? "k" :
        !strcmp(k.name, "down") ? "j" :
        !strcmp(k.name, "left") ? "h" :
        !strcmp(k.name, "right") ? "l" :
        !strcmp(k.name, "home") ? "0" :
        !strcmp(k.name, "end") ? "$" : NULL;
      if (k.shift == true) {
        passThrough = false;
        if (!vi_state.selecting) {
          vi_state.selecting = true;
          // Do we need the l before mbv for columns > 0?
          write(vi_fd, "\x1bmbv", 4);
        }
        if (k.ctrl) {
          if (!strcmp(k.name, "down")) write(vi_fd, "}", 1);
          else if (!strcmp(k.name, "up")) write(vi_fd, "}", 1);
          else if (!strcmp(k.name, "left")) write(vi_fd, "b", 1);
          else if (!strcmp(k.name, "right")) write(vi_fd, "w", 1);
          else if (!strcmp(k.name, "home")) write(vi_fd, "1G0", 3);
          else if (!strcmp(k.name, "end")) write(vi_fd, "G$", 2);
        } else {
          write(vi_fd, direction, 1);
        }
      } else if (vi_state.selecting) {
        debug("Stop selecting\n");
        // Stop selecting, stop absorbing keystrokes, and return to insert mode.
        vi_state.selecting = false;
        write(vi_fd, "\x1bi", 2);
        passThrough = true;
      }
    }
  }

  if (passThrough) {
    write(vi_fd, stdin_buf->buf, stdin_buf->offset);
    stdin_buf->offset = 0;
  }
}

static int drain_stdin(ReadBuf* stdin_buf, int vi_fd) {
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
    process_stdin(stdin_buf, vi_fd);
  }
}

int fork_vi(const char* fname) {
  struct winsize ws;
  int result;
  pid_t child_pid;
  int master_fd;

  result = ioctl(0, TIOCGWINSZ, &ws);
  if (result == -1) return result;

  child_pid = forkpty(&master_fd, NULL, NULL, &ws);
  if (child_pid == -1) return -1;

  if (child_pid > 0) {
    return master_fd;
  }

  char* const argv[] = {
    "-n", "+star",
    "-c", ":0",
    "-c", ":set tabstop=2",
    "-c", ":set expandtab",
    "-c", ":set whichwrap+=<,>,[,]",
    "-c", ":set selection=exclusive",
    "-c", ":set nohlsearch",
    fname, NULL
  };

  execvp("vim", argv);
}

static int drain_vi(int vi_fd) {
  char buf[BUF_SIZE] = "";
  int result;
  int vi_avail = -1;
  ssize_t vi_read = -1;

  result = ioctl(vi_fd, FIONREAD, &vi_avail);
  if (result == -1) return result;

  // EOF from vi, meaning it exited.
  if (vi_avail == 0) exit(0);

  while(vi_avail > 0) {
    vi_read = read(vi_fd, buf, BUF_SIZE);
    if (vi_read < 0) return vi_read;
    if (write(1, buf, vi_read) < 0) {
      return -1;
    }
    vi_avail -= vi_read;
  }
}

int main(int argc, char** argv) {
  freopen("/home/nix/editor/log", "a", stderr);

  debug("\nStarting up\n");

  ReadBuf stdin_buf = { "", 0 };

  if (argc < 2) {
    debug("Need a file name\n");
    return 1;
  }

  if (make_tty_raw() == -1) return 1;

  struct pollfd fds[2] = {
    { 0, POLLIN, 0 },
    { -1, POLLIN, 0 },
  };

  fds[1].fd = fork_vi(argv[1]);
  if (fds[1].fd == -1) {
    debug("Failed to spawn vi\n");
    return 2;
  }

  while (ppoll(fds, sizeof(fds) / sizeof(*fds), NULL, NULL) >= 0) {
    if (fds[0].revents != 0) {
      fds[0].revents = 0;
      drain_stdin(&stdin_buf, fds[1].fd);
    } else if (fds[1].revents != 0) {
      fds[1].revents = 0;
      drain_vi(fds[1].fd);
    }
  }

  return 0;
}
