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
void print_progress_bar(int progress, int bar_length);
#define AVAILABLE_OPTIONS  ":t:a:p:vh"

int main(int argc, char *argv[])
{
    FILE *tracef;
    char type;
    unsigned int address;
    unsigned long long int currentCycle;
    unsigned long long int startCycle;

    double bytesRead = 0;
    int progress = -1;
    unsigned long long int fileSize;
    

    /* Customize commandline args */
    int verbose = 0;
    double percentage = 0;
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
        case 'p':
            if (sscanf (optarg, "%lf", &percentage) != 1) {
                fprintf(stderr, "Percentage needs a double value between 0-100.\n");
            }
            if(percentage < 0)
                percentage = 0;
            if(percentage > 100)
                percentage = 100;

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

    init(progName, addr2linePath, verbose, percentage);
    stack_init();

    tracef = fopen(traceFilename, "rb");

    if (tracef == NULL) {
        printf("Can't open %s\n", traceFilename);
        exit(1);
    }

    fseek(tracef, 0, SEEK_END); // seek to end of file
    fileSize = ftell(tracef); // get current file pointer
    fseek(tracef, 0, SEEK_SET); // seek back to beginning of file
    
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

        bytesRead += 5 + llu_length;

        if(progress != (int)((bytesRead/fileSize)*100))
        {
            progress = (int)((bytesRead/fileSize)*100);
            print_progress_bar(progress, 50);
        }

        // Delta decoding
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
    printf("-t <filename>   Set trace file to <filename> (default: trace.txt)\n");
    printf("-a <filepath>   Set sh-elf-addr2line filepath to <filepath>\n");
    printf("-p <percentage> Set percentage threshold. Every function under this threshold\n");
    printf("                will not show up in the dot file (default: 0; 0-100 range)\n");
    printf("                default: /opt/toolchains/dc/sh-elf/bin/sh-elf-addr2line)\n");
    printf("-v              Verbose\n");
    printf("-h              Usage information (you\'re looking at it)\n\n");
}

void print_progress_bar(int progress, int bar_length) {
    printf("\r["); // Move the cursor to the beginning of the line

    int filled_length = (int)((float)progress / 100 * bar_length);
    for (int i = 0; i < bar_length; ++i) {
        if (i < filled_length) {
            printf("#");
        }
        else {
            printf("-");
        }
    }

    printf("] %d%%", progress);

    if(progress == 100) 
        printf("\n");

    fflush(stdout); // Force the output to be written immediately
}
