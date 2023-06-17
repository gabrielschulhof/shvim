#ifndef _SRC_VI_H_
#define _SRC_VI_H_

#define _GNU_SOURCE
#include <errno.h>
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
#include <uv.h>

#define BUF_SIZE 65536

typedef struct {
  int fd;
  pid_t pid;
  bool selecting;
  bool searching;
  bool jumping;
  char vimrc[24];
} ViState;

typedef struct {
  unsigned char buf[BUF_SIZE];
  size_t offset;
} ReadBuf;

#define VI_STATE_INIT         \
  {                           \
    -1,                       \
    -1,                       \
    false,                    \
    false,                    \
    false,                    \
    "/tmp/shvim.vimrc.XXXXXX" \
  }

int vi_fork(ViState* vi, char* fname, struct winsize* ws);

void vi_process_stdin(ViState* vi, ReadBuf* stdin_buf);

#endif  // _SRC_VI_H_
