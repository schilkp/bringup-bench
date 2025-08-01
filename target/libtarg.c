#include "libtarg.h"
#include "libmin.h"

#if defined(TARGET_HOST)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#elif defined(TARGET_SA)
/* standalone */
#include <stdio.h>
#include <stdlib.h>
#define MAX_SPIN  10000     /* make this larger for a real hardware platform */
#elif defined(TARGET_HAHOST)
/* hashalone-host */
#include <stdio.h>
#include <stdlib.h>
#define MAX_SPIN  10000     /* make this larger for a real hardware platform */
#elif defined(TARGET_SIMPLE) || defined(TARGET_SPIKE) || defined(TARGET_HASPIKE)
#include <stdlib.h>

/* simple system MMAP'ed registers */
#define SIMPLE_CTRL_BASE   0x20000
#define SIMPLE_CTRL_OUT    0x00
#define SIMPLE_CTRL_CTRL   0x08
#define SIMPLE_CTRL_HIHASH 0x10
#define SIMPLE_CTRL_LOHASH 0x14

/* MMAP'ed register accessors */
#define SIMPLE_DEV_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = val)
#define SIMPLE_DEV_READ(addr, val) (*((volatile uint32_t *)(addr)))

extern inline int
simple_putchar(char c)
{
  SIMPLE_DEV_WRITE(SIMPLE_CTRL_BASE + SIMPLE_CTRL_OUT, (unsigned char)c);
  return c;
}

extern inline void
simple_halt(void)
{
  SIMPLE_DEV_WRITE(SIMPLE_CTRL_BASE + SIMPLE_CTRL_CTRL, 1);
}

void *
memset(void *dest, int val, size_t len)
{
  return libmin_memset(dest, val, len);
}

void *
memcpy(void *dest, const void *src, size_t len)
{
  return libmin_memcpy(dest, src, len);
}

extern inline uint32_t
simple_get_mepc(void)
{
  uint32_t result;
  __asm__ volatile("csrr %0, mepc;" : "=r"(result));
  return result;
}

extern inline uint32_t
simple_get_mcause(void)
{
  uint32_t result;
  __asm__ volatile("csrr %0, mcause;" : "=r"(result));
  return result;
}

extern inline uint32_t
simple_get_mtval(void)
{
  uint32_t result;
  __asm__ volatile("csrr %0, mtval;" : "=r"(result));
  return result;
}

void
simple_exc_handler(void)
{
  libmin_printf("EXCEPTION!!!\n");
  libmin_printf("============\n");
  libmin_printf("MEPC:0x%08x, CAUSE:0x%08x, MTVAL:0x%08x\n", simple_get_mepc(), simple_get_mcause(), simple_get_mtval());

  simple_halt();
  while(1);
}

void
simple_timer_handler(void)
{
  libmin_printf("TIMER EXCEPTION!!!\n");

  simple_halt();
  while(1);
}

extern inline void
simple_output_hash(uint64_t __hashval)
{
  SIMPLE_DEV_WRITE(SIMPLE_CTRL_BASE + SIMPLE_CTRL_HIHASH, (uint32_t)(__hashval >> 32));
  SIMPLE_DEV_WRITE(SIMPLE_CTRL_BASE + SIMPLE_CTRL_LOHASH, (uint32_t)__hashval);
}
#elif defined(TARGET_CVA6_DCHECK)
#include <stdlib.h>

volatile uint8_t *dcheck_exit = (uint8_t*) 0x1000;
volatile uint8_t *dcheck_putc = (uint8_t*) 0x1008;
volatile uint32_t *dcheck_putword = (uint32_t*) 0x1008;

#else /* undefined target */
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif

#ifdef TARGET_SA
#define MAX_OUTBUF    (128*1024)
static uint8_t __outbuf[MAX_OUTBUF];
static uint32_t __outbuf_ptr = 0;
#endif /* TARGET_SA */

#if defined(TARGET_HAHOST) || defined(TARGET_HASPIKE)
uint64_t __hashval = FNV64a_INIT;
#endif /* TARGET_HAHOST */

/* benchmark completed successfully */
void
libtarg_success(void)
{
#if defined(TARGET_HOST)
  exit(0);
#elif defined(TARGET_SA)
  uint64_t spincnt = 0;
  /* spin @ SPIN_SUCCESS_ADDR */
SPIN_SUCCESS_ADDR:
  spincnt++;
  if (spincnt < MAX_SPIN)
    goto SPIN_SUCCESS_ADDR;

  /* output any outbuf data */
  for (uint32_t i=0; i < __outbuf_ptr; i++)
    fputc(__outbuf[i], stdout);

  /* exit if we ever get here */
  exit(0);
#elif defined(TARGET_HAHOST)
  /* print the output hash value */
  fprintf(stdout, "** hashval = 0x%016lx\n", __hashval);

  /* exit if we ever get here */
  exit(0);
#elif defined(TARGET_HASPIKE)
  /* print the output hash value */
  simple_output_hash(__hashval);
  simple_halt();
#elif defined(TARGET_SIMPLE) || defined(TARGET_SPIKE)
  // libmin_printf("EXIT: success\n");
  simple_halt();
#elif defined(TARGET_CVA6_DCHECK)
  // printf("** hashval = 0x%016lx\n", __hashval);
  // printf("EXIT: success\n");
  while (1) {
    *dcheck_exit = 0;
  }
#else
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif
}

