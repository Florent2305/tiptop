/*
 * This file is part of tiptop.
 *
 * Author: Erven ROHOU
 * Copyright (c) 2011, 2012 Inria
 *
 * License: GNU General Public License version 2.
 *
 */

#include <config.h>

#if TARGET_X86

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
  int assist_cnt;
  screen_t* s;

  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("floating point","");

  /* setup counters */
  add_counter_by_value(s, "CYCLE", (uint64_t) PERF_COUNT_HW_CPU_CYCLES, (uint32_t) PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "INSN", (uint64_t) PERF_COUNT_HW_INSTRUCTIONS, (uint32_t) PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "X87", (uint64_t) FP_COMP_OPS_EXE_X87, (uint32_t) PERF_TYPE_RAW);
  add_counter_by_value(s, "SP",  (uint64_t) FP_COMP_OPS_EXE_SSE_SINGLE_PRECISION, (uint32_t) PERF_TYPE_RAW);
  add_counter_by_value(s, "DP",  (uint64_t) FP_COMP_OPS_EXE_SSE_DOUBLE_PRECISION, (uint32_t) PERF_TYPE_RAW);

  if (tab == A_2)
    assist_cnt = FP_ASSIST_1;
  else
    assist_cnt = FP_ASSIST_2;

  add_counter_by_value(s, "Assist", (uint64_t) assist_cnt, (uint32_t) PERF_TYPE_RAW);

  /* add columns */
  add_column(s, " %CPU"    , "%5.1f"  ,  NULL, "CPU_TOT");
  add_column(s, "  Mcycle", "%8.2f", "Cycles (millions)"  , "delta(CYCLE) / 1000000");
  add_column(s, "  Minstr", "%8.2f", "Instructions (millions)", "delta(INSN) / 1000000");
  add_column(s, " IPC",     "%4.2f", "Executed instructions per cycle", "delta(INSN)/delta(CYCLE)");
  add_column(s," %x87", "%5.1f", "FP computational uops (FP_COMP_OPS_EXE.X87) per insn", "100*delta(X87)/delta(INSN)");
  add_column(s, "%SSES", "%5.1f", "SSE* FP single precision uops per insn", "100*delta(SP)/delta(INSN)");

  add_column(s, "%SSED", "%5.1f", "SSE* FP double precision uops per insn", "100*delta(DP)/delta(INSN)");
  add_column(s, "%assist", "  %5.1f", "FP op that required micro-code assist per instruction", "100*delta(Assist)/delta(INSN)");

  return s;
}


screen_t* intel_imix()
{
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("instruction mix","");

  /* setup counters */
  add_counter_by_value(s, "CYCLE", (uint64_t)PERF_COUNT_HW_CPU_CYCLES, (uint32_t) PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "INSN", (uint64_t) PERF_COUNT_HW_INSTRUCTIONS, (uint32_t)PERF_TYPE_HARDWARE);

  add_counter_by_value(s, "LD",  (uint64_t)MEM_INST_RETIRED_LOADS, (uint32_t) PERF_TYPE_RAW);
  add_counter_by_value(s, "ST",  (uint64_t)MEM_INST_RETIRED_STORES,(uint32_t) PERF_TYPE_RAW);
  add_counter_by_value(s, "X87", (uint64_t)INST_RETIRED_X87, (uint32_t)PERF_TYPE_RAW);
  add_counter_by_value(s, "BR",  (uint64_t)BR_INST_RETIRED_ALL_BRANCHES,(uint32_t) PERF_TYPE_RAW);

  /* add columns */
  add_column(s, " %CPU"    , "%5.1f"  ,  NULL, "CPU_TOT");
  add_column(s, "  Mcycle", "%8.2f", "Cycles (millions)"  , "delta(CYCLE) / 1000000");
  add_column(s, "  Minstr", "%8.2f", "Instructions (millions)", "delta(INSN) / 1000000");
  add_column(s, " IPC",     "%4.2f", "Executed instructions per cycle", "delta(INSN)/delta(CYCLE)");
  add_column(s, "%LD/I", " %4.1f", "Fraction of loads","100*delta(LD)/delta(INSN)");
  add_column(s, "%ST/I", " %4.1f", "Fraction of stores","100*delta(ST)/delta(INSN)");
  add_column(s, "%FP/I", " %4.1f", "Fraction of x87","100*delta(X87)/delta(INSN)");
  add_column(s, "%BR/I", " %4.1f", "Fraction of branch instructions","100*delta(BR)/delta(INSN)");

  return s;
}


