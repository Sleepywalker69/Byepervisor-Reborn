#ifndef BYEPERVISOR_STAGE_LOG_H
#define BYEPERVISOR_STAGE_LOG_H

#define STAGE_LOG_PATH "/data/ps5-s3-1360/byepervisor-reborn-1360.log"

int stage_log_append(const char *path, const char *message);
int stage_logf(const char *fmt, ...);

#endif
