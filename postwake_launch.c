#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <ps5/kernel.h>

#include "attempt_receipt.h"
#include "kdlsym.h"
#include "postwake_config.h"
#include "postwake_launch.h"
#include "stage_log.h"
#include "util.h"

#define DATA_CAVE_OFF 0x2D39000ULL
#define KH_OFF_USER 0x440ULL
#define CAVE_STATE_OFF 0x600ULL
#define CAVE_SAVE_OFF 0x620ULL
#define HOOK_IST_OFF 0x4cULL
#define MAX_ELF_SIZE (16LL * 1024 * 1024)
#define PM_VERSION "0.5.2"

static int read_q(uint64_t address, uint64_t *value)
{
  *value = 0;
  return kernel_copyout(address, value, sizeof(*value));
}

static int receipt_matches(uint64_t expected_arm, uint64_t expected_base,
                           uint64_t expected_root)
{
  uint64_t base = KERNEL_ADDRESS_TEXT_BASE;
  uint64_t cave = base + DATA_CAVE_OFF;
  uint64_t stage, hook, arm, complete, root;
  uint64_t gate_lo, gate_hi, saved_ist, live_lo, live_hi, live_ist;
  if (kernel_get_fw_version() != 0x13600007U || base != expected_base ||
      read_q(cave + KH_OFF_USER, &stage) ||
      read_q(cave + CAVE_STATE_OFF, &hook) ||
      read_q(cave + CAVE_ARM_ID_OFF, &arm) ||
      read_q(cave + CAVE_COMPLETE_ID_OFF, &complete) ||
      read_q(kdlsym(KERNEL_SYM_PMAP_STORE) + 0x28, &root) ||
      read_q(cave + CAVE_SAVE_OFF, &gate_lo) ||
      read_q(cave + CAVE_SAVE_OFF + 8, &gate_hi) ||
      read_q(cave + CAVE_SAVE_OFF + 16, &saved_ist) ||
      read_q(kdlsym(KERNEL_SYM_IDT_ARRAY) + 16, &live_lo) ||
      read_q(kdlsym(KERNEL_SYM_IDT_ARRAY) + 24, &live_hi) ||
      read_q(kdlsym(KERNEL_SYM_COMMON_TSS) + HOOK_IST_OFF, &live_ist))
    return 0;
  return root == expected_root && arm == expected_arm &&
         attempt_receipt_valid(stage, hook, arm, complete, gate_lo, gate_hi,
                               live_lo, live_hi, saved_ist, live_ist);
}

static int match_arm_file(const char *title, uint64_t arm, uint64_t base,
                          uint64_t root)
{
  char path[128], line[256], expected[128], heading[64];
  struct stat st;
  snprintf(path, sizeof path, "/user/temp/common_temp/bpr-y2jb-%s.log", title);
  if (stat(path, &st) || st.st_size <= 0 || st.st_size > 4096)
    return 0;
  FILE *file = fopen(path, "r");
  if (!file) return 0;
  snprintf(heading, sizeof heading, "Title: %s\n", title);
  snprintf(expected, sizeof expected,
           "arm: %016" PRIx64 " base=%016" PRIx64 " root=%016" PRIx64 "\n",
           arm, base, root);
  int ok = fgets(line, sizeof line, file) &&
           strcmp(line, "BPR Y2JB V3 receipt test\n") == 0 &&
           fgets(line, sizeof line, file) && strcmp(line, heading) == 0 &&
           fgets(line, sizeof line, file) && strcmp(line, expected) == 0;
  fclose(file);
  return ok;
}

static const char *attempt_title(uint64_t arm, uint64_t base, uint64_t root)
{
  int one = match_arm_file("PPSA01651", arm, base, root);
  int two = match_arm_file("PPSA01652", arm, base, root);
  if (one == two) return NULL;
  return one ? "PPSA01651" : "PPSA01652";
}

static int connect_local(uint16_t port)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, (const struct sockaddr *)&addr, sizeof addr)) {
    close(fd);
    return -1;
  }
  return fd;
}

static int manager_version_ready(void)
{
  int fd = connect_local(8084);
  if (fd < 0) return 0;
  struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
  int no_sigpipe = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof no_sigpipe);
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  const char req[] = "GET /version HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
  char reply[512] = {0};
  int ok = send(fd, req, sizeof req - 1, 0) == (ssize_t)(sizeof req - 1);
  size_t used = 0;
  while (ok && used < sizeof reply - 1) {
    ssize_t n = recv(fd, reply + used, sizeof reply - 1 - used, 0);
    if (n <= 0) break;
    used += (size_t)n;
  }
  close(fd);
  if (used == 0) return 0;
  reply[used] = 0;
  const char *body = strstr(reply, "\r\n\r\n");
  return (strstr(reply, "HTTP/1.1 200 ") == reply ||
          strstr(reply, "HTTP/1.0 200 ") == reply) &&
         body && !strcmp(body + 4, PM_VERSION);
}