screen_t* intel_mem()
{
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6))
    return NULL;

  s = new_screen("memory hierarchy","");

  /* setup counters */
  add_counter_by_value(s, "INSN", (uint64_t)PERF_COUNT_HW_INSTRUCTIONS, (uint32_t)PERF_TYPE_HARDWARE);
  /* L1I accesses */
  if (tab == A_2)
    add_counter_by_value(s, "L1MissI", (uint64_t) ICACHE_MISSES_1, (uint32_t)PERF_TYPE_RAW);
  else
    add_counter_by_value(s, "L1MissI", (uint64_t) L1I_MISSES, (uint32_t) PERF_TYPE_RAW);

  /* L2 loads */
  if (tab == A_2) {
    /* no support for L2 data miss in Table A-2 */
    add_counter_by_value(s, "L2MissI", (uint64_t) L2_RQSTS_CODE_RD_MISS,(uint32_t)PERF_TYPE_RAW);
  }
  else {
    add_counter_by_value(s, "L2MissD", (uint64_t)L2_RQSTS_LD_MISS,  (uint32_t)PERF_TYPE_RAW);
    add_counter_by_value(s, "L2MissI", (uint64_t)L2_RQSTS_IFETCH_MISS,  (uint32_t)PERF_TYPE_RAW);
  }

  add_counter_by_value(s, "L3Miss", (uint64_t)PERF_COUNT_HW_CACHE_MISSES, (uint32_t) PERF_TYPE_HARDWARE);

  /* add columns */
  add_column(s, " %CPU"    , "%5.2f"  ,  NULL, "CPU_TOT");
  add_column(s, " L1iMiss", "   %5.1" PRIu64, "Instruction fetches that miss in L1I (L1I.MISSES) In Million","delta(L1MissI)");
  add_column(s, "   L1i", "  %4.1f", "Same L1iMiss per instruction", "100*delta(L1MissI)/delta(INSN)");
  add_column(s, " L2iMiss", " %7" PRIu64,"Insn fetches that miss L2 cache (L2_RQSTS.IFETCH_MISS) In Million", "delta(L2MissI)");
  add_column(s, "   L2i", "  %4.1f", "same L2iMiss per instruction", "100*delta(L2MissI)/delta(INSN)");

  if (tab != A_2) {
    add_column(s, "  L2dMiss", "%9" PRIu64, "Loads that miss L2 cache", "delta(L2MissD)");
    add_column(s, "  L2d", " %4.1f", "   same L2dMiss, per instruction","100*delta(L2MissD)/delta(INSN)");
  }

  add_column(s, "    L3Miss", " %9" PRIu64, "LLC Misses", "delta(L3Miss)");
  add_column(s, "   L3", " %4.1f","same L3Miss, per instruction","100*delta(L3Miss)/delta(INSN)");

  return s;
}


screen_t* intel_branch()
{
  screen_t* s;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6) && (tab != A_10) &&
      (tab != A_10) && (tab != A_11) && (tab != A_12) && (tab != A_22))
    return NULL;

  s = new_screen("branch instructions","");

  /* setup counters */

  add_counter_by_value(s, "INSN", (uint64_t) PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "MISS", (uint64_t) PERF_COUNT_HW_CACHE_MISSES, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "BR", (uint64_t) PERF_COUNT_HW_BRANCH_MISSES, PERF_TYPE_HARDWARE);

  /* add columns */
  add_column(s, " %CPU"    , "%5.1f"  ,  NULL, "CPU_TOT");
  add_column(s, "  %MISP"  , "  %5.2f", "Mispredictions per 100 branch instructions", "100*delta(MISS)/delta(BR)");
  add_column(s, "  %MIS/I ", "  %5.2f", "Mispredictions per 100 branch instructions", "100*delta(MISS)/delta(INSN)");

  return s;
}


screen_t* intel_uop()
{
  int fused, macrof;
  screen_t* s = NULL;
  enum tables tab = table();

  if ((tab != A_2) && (tab != A_4) && (tab != A_6) && (tab != A_10))
    return NULL;

  s = new_screen("micro operations","");

  /* setup counters */
  add_counter_by_value(s, "CYCLE", (uint64_t)PERF_COUNT_HW_CPU_CYCLES, PERF_TYPE_HARDWARE);
  add_counter_by_value(s, "INSN", (uint64_t) PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE);

  if (tab == A_10)
    add_counter_by_value(s, "UOP",(uint64_t) UOPS_RETIRED_ALL_2, (uint32_t)PERF_TYPE_RAW);
  else
    add_counter_by_value(s, "UOP", (uint64_t)UOPS_RETIRED_ALL_1, (uint32_t)PERF_TYPE_RAW);

  if (tab == A_10)
    fused =  add_counter_by_value(s, "FUSE", (uint64_t) UOPS_RETIRED_FUSED,(uint32_t) PERF_TYPE_RAW);
  else
    fused = -1;

  if ((tab == A_4) || (tab == A_6))
    macrof = add_counter_by_value(s, "MACROF", (uint64_t)UOPS_RETIRED_MACRO_FUSED, (uint32_t)PERF_TYPE_RAW);
  else
    macrof = -1;

  /* add columns */
  add_column(s, " %CPU",    "%5.1f", NULL, "CPU_TOT");
  add_column(s, " %SYS",    "%5.1f", NULL, "CPU_SYS");
  add_column(s, "  Mcycle", "%8.2f", "Cycles (millions)"  , "delta(CYCLE) / 1000000");
  add_column(s, "  Minstr", "%8.2f", "Instructions (millions)", "delta(INSN) / 1000000");
  add_column(s, " IPC",     "%4.2f", "Executed instructions per cycle", "delta(INSN)/delta(CYCLE)");
  add_column(s, "   Muops", "%8.2f", "Retired uops (millions)", "delta(UOP)/1000000");

  add_column(s, "  uPI", " %4.2f", "Retired uops per instruction", "delta(UOP)/delta(INSN)");
  if (fused != -1)
    add_column(s, " fused", "  %4.2f", "Fused uops (UOPS_RETIRED.FUSED) per instruction",  "delta(FUSE)/delta(INSN)");

  if (macrof != -1)
    add_column(s, " macrof", "   %4.2f", "Macro fused uops (UOPS_RETIRED.MACRO_FUSED) per instruction", "delta(MACROF)/delta(INSN)");

  return s;
}

#endif  /* TARGET_X86 */
