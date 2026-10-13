#include <string.h>

#include "postwake_config.h"

int postwake_parse_config(const char *line, char *name, size_t cap)
{
  if (!line || !name || !cap) return -1;
  name[0] = 0;
  size_t length = strlen(line);
  while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
    --length;
  if (length == 6 && !memcmp(line, "manual", 6)) return 0;
  if (length < 9 || memcmp(line, "elf=", 4)) return -1;
  const char *elf = line + 4;
  size_t elf_length = length - 4;
  if (elf_length >= cap || memcmp(elf + elf_length - 4, ".elf", 4)) return -1;
  for (size_t i = 0; i < elf_length; ++i)
    if (!((elf[i] >= 'a' && elf[i] <= 'z') ||
          (elf[i] >= 'A' && elf[i] <= 'Z') ||
          (elf[i] >= '0' && elf[i] <= '9') || elf[i] == '_' ||
          elf[i] == '-' || elf[i] == '.')) return -1;
  for (size_t i = 1; i < elf_length; ++i)
    if (elf[i - 1] == '.' && elf[i] == '.') return -1;
  memcpy(name, elf, elf_length);
  name[elf_length] = 0;
  return 1;
}
