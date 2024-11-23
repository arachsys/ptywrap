#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <linux/kcmp.h>
#endif

static int events[2];
static pid_t child;

static void copy(int src, int dst) {
  ssize_t count, length, offset;
  char buffer[PIPE_BUF];

  while ((length = read(src, buffer, sizeof buffer))) {
    if (length < 0 && errno != EINTR && errno != EAGAIN)
      return;
    offset = 0;

    while (length > 0) {
      if ((count = write(dst, buffer + offset, length)) > 0)
        length -= count, offset += count;
      if (count < 0 && errno != EINTR && errno != EAGAIN)
        return;
    }
  }
}

static void reap(int signal) {
  int status;
  pid_t pid;

  if (signal != SIGCHLD)
    return;
  while ((pid = waitpid(-1, &status, WNOHANG)) < 0)
    if (errno != EINTR)
      return;
  if (pid != child)
    return;

  if (WIFSIGNALED(status))
    status = 128 + WTERMSIG(status);
  else
    status = WEXITSTATUS(status);

  while (write(events[1], &(unsigned char) { status }, 1))
    if (errno != EINTR && errno != EAGAIN)
      break;
  child = -1;
}

static void wrap(int fd) {
  unsigned char status = 1;
  struct termios termios;
  int master, slave;

  if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
    err(1, "openpty");
  if (tcgetattr(slave, &termios) < 0)
    err(1, "tcgetattr");
  cfmakeraw(&termios);
  if (tcsetattr(slave, TCSANOW, &termios) < 0)
    err(1, "tcsetattr");

  child = fork();
  switch (child) {
    case -1:
      err(1, "fork");
    case 0:
      if (dup2(slave, fd) < 0)
        err(1, "dup2");
      close(master);
      close(slave);
      return;
  }

  close(slave);
  pipe(events);
  signal(SIGCHLD, reap);
  reap(SIGCHLD);
  copy(master, fd);

  while (read(events[0], &status, 1) < 0)
    if (errno != EINTR && errno != EAGAIN)
      break;
  exit(status);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s CMD...\n", argv[0]);
    return 64;
  }

#ifdef __linux__
  if (fcntl(1, F_GETFD) >= 0 && !isatty(1))
    if (!syscall(SYS_kcmp, getpid(), getpid(), KCMP_FILE, 1, 2))
      if (wrap(1), dup2(1, 2) < 0)
        err(1, "dup2");
#endif

  if (fcntl(1, F_GETFD) >= 0 && !isatty(1))
    wrap(1);
  if (fcntl(2, F_GETFD) >= 0 && !isatty(2))
    wrap(2);

  execvp(argv[1], argv + 1);
  err(1, "exec %s", argv[1]);
}