/* benchmark completed with error CODE */
void
libtarg_fail(int code)
{
#ifdef TARGET_HOST
  exit(code);
#elif defined(TARGET_SA)
  uint64_t spincnt = 0;
  /* spin @ SUCCESS_SPIN_ADDR */
SPIN_FAIL_ADDR:
  spincnt++;
  if (spincnt < MAX_SPIN)
    goto SPIN_FAIL_ADDR;
  /* exit if we ever get here */
  exit(code);
#elif defined(TARGET_HAHOST)
  /* exit if we ever get here */
  exit(code);
#elif defined(TARGET_SIMPLE) || defined(TARGET_SPIKE) || defined(TARGET_HASPIKE)
  // libmin_printf("EXIT: fail code = %d\n", code);
  simple_halt();
#elif defined(TARGET_CVA6_DCHECK)
  // printf("** hashval = 0x%016lx\n", __hashval);
  // printf("EXIT: fail code = %d\n", code);
  while (1) {
    *dcheck_exit = code;
  }
#else
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif
}

/* output a single character, to wherever the target wants to send it... */
void
libtarg_putc(char c)
{
#if defined(TARGET_HOST)
  fputc(c, stdout);
#elif defined(TARGET_SA)
  /* add to outbuf pool */
  if (__outbuf_ptr >= MAX_OUTBUF)
    libtarg_fail(1);
  __outbuf[__outbuf_ptr++] = c;
#elif defined(TARGET_HAHOST)
  // fputc(c, stdout);
  __hashval = libmin_fnv64a(&c, 1, __hashval);
#elif defined(TARGET_HASPIKE)
  // simple_putchar(c);
  __hashval = libmin_fnv64a(&c, 1, __hashval);
#elif defined(TARGET_SIMPLE) || defined(TARGET_SPIKE)
  simple_putchar(c);
#elif defined(TARGET_CVA6_DCHECK)
  *dcheck_putc = c;
#else
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif
}

#ifdef TARGET_SA
#define MAX_HEAP    (8*1024*1024)
static uint8_t __heap[MAX_HEAP];
static uint32_t __heap_ptr = 0;
#endif /* TARGET_SA */

#ifdef TARGET_HAHOST
#define MAX_HEAP    (8*1024*1024)
static uint8_t __heap[MAX_HEAP];
static uint32_t __heap_ptr = 0;
#endif /* TARGET_HAHOST */

#if defined(TARGET_SIMPLE) || defined(TARGET_SPIKE) || defined(TARGET_HASPIKE) || defined(TARGET_CVA6_DCHECK)
#define MAX_HEAP    (32*1024)
static uint8_t __heap[MAX_HEAP];
static uint32_t __heap_ptr = 0;
#endif /* TARGET_SIMPLE || TARGET_SPIKE */

/* get some memory */
void *
libtarg_sbrk(size_t inc)
{
#if defined(TARGET_HOST)
#if __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif /* __clang__ */
  return sbrk(inc);
#elif defined(TARGET_SA) || defined(TARGET_HAHOST) || defined(TARGET_SIMPLE) || defined(TARGET_SPIKE) || defined(TARGET_HASPIKE) || defined(TARGET_CVA6_DCHECK)
  uint8_t *ptr = &__heap[__heap_ptr];
  if (inc == 0)
    return ptr;
  
  __heap_ptr += inc;
  if (__heap_ptr >= MAX_HEAP)
    libtarg_fail(1);

  return ptr;
#else
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif
}

uint64_t cycles_total, instr_total, cycle_count, instret_count;

void libtarg_start_perf(){
#if defined(TARGET_CVA6_DCHECK)
#if defined(PROFILE) && PROFILE == 1
  __asm__ volatile("csrr %0, mcycle" : "=r"(cycle_count));
  __asm__ volatile("csrr %0, minstret" : "=r"(instret_count));
  cycles_total = -cycle_count;
  instr_total = -instret_count;
#endif
#endif
}

void libtarg_stop_perf(){
#if defined(TARGET_CVA6_DCHECK)
#if defined(PROFILE) && PROFILE == 1
  __asm__ volatile("csrr %0, mcycle" : "=r"(cycle_count));
  __asm__ volatile("csrr %0, minstret" : "=r"(instret_count));
#endif
  libmin_printf("bringup: perf finished.\n");
#if defined(PROFILE) && PROFILE == 1
  cycles_total += cycle_count;
  instr_total += instret_count;
  libmin_printf("tot_cycles = %d\n\rinstret = %d\n\r", cycles_total, instr_total);
#endif
#endif
}
