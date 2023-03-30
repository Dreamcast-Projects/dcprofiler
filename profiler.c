#include <stdio.h>
#include <stdlib.h>
#include <arch/timer.h>

#include <string.h>

#include "profiler.h"

/*
*  The code show below is by Moop and is in the public domain.  Its a module from the DreamHAL library
*  which can be found here: https://github.com/sega-dreamcast/dreamhal.  I included it here to make
*  it easier for the coder using this profiler (Just two files). I stripped away a LOT of very 
*  informative comments. Really check out his DreamHAL library
*/

// unsigned short pmcr_ctrl = PMCR_RUN_COUNTER | (reset_count << PMCR_RESET_COUNTER_SHIFT) | (count_type << PMCR_CLOCK_TYPE_SHIFT) | mode;

// These registers are 16 bits only and configure the performance counters
#define PMCR1_CTRL_REG 0xFF000084
#define PMCR2_CTRL_REG 0xFF000088

// These registers are 32-bits each and hold the high low parts of each counter
#define PMCTR1H_REG 0xFF100004
#define PMCTR1L_REG 0xFF100008

#define PMCTR2H_REG 0xFF10000C
#define PMCTR2L_REG 0xFF100010

#define PMCR_CLOCK_TYPE 0x0100
#define PMCR_CLOCK_TYPE_SHIFT 8

#define PMCR_STOP_COUNTER 0x2000
#define PMCR_RESET_COUNTER_SHIFT 13

#define PMCR_RUN_COUNTER 0xC000

#define PMCR_DISABLE_COUNTER 0x0000


// No 0x1b-0x20
#define PMCR_INSTRUCTION_CACHE_FILL_MODE            0x21 // Cycles
#define PMCR_OPERAND_CACHE_FILL_MODE                0x22 // Cycles
#define PMCR_ELAPSED_TIME_MODE                      0x23 // Cycles; For 200MHz CPU: 5ns per count in 1 cycle = 1 count mode, or around 417.715ps per count (increments by 12) in CPU/bus ratio mode
#define PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE    0x24 // Cycles
#define PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE    0x25 // Cycles
// No 0x26
#define PMCR_PIPELINE_FREEZE_BY_BRANCH_MODE         0x27 // Cycles
#define PMCR_PIPELINE_FREEZE_BY_CPU_REGISTER_MODE   0x28 // Cycles
#define PMCR_PIPELINE_FREEZE_BY_FPU_MODE            0x29 // Cycles

// counter can be made to run for 16.3 days.
#define PMCR_COUNT_CPU_CYCLES 0
// Likewise this uses the CPU/bus ratio method
#define PMCR_COUNT_RATIO_CYCLES 1

// These definitions are for the enable function and specify whether to reset
// a counter to 0 or to continue from where it left off
#define PMCR_CONTINUE_COUNTER 0
#define PMCR_RESET_COUNTER 1

// Clear counter and enable
static inline void PMCR_Init(unsigned char which, unsigned char mode, unsigned char count_type)
    __attribute__((no_instrument_function));

// Enable one or both of these "undocumented" performance counters.
static inline void PMCR_Enable(unsigned char which, unsigned char mode, unsigned char count_type, unsigned char reset_counter)
    __attribute__((no_instrument_function));

// Disable, clear, and re-enable with new mode (or same mode)
static inline void PMCR_Restart(unsigned char which, unsigned char mode, unsigned char count_type)
    __attribute__((no_instrument_function));

// Read a counter
// 48-bit value needs a 64-bit storage unit
// Return value of 0xffffffffffff means invalid 'which'
static inline unsigned long long int PMCR_Read(unsigned char which)
    __attribute__((no_instrument_function));

// Get a counter's current configuration
// Return value of 0xffff means invalid 'which'
static inline unsigned short PMCR_Get_Config(unsigned char which)
    __attribute__((no_instrument_function));

// Stop counter(s) (without clearing)
static inline void PMCR_Stop(unsigned char which)
    __attribute__((no_instrument_function));

// Disable counter(s) (without clearing)
static inline void PMCR_Disable(unsigned char which)
    __attribute__((no_instrument_function));

static unsigned char pmcr_enabled = 0;

static inline void PMCR_Init(unsigned char which, unsigned char mode, unsigned char count_type) // Will do nothing if perfcounter is already running!
{
		// counter 1
		PMCR_Enable(1, mode, count_type, PMCR_RESET_COUNTER);
}

// Enable "undocumented" performance counters (well, they were undocumented at one point. They're documented now!)
static inline void PMCR_Enable(unsigned char which, unsigned char mode, unsigned char count_type, unsigned char reset_count) // Will do nothing if perfcounter is already running!
{
	// Don't do anything if count_type or reset_count are invalid
	if((count_type | reset_count) > 1)
	{
		return;
	}

	// Build config from parameters
	unsigned short pmcr_ctrl = PMCR_RUN_COUNTER | (reset_count << PMCR_RESET_COUNTER_SHIFT) | (count_type << PMCR_CLOCK_TYPE_SHIFT) | mode;

	
		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = pmcr_ctrl;

		pmcr_enabled += 1;
	
}

