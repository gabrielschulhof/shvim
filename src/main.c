#define _GNU_SOURCE
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>

#define BUF_SIZE 256

static struct termios orig_termios;

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

static void process_stdin(ReadBuf* stdin_buf) {
  if (stdin_buf->offset == 1 && stdin_buf->buf[0] == 3) {
    exit(1);
  }
  for (size_t idx = 0; idx < stdin_buf->offset; idx++) {
    fprintf(stderr, "%02x%s", stdin_buf->buf[idx],
      ((idx == stdin_buf->offset - 1) ? " " : ""));
  }
  fprintf(stderr, "\r\n");
  stdin_buf->offset = 0;
}

static int drain_stdin(ReadBuf* stdin_buf) {
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
    process_stdin(stdin_buf);
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

  execvp("vim", "");
}

int main(int argc, char** argv) {
  ReadBuf stdin_buf = { "", 0 };

  if (argc < 2) {
    fprintf(stderr, "Need a file name\n");
    return 1;
  }

  if (make_tty_raw() == -1) return 1;

  struct pollfd fds[2] = {
    { 0, POLLIN, 0 },
    { -1, POLLIN, 0 },
  };

  fds[1].fd = fork_vi(argv[1]);
  if (fds[1].fd == -1) {
    fprintf(stderr, "Failed to spawn vi\n");
    return 2;
  }

  while (ppoll(fds, sizeof(fds) / sizeof(*fds), NULL, NULL) >= 0) {
    if (fds[0].revents != 0) {
      fds[0].revents = 0;
      drain_stdin(&stdin_buf);
    }
  }

  return 0;
}
