#define _GNU_SOURCE
#include <stdarg.h>
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

static void process_stdin(ReadBuf* stdin_buf, int vi_fd) {
  write(vi_fd, stdin_buf->buf, stdin_buf->offset);
  stdin_buf->offset = 0;
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
    "-n", fname, NULL
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