// Reset counter to 0 and start it again
// NOTE: It does not appear to be possible to clear a counter while it is running.
static inline void PMCR_Restart(unsigned char which, unsigned char mode, unsigned char count_type)
{
	
 		// counter 1
		PMCR_Stop(1);
		PMCR_Enable(1, mode, count_type, PMCR_RESET_COUNTER);
 	
}

static inline unsigned long long int PMCR_Read(unsigned char which)
{
	
 		return (unsigned long long int)(*((volatile unsigned int*)PMCTR1H_REG) & 0xffff) << 32 | (unsigned long long int)(*((volatile unsigned int*)PMCTR1L_REG));
		
}

static inline unsigned short PMCR_Get_Config(unsigned char which)
{
	
		return *(volatile unsigned short*)PMCR1_CTRL_REG;
	
}

static inline void PMCR_Stop(unsigned char which)
{

		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_STOP_COUNTER;

		pmcr_enabled &= 0x2;
	
}

static inline void PMCR_Disable(unsigned char which)
{
		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_DISABLE_COUNTER;

		pmcr_enabled &= 0x2;
	
}

/***************************************  MY CODE  ***************************************/

static inline int __attribute__ ((no_instrument_function)) ptr_to_binary(void *ptr, unsigned char *buffer) {
	uint8_t *uint8ptr = (uint8_t*)(&ptr);
	// 0x8c123456 - Format of address of ptr
	// we dont care about base address portion 0x8c000000
	// dctrace rebuild it  (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]

	buffer[0] = uint8ptr[0]; // 56
	buffer[1] = uint8ptr[1]; // 34
	buffer[2] = uint8ptr[2]; // 12

	return 3;
}

static unsigned long long startTime;

/* Convert an unsigned long long to a binary format. We only care about the difference
   from the last timestamp value. Delta Encoding!
*/
static inline int __attribute__ ((no_instrument_function)) ull_to_binary(unsigned long long value, unsigned char *buffer) {
	int i, length;
	unsigned long long temp = value;
	uint8_t *uint8ptr = (uint8_t*)(&value);

	value -= startTime;
	startTime = temp;
	
    // Calculates the minimum number of bytes required to store the unsigned long long value value in binary format.
	length = (sizeof(unsigned long long) * 8 - __builtin_clzll(value) + 7) / 8;

	buffer[0] = length; // write the number of bytes to be able to decode the variable length

	for (i = 0; i < length; i++)
		buffer[i+1] = uint8ptr[i];

	return length+1;
}

void main_constructor(void)
	__attribute__ ((no_instrument_function, constructor));

void main_destructor(void)
	__attribute__ ((no_instrument_function, destructor));

void __cyg_profile_func_enter(void *, void *) 
    __attribute__ ((no_instrument_function));

void __cyg_profile_func_exit(void *, void *)
    __attribute__ ((no_instrument_function));

static int active = 0;
static int initialized = 0;
static FILE *fp = NULL;

#define BUFFER_SIZE (1024 * 8)  /* 8k buffer */

static unsigned char *ptr;
static unsigned char buffer[BUFFER_SIZE] = {0};
static size_t buffer_index;
static size_t print_amount;

void initializeProfiling(void) {
    initialized = 1;
}

void shutdownProfiling(void) {
	PMCR_Stop(1);
	// write the rest of the buffer
	write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

    initialized = 0;
    active = 0;
}

void startProfiling() {
    active = 1;
}

void stopProfiling() {
    active = 0;
}

void main_constructor(void) {
	ptr = buffer;
    buffer_index = 0;
	startTime = 0;
    
    fp = fopen("/pc/trace.bin", "wb");
    if(fp == NULL) {
        fprintf(stderr, "trace.bin file not opened\n");
        exit(-1);
    }

    PMCR_Init(1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
}

void main_destructor(void) {
	PMCR_Stop(1);
	// write the rest of the buffer
	write(fp->_file, buffer, buffer_index);

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

void __cyg_profile_func_enter(void *this, void *callsite) {
	//if(active && initialized)

	//uint64_t starttime = PMCR_Read(1);

	unsigned char* start = ptr;
	*ptr++ = '>' | 0b00000000;
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(PMCR_Read(1), ptr);
	buffer_index = ptr - buffer;
	print_amount = ptr - start;

	if(__builtin_expect((buffer_index+print_amount) >= BUFFER_SIZE, 0)) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}

	// uint64_t endtime = PMCR_Read(1);
	// printf("Timing in nanoseconds: %llu\n", endtime - starttime);
	// fflush(stdout);
}

void __cyg_profile_func_exit(void *this, void *callsite) {
    //if(active && initialized)

	unsigned char* start = ptr;
	*ptr++ = '<' | 0b00000000;
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(PMCR_Read(1), ptr);
	buffer_index = ptr - buffer;
	print_amount = ptr - start;

	if(__builtin_expect((buffer_index+print_amount) >= BUFFER_SIZE, 0)) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

