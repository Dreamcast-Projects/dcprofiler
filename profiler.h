#ifndef __PROFILING_H__
#define __PROFILING_H__

#ifdef __cplusplus
extern "C" {
#endif

uint64_t timer_ns_gettime64() __attribute__((no_instrument_function));

void shutdown_profiling() __attribute__((no_instrument_function));

#ifdef __cplusplus
}
#endif

#endif