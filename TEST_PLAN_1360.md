# 13.60 controlled-stage test

This is a **first live test plan**, not a verified 13.60 hypervisor result.
The ELF changes kernel page-table entries, installs a CPU 0 breakpoint hook,
and, if the AP payload reaches the expected VMCBs, clears their nested-paging
control bit. A console shutdown or storage repair is possible. The source
checks reduce false success reports but do not establish a safe live path.

## Before loading

1. Confirm the console reports firmware **13.60** and note whether it is a
   standard or Pro model. This build selects the hypervisor physical base from
   the detected SoC ID; do not proceed if its notification names the wrong
   model.
2. Finish any storage repair and return to the home screen. Have the normal
   kernel exploit and **elfldr** running. The payload depends on elfldr
   surviving Rest Mode; ProsperoManager or another launcher has not been
   validated as equivalent for this test.
3. On the PC, compare the file's SHA-256 with `dist/SHA256SUMS`. Use only that
   exact ELF for this run and record the hash in the test log.
   The console log will be `/data/ps5-s3-1360/byepervisor-reborn-1360.log`.
4. Enable the PS5 Rest Mode option **Supply Power to USB Ports: Always** if it
   is not already set.

## One run

1. Send `dist/byepervisor_reborn_1360_stage_logged.elf` to the running elfldr.
   Record the launch time and every notification verbatim.
2. If the payload reports an unsupported firmware, wrong model, prior stage
   state, failed staging, invalid page table, or hook install error, **stop**.
   Do not enter Rest Mode or relaunch on that boot.
3. Wait for **`Armed. Enter Rest Mode manually, then wake.`** This means the
   loader reached the hook-arm point; it does not mean the resume hook fired.
4. From the PS5 power menu, choose **Enter Rest Mode**. Wait for the light to
   settle to steady amber for about one minute. Wake the console once.
5. Record whether it returns to the home screen and the latest payload notice:
   `Resume hook fired`, `AP entered`, `AP rejected VMCB layout`, or
   `AP stage complete; gate/IST restored. XOM untested.` Also note any
   `Hook cleanup failed` notice or shutdown.
6. After the console is stable, retrieve the `.log` file through FTP and
   preserve its size, SHA-256, and exact contents. The ELF appends to that
   path, so a later run does not erase earlier sessions.

## Interpretation and stop conditions

| Observation | What it establishes | Action |
| --- | --- | --- |
| Armed only | Hook setup reported success | Stop after one wake; collect logs. |
| Resume hook fired | The saved hook ran | Wait for AP result; do not relaunch. |
| AP entered | AP code began | Wait for completion or failure; do not relaunch. |
| AP rejected VMCB layout | One expected VMCB field failed validation | Record result; do not relaunch. |
| AP stage complete; gate/IST restored | All 16 intended VMCB control writes read back and hook cleanup reported success | Record result; perform a separate read-only access check later. |
| Timeout, unknown stage, cleanup failure, freeze, shutdown, or storage repair | No verified completion | Stop testing this build and preserve logs. |

Even the AP-complete result does not demonstrate a kernel text read, a
hypervisor memory read, or lasting hypervisor control. The later access check
must be a separately reviewed step and must not be inferred from a notice.
