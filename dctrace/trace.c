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
        strcpy(traceFilename, "trace.bin");
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

    tracef = fopen(traceFilename, "rb");

    if (tracef == NULL) {
        printf("Can't open %s\n", traceFilename);
        exit(1);
    }

    
    uint32_t base_address = 0x8C000000;
    uint8_t address_buffer[3];
    uint8_t llu_length;
    int i;

    uint64_t reference = 0;
    while ((type = fgetc(tracef)) != EOF) { // Should be >, <, or EOF
        address_buffer[0] = fgetc(tracef);  
        address_buffer[1] = fgetc(tracef);
        address_buffer[2] = fgetc(tracef);

        // Build address
        address = base_address | ((address_buffer[2] << 16) | (address_buffer[1] << 8) | address_buffer[0]);

        currentCycle = 0;
        llu_length = fgetc(tracef);  // Grab the byte length of the number of cycles
        for(i = 0; i < llu_length; i++) {
            int byte = fgetc(tracef);
            currentCycle |= (uint64_t)byte << (8 * i);
        }

        // Delta decoding
        currentCycle += reference;
        reference = currentCycle;
        printf("%c0x%08x-%llu\n", type, address, currentCycle);

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
