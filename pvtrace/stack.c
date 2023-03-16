/********************************************************************
 * File: stack.c
 *
 * Simple stack implementation.
 *
 * Author: M. Tim Jones <mtj@mtjones.com>
 * Edited: Andress A Barajas
 * 
 */

#include <assert.h>

#define MAX_ELEMENTS	100

typedef struct {
    unsigned int address;                /* Address of function */
    unsigned long long int startCycle;   /* The cycle(time) this function started on */
} stack_func_t;

static int index;
static stack_func_t stack[MAX_ELEMENTS];

void stack_init(void) {
    index = 0;
}

int stack_num_elems(void) {
    return index;
}

unsigned int stack_top_address(void) {
    assert(index > 0);
    return (stack[index-1].address);
}

void stack_push(unsigned int address, unsigned long long int startCycle) {
    assert (index < MAX_ELEMENTS);

    stack[index].address = address;
    stack[index].startCycle = startCycle;
    index++;

    return;
}

unsigned long long int stack_pop_start_cycle(void) {
    unsigned long long int value;

    assert(index > 0);

    index--;
    value = stack[index].startCycle;

    return value;
}

