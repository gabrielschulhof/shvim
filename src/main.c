#include "debug.h"
#include "vi.h"

static struct termios orig_termios;

ViState* current_vi = NULL;

static void handle_sigwinch(int signal) {
  int result;
  struct winsize ws;

  debug("sigwinch\n");

  result = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
  if (result == -1) exit(1);

  result = ioctl(current_vi->fd, TIOCSWINSZ, &ws);
  if (result == -1) exit(1);
}

static void vi_atexit() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
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

int main(int argc, char** argv) {
  int result;
  ViState vi_state = VI_STATE_INIT;

  struct pollfd fds[2] = {
    { 0, POLLIN, 0 },
    { -1, POLLIN, 0 },
  };

  ReadBuf stdin_buf = { "", 0 };

  DEBUG(freopen("/home/nix/shvim/log", "a", stderr));

  debug("\nStarting up\n");

  if (argc < 2) {
    printf("Need a file name\n");
    return 1;
  }

  if (make_tty_raw() == -1) return 1;

  struct sigaction sa;
  if (sigemptyset(&sa.sa_mask) == -1) return 1;
  sa.sa_flags = 0;
  sa.sa_handler = handle_sigwinch;
  if (sigaction(SIGWINCH, &sa, NULL) == -1) return 1;

  current_vi = &vi_state;

  fds[1].fd = vi_fork(&vi_state, argv[1]);
  if (fds[1].fd == -1) {
    printf("Failed to spawn vi\r\n");
    return 2;
  }

  while (true) {
    result = ppoll(fds, sizeof(fds) / sizeof(*fds), NULL, NULL);
    if (result >= 0) {
      if (fds[0].revents != 0) {
        fds[0].revents = 0;
        vi_drain_stdin(&vi_state, &stdin_buf);
      } else if (fds[1].revents != 0) {
        fds[1].revents = 0;
        vi_drain(&vi_state);
      }
    } else if (errno != EINTR) {
      break;
    }
  }

  return 0;
}
