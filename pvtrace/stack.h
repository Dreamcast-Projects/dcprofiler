/********************************************************************
 * File: stack.h
 *
 * Simple stack implementation header.
 *
 * Author: M. Tim Jones <mtj@mtjones.com>
 * Edited: Andress A Barajas
 * 
 */

#ifndef __STACK_H
#define __STACK_H

void stack_init(void);

int stack_num_elems(void);

/* Returns the top element(address) of the stack */
unsigned int stack_top_address(void);

/* Push the address of a function as well as the cycle(time)
*  it started.
*/
void stack_push(unsigned int address, unsigned long long int start_cycle);

/* Pop a function off the top of the stack and return the 
*  cycle(time) it started.
*/
unsigned long long int stack_pop_start_cycle(void);

#endif /* __STACK_H */