static int find_title_file(const char *title, const char *name, char *path,
                           size_t cap)
{
  for (unsigned slot = 0; slot < 3; ++slot) {
    int n = snprintf(path, cap,
        "/mnt/sandbox/%s_%03u/download0/cache/splash_screen/"
        "aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=/%s", title, slot, name);
    struct stat st;
    if (n <= 0 || (size_t)n >= cap) return -1;
    if (!stat(path, &st) && S_ISREG(st.st_mode) && st.st_size > 0 &&
        st.st_size <= MAX_ELF_SIZE) return 0;
  }
  return -1;
}

static int configured_elf(const char *title, char *name, size_t cap)
{
  char path[256], line[128];
  if (find_title_file(title, "postwake.txt", path, sizeof path)) return 0;
  FILE *file = fopen(path, "r");
  if (!file) return -1;
  int ok = fgets(line, sizeof line, file) != NULL;
  int extra = fgetc(file);
  fclose(file);
  if (!ok || extra != EOF) return -1;
  return postwake_parse_config(line, name, cap);
}

static int send_elf(const char *path, off_t expected_size)
{
  int input = open(path, O_RDONLY);
  if (input < 0) return -1;
  unsigned char buf[8192];
  ssize_t first = read(input, buf, sizeof buf);
  if (first < 4 || memcmp(buf, "\177ELF", 4)) {
    close(input);
    return -1;
  }
  int socket_fd = connect_local(9021);
  if (socket_fd < 0) {
    close(input);
    return -1;
  }
  int no_sigpipe = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                 sizeof no_sigpipe)) {
    close(socket_fd);
    close(input);
    return -1;
  }
  long long total = 0;
  ssize_t got = first;
  int result = 0;
  do {
    size_t offset = 0;
    while (offset < (size_t)got) {
      ssize_t sent = send(socket_fd, buf + offset, (size_t)got - offset, 0);
      if (sent <= 0) { result = -1; break; }
      offset += (size_t)sent;
      total += sent;
    }
    if (result) break;
    got = read(input, buf, sizeof buf);
  } while (got > 0);
  if (got < 0 || total != expected_size) result = -1;
  if (!result && shutdown(socket_fd, SHUT_WR)) result = -1;
  close(socket_fd);
  close(input);
  return result;
}

int postwake_launch_configured_elf(uint64_t expected_arm,
                                    uint64_t expected_base,
                                    uint64_t expected_root)
{
  if (!receipt_matches(expected_arm, expected_base, expected_root)) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=kernel_receipt", expected_arm);
    return -1;
  }
  const char *title = attempt_title(expected_arm, expected_base, expected_root);
  if (!title) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=arm_file", expected_arm);
    return -1;
  }
  char name[64] = {0};
  int configured = configured_elf(title, name, sizeof name);
  if (configured < 0) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=bad_config", expected_arm);
    return -1;
  }
  if (!configured) {
    stage_logf("POSTWAKE_MANUAL id=%016" PRIx64 " title=%s", expected_arm, title);
    return 0;
  }
  stage_logf("POSTWAKE_GATE id=%016" PRIx64 " title=%s elf=%s", expected_arm,
             title, name);
  sleep(15);
  if (!receipt_matches(expected_arm, expected_base, expected_root)) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=receipt_changed", expected_arm);
    return -1;
  }
  int is_manager = !strcmp(name, "pldmgr.elf");
  if (is_manager) {
    int preexisting = connect_local(8084);
    if (preexisting >= 0) {
      close(preexisting);
      stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=port_preexisting", expected_arm);
      return -1;
    }
  }
  char path[256];
  if (find_title_file(title, name, path, sizeof path)) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=elf_missing", expected_arm);
    return -1;
  }
  struct stat st;
  if (stat(path, &st) || (is_manager && st.st_size != 2410776LL)) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=elf_size", expected_arm);
    return -1;
  }
  if (send_elf(path, st.st_size)) {
    stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=elf_send", expected_arm);
    return -1;
  }
  if (!is_manager) {
    stage_logf("POSTWAKE_SENT_UNVERIFIED id=%016" PRIx64 " elf=%s", expected_arm, name);
    return 0;
  }
  for (int attempt = 0; attempt < 30; ++attempt) {
    if (manager_version_ready()) {
      stage_logf("POSTWAKE_PM_READY id=%016" PRIx64 " title=%s version=%s", expected_arm,
                 title, PM_VERSION);
      notifyf("Byepervisor Reborn\nPayload Manager %s ready after AP cleanup",
              PM_VERSION);
      return 0;
    }
    sleep(1);
  }
  stage_logf("POSTWAKE_STOP id=%016" PRIx64 " reason=version_timeout", expected_arm);
  return -1;
}
