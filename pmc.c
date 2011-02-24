#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "pmc.h"

/* Manually call the syscall, because the definition is missing in
 * some kernels.
 */
#undef __NR_perf_counter_open
#ifdef __powerpc__
#define __NR_perf_counter_open	319
#elif defined(__ARM_EABI__)
#define __NR_perf_counter_open	364
#elif defined(__x86_64__)
#define __NR_perf_counter_open	298
#elif defined(__i386__)
#define __NR_perf_counter_open	336
#endif

#ifdef KERNEL31
#define perf_event_attr		perf_counter_attr
#endif

long sys_perf_counter_open(struct STRUCT_NAME *hw_event,
                           pid_t pid,
                           int cpu,
                           int group_fd,
                           unsigned long flags)
{
  int ret;

  ret = syscall(__NR_perf_counter_open, hw_event, pid, cpu, group_fd, flags);
#if defined(__x86_64__) || defined(__i386__)
  if ( ret < 0 && ret > -4096 ) {
    errno = -ret;
    ret = -1;
  }
#endif

  return ret;
}
