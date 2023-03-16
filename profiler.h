#ifndef __PROFILING_H__
#define __PROFILING_H__

#ifdef __cplusplus
extern "C" {
#endif

void initializeProfiling() __attribute__((no_instrument_function));

void shutdownProfiling() __attribute__((no_instrument_function));

void startProfiling() __attribute__((no_instrument_function));

void stopProfiling() __attribute__((no_instrument_function));

#ifdef __cplusplus
}
#endif

#endif