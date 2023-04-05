#include <stdio.h>
#include <stdlib.h>
#include <arch/timer.h>

#include "profiler.h"

/*
 * Compression Algorithm Summary:
 * 
 * This algorithm is designed to output compressed binary entries to a 'trace.bin' file,
 * leveraging the -finstrument-functions functionality of GCC for function
 * instrumentation.
 *
 * Each entry in the output file has the following format:
 *   1. A '<' or '>' character indicating whether the function was entered or exited.
 *   2. A 3-byte address representing the function's address in memory.
 *   3. A 1-byte length field specifying the number of following bytes.
 *   4. The following bytes, representing the delta-encoded cycle timestamp of
 *      entering or leaving the function.
 *
 * The main purpose of this algorithm is to reduce the overhead of function
 * instrumentation and to provide a compact representation of function call traces,
 * allowing for efficient storage and processing of the collected data using 'dctrace'.
 *
 * Delta compression is used for the cycle timestamps to further reduce the size
 * of the output file. This compression method encodes timestamps as the difference
 * between the current timestamp and the previous one, resulting in smaller values
 * that often require fewer bytes to represent.
 */

#define LIKELY(exp)    __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp)  __builtin_expect(!!(exp), 0)

#define MAX_ENTRY_SIZE 13 // > or < + 3 for address + ~9 max for cycle count
#define MAGIC_NUMBER   71 // (sizeof(uint64_t) * 8 + 7) => 8*8+7 => 71

#define BUFFER_SIZE    (1024 * 8)  // 8k buffer

static uint64_t last_time;

static FILE *fp;
static uint8_t *ptr;
static size_t buffer_index;
static uint8_t buffer[BUFFER_SIZE] __attribute__((aligned(32)));

static inline int __attribute__ ((no_instrument_function, hot)) ptr_to_binary(void *address, unsigned char *buffer) {
	uint8_t *uint8ptr = (uint8_t*)(&address);
	// 0x8c 12 34 56 - Format of address of ptr
	// we dont care about base address portion 0x8c000000
	// dctrace rebuild it  (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]

	buffer[0] = uint8ptr[0]; // 56
	buffer[1] = uint8ptr[1]; // 34
	buffer[2] = uint8ptr[2]; // 12

	return 3;
}

// Convert an uint64_t to a binary format. We only care about the difference
// from the last timestamp value. Delta Encoding!
static inline int __attribute__ ((no_instrument_function, hot)) ull_to_binary(uint64_t timestamp, unsigned char *buffer) {
	int i, length;
	uint64_t temp = timestamp;
	uint8_t *uint8ptr = (uint8_t*)(&timestamp);

	timestamp -= last_time;
	last_time = temp;
	
    // Calculates the minimum number of bytes required to store the uint64_t value value in binary format.
	// (sizeof(uint64_t) * 8 - __builtin_clzll(timestamp) + 7) / 8;
	length = (MAGIC_NUMBER - __builtin_clzll(timestamp)) >> 3;

	buffer[0] = length; // Write the number of bytes to be able to decode the variable length

	for (i = 0; LIKELY(i < length); i++)
		buffer[i+1] = uint8ptr[i];

	return length+1;
}

void shutdown_profiling(void) {
	if(buffer_index > 0)
		write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

void __attribute__ ((no_instrument_function, hot)) __cyg_profile_func_enter(void *this, void *callsite) {
	if(UNLIKELY(fp == NULL))
		return;

	//uint64_t start_time = perf_cntr_count(0);

	*ptr++ = 0b00111110; // '>'
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(perf_cntr_count(0), ptr);
	buffer_index = ptr - buffer;

	// uint64_t end_time = perf_cntr_count(0);
	// printf("Timing in cycles: %llu\n", end_time - start_time);
	// fflush(stdout);

	if(UNLIKELY((buffer_index+MAX_ENTRY_SIZE) >= BUFFER_SIZE)) {
		uint64_t start_time = perf_cntr_count(0);
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
		uint64_t end_time = perf_cntr_count(0);
		printf("Timing in cycles: %llu\n", end_time - start_time);
		fflush(stdout);
	}
}

void __attribute__ ((no_instrument_function, hot)) __cyg_profile_func_exit(void *this, void *callsite) {
	if(UNLIKELY(fp == NULL))
		return;

	*ptr++ = 0b00111100; //'<'
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(perf_cntr_count(0), ptr);
	buffer_index = ptr - buffer;

	if(UNLIKELY((buffer_index+MAX_ENTRY_SIZE) >= BUFFER_SIZE)) {
		uint64_t start_time = perf_cntr_count(0);
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
		uint64_t end_time = perf_cntr_count(0);
		printf("Timing in cycles: %llu\n", end_time - start_time);
		fflush(stdout);
	}
}

void __attribute__ ((no_instrument_function, constructor)) main_constructor(void) {
	ptr = buffer;
    buffer_index = 0;
	last_time = 0;
    
    fp = fopen("/pc/trace.bin", "wb");
    if(fp == NULL)
        fprintf(stderr, "trace.bin file not opened\n");
}

void __attribute__ ((no_instrument_function, destructor)) main_destructor(void) {
	if(buffer_index > 0)
		write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}
