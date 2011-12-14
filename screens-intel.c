/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#if defined(TARGET_X86)

#include "pmc.h"
#include "screen.h"
#include "screens-intel.h"

/* This is based on the "Intel 64 and IA-32 Architectures Software
   Developer's Manual, Volume 3B: System Programming Guide, Part
   2". May 2011.
 */


/* *** This is tested mostly for tables A-4 and A-6 *** */


/* Tables:
   A-1: 06_2a, 06_1a, 06_1e, 06_1f, 06_2e, 06_25, 06_2c
  
   A-2: Intel Core i7, i5, i3 Processors 2xxx Series (Sandy Bridge).
   06_2a

   A-4: Intel Core i7 and Intel Xeon Processor 5500 Series (Nehalem).
   06_1a, 06_1e, 06_1f, 06_2e

   A-6: Intel microarchitecture Westmere.
   06_25, 06_2c

   A-10: Intel Core microarchitecture.

   A-11: Intel Atom Processors.

   A-12: Intel Core Solo and Intel Core Duo Processors.

   A-22: P6
 */


enum tables { A_UNK, A_1, A_2, A_4, A_6, A_10, A_11, A_12, A_22 };

static const char* table_names[] = {
  "unknown", "A-1", "A-2", "A-4", "A-6", "A-10", "A-11", "A-12", "A-22"
};


#define FP_COMP_OPS_EXE_X87          0x0110 /* A-2, A-4, A-6 */
#define FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION  0x4010 /* A-2, A-4, A-6 */
#define FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION  0x8010 /* A-2, A-4, A-6 */

#define FP_ASSIST_1                  0x1eca  /* A-2 */
#define FP_ASSIST_2                  0x01f7  /* A-4, A-6 */
#define FP_ASSIST_3                  0x0011  /* A-10, A-12, A-22 */
#define FP_ASSIST_4                  0x0111  /* A-11 */

#define MEM_INST_RETIRED_LOADS       0x010b  /* A-4, A-6 */
#define MEM_INST_RETIRED_STORES      0x020b  /* A-4, A-6 */
#define BR_INST_RETIRED_ALL_BRANCHES 0x00c4  /* A-2, A-4, A-6, A-10, A-11, A-12, A-22 */

/* FIXME: check x87 when adding 0 and INFINITY */
#define INST_RETIRED_X87             0x02c0  /* A-2, A-4, A-6 */

#define BR_MISP_RETIRED_ALL_BRANCHES 0x00c5  /* A-2 */
#define BR_MISP_EXEC_ANY             0x7f89

#define ICACHE_MISSES_1              0x0280  /* A-2, A-11 */
#define ICACHE_MISSES_2              0x0081  /* A-12 */
#define L1I_MISSES                   0x0081  /* A-4, A-6, A-10 */

#define L2_RQSTS_LD_MISS             0x0224  /* A-4, A-6 */

#define L2_RQSTS_CODE_RD_MISS        0x2024  /* A-2 */
#define L2_RQSTS_IFETCH_MISS         0x2024  /* A-4, A-6 */

#define UOPS_RETIRED_ALL_1           0x01c2  /* A-2, A-4, A-6 */
#define UOPS_RETIRED_ALL_2           0x0fc2  /* A-10 */

#define UOPS_RETIRED_FUSED           0x07c2  /* A-10 */
#define UOPS_RETIRED_MACRO_FUSED     0x04c2  /* A-4, A-6 */


void screens_hook()
{
  intel_uop();
  intel_fp();
  intel_mem();
  intel_branch();
  intel_imix();
}

#if 0
/* Identify the CPU. Not the Intel recommended way (yet), but works for
 * recent CPUs. */
static void cpuid(int input, int* eax, int* ebx, int* ecx, int* edx)
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
#endif


static int disp_family_model()
{
  int a;
  int disp_model, disp_family;

  asm("mov $1, %%eax; " /* a into eax */
      "cpuid;"
      "mov %%eax, %0;"  /* eax into a */
      :"=r"(a) /* output */
      :
      :"%eax","%ebx","%ecx","%edx" /* clobbered register */
     );
  disp_model = ((a >> 4) & 0xf) | (((a >> 16) & 0xf) << 4);
  disp_family = ((a >> 8) & 0xf) | (((a >> 20) & 0xff) << 4);
  return (disp_family << 8) | disp_model;
}


