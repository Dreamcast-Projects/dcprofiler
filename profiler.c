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

// --- Performance Counter Event Code Definitions ---
//
// Interestingly enough, it so happens that the SEGA Dreamcast's CPU seems to
// contain the same performance counter functionality as SH4 debug adapters for
// the SH7750R. Awesome!
//

//                MODE DEFINITION                  VALUE   MEASURMENT TYPE & NOTES
#define PMCR_INIT_NO_MODE                           0x00 // None; Just here to be complete
#define PMCR_OPERAND_READ_ACCESS_MODE               0x01 // Quantity; With cache
#define PMCR_OPERAND_WRITE_ACCESS_MODE              0x02 // Quantity; With cache
#define PMCR_UTLB_MISS_MODE                         0x03 // Quantity
#define PMCR_OPERAND_CACHE_READ_MISS_MODE           0x04 // Quantity
#define PMCR_OPERAND_CACHE_WRITE_MISS_MODE          0x05 // Quantity
#define PMCR_INSTRUCTION_FETCH_MODE                 0x06 // Quantity; With cache
#define PMCR_INSTRUCTION_TLB_MISS_MODE              0x07 // Quantity
#define PMCR_INSTRUCTION_CACHE_MISS_MODE            0x08 // Quantity
#define PMCR_ALL_OPERAND_ACCESS_MODE                0x09 // Quantity
#define PMCR_ALL_INSTRUCTION_FETCH_MODE             0x0a // Quantity
#define PMCR_ON_CHIP_RAM_OPERAND_ACCESS_MODE        0x0b // Quantity
// No 0x0c
#define PMCR_ON_CHIP_IO_ACCESS_MODE                 0x0d // Quantity
#define PMCR_OPERAND_ACCESS_MODE                    0x0e // Quantity; With cache, counts both reads and writes
#define PMCR_OPERAND_CACHE_MISS_MODE                0x0f // Quantity
#define PMCR_BRANCH_ISSUED_MODE                     0x10 // Quantity; Not the same as branch taken!
#define PMCR_BRANCH_TAKEN_MODE                      0x11 // Quantity
#define PMCR_SUBROUTINE_ISSUED_MODE                 0x12 // Quantity; Issued a BSR, BSRF, JSR, JSR/N
#define PMCR_INSTRUCTION_ISSUED_MODE                0x13 // Quantity
#define PMCR_PARALLEL_INSTRUCTION_ISSUED_MODE       0x14 // Quantity
#define PMCR_FPU_INSTRUCTION_ISSUED_MODE            0x15 // Quantity
#define PMCR_INTERRUPT_COUNTER_MODE                 0x16 // Quantity
#define PMCR_NMI_COUNTER_MODE                       0x17 // Quantity
#define PMCR_TRAPA_INSTRUCTION_COUNTER_MODE         0x18 // Quantity
#define PMCR_UBC_A_MATCH_MODE                       0x19 // Quantity
#define PMCR_UBC_B_MATCH_MODE                       0x1a // Quantity
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

//
// --- Performance Counter Miscellaneous Definitions ---
//
// For convenience; assume stock bus clock of 99.75MHz
// (Bus clock is the external CPU clock, not the peripheral bus clock)
//

#define PMCR_SH4_CPU_FREQUENCY 199500000
#define PMCR_CPU_CYCLES_MAX_SECONDS 1410902
#define PMCR_SH4_BUS_FREQUENCY 99750000
#define PMCR_SH4_BUS_FREQUENCY_SCALED 2394000000 // 99.75MHz x 24
#define PMCR_BUS_RATIO_MAX_SECONDS 117575

// Clear counter and enable
void PMCR_Init(unsigned char which, unsigned char mode, unsigned char count_type)
    __attribute__((no_instrument_function));

// Enable one or both of these "undocumented" performance counters.
void PMCR_Enable(unsigned char which, unsigned char mode, unsigned char count_type, unsigned char reset_counter)
    __attribute__((no_instrument_function));

// Disable, clear, and re-enable with new mode (or same mode)
void PMCR_Restart(unsigned char which, unsigned char mode, unsigned char count_type)
    __attribute__((no_instrument_function));

// Read a counter
// 48-bit value needs a 64-bit storage unit
// Return value of 0xffffffffffff means invalid 'which'
unsigned long long int PMCR_Read(unsigned char which)
    __attribute__((no_instrument_function));

// Get a counter's current configuration
// Return value of 0xffff means invalid 'which'
unsigned short PMCR_Get_Config(unsigned char which)
    __attribute__((no_instrument_function));

// Stop counter(s) (without clearing)
void PMCR_Stop(unsigned char which)
    __attribute__((no_instrument_function));

// Disable counter(s) (without clearing)
void PMCR_Disable(unsigned char which)
    __attribute__((no_instrument_function));

static unsigned char pmcr_enabled = 0;

