#ifdef KERNEL31
#include <linux/perf_counter.h>
#else
#include <linux/perf_event.h>
#endif

#include "screen.h"


/* This is an Intel specific screen (Nehalem??) */
screen_t* my_screen()
{
  int cycle, insn, x87, sp, dp;
  screen_t* s = new_screen("nehalem FP");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  x87 = add_counter(s, PERF_TYPE_RAW,  0x110);  /* FP_COMP_OPS_EXE.X87 */
  sp =  add_counter(s, PERF_TYPE_RAW, 0x4010);  /* FP_COMP_OPS_EXE.SSE_SINGLE_PRECISION */
  dp =  add_counter(s, PERF_TYPE_RAW, 0x8010);  /* FP_COMP_OPS_EXE.SSE_DOUBLE_PRECISION */

  /* add columns */
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle);
  add_column_raw_m(s, "  Minstr", "%8.2f", insn);
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle);
  add_column_percent(s, " %x87", "%5.1f", x87, insn);
  add_column_percent(s, "%SSES", "%5.1f", sp, insn);
  add_column_percent(s, "%SSED", "%5.1f", dp, insn);

  return s;
}
