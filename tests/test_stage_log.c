#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/stage_log.h"

int main(void)
{
  char path[128];
  char contents[64] = {0};
  snprintf(path, sizeof path, "/tmp/byepervisor-stage-log-%ld.log",
           (long)getpid());
  unlink(path);

  assert(stage_log_append(NULL, "bad\n") == -1);
  assert(stage_log_append(path, "first\n") == 0);
  assert(stage_log_append(path, "second\n") == 0);

  FILE *file = fopen(path, "rb");
  assert(file != NULL);
  size_t len = fread(contents, 1, sizeof contents, file);
  assert(fclose(file) == 0);
  assert(len == strlen("first\nsecond\n"));
  assert(memcmp(contents, "first\nsecond\n", len) == 0);
  assert(unlink(path) == 0);
  puts("durable stage log append: 3 host cases passed");
  return 0;
}
