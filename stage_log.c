#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "stage_log.h"

int stage_log_append(const char *path, const char *message)
{
  if (!path || !message)
    return -1;

  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0)
    return -1;

  size_t len = strlen(message);
  size_t done = 0;
  while (done < len) {
    ssize_t n = write(fd, message + done, len - done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      close(fd);
      return -1;
    }
    done += (size_t)n;
  }

  int result = fsync(fd);
  if (close(fd) != 0)
    result = -1;
  return result;
}

int stage_logf(const char *fmt, ...)
{
  char message[512];
  char line[640];
  static unsigned sequence;
  va_list ap;

  va_start(ap, fmt);
  int n = vsnprintf(message, sizeof message, fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)sizeof message)
    return -1;

  for (int i = 0; i < n; i++) {
    if (message[i] == '\n' || message[i] == '\r')
      message[i] = ' ';
  }

  long long epoch = (long long)time(NULL);
  int count = snprintf(line, sizeof line, "epoch=%lld pid=%d seq=%u %s\n",
                       epoch, (int)getpid(), ++sequence, message);
  if (count < 0 || count >= (int)sizeof line)
    return -1;
  return stage_log_append(STAGE_LOG_PATH, line);
}
