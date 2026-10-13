#ifndef BYEPERVISOR_POSTWAKE_CONFIG_H
#define BYEPERVISOR_POSTWAKE_CONFIG_H

#include <stddef.h>

/* 0 = manual, 1 = one named ELF, -1 = invalid. */
int postwake_parse_config(const char *line, char *name, size_t cap);

#endif
