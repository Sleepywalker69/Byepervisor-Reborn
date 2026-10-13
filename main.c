#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <ps5/kernel.h>
#include <sys/syscall.h>

#include "kdlsym.h"
#include "khook.h"
#include "util.h"
#include "paging.h"
#include "stage_status.h"
#include "attempt_receipt.h"
#include "stage_log.h"

#include "break_hv_payload/break_hv.h"

#define STATE_ARMED               0x4B484F4F4B454401ULL // 'KHOOKED.'
#define COOKIE_VAL                0xCAFE133741414141ULL
#define HOOK_CORE                 0
#define HOOK_IST                  6
#define CAVE_STATE_OFF            0x600
#define CAVE_SAVE_OFF             0x620
#define TSS_STRIDE                0x68
#define STARTUP_MP_PAYLOAD_PAGE   0x3FFFF000

#define HV_LOADER_PA_REGULAR      0x062800000
#define HV_LOADER_PA_PRO          0x8e0800000

uint64_t g_hv_base_pa;
int sceSystemStateMgrEnterStandby(void);

static int read_u64(uint64_t addr, uint64_t *out);
static int write_u64(uint64_t addr, uint64_t value);

static int arm_saved_hook(khook_t *hook, uint64_t cave)
{
  /* Persist the original gate and IST before arming the breakpoint. */
  if (write_u64(cave + CAVE_SAVE_OFF + 0x00, hook->saved_gate_lo) != 0 ||
      write_u64(cave + CAVE_SAVE_OFF + 0x08, hook->saved_gate_hi) != 0 ||
      write_u64(cave + CAVE_SAVE_OFF + 0x10, hook->saved_ist) != 0) {
    return kh_restore(hook) == 0 ? -20 : -22;
  }

  if (kh_trigger(hook) != 0)
    return kh_restore(hook) == 0 ? -23 : -22;
  if (write_u64(cave + CAVE_STATE_OFF, STATE_ARMED) != 0) {
    if (kh_disarm(hook) != 0 || kh_restore(hook) != 0)
      return -22;
    return -21;
  }
  return 0;
}

static int install_test_hook(int core_id, uint64_t cave)
{
  khook_t    hook;
  kh_chain_t chain;
  uint8_t    chain_buf[0x400];
  int        ret;

  printf("[+] install_test_hook addr = %p\n", &install_test_hook);

  ret = kh_init(&hook, kread, kwrite, ktext(0), cave, HOOK_IST);
  if (ret) return ret;

  hook.tss_stride      = TSS_STRIDE;
  kh_set_tables(&hook, kdlsym(KERNEL_SYM_IDT_ARRAY), kdlsym(KERNEL_SYM_COMMON_TSS));
  kh_set_core(&hook, core_id);
  hook.oneshot   = 1;

  // kh_set_hook_target(&hook, kdlsym(KERNEL_SYM_SYS_GETUID) + 0xB);    // 8b4008             mov     eax, dword [rax+0x8]
  // kh_set_resume_rax(&hook, ktext(0x12F61C6));
  // kh_set_resume_skip(&hook, 3);
  // kh_set_resume_rip(&hook, kdlsym(KERNEL_SYM_SYS_GETUID) + 0xB + 3); // skip ^^^

  kh_set_hook_target(&hook, kdlsym(KERNEL_SYM_SYS_GETUID) + 0xE);       // 48898708040000     mov     qword [rdi+0x408], rax
  kh_set_resume_rax(&hook, 0x12345678);
  //kh_set_resume_skip(&hook, 7);
  //kh_set_resume_rip(&hook, kdlsym(KERNEL_SYM_SYS_GETUID) + 0xE + 7); // skip ^^^

  uint64_t oracle = kh_user_data_addr(&hook);
  kh_chain_init(&chain, chain_buf, sizeof chain_buf);
  kh_chain_store(&chain, oracle, COOKIE_VAL);

  ret = kh_install(&hook, &chain);
  if (ret) return ret;

  return arm_saved_hook(&hook, cave);
}

