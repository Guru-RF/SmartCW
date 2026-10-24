#include "crash.h"
#include <hardware/watchdog.h>
#include <pico/platform.h>
#include <stdarg.h>

struct CrashInfo {
  uint32_t magic;
  uint32_t pc, lr;
  char msg[60];
};
static const uint32_t CRASH_MAGIC = 0x5C0FFEE5;
static CrashInfo __uninitialized_ram(crash);
static CrashInfo last;
static bool haveLast = false;

extern "C" {
// Linked with -Wl,--wrap=panic: every panic(), lwIP asserts included.
void __real_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...) {
  crash.magic = CRASH_MAGIC;
  crash.lr = (uint32_t)__builtin_return_address(0);
  crash.pc = 0;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(crash.msg, sizeof(crash.msg), fmt, ap);
  va_end(ap);
  watchdog_reboot(0, 0, 10);
  while (true)
    tight_loop_contents();
}

void hardfaultC(uint32_t *sp) {
  crash.magic = CRASH_MAGIC;
  crash.pc = sp[6];
  crash.lr = sp[5];
  strcpy(crash.msg, "hard fault");
  watchdog_reboot(0, 0, 10);
  while (true)
    tight_loop_contents();
}

// Pick the stack the fault happened on and hand its frame to hardfaultC.
__attribute__((naked)) void isr_hardfault(void) {
  __asm volatile("movs r0, #4      \n"
                 "mov  r1, lr      \n"
                 "tst  r0, r1      \n"
                 "beq  1f          \n"
                 "mrs  r0, psp     \n"
                 "b    2f          \n"
                 "1: mrs r0, msp   \n"
                 "2: ldr r1, =hardfaultC \n"
                 "bx   r1          \n");
}
}

void crashBegin() {
  if (crash.magic == CRASH_MAGIC) {
    last = crash;
    last.msg[sizeof(last.msg) - 1] = 0;
    haveLast = true;
  }
  crash.magic = 0;
}

bool crashReport(Print &o) {
  if (!haveLast)
    return false;
  o.printf("!last crash: %s (pc 0x%08lx, lr 0x%08lx)\n", last.msg, (unsigned long)last.pc, (unsigned long)last.lr);
  return true;
}