void PMCR_Init(unsigned char which, unsigned char mode, unsigned char count_type) // Will do nothing if perfcounter is already running!
{
	// Don't do anything if being asked to enable an already-enabled counter
	if( (which == 1) && ((!pmcr_enabled) || (pmcr_enabled == 2)) )
	{
		// counter 1
		PMCR_Enable(1, mode, count_type, PMCR_RESET_COUNTER);
	}
	else if( (which == 2) && ((!pmcr_enabled) || (pmcr_enabled == 1)) )
	{
		// counter 2
		PMCR_Enable(2, mode, count_type, PMCR_RESET_COUNTER);
	}
	else if( (which == 3) && (!pmcr_enabled) )
	{
		// Both
		PMCR_Enable(3, mode, count_type, PMCR_RESET_COUNTER);
	}
}

// Enable "undocumented" performance counters (well, they were undocumented at one point. They're documented now!)
void PMCR_Enable(unsigned char which, unsigned char mode, unsigned char count_type, unsigned char reset_count) // Will do nothing if perfcounter is already running!
{
	// Don't do anything if count_type or reset_count are invalid
	if((count_type | reset_count) > 1)
	{
		return;
	}

	// Build config from parameters
	unsigned short pmcr_ctrl = PMCR_RUN_COUNTER | (reset_count << PMCR_RESET_COUNTER_SHIFT) | (count_type << PMCR_CLOCK_TYPE_SHIFT) | mode;

	// Don't do anything if being asked to enable an already-enabled counter
	if( (which == 1) && ((!pmcr_enabled) || (pmcr_enabled == 2)) )
	{
		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = pmcr_ctrl;

		pmcr_enabled += 1;
	}
	else if( (which == 2) && ((!pmcr_enabled) || (pmcr_enabled == 1)) )
	{
		// counter 2
		*((volatile unsigned short*)PMCR2_CTRL_REG) = pmcr_ctrl;

		pmcr_enabled += 2;
	}
	else if( (which == 3) && (!pmcr_enabled) )
	{
		// Both
		*((volatile unsigned short*)PMCR1_CTRL_REG) = pmcr_ctrl;
		*((volatile unsigned short*)PMCR2_CTRL_REG) = pmcr_ctrl;

		pmcr_enabled = 3;
	}
}

// Reset counter to 0 and start it again
// NOTE: It does not appear to be possible to clear a counter while it is running.
void PMCR_Restart(unsigned char which, unsigned char mode, unsigned char count_type)
{
	if( (which == 1) && (pmcr_enabled & 0x1) )
 	{
 		// counter 1
		PMCR_Stop(1);
		PMCR_Enable(1, mode, count_type, PMCR_RESET_COUNTER);
 	}
	else if( (which == 2) && (pmcr_enabled & 0x2) )
 	{
 		// counter 2
		PMCR_Stop(2);
		PMCR_Enable(2, mode, count_type, PMCR_RESET_COUNTER);
 	}
	else if( (which == 3) && (pmcr_enabled == 3) )
 	{
		// Both
		PMCR_Stop(3);
		PMCR_Enable(3, mode, count_type, PMCR_RESET_COUNTER);
 	}
}

// For reference:
// #define PMCTR1H_REG 0xFF100004
// #define PMCTR1L_REG 0xFF100008

// #define PMCTR2H_REG 0xFF10000C
// #define PMCTR2L_REG 0xFF100010

// Sorry, can only read one counter at a time!
// Return value of 0xffffffffffff means invalid 'which'
unsigned long long int PMCR_Read(unsigned char which)
{
	// if a counter is disabled, it will just return 0

	union _union_32_and_64 {
		unsigned int output32[2];
		unsigned long long int output64;
	} output_value = {0};

	// Note: These reads really do need to be done in assembly: unfortunately it
	// appears that using C causes GCC to insert a branch right smack in between
	// the high and low reads of perf counter 2 (with a nop, so it's literally
	// delaying the reads by several cycles!), which is totally insane. Doing it
	// the assembly way ensures that nothing ridiculous like that happens. It's
	// also portable between versions of GCC that do put the nonsensical branch in.
	//
	// One thing that would be nice is if SH4 had the movi20s instruction to make
	// absolute addresses in 3 cycles, but only the SH2A has that... :(

	if(which == 1)
	{
		output_value.output32[1] = PMCTR1H_REG;
		output_value.output32[0] = PMCTR1L_REG;

		// counter 1
 		// output value = (unsigned long long int)(*((volatile unsigned int*)PMCTR1H_REG) & 0xffff) << 32 | (unsigned long long int)(*((volatile unsigned int*)PMCTR1L_REG));
		asm volatile (
			"mov.l @%[reg1h],%[reg1h]\n\t" // read counter (high)
			"mov.l @%[reg1l],%[reg1l]\n\t" // read counter (low)
			"extu.w %[reg1h],%[reg1h]\n" // zero-extend high, aka high & 0xffff
			: [reg1h] "+&r" (output_value.output32[1]), [reg1l] "+r" (output_value.output32[0])
			: // no inputs
			: // no clobbers
		);
	}
	else if(which == 2)
	{
		output_value.output32[1] = PMCTR2H_REG;
		output_value.output32[0] = PMCTR2L_REG;

		// counter 2
		// output value = (unsigned long long int)(*((volatile unsigned int*)PMCTR2H_REG) & 0xffff) << 32 | (unsigned long long int)(*((volatile unsigned int*)PMCTR2L_REG));
		asm volatile (
			"mov.l @%[reg2h],%[reg2h]\n\t" // read counter (high)
			"mov.l @%[reg2l],%[reg2l]\n\t" // read counter (low)
			"extu.w %[reg2h],%[reg2h]\n" // zero-extend high, aka high & 0xffff
			: [reg2h] "+&r" (output_value.output32[1]), [reg2l] "+r" (output_value.output32[0])
			: // no inputs
			: // no clobbers
		);
	}
	else // Invalid
	{
		output_value.output64 = 0xffffffffffff;
	}

	return output_value.output64;
}