static int install_resume_hook(int core_id, uint64_t cave, uint64_t cr3)
{
  khook_t hook;
  kh_chain_t chain;
  uint8_t chain_buf[0x800];
  uint64_t set_rsi;
  uint64_t oracle;
  int ret;

  ret = kh_init(&hook, kread, kwrite, ktext(0), cave, HOOK_IST);
  if (ret) return ret;

  hook.tss_stride      = TSS_STRIDE;
  kh_set_tables(&hook, kdlsym(KERNEL_SYM_IDT_ARRAY), kdlsym(KERNEL_SYM_COMMON_TSS));
  kh_set_core(&hook, core_id);
  hook.oneshot   = 1;

  set_rsi = kdldmap(STARTUP_MP_PAYLOAD_PAGE); // our payload
  // set_rsi = ktext(0x269FB8); // original vector (13.60)

  printf("[+] install_resume_hook: setting hook target = 0x%lx (base: 0x%lx), set rsi=0x%lx\n", kdlsym(KERNEL_SYM_HV_MP_STARTUP_CALL), ktext(0), set_rsi);
  kh_set_hook_target(&hook, kdlsym(KERNEL_SYM_HV_MP_STARTUP_CALL));
  kh_set_resume_rsi(&hook, set_rsi);
  kh_set_resume_rdx(&hook, cr3);

  oracle = kh_user_data_addr(&hook);
  kh_chain_init(&chain, chain_buf, sizeof chain_buf);
  kh_chain_store(&chain, oracle, STAGE_HOOK_FIRED);

  kh_chain_g(&chain, KERNEL_GADGET_POP_RDX);
  kh_chain_q(&chain, cr3);

  kh_chain_g(&chain, KERNEL_GADGET_POP_RCX);
  kh_chain_q(&chain, oracle + 0x08);
  kh_chain_g(&chain, KERNEL_GADGET_ST_RCX_RDX);

  kh_chain_memcpy(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE), kdlsym(KERNEL_SYM_DATA_CAVE) + 0x1000, 0xF00);

  kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF00), kdlsym(KERNEL_SYM_MP_STARTUP_INIT_VECTOR));
  kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF08), kdldmap(0));
  kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF10), oracle);

  // carveout addr
  if (g_hv_base_pa == HV_LOADER_PA_PRO)
    kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF18), g_hv_base_pa - 0x800000);
  else
    kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF18), g_hv_base_pa);

  kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF20), g_hv_base_pa + rawoff(HV_SYM_HEAP_BASE));
  kh_chain_store(&chain, kdldmap(STARTUP_MP_PAYLOAD_PAGE + 0xF28), g_hv_base_pa + rawoff(HV_SYM_PCPUS));

  ret = kh_install(&hook, &chain);
  if (ret) return ret;

  return arm_saved_hook(&hook, cave);
}

static int uninstall_hook(int core_id, uint64_t cave)
{
  khook_t hook;
  if (kh_init(&hook, kread, kwrite, ktext(0), cave, HOOK_IST) != 0)
    return -1;
  kh_set_tables(&hook, kdlsym(KERNEL_SYM_IDT_ARRAY), kdlsym(KERNEL_SYM_COMMON_TSS));
  kh_set_core(&hook, core_id);
  if (read_u64(cave + CAVE_SAVE_OFF + 0x00, &hook.saved_gate_lo) != 0 ||
      read_u64(cave + CAVE_SAVE_OFF + 0x08, &hook.saved_gate_hi) != 0 ||
      read_u64(cave + CAVE_SAVE_OFF + 0x10, &hook.saved_ist) != 0)
    return -4;
  hook.installed     = 1;
  if (kh_disarm(&hook) != 0)
    return -2;
  if (kh_restore(&hook) != 0)
    return -3;
  uint64_t cleared = 0;
  return kwrite(cave + CAVE_STATE_OFF, &cleared, sizeof cleared);
}

static int read_u64(uint64_t addr, uint64_t *out)
{
  *out = 0;
  return kread(addr, out, sizeof *out);
}

static int write_u64(uint64_t addr, uint64_t value)
{
  return kwrite(addr, &value, sizeof value);
}

static int write_entry_checked(uint64_t addr, uint64_t value)
{
  uint64_t readback;
  if (!addr || addr == UINT64_MAX || write_u64(addr, value) != 0 ||
      read_u64(addr, &readback) != 0 || readback != value)
    return -1;
  return 0;
}