static enum tables table()
{
  int disp_fam = disp_family_model();

  if (disp_fam == 0x062a)
    return A_2;
  else if ((disp_fam == 0x061a) || (disp_fam == 0x061e) ||
           (disp_fam == 0x061f) || (disp_fam == 0x062e))
    return A_4;
  else if ((disp_fam == 0x0625) || (disp_fam == 0x062c))
    return A_6;
  else
    return A_UNK;
}


/* Generate a target-dependent string, displayed in the help window. */
void target_dep_string(char* buf, int size)
{
  enum tables tab = table();
  int disp_fam = disp_family_model();

  snprintf(buf, size, "Processor: x86, display_family = %02X_%02X (table %s)",
           disp_fam >> 8, disp_fam & 0xff,
           table_names[tab]);
}


screen_t* intel_fp()
{
  int cycle, insn, x87, sp, dp, assist, assist_cnt;
  screen_t* s;

  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("floating point");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  x87 =   add_counter(s, PERF_TYPE_RAW, FP_COMP_OPS_EXE_X87);
  sp =    add_counter(s, PERF_TYPE_RAW, FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION);
  dp =    add_counter(s, PERF_TYPE_RAW, FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION);

  if (tab == A_2)
    assist_cnt = FP_ASSIST_1;
  else
    assist_cnt = FP_ASSIST_2;
  assist =  add_counter(s, PERF_TYPE_RAW, assist_cnt);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle, "Cycles (millions)");
  add_column_raw_m(s, "  Minstr", "%8.2f", insn,  "Instructions (millions)");
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle,
                   "Executed instructions per cycle");
  add_column_percent(s, " %x87", "%5.1f", x87, insn,
                     "FP computational uops (FP_COMP_OPS_EXE.X87) per insn");
  add_column_percent(s, "%SSES", "%5.1f", sp, insn,
                     "SSE* FP single precision uops per insn");
  add_column_percent(s, "%SSED", "%5.1f", dp, insn,
                     "SSE* FP double precision uops per insn");
  add_column_percent(s, "%assist", "  %5.1f", assist, insn,
                     "FP op that required micro-code assist per instruction");

  return s;
}


screen_t* intel_imix()
{
  int cycle, insn, ld, st, x87, br;
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("instruction mix");

  /* setup counters */
  cycle = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  ld =  add_counter(s, PERF_TYPE_RAW, MEM_INST_RETIRED_LOADS);
  st =  add_counter(s, PERF_TYPE_RAW, MEM_INST_RETIRED_STORES);
  x87 = add_counter(s, PERF_TYPE_RAW, INST_RETIRED_X87);
  br =  add_counter(s, PERF_TYPE_RAW, BR_INST_RETIRED_ALL_BRANCHES);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle, "Cycles (millions)");
  add_column_raw_m(s, "  Minstr", "%8.2f", insn, "Instructions (millions)");
  add_column_ratio(s, " IPC", "%4.2f", insn, cycle, 
                   "Executed instructions per cycle");
  add_column_percent(s, "%LD/I", " %4.1f", ld, insn, "Fraction of loads");
  add_column_percent(s, "%ST/I", " %4.1f", st, insn, "Fraction of stores");
  add_column_percent(s, "%FP/I", " %4.1f", x87, insn, "Fraction of x87");
  add_column_percent(s, "%BR/I", " %4.1f", br, insn,
                     "Fraction of branch instructions");

  return s;
}


