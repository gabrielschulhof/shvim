#include "debug.h"
#include "vi.h"

extern const char* vi_rc_string();

typedef struct {
  char* name;
  bool shift;
  bool ctrl;
  bool meta;
} keystroke;

static const char* ctrl_sequences[] = {
  "`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "return", "k", "l", "enter",
  "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "escape",
  NULL, NULL, "~",
};

#define write0(fd, str) \
  write((fd), (str), strlen((str)))

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

static bool keystroke_complete(ReadBuf* buf) {
  if (buf->offset >= 1) {
    if (buf->buf[0] == 0x1b) {
      if (buf->offset >= 2) {
        if (buf->buf[1] == 0x4f) {
          return (buf->offset >= 3);
        } else if (buf->buf[1] == 0x5b) {
          if (buf->offset < 4) {
            return false;
          } else {
            if (buf->buf[2] == 0x3b) {
              return (buf->offset >= 6);
            }
          }
        }
      }
    }
  }
  return true;
}

// Produce a keystroke from an input escape sequence. This is not an exhaustive
// interpreter.
static bool interpret_keystroke(ReadBuf* buf, keystroke* result) {
  if (buf->offset >= 6 &&
      buf->buf[0] == 0x1b &&
      buf->buf[1] == 0x5b) {
    if (buf->buf[2] == 0x31) {
      if ((buf->buf[4] == 0x32 || buf->buf[4] == 0x35 || buf->buf[4] == 0x36) &&
          (IS_UDLREH(buf, 5) || buf->buf[5] == 0x7e)) {
        // [Ctrl +] Shift + <arrow|home|end>
        result->name =
            ((buf->buf[5] == 0x7e) ? "delete" : UDLREH_TO_NAME(buf->buf[5]));
        result->shift = (buf->buf[4] != 0x35);
        result->ctrl = (buf->buf[4] == 0x36 || buf->buf[4] == 0x35);
        result->meta = false;
        return true;
      }
    } else if (buf->buf[2] == 0x35 ||
        buf->buf[2] == 0x36) {
      if (buf->buf[5] == 0x7e) {
        result->name = buf->buf[2] == 0x35 ? "pageup" : "pagedown";
        result->shift = (buf->buf[4] != 0x35);
        result->ctrl = (buf->buf[4] == 0x36 || buf->buf[4] == 0x35);
        result->meta = false;
        return true;
      }
    }
  } else if (buf->offset >= 4) {
    if (buf->buf[0] == 0x1b &&
        buf->buf[1] == 0x5b &&
        buf->buf[3] == 0x7e) {
      if (buf->buf[2] == 0x34) {
        result->name = "end";
        result->shift = false;
        result->ctrl = false;
        result->meta = false;
        return true;
      } else if (buf->buf[2] == 0x31) {
        result->name = "home";
        result->shift = false;
        result->ctrl = false;
        result->meta = false;
        return true;
      } else if (buf->buf[2] == 0x33) {
        result->name = "delete";
        result->shift = false;
        result->ctrl = false;
        result->meta = false;
        return true;
      }
    }
  } else if (buf->offset >= 3 &&
      buf->buf[0] == 0x1b &&
      buf->buf[1] == 0x4f &&
      IS_UDLREH(buf, 2)) {
    // <arrow|home|end>
    result->name = UDLREH_TO_NAME(buf->buf[2]);
    result->shift = false;
    result->ctrl = false;
    result->meta = false;
    return true;
  } else if (buf->offset >= 1) {
    if (buf->buf[0] < 0x1e &&
        ctrl_sequences[buf->buf[0]] != NULL) {
      result->name = ctrl_sequences[buf->buf[0]];
      result->shift = false;
      result->ctrl =
          (!(buf->buf[0] == 0x1b ||
             buf->buf[0] == 0x0a ||
             buf->buf[0] == 0x0d));
      result->meta = false;
      return true;
    } else if (buf->buf[0] == 0x7f) {
      result->name = "backspace";
      result->shift = false;
      result->ctrl = false;
      result->meta = false;
      return true;
    } else if (buf->buf[0] == 0x3c || buf->buf[0] == 0x3e) {
      result->name = (buf->buf[0] == 0x3c ? "<" : ">");
      result->shift = false;
      result->ctrl = false;
      result->meta = false;
      return true;
    }
  } else if (!keystroke_complete(buf)) {
    debug("incomplete\n");
    return true;
  }

  return false;
}

