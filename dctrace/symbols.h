/********************************************************************
 * File: symbols.h
 *
 * Symbols types and prototypes file.
 *
 * Author: M. Tim Jones <mtj@mtjones.com>
 * Edited: Andress A Barajas
 * 
 */

#ifndef __SYMBOLS_H
#define __SYMBOLS_H

#define MAX_FUNCTIONS		400
#define MAX_FUNCTION_NAME	50

/* Sets/Clears the function list and function matrix and other stuff */
void init(char *name, const char *path, int verb, double percent);  

/* Pass in the function address (e.g. 0x8c01053c) to get its index 
*  in the function list.
*/
int lookup_symbol(unsigned int address);

/* Add a function to the functions list. This also keeps track how 
*  many times a function has been called.
*/
void add_symbol(unsigned int address);

/* This helps us keep track of the total amount of cycles(time)
*  spent in a function when we exit(pop it off the stack).
*  This also keeps track on how much time spent in the function
*  from a specific function path (A() => B() & C() => B(); Time in 
*  B() could different depending on the parameters passed by A()
*  and C().
*/
void end_symbol(unsigned int address, unsigned long long int cycles);

/* This adds a function to the function matrix. Which keeps track of
*  the total number of times this function gets called by its 
*  parent function(the caller).
*/
void add_call_trace(unsigned int address);

/* This function takes all the info in the function list and the
*  function matrix and generates a Dot file that can be used
*  to create a visual graph of a program  */
void create_dot_file(void);

/* This utitlity method is used to keep track of total profile 
*  time.
*/
void calculate_total_profile_time(unsigned long long int cycle);

#endif /* __SYMBOLS_H */
