#include <stdio.h>
#include <stdlib.h>
#include <arch/timer.h>

#include "profiler.h"

#define LIKELY(exp)    __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp)  __builtin_expect(!!(exp), 0)

#define MAX_ENTRY_SIZE 13 // > or < + 3 for address + ~9 for cycle count
#define MAGIC_NUMBER   71 // (sizeof(uint64_t) * 8 + 7) => 8*8+7 => 71

#define BUFFER_SIZE    (1024 * 8)  // 8k buffer

static uint64_t last_time;

static FILE *fp;
static uint8_t *ptr;
static size_t buffer_index;
static uint8_t buffer[BUFFER_SIZE] __attribute__((aligned(32)));

static inline int __attribute__ ((no_instrument_function)) ptr_to_binary(void *address, unsigned char *buffer) {
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
static inline int __attribute__ ((no_instrument_function)) ull_to_binary(uint64_t timestamp, unsigned char *buffer) {
	int i, length;
	uint64_t temp = timestamp;
	uint8_t *uint8ptr = (uint8_t*)(&timestamp);

	timestamp -= last_time;
	last_time = temp;
	
    // Calculates the minimum number of bytes required to store the uint64_t value value in binary format.
	// (sizeof(uint64_t) * 8 - __builtin_clzll(timestamp) + 7) / 8;
	length = (MAGIC_NUMBER - __builtin_clzll(timestamp)) >> 3;

	buffer[0] = length; // Write the number of bytes to be able to decode the variable length

	for (i = 0; i < length; i++)
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

void __attribute__ ((no_instrument_function)) __cyg_profile_func_enter(void *this, void *callsite) {
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
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

void __attribute__ ((no_instrument_function)) __cyg_profile_func_exit(void *this, void *callsite) {
	if(UNLIKELY(fp == NULL))
		return;

	*ptr++ = 0b00111100; //'<'
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(perf_cntr_count(0), ptr);
	buffer_index = ptr - buffer;

	if(UNLIKELY((buffer_index+MAX_ENTRY_SIZE) >= BUFFER_SIZE)) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
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
