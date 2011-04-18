#if defined(TARGET_X86)

#ifdef KERNEL31
#include <linux/perf_counter.h>
#else
#include <linux/perf_event.h>
#endif

#include "screen.h"


/* Identify the CPU. Not the Intel recommended way (yet), but works for
 * recent CPUs. */
void cpuid(int input, int* eax, int* ebx, int* ecx, int* edx)
{
  int a, b, c, d;
  asm("mov %4, %%eax; " // a into eax
      "cpuid;"
      "mov %%eax, %0;" // eax into a
      "mov %%ebx, %1;" // ebx into b
      "mov %%ecx, %2;" // ecx into c
      "mov %%edx, %3;" // edx into d
      :"=r"(a), "=r"(b), "=r"(c), "=r"(d) /* output */
      :"r"(input) /* input */
      :"%eax","%ebx","%ecx","%edx" /* clobbered register */
     );
  *eax = a;
  *ebx = b;
  *ecx = c;
  *edx = d;
}


/* This is an Intel specific screen (Nehalem??) */
screen_t* nehalem_fp()
{
  int cycle, insn, x87, sp, dp, assist;
  screen_t* s = new_screen("nehalem FP");

#if 0
  {
    int a, b, c, d;
    cpuid(1, &a, &b, &c, &d); 
    printf("Family: %x  Model: %x  Stepping: %x\n", (a >> 8) & 0xf, (a >> 4) & 0xf, a & 0xf);
    printf("Ext Family: %x  Ext Model: %x\n", (a >> 20) & 0xff, (a >> 16) & 0xf);
  }
#endif

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  x87 = add_counter(s, PERF_TYPE_RAW,  0x110);  /* FP_COMP_OPS_EXE.X87 */
  sp =  add_counter(s, PERF_TYPE_RAW, 0x4010);  /* FP_COMP_OPS_EXE.SSE_SINGLE_PRECISION */
  dp =  add_counter(s, PERF_TYPE_RAW, 0x8010);  /* FP_COMP_OPS_EXE.SSE_DOUBLE_PRECISION */
  assist =  add_counter(s, PERF_TYPE_RAW, 0x01f7);  /* FP_ASSIST */

  /* add columns */
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle);
  add_column_raw_m(s, "  Minstr", "%8.2f", insn);
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle);
  add_column_percent(s, " %x87", "%5.1f", x87, insn);
  add_column_percent(s, "%SSES", "%5.1f", sp, insn);
  add_column_percent(s, "%SSED", "%5.1f", dp, insn);
  add_column_percent(s, "%assist", "  %5.1f", assist, insn);

  return s;
}


screen_t* nehalem_app()
{
  int cycle, insn, ld, st, x87, br;
  screen_t* s = new_screen("nehalem app properties");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  ld = add_counter(s, PERF_TYPE_RAW, 0x010b);  /* MEM_INST_RETIRED.LOADS */
  st = add_counter(s, PERF_TYPE_RAW, 0x020b);  /* MEM_INST_RETIRED.STORES */
  x87 = add_counter(s, PERF_TYPE_RAW,  0x110);  /* FP_COMP_OPS_EXE.X87 */
  br = add_counter(s, PERF_TYPE_RAW,  0x00c4);  /* BR_INST_RETIRED.ALL_BRANCHES */


  /* add columns */
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle);
  add_column_raw_m(s, "  Minstr", "%8.2f", insn);
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle);
  add_column_percent(s, "%LD/I", " %4.1f", ld, insn);
  add_column_percent(s, "%ST/I", " %4.1f", st, insn);
  add_column_percent(s, "%FP/I", " %4.1f", x87, insn);
  add_column_percent(s, "%BR/I", " %4.1f", br, insn);

  return s;
}


screen_t* nehalem_mem()
{
  int insn, l1miss_i, l1wb, l2miss_d, l2miss_i, l3miss;
  screen_t* s = new_screen("nehalem memory hierarchy");

  /* setup counters */
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  l1miss_i = add_counter(s, PERF_TYPE_RAW, 0x0280);  /* L1I.MISSES */
/*  l1wb = add_counter(s, PERF_TYPE_RAW, 0x0128);  /* L1D_WB_L2.I_STATE */
  /* L2 loads */
  l2miss_d = add_counter(s, PERF_TYPE_RAW, 0x0224);  /* L2_RQSTS.LD_MISS */
  l2miss_i = add_counter(s, PERF_TYPE_RAW, 0x2024);  /* L2_RQSTS.IFETCH_MISS */
  l3miss = add_counter(s, PERF_TYPE_RAW, 0x412e);  /* L3_LAT_CACHE.MISS */

  /* add columns */
  add_column_raw(s, " miss L1I", "%9d", l1miss_i);
  add_column_percent(s, " L1I", "%4.1f", l1miss_i, insn);

  add_column_raw(s, " miss L2I", "%9d", l2miss_i);
  add_column_percent(s, " L2I", "%4.1f", l2miss_i, insn);
 /* add_column_raw(s, "      L1D", "%9d", l1wb); */
  add_column_raw(s, " miss L2D", "%9d", l2miss_d);
/*  add_column_percent(s, " L1D", "%4.1f", l1wb, insn); */
  add_column_percent(s, " L2D", "%4.1f", l2miss_d, insn);
  add_column_raw(s, "  miss L3", "%9d", l3miss);
  add_column_percent(s, "  L3", "%4.1f", l3miss, insn);

  return s;
}


screen_t* nehalem_br()
{
  int insn, br, misp;
  screen_t* s = new_screen("nehalem branches");

  /* setup counters */
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  br = add_counter(s, PERF_TYPE_RAW,  0x00c4);  /* BR_INST_RETIRED.ALL_BRANCHES */
  misp = add_counter(s, PERF_TYPE_RAW, 0x7f89);  /* BR_MISP_EXEC_ANY */

  /* add columns */
  add_column_percent(s, "%MISP", " %4.1f", misp, br);
  add_column_percent(s, "MIS/I", " %4.1f", misp, insn);

  return s;
}

#endif  /* TARGET_X86 */
