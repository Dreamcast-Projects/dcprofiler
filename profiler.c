#include <stdio.h>
#include <stdlib.h>
#include <arch/timer.h>

#include "profiler.h"

#define LIKELY(exp) __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

#define BUFFER_SIZE (1024 * 8)  /* 8k buffer */

static FILE *fp = NULL;
static unsigned char *ptr;
static unsigned char buffer[BUFFER_SIZE] __attribute__((aligned(32)));
static size_t buffer_index;
static size_t print_amount;

static unsigned long long startTime;

static inline int __attribute__ ((no_instrument_function)) ptr_to_binary(void *address, unsigned char *buffer) {
	uint8_t *uint8ptr = (uint8_t*)(&address);
	// 0x8c123456 - Format of address of ptr
	// we dont care about base address portion 0x8c000000
	// dctrace rebuild it  (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]

	buffer[0] = uint8ptr[0]; // 56
	buffer[1] = uint8ptr[1]; // 34
	buffer[2] = uint8ptr[2]; // 12

	return 3;
}

// Convert an unsigned long long to a binary format. We only care about the difference
// from the last timestamp value. Delta Encoding!
static inline int __attribute__ ((no_instrument_function)) ull_to_binary(unsigned long long timestamp, unsigned char *buffer) {
	int i, length;
	unsigned long long temp = timestamp;
	uint8_t *uint8ptr = (uint8_t*)(&timestamp);

	timestamp -= startTime;
	startTime = temp;
	
    // Calculates the minimum number of bytes required to store the unsigned long long value value in binary format.
	length = (sizeof(unsigned long long) * 8 - __builtin_clzll(timestamp) + 7) / 8;

	buffer[0] = length; // Write the number of bytes to be able to decode the variable length

	for (i = 0; i < length; i++)
		buffer[i+1] = uint8ptr[i];

	return length+1;
}

void shutdown_profiling(void) {

	// Write the rest of the buffer
	if(buffer_index > 0)
		write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

void __attribute__ ((no_instrument_function)) __cyg_profile_func_enter(void *this, void *callsite) {
	if(UNLIKELY(fp == NULL))
		return;

	uint64_t start_time = perf_cntr_count(0);

	unsigned char* start = ptr;
	*ptr++ = '>' | 0b00000000;
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(start_time, ptr);
	buffer_index = ptr - buffer;
	print_amount = ptr - start;

	// uint64_t end_time = perf_cntr_count(0);
	// printf("Timing in cycles: %llu\n", end_time - start_time);
	// fflush(stdout);

	if(UNLIKELY((buffer_index+print_amount) >= BUFFER_SIZE)) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

void __attribute__ ((no_instrument_function)) __cyg_profile_func_exit(void *this, void *callsite) {
	if(UNLIKELY(fp == NULL))
		return;

	uint64_t end_time = perf_cntr_count(0);

	unsigned char* start = ptr;
	*ptr++ = '<' | 0b00000000;
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(end_time, ptr);
	buffer_index = ptr - buffer;
	print_amount = ptr - start;

	if(UNLIKELY((buffer_index+print_amount) >= BUFFER_SIZE)) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

void __attribute__ ((no_instrument_function, constructor)) main_constructor(void) {
	ptr = buffer;
    buffer_index = 0;
	startTime = 0;
    
    fp = fopen("/pc/trace.bin", "wb");
    if(fp == NULL) {
        fprintf(stderr, "trace.bin file not opened\n");
    }
}

void __attribute__ ((no_instrument_function, destructor)) main_destructor(void) {

	// Write whatever is left in the buffer
	if(buffer_index > 0)
		write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}