static bool vi_process_keystroke(ViState* vi, keystroke* k) {
  int row, col;
  bool passThrough = true;

  if ((!strcmp(k->name, "backspace") || !strcmp(k->name, "delete")) &&
      k->shift == false && k->ctrl == false && vi->selecting) {
    debug("backspace or delete, turning off selecting\n");
    vi->selecting = false;
    if (!strcmp(k->name, "backspace")) {
      write0(vi->fd, "\"_di");
      return false;
    }
    return true;
  }

  if (!strcmp(k->name, ">") || !strcmp(k->name, "<")) {
    if (vi->selecting == true) {
      return true;
    }
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
      if (!vi->searching) {
        vi->selecting = false;
        write0(vi->fd, "\x1bi");
      }
    }
  }

  if (!strcmp(k->name, "enter")) {
    if (vi->searching) {
      passThrough = false;
      vi->searching = false;
      vi->selecting = true;
      write0(vi->fd, "\rmbgn");
    } else if (vi->jumping) {
      passThrough = false;
      vi->jumping = false;
      write0(vi->fd, "\ri");
    } else if (vi->selecting) {
      vi->selecting = false;
      write0(vi->fd, "\"_di");
    }
  }

  if (k->ctrl == true && k->shift == false) {
    if (!strcmp(k->name, "a")) {
      vi->selecting = true;
    } else if (!strcmp(k->name, "f")) {
      vi->selecting = false;
      vi->searching = true;
    } else if (!strcmp(k->name, "g")) {
      vi->selecting = true;
    } else if (!strcmp(k->name, "i")) {
      if (vi->selecting) {
        vi->selecting = false;
        write0(vi->fd, "\"_di");
        passThrough = true;
      }
    } else if (!strcmp(k->name, "l")) {
      vi->jumping = true;
    } else if (!strcmp(k->name, "q")) {
      passThrough = false;
      write0(vi->fd, "\x1b:q\r");
    } else if (!strcmp(k->name, "s")) {
      passThrough = false;
      write0(vi->fd, "\x1b:w\r:call NMaybeMoveForward()\ri");
    } else if (!strcmp(k->name, "v")) {
      if (vi->searching) {
        passThrough = false;
        write0(vi->fd, "\x12");
        write0(vi->fd, "0");
      } else {
        vi->selecting = false;
      }
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

static int vi_create_vimrc(char* vimrc) {
  int fd = mkstemp(vimrc);
  if (fd < 0) return fd;

  write0(fd, vi_rc_string());

  if (close(fd) < 0) return -1;
}

int vi_drain_stdin(ViState* vi, ReadBuf* stdin_buf) {
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

  if (vi_create_vimrc(vi->vimrc) == -1) return -1;

  vi->pid = forkpty(&vi->fd, NULL, NULL, &ws);
  if (vi->pid == -1) return -1;

  if (vi->pid > 0) {
    return vi->fd;
  }

  char* const argv[] = { "vim", "-S", vi->vimrc, "-c" "startinsert", "-n", fname, NULL };

  execvp("vim", argv);
}

int vi_drain(ViState* vi) {
  char buf[BUF_SIZE] = "";
  int result;
  int vi_avail = -1;
  ssize_t vi_read = -1;

  result = ioctl(vi->fd, FIONREAD, &vi_avail);
  if (result == -1) return result;

  // EOF from vi, meaning it exited.
  if (vi_avail == 0) {
    debug("EOF from vim, exiting\n");
    unlink(vi->vimrc);
    exit(0);
  }

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
