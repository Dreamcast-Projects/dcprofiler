/********************************************************************
 * File: trace.c
 *
 * Main function for the pvtrace utility.
 *
 * Author: M. Tim Jones <mtj@mtjones.com>
 * Edited: Andress A Barajas
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "stack.h"
#include "symbols.h"

void usage(void);
#define AVAILABLE_OPTIONS  ":t:a:vh"

int main(int argc, char *argv[])
{
    FILE *tracef;
    char type;
    unsigned int address;
    unsigned long long int currentCycle;
    unsigned long long int startCycle;

    /* Customize commandline args */
    int verbose = 0;
    char progName[256] = {0};
    char traceFilename[256] = {0};
    char addr2linePath[256] = {0};

    int opt;

    if (argc < 2) {
        usage();
        exit(1);
    }

    while ((opt = getopt(argc, argv, AVAILABLE_OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            strncpy(traceFilename, optarg, strlen(optarg));
            break;
        case 'a':
            strncpy(addr2linePath, optarg, strlen(optarg));
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            usage();
            exit(0);
        case '?':
            printf("Unknown option: %c\n", optopt);
            usage();
            exit(1);
            break;
        case ':':
            printf("Missing arg for %c\n", optopt);
            usage();
            exit(1);
            break;
        }
    }

    /* Default values if not set */
    if(!traceFilename[0])
        strcpy(traceFilename, "trace.txt");
    if(!addr2linePath[0])
        strcpy(addr2linePath, "/opt/toolchains/dc/sh-elf/bin/sh-elf-addr2line");

    if (optind == (argc - 1))
        strncpy(progName, argv[optind], strlen(argv[optind]));
    else {
        usage();
        exit(1);
    }

    init(progName, addr2linePath, verbose);
    stack_init();

    tracef = fopen(traceFilename, "r");

    if (tracef == NULL) {
        printf("Can't open %s\n", traceFilename);
        exit(1);
    }

    uint64_t reference = 0;
    uint32_t base_address = 0x8C000000;
    while (!feof(tracef)) {
        fscanf(tracef, "%c%6x%llu", &type, &address, &currentCycle);
        address = base_address | address;

        currentCycle += reference;
        reference = currentCycle;

        calculate_total_profile_time(currentCycle);

        if(type == '>') {
            /* Function Entry */
            add_symbol(address);
            add_call_trace(address);
            stack_push(address, currentCycle);
        } else if (type == '<') {
            /* Function Exit */
            startCycle = stack_pop_start_cycle();
            end_symbol(address, currentCycle - startCycle);
        }
    }

    fclose(tracef);

    /* Check if there are still functions in the stack. 
       Give them the last cycle recorded.
    */
    while(stack_num_elems() > 0) {
        address = stack_top_address();
        startCycle = stack_pop_start_cycle();
        end_symbol(address, currentCycle - startCycle);
    }

    create_dot_file();
    
    return 0;
}

void usage(void) {
    printf("Usage: pvtrace [OPTIONS] <program.elf>\n");
    printf("Requires: A trace.txt, sh-elf-addr2line \n\n");
    printf("OPTIONS:\n");
    printf("-t <filename> Set trace file to <filename> (default: trace.txt)\n");
    printf("-a <filepath> Set sh-elf-addr2line filepath to <filepath>\n");
    printf("              default: /opt/toolchains/dc/sh-elf/bin/sh-elf-addr2line)\n");
    printf("-v            Verbose\n");
    printf("-h            Usage information (you\'re looking at it)\n\n");
}