int main(void)
{
  uint32_t fw;
  uint64_t cave;
  uint64_t oracle;
  uint64_t kernel_pmap;
  uint64_t kernel_pm_cr3;
  uint64_t pml4e_addr;
  uint64_t pdpe_addr;
  uint64_t pte_addr;
  uint64_t pde_addr;
  uint64_t pml4e;
  uint64_t pdpe;
  uint64_t pde;
  uint64_t pte;
  uint64_t o;
  uint64_t cave_state;
  uint64_t arm_id;
  uint64_t previous_arm;
  uint64_t previous_complete;

  printf("[+] Byepervisor reborn v1\n");

  // Do a quick firmware version detect, if we don't have offsets for it, goodbye!
  fw = kernel_get_fw_version();
  printf("[+] Detected firmware: %x.%x\n", (fw & 0xFF000000) >> 0x18, (fw & 0x00FF0000) >> 0x10);
  if (stage_logf("SESSION start build=13.60-stage-log-v2 firmware=0x%x pid=%d",
                 fw, getpid()) != 0) {
    notifyf("[ERROR] Byepervisor Reborn\nCannot write stage log; not arming");
    return 1;
  }

  if (kdlsym(KERNEL_SYM_HV_MP_STARTUP_CALL) == 0) {
    printf("[!] Undetected firmware, exiting!\n");
    notifyf("[ERROR] Byepervisor Reborn\nFirmware is not supported :(\n");
    return 1;
  }

  // The hypervisor loader's base physical address changes depending on if its a regular or a pro unit
  int soc_id = sceKernelGetMainSocId();
  if (soc_id == 0x840fd0) {
    g_hv_base_pa = HV_LOADER_PA_PRO;
    printf("[+] Pro model detected, HV loader PA = 0x%lx\n", g_hv_base_pa);
    notifyf("Byepervisor Reborn\nDetected pro model (0x%x)\n", soc_id);
  } else {
    g_hv_base_pa = HV_LOADER_PA_REGULAR;
    printf("[+] Regular model detected, HV loader PA = 0x%lx\n", g_hv_base_pa);
    notifyf("Byepervisor Reborn\nDetected regular model (0x%x)\n", soc_id);
  }

  // We need system ucred to pin to core 0
  kernel_set_ucred_authid(getpid(), 0x4800000000000007);

  cave   = kdlsym(KERNEL_SYM_DATA_CAVE);
  oracle = cave + KH_OFF_USER;

  if (pin_to_core(HOOK_CORE) != 0) {
    notifyf("Byepervisor Reborn\nCannot pin to hook core");
    return 1;
  }

  // This is so we can send the same payload after resume to experiment with post-HV hax (or not if we want to debug the resume payload)
#define DEBUG_MP_STARTUP_PAYLOAD 0
#if DEBUG_MP_STARTUP_PAYLOAD == 0
  if (read_u64(oracle, &o) != 0) {
    notifyf("Byepervisor Reborn\nCannot read prior stage state");
    return 1;
  }
  if (o != 0) {
    notifyf("Byepervisor Reborn\nPrior stage state 0x%lx; refusing to re-arm", o);
    return 1;
  }
#endif

#if defined(KHOOK_TEST)
  int ret;

  ret = install_test_hook(HOOK_CORE, cave);
  printf("installed test khook = %d\n", ret);

  printf("1\n");
  sceKernelSleep(1);

  kwrite64(oracle, 0);
  printf("2\n");
  sceKernelSleep(1);

  pin_to_core(HOOK_CORE);
  printf("core = %d\n", get_cpu_core());

  ret = syscall(SYS_getuid);

  o = kread64(oracle);
  printf("test khook oracle=0x%lx (ret = 0x%x)\n", o, ret);
  kwrite64(oracle, 0);
  sceKernelSleep(1);

  ret = syscall(SYS_getuid);

  o = kread64(oracle);
  printf("test khook oracle=0x%lx (ret = 0x%x)\n", o, ret);

  sceKernelSleep(1);

  if (uninstall_hook(HOOK_CORE, cave) != 0)
    return 1;
  notifyf("uninstalled test khook\n");
  return 0;

#else // !KHOOK_TEST

  // Copy our payload that runs from startup_mp into kernel cave
  if (kwrite(kdlsym(KERNEL_SYM_DATA_CAVE) + 0x1000, break_hv, break_hv_len) != 0) {
    notifyf("Byepervisor Reborn\nCannot stage AP payload");
    return 1;
  }

  // Make the physical region executable in guest page tables
  kernel_pmap   = kdlsym(KERNEL_SYM_PMAP_STORE);
  if (read_u64(kernel_pmap + 0x28, &kernel_pm_cr3) != 0 ||
      !kernel_pm_cr3 || (kernel_pm_cr3 & 0xfff)) {
    notifyf("Byepervisor Reborn\nInvalid kernel page-table root");
    return 1;
  }

  pml4e_addr = find_pml4e(kernel_pmap, kdldmap(STARTUP_MP_PAYLOAD_PAGE), &pml4e);
  if (pml4e_addr == UINT64_MAX || !PDE_FIELD(pml4e, PRESENT)) {
    notifyf("Byepervisor Reborn\nCarrier PML4 entry missing");
    return 1;
  }
  CLEAR_PDE_BIT(pml4e, EXECUTE_DISABLE);
  if (write_entry_checked(pml4e_addr, pml4e) != 0)
    return 1;

  pdpe_addr = find_pdpe(kernel_pmap, kdldmap(STARTUP_MP_PAYLOAD_PAGE), &pdpe);
  if (pdpe_addr == UINT64_MAX || !PDE_FIELD(pdpe, PRESENT)) {
    notifyf("Byepervisor Reborn\nCarrier PDP entry missing");
    return 1;
  }
  CLEAR_PDE_BIT(pdpe, EXECUTE_DISABLE);
  if (write_entry_checked(pdpe_addr, pdpe) != 0)
    return 1;

  if (PDE_FIELD(pdpe, PS) != 1) {
    pde_addr = find_pde(kernel_pmap, kdldmap(STARTUP_MP_PAYLOAD_PAGE), &pde);
    if (pde_addr == UINT64_MAX || !PDE_FIELD(pde, PRESENT)) {
      notifyf("Byepervisor Reborn\nCarrier PD entry missing");
      return 1;
    }
    CLEAR_PDE_BIT(pde, EXECUTE_DISABLE);
    if (write_entry_checked(pde_addr, pde) != 0)
      return 1;

    if (PDE_FIELD(pde, PS) != 1) {
      pte_addr = find_pte(kernel_pmap, kdldmap(STARTUP_MP_PAYLOAD_PAGE), &pte);
      if (pte_addr == UINT64_MAX || !PDE_FIELD(pte, PRESENT)) {
        notifyf("Byepervisor Reborn\nCarrier PT entry missing");
        return 1;
      }
      CLEAR_PDE_BIT(pte, EXECUTE_DISABLE);
      if (write_entry_checked(pte_addr, pte) != 0)
        return 1;
    }
  }

  /* Leave kernel text mappings untouched in the first controlled test.
     A separate read-only check can follow only after AP completion. */

  // Install hook on startup_mp hypercall
  printf("[+] Installing hook...\n");
  if (read_u64(cave + CAVE_STATE_OFF, &cave_state) != 0 || cave_state != 0) {
    notifyf("Byepervisor Reborn\nPrior hook state; refusing to re-arm");
    return 1;
  }
  if (write_u64(oracle, 0) != 0) {
    notifyf("Byepervisor Reborn\nCannot reset stage state");
    return 1;
  }
  if (stage_logf("HOOK arming begins; file logging paused until cleanup") != 0) {
    notifyf("Byepervisor Reborn\nCannot write pre-arm stage log");
    return 1;
  }
  struct timeval arm_time;
  if (gettimeofday(&arm_time, NULL) != 0 || arm_time.tv_sec <= 0 ||
      arm_time.tv_usec < 0 || arm_time.tv_usec >= 1000000) {
    notifyf("Byepervisor Reborn\nCannot obtain arm time; not arming");
    return 1;
  }
  arm_id = ((uint64_t)arm_time.tv_sec << 32) |
           ((uint64_t)arm_time.tv_usec << 12) |
           ((uint64_t)getpid() & 0xfff);
  if (!arm_id ||
      read_u64(cave + CAVE_ARM_ID_OFF, &previous_arm) != 0 ||
      read_u64(cave + CAVE_COMPLETE_ID_OFF, &previous_complete) != 0 ||
      previous_arm != 0 || previous_complete != 0 ||
      write_entry_checked(cave + CAVE_ARM_ID_OFF, arm_id) != 0 ||
      write_entry_checked(cave + CAVE_COMPLETE_ID_OFF, 0) != 0) {
    notifyf("Byepervisor Reborn\nCannot initialize attempt receipt; not arming");
    return 1;
  }
  notify_set_file_logging(0);
  int r = install_resume_hook(HOOK_CORE, cave, kernel_pm_cr3);
  if (r) {
    printf("[!] Hook installation returned %d\n", r);
    notifyf("[ERROR] Byepervisor Reborn\nHook install error %d; do not enter Rest Mode", r);
    return r;
  }

  printf("[+] Hook installed; automatic Rest Mode in 3 seconds\n");
  printf("BPR_ARM_V2 id=%016lx base=%016lx root=%016lx\n",
         arm_id, ktext(0), kernel_pm_cr3);
  fflush(stdout);
  notifyf("Byepervisor Reborn\nArmed. Entering Rest Mode in 3 seconds.");
  sleep(3);
  sceSystemStateMgrEnterStandby();

  // Poll for resume and hook trigger
  uint64_t last_stage = 0;
  int pending_after_hook = 0;
  for (;;) {
    if (pin_to_core(HOOK_CORE) != 0) {
      notifyf("Byepervisor Reborn\nLost hook-core affinity; do not rerun");
      return 1;
    }

    if (read_u64(oracle, &o) != 0) {
      notifyf("Byepervisor Reborn\nStage read failed");
      return 1;
    }
    if (o != last_stage) {
      printf("[+] Stage changed: 0x%lx\n", o);
      last_stage = o;
      if (o == STAGE_HOOK_FIRED)
        notifyf("Byepervisor Reborn\nResume hook fired; waiting for AP");
      else if (o == STAGE_AP_ENTERED)
        notifyf("Byepervisor Reborn\nAP entered; checking VMCBs");
    }
    if (o == STAGE_AP_COMPLETE) {
      notifyf("Byepervisor Reborn\nAP VMCB stage completed; checking cleanup");
      
      printf("[+] Uninstalling hook\n");
      if (uninstall_hook(HOOK_CORE, cave) != 0) {
        notifyf("Byepervisor Reborn\nHook cleanup failed; do not rerun");
        return 1;
      }
      if (write_entry_checked(cave + CAVE_COMPLETE_ID_OFF, arm_id) != 0) {
        notifyf("Byepervisor Reborn\nAttempt receipt write failed");
        return 1;
      }

      notify_set_file_logging(1);
      if (stage_logf("STAGE 0x%lx after hook cleanup", o) != 0)
        printf("[!] Could not append final stage to %s\n", STAGE_LOG_PATH);
      notifyf("Byepervisor Reborn\nAP stage complete; gate/IST restored. XOM untested.");

      // TODO: Patch kernel and all that fun stuff, we don't feel like doing this so someone else can do it :)

      return 0;
    }

    if (o == STAGE_AP_INVALID) {
      notifyf("Byepervisor Reborn\nAP rejected VMCB layout; checking cleanup");
      if (uninstall_hook(HOOK_CORE, cave) != 0) {
        notifyf("Byepervisor Reborn\nHook cleanup failed; do not rerun");
        return 1;
      }
      notify_set_file_logging(1);
      notifyf("Byepervisor Reborn\nAP rejected VMCB layout; hook restored");
      return 1;
    }

    if (o != 0 && o != STAGE_HOOK_FIRED && o != STAGE_AP_ENTERED) {
      notifyf("Byepervisor Reborn\nUnknown stage value 0x%lx", o);
      return 1;
    }

    if (o == STAGE_HOOK_FIRED || o == STAGE_AP_ENTERED) {
      if (++pending_after_hook >= 12) {
        notifyf("Byepervisor Reborn\nAP completion timed out; checking cleanup");
        if (uninstall_hook(HOOK_CORE, cave) != 0) {
          notifyf("Byepervisor Reborn\nHook cleanup failed; do not rerun");
          return 1;
        }
        notify_set_file_logging(1);
        notifyf("Byepervisor Reborn\nAP completion timed out; hook restored; last stage 0x%lx", o);
        return 1;
      }
    }

    sceKernelSleep(5);
  }
  return 0;
#endif
}