// Get a counter's current configuration
// Can only get the config for one counter at a time.
// Return value of 0xffff means invalid 'which'
unsigned short PMCR_Get_Config(unsigned char which)
{
	if(which == 1)
	{
		return *(volatile unsigned short*)PMCR1_CTRL_REG;
	}
	else if(which == 2)
	{
		return *(volatile unsigned short*)PMCR2_CTRL_REG;
	}
	else // Invalid
	{
		return 0xffff;
	}
}

// Clearing only works when the counter is disabled. Otherwise, stopping the
// counter via setting the 0x2000 bit holds the data in the data registers,
// whereas disabling without setting that bit reads back as all 0 (but doesn't
// clear the counters for next start). This function just stops a running
// counter and does nothing if the counter is already stopped or disabled, as
// clearing is handled by PMCR_Enable().
void PMCR_Stop(unsigned char which)
{
	if( (which == 1) && (pmcr_enabled & 0x1) )
	{
		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_STOP_COUNTER;

		pmcr_enabled &= 0x2;
	}
	else if( (which == 2) && (pmcr_enabled & 0x2) )
	{
		// counter 2
		*((volatile unsigned short*)PMCR2_CTRL_REG) = PMCR_STOP_COUNTER;

		pmcr_enabled &= 0x1;
	}
	else if( (which == 3) && (pmcr_enabled == 3) )
	{
		// Both
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_STOP_COUNTER;
		*((volatile unsigned short*)PMCR2_CTRL_REG) = PMCR_STOP_COUNTER;

		pmcr_enabled = 0;
	}
}

// Note that disabling does NOT clear the counter.
// It may appear that way because reading a disabled counter returns 0, but re-
// enabling without first clearing will simply continue where it left off.
void PMCR_Disable(unsigned char which)
{
	if(which == 1)
	{
		// counter 1
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_DISABLE_COUNTER;

		pmcr_enabled &= 0x2;
	}
	else if(which == 2)
	{
		// counter 2
		*((volatile unsigned short*)PMCR2_CTRL_REG) = PMCR_DISABLE_COUNTER;

		pmcr_enabled &= 0x1;
	}
	else if(which == 3)
	{
		// Both
		*((volatile unsigned short*)PMCR1_CTRL_REG) = PMCR_DISABLE_COUNTER;
		*((volatile unsigned short*)PMCR2_CTRL_REG) = PMCR_DISABLE_COUNTER;

		pmcr_enabled = 0;
	}
}

/***************************************  MY CODE  ***************************************/

static int __attribute__ ((no_instrument_function)) ptr_to_binary(void *ptr, unsigned char *buffer) {
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
static int __attribute__ ((no_instrument_function)) ull_to_binary(unsigned long long value, unsigned char *buffer) {
	int i, length = 0;
	uint8_t *uint8ptr = (uint8_t*)(&value);
	unsigned long long temp = value;

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

	// uint64_t endtime = PMCR_Read(1);
	// printf("Timing in nanoseconds: %llu\n", endtime - starttime);
	// fflush(stdout);

	if((buffer_index+print_amount) >= BUFFER_SIZE) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

void __cyg_profile_func_exit(void *this, void *callsite) {
    //if(active && initialized)

	unsigned char* start = ptr;
	*ptr++ = '<' | 0b00000000;
	ptr += ptr_to_binary(this, ptr);
	ptr += ull_to_binary(PMCR_Read(1), ptr);
	buffer_index = ptr - buffer;
	print_amount = ptr - start;

	if((buffer_index+print_amount) >= BUFFER_SIZE) {
		write(fp->_file, buffer, buffer_index);
		buffer_index = 0;
		ptr = buffer;
	}
}