screen_t* intel_mem()
{
  int insn, l1miss_i, l2miss_d, l2miss_i, l3miss;
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("memory hierarchy");

  /* setup counters */
  insn =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  /* L1I accesses */
  if (tab == A_2)
    l1miss_i = add_counter(s, PERF_TYPE_RAW, ICACHE_MISSES_1);
  else
    l1miss_i = add_counter(s, PERF_TYPE_RAW, L1I_MISSES);

  /* L2 loads */
  if (tab == A_2) {
    l2miss_d = -1; /* no support for L2 data miss in Table A-2 */
    l2miss_i = add_counter(s, PERF_TYPE_RAW, L2_RQSTS_CODE_RD_MISS);
  }
  else {
    l2miss_d = add_counter(s, PERF_TYPE_RAW, L2_RQSTS_LD_MISS);
    l2miss_i = add_counter(s, PERF_TYPE_RAW, L2_RQSTS_IFETCH_MISS);
  }

  l3miss = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_raw(s, " miss L1I", "%9" PRIu64, l1miss_i,
                 "Instruction fetches that miss in L1I (L1I.MISSES)");
  add_column_percent(s, " L1I", "%4.1f", l1miss_i, insn,
                     "   same, per instruction");

  add_column_raw(s, " miss L2I", "%9" PRIu64, l2miss_i,
                 "Insn fetches that miss L2 cache (L2_RQSTS.IFETCH_MISS)");
  add_column_percent(s, " L2I", "%4.1f", l2miss_i, insn,
                     "   same, per instruction");

  if (tab != A_2) {
    add_column_raw(s, " miss L2D", "%9" PRIu64, l2miss_d,
                   "Loads that miss L2 cache");
    add_column_percent(s, " L2D", "%4.1f", l2miss_d, insn,
                       "   same, per instruction");
  }

  add_column_raw(s, "   miss L3", " %9" PRIu64, l3miss, "LLC Misses");
  add_column_percent(s, "   L3", " %4.1f", l3miss, insn,
                     "   same, per instruction");

  return s;
}


screen_t* intel_branch()
{
  int insn, br, misp;
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6) && (tab != A_10) &&
      (tab != A_10) && (tab != A_11) && (tab != A_12) && (tab != A_22))
    return NULL;

  s = new_screen("branch instructions");

  /* setup counters */
  insn = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  br =   add_counter(s, PERF_TYPE_RAW,  BR_INST_RETIRED_ALL_BRANCHES);
  misp = add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_percent(s, " %MISP", " %5.2f", misp, br,
                     "Mispredictions per 100 branch instructions");
  add_column_percent(s, " %MIS/I", "  %5.2f", misp, insn,
                     "Mispredictions per 100 instructions");

  return s;
}


screen_t* intel_uop()
{
  int cycle, insn, uop, fused, macrof;
  screen_t* s = NULL;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6) && (tab != A_10))
    return NULL;

  s = new_screen("micro operations");

  /* setup counters */
  cycle =  add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  insn =   add_counter(s, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  if (tab == A_10)
    uop =    add_counter(s, PERF_TYPE_RAW, UOPS_RETIRED_ALL_2);
  else
    uop =    add_counter(s, PERF_TYPE_RAW, UOPS_RETIRED_ALL_1);

  if (tab == A_10)
    fused =  add_counter(s, PERF_TYPE_RAW, UOPS_RETIRED_FUSED);
  else
    fused = -1;

  if ((tab == A_4) || (tab == A_6))
    macrof = add_counter(s, PERF_TYPE_RAW, UOPS_RETIRED_MACRO_FUSED);
  else
    macrof = -1;

  /* add columns */
  add_column_cpu(s, " %CPU", "%5.1f");
  add_column_cpu_s(s, " %SYS", "%5.1f");
  add_column_raw_m(s, "  Mcycle", "%8.2f", cycle, "Cycles (millions)");
  add_column_raw_m(s, "   Muops", "%8.2f", uop, "Retired uops (millions)");
  add_column_raw_m(s, "  Minstr", "%8.2f", insn,
                   "Retired instructions (millions)");

  add_column_ratio(s, "  IPC", " %4.2f", insn, cycle,
                   "Retired instructions per cycle");
  add_column_ratio(s, "  uPI", " %4.2f", uop, insn,
                   "Retired uops per instruction");
  if (fused != -1)
    add_column_ratio(s, " fused", "  %4.2f", fused, insn,
                     "Fused uops (UOPS_RETIRED.FUSED) per instruction");

  if (macrof != -1)
    add_column_ratio(s, " macrof", "   %4.2f", macrof, insn,
                 "Macro fused uops (UOPS_RETIRED.MACRO_FUSED) per instruction");

  return s;
}

#endif  /* TARGET_X86 */
