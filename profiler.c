#include <stdio.h>
#include <stdlib.h>

#include <kos/thread.h>

#include <arch/timer.h>

#include <dc/perf_monitor.h>

/*
 * Dreamcast Function Profiler (via -finstrument-functions)
 *
 * Writes 20-byte binary records to "/pc/trace.bin" on every function entry/exit.
 * Record format (per event):
 *   uint32_t flag_tid;     // Bit 31 = entry/exit, Bits 0–30 = thread ID
 *   uint32_t address;      // Function address
 *   uint32_t delta_time;   // Time since last event (ns)
 *   uint32_t delta_evt0;   // PRFC0 delta
 *   uint32_t delta_evt1;   // PRFC1 delta
 *
 * Startup:
 *   - Opens "trace.bin" for writing
 *   - Registers cleanup with atexit()
 *   - Starts PRFC0/PRFC1 counters
 *
 * On each instrumented function call:
 *   - Captures time and counters
 *   - Computes deltas vs last event
 *   - Buffers the record in an 8KB TLS buffer (flushed when full)
 *
 * Cleanup:
 *   - Flushes remaining records
 *   - Stops and clears counters
 *   - Closes file
 */

/* Use TLS to keep things separate */ 
#define thread_local _Thread_local

#define BUFFER_SIZE    (1024 * 8)

#define ENTRY_FLAG     0x80000000U
#define EXIT_FLAG      0x00000000U

static int fd;
static FILE *fp;
static mutex_t io_lock = MUTEX_INITIALIZER;

/* TLS buffer management */
static thread_local size_t  tls_buffer_idx;
static thread_local uint8_t tls_buffer[BUFFER_SIZE] __attribute__((aligned(32)));

/* TLS stats management */
static thread_local bool     tls_inited;
static thread_local uint32_t tls_thread_id;
static thread_local uint64_t tls_last_time;
static thread_local uint64_t tls_last_event0;
static thread_local uint64_t tls_last_event1;

typedef struct {
    uint32_t flag_tid;     /* Bit 31 = entry(1)/exit(0), Bits 0–30 = thread ID */
    uint32_t address;      /* Function address */
    uint32_t delta_time;   /* Delta nanoseconds */
    uint32_t delta_evt0;   /* Delta event0 */
    uint32_t delta_evt1;   /* Delta event1 */
} prof_record_t;

static void __attribute__ ((no_instrument_function)) init_tls(void) {
    kthread_t *th = thd_get_current();
    tls_thread_id = th->tid & 0x7FFFFFFFU; /* Reserve bit 31 for entry/exit */
    tls_buffer_idx = 0;
    tls_last_time = timer_ns_gettime64();
    tls_last_event0 = perf_cntr_count(PRFC0);
    tls_last_event1 = perf_cntr_count(PRFC1);

    tls_inited = true;
}

static void __attribute__ ((no_instrument_function, hot)) create_entry(void *this, uint32_t flag) {
    if(__unlikely(!tls_inited))
        init_tls();
    
    uint64_t now = timer_ns_gettime64();
    uint64_t e0  = perf_cntr_count(PRFC0);
    uint64_t e1  = perf_cntr_count(PRFC1);

    prof_record_t *entry = (void *)(tls_buffer + tls_buffer_idx);
    entry->flag_tid = flag | tls_thread_id;
    entry->address = (uint32_t)(this);
    entry->delta_time = (uint32_t)(now - tls_last_time);
    entry->delta_evt0 = (uint32_t)(e0  - tls_last_event0);
    entry->delta_evt1 = (uint32_t)(e1  - tls_last_event1);

    /* Advance this thread’s buffer */
    tls_buffer_idx += sizeof(prof_record_t);

    /* Update for next delta */
    tls_last_time = now;
    tls_last_event0 = e0;
    tls_last_event1 = e1;

    /* When this thread’s buffer is full, flush under lock */
    if(__unlikely(tls_buffer_idx >= BUFFER_SIZE - sizeof(prof_record_t))) {
        mutex_lock(&io_lock);
        write(fd, tls_buffer, tls_buffer_idx);
        mutex_unlock(&io_lock);

        tls_buffer_idx = 0;
    }
}

static void __attribute__ ((no_instrument_function)) cleanup(void) {
    if(tls_buffer_idx > 0) {
        mutex_lock(&io_lock);
        write(fd, tls_buffer, tls_buffer_idx);
        mutex_unlock(&io_lock);
    }
    
    perf_cntr_stop(PRFC0);
    perf_cntr_stop(PRFC1);

    perf_cntr_clear(PRFC0);
    perf_cntr_clear(PRFC1);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

void __attribute__ ((no_instrument_function, hot)) __cyg_profile_func_enter(void *this, void *callsite) {
    (void)callsite;

    if(__unlikely(fp == NULL))
        return;

    create_entry(this, ENTRY_FLAG);
}

void __attribute__ ((no_instrument_function, hot)) __cyg_profile_func_exit(void *this, void *callsite) {
    (void)callsite;

    if(__unlikely(fp == NULL))
        return;

    create_entry(this, EXIT_FLAG);
}

void __attribute__ ((no_instrument_function, constructor)) main_constructor(void) {
    fp = fopen("/pc/trace.bin", "wb");
    if(fp == NULL) {
        fprintf(stderr, "trace.bin file not opened\n");
        return;
    }

    fd = fileno(fp);

    /* Cleanup at exit */
    atexit(cleanup);

    /* Start performance counters */
    perf_cntr_timer_disable();
    perf_cntr_clear(PRFC0);
    perf_cntr_clear(PRFC1);
    perf_cntr_start(PRFC0, PMCR_OPERAND_CACHE_MISS_MODE, PMCR_COUNT_CPU_CYCLES);
    perf_cntr_start(PRFC1, PMCR_INSTRUCTION_CACHE_MISS_MODE, PMCR_COUNT_CPU_CYCLES);
}
