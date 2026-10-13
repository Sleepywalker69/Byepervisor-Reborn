#include <assert.h>
#include <string.h>

#include "postwake_config.h"

static void check(const char *input, int result, const char *expected)
{
  char name[64] = "old";
  assert(postwake_parse_config(input, name, sizeof name) == result);
  assert(strcmp(name, expected) == 0);
}

int main(void)
{
  check("manual\n", 0, "");
  check("manual\r\n", 0, "");
  check("elf=pldmgr.elf\n", 1, "pldmgr.elf");
  check("elf=my-payload_1.elf", 1, "my-payload_1.elf");
  check("", -1, "");
  check("auto", -1, "");
  check("elf=payload.bin", -1, "");
  check("elf=../payload.elf", -1, "");
  check("elf=a/b.elf", -1, "");
  check("elf=a.elf\nelf=b.elf", -1, "");
  check("elf=payload.elf ", -1, "");
  char short_name[5];
  assert(postwake_parse_config("elf=pldmgr.elf", short_name,
                               sizeof short_name) == -1);
  assert(postwake_parse_config(NULL, short_name, sizeof short_name) == -1);
  return 0;
}
