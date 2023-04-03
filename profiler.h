#ifndef __PROFILING_H__
#define __PROFILING_H__

#ifdef __cplusplus
extern "C" {
#endif

void shutdown_profiling() __attribute__((no_instrument_function));

#ifdef __cplusplus
}
#endif

#endif