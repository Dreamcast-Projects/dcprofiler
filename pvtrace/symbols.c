/********************************************************************
 * File: symbols.c
 *
 * Symbols functions.  This file has functions for symbols mgmt
 *  (such as translating addresses to function names with 
 *  addr2line) and also connectivity matrix functions to keep
 *  the function call trace counts as well as cycle counts
 *
 * Author: M. Tim Jones <mtj@mtjones.com>
 * Edited: Andress A Barajas
 * 
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "stack.h"
#include "symbols.h"
#include "priorityqueue.h"

typedef struct {
    unsigned int totalCalls;
    unsigned long long int totalCycles;
} calls_t;

typedef struct {
    unsigned int address;
    unsigned int totalCalls;
    unsigned long long int totalCycles;
    char funcName[MAX_FUNCTION_NAME+1];
} func_t;

static func_t  functions[MAX_FUNCTIONS];
static calls_t calls[MAX_FUNCTIONS][MAX_FUNCTIONS];

static int verbose;
static char progName[50] = {0};
static char addr2linePath[256] = {0};

static PriorityQueue pq;

static unsigned long long int profileStartTime;
static unsigned long long int profileEndTime;

void init(char *name, const char *path, int verb) {
    int from, to;

    verbose = verb;
    strncpy(progName, name, strlen(name));
    strncpy(addr2linePath, path, strlen(path));

    pq_init(&pq);

    for (from = 0; from < MAX_FUNCTIONS; from++) {
        functions[from].address = 0;
        functions[from].funcName[0] = 0;
        functions[from].totalCalls = 0;
        functions[from].totalCycles = 0;

        for (to = 0; to < MAX_FUNCTIONS; to++) {
            calls[from][to].totalCalls = 0;
            calls[from][to].totalCycles = 0;
        }
    }

    profileStartTime = 0;
    profileEndTime = 0;
}

int lookup_symbol(unsigned int address) {
    int index;

    for (index = 0; index < MAX_FUNCTIONS; index++) {
        if (functions[index].address == 0) {
            fprintf(stderr, "Error: Address 0x%x not found in functions array.\n", address);
            return -1;
        }

        if (functions[index].address == address) {
            return index;
        }
    }

    fprintf(stderr, "Error: Address 0x%x not found, and functions array is full.\n", address);
    return -1;
}

int translateFunctionFromSymbol(unsigned int address, char *func) {
    FILE *p;
    char line[100];

    int ret = snprintf(line, sizeof(line), "%s -e %s -f -s 0x%x", addr2linePath, progName, address);
    if (ret < 0 || ret >= sizeof(line)) {
        return 0;
    }

    p = popen(line, "r");
    if (p == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), p) != NULL) {
        size_t pos = strcspn(line, "\r\n");
        line[pos] = '\0';
        strncpy(func, line, MAX_FUNCTION_NAME);
        func[MAX_FUNCTION_NAME] = '\0';
    }

    pclose(p);
    return 1;
}

void add_symbol(unsigned int address) {
    int index;

    /* Check if we already have it in our list(array) */
    for (index = 0; index < MAX_FUNCTIONS; index++) {
        if (functions[index].address == address) {
            /* We have it so increase totalCalls and return */
            functions[index].totalCalls++;
            return;
        }

        if (functions[index].address == 0) 
            break;
    }

    /* We dont have it so lets add it */
    if (index < MAX_FUNCTIONS) {
        functions[index].address = address;
        functions[index].totalCalls = 1;
        if (!translateFunctionFromSymbol(address, functions[index].funcName)) {
            fprintf(stderr, "Error: Failed to translate address 0x%x to a function name.\n", address);
        }
    } else {
        fprintf(stderr, "Error: Functions array is full. Cannot add address 0x%x.\n", address);
        exit(1);
    }
}

void end_symbol(unsigned int address, unsigned long long int cycles) {
    int from, to;

    to = lookup_symbol(address);

    if (stack_num_elems()) {
        from = lookup_symbol(stack_top_address());
        
        /* Handle recursion */
        if(from != to) {
            functions[to].totalCycles += cycles;
            calls[from][to].totalCycles += cycles;
        }
    } 
    else {
        functions[to].totalCycles += cycles;
    }
}

void add_call_trace(unsigned int address) {
    int from, to;

    if (stack_num_elems()) {
        to = lookup_symbol(address);
        from = lookup_symbol(stack_top_address());
        calls[from][to].totalCalls++;
    }
}

/* Returns, via hex parameter, a RGB tuple on the spectrum from blue to red */
void color_from_percent(double percent, char* hex) {
    double r, g, b;
    double wavelength;

    wavelength = 440.0 + ((percent) * (220.0 / 100));
    if (wavelength < 490) {
        r = 0.0;
        g = (wavelength - 440) / (490 - 440);
        b = 1.0;
    }
    else if (wavelength < 510) {
        r = 0.0;
        g = 1.0;
        b = -(wavelength - 510) / (510 - 490);
    }
    else if (wavelength < 580) {
        r = (wavelength - 510) / (580 - 510);
        g = 1.0;
        b = 0.0;
    }
    else if (wavelength < 645) {
        r = 1.0;
        g = -(wavelength - 645) / (645 - 580);
        b = 0.0;
    }
    else {
        r = 1.0;
        g = 0.0;
        b = 0.0;
    }

    /* Multiply by 0.7 to make color 30% darker */
    r *= 255 * 0.7;
    g *= 255 * 0.7;
    b *= 255 * 0.7;

    snprintf(hex, 8, "#%02x%02x%02x", (int)r, (int)g, (int)b);
}

void write_node_shapes(FILE *fp, unsigned long long int profileTotalCycles) {
    int from, to;
    char hexColor[8] = {0};
    unsigned long long int otherFunctionCycles;
    double cumulativeCycles, actualCycles;

    for (from = 0; from < MAX_FUNCTIONS; from++) {
        otherFunctionCycles = 0;

        if (functions[from].address == 0) 
            break;

        for (to = 0; to < MAX_FUNCTIONS; to++) {
            if (functions[to].address == 0) 
                break;

            if(from != to) 
                otherFunctionCycles += calls[from][to].totalCycles;
        }

        /* Total cycles spent inside this function and the functions it calls */
        cumulativeCycles = functions[from].totalCycles;

        /* Total cycles spend inside this function MINUS the cycles spent in the 
           functions it calls
        */
        actualCycles = cumulativeCycles - otherFunctionCycles;

        /* Add it to the priority queue so we can print a nice table with our graph */
        pq_insert(&pq, from, (actualCycles / profileTotalCycles) * 100, actualCycles);

        /* If recursive, we want the total cumulative amount of cycles spent inside */
        if(calls[from][from].totalCalls)
            calls[from][from].totalCycles = cumulativeCycles;

        color_from_percent((cumulativeCycles / profileTotalCycles) * 100, hexColor);

        fprintf(fp, 
        "\t\t%s "         /* Function name */
        "[label=\""
        "%s\\n"         /* Function name */
        "%.2f%%\\n"     /* Cumulative Time in function */
        "(%.2f%%)\\n"   /* Time Inside this function (without considering functions it called) */
        "%d x\" "       /* Total times called */
        "fontcolor=\"white\" "
        "color=\"%s\" "
        "%s\n",         /* Node shape */
        functions[from].funcName,
        functions[from].funcName,
        (cumulativeCycles / profileTotalCycles) * 100, /* Express as percentage of total runtime */
        (actualCycles / profileTotalCycles) * 100,     /* Express as percentage of total runtime */
        functions[from].totalCalls,
        hexColor,
        otherFunctionCycles ? "shape=rectangle]" : "shape=ellipse]");
    }

    fprintf(fp, "\n");
}

void write_call_graph(FILE *fp, unsigned long long int profileTotalCycles) {
    int from, to;
    char hexColor[8] = {0};
    double temp;

    for (from = 0; from < MAX_FUNCTIONS; from++) {
        if (functions[from].address == 0)
            break;

        for (to = 0; to < MAX_FUNCTIONS; to++) {
            if (calls[from][to].totalCalls) {
                temp = ((double)calls[from][to].totalCycles / profileTotalCycles) * 100;

                color_from_percent(temp, hexColor);

                if(from != to) {
                    fprintf(fp, 
                        "\t\t%s -> %s [label=\"  "  /* A() => B() */
                        "%0.2f%%\\n"              /* Percentage of time B() spent in parent function A() */
                        " %d x\" "
                        "color=\"%s\" "
                        "style=\"%s\" "
                        "fontsize=\"10\"]\n", 
                        functions[from].funcName, 
                        functions[to].funcName,
                        temp,
                        calls[from][to].totalCalls,
                        hexColor,
                        temp > 0.35 ? "bold" : "solid");
                }
                else {
                    fprintf(fp, 
                        "\t\t%s -> %s [label=\"  "  /* A() => A() */
                        " %d x\" "
                        "color=\"%s\" "
                        "style=\"%s\" "
                        "fontsize=\"10\"]\n", 
                        functions[from].funcName, 
                        functions[to].funcName,
                        calls[from][to].totalCalls,
                        hexColor,
                        temp > 0.35 ? "bold" : "solid");
                }
            }

            if (functions[to].address == 0) 
                break;
        }
    }
}

void write_table(FILE *fp) {
    int i;

    fprintf(fp, "\t\ta0 [shape=none label=<<TABLE border=\"0\" cellspacing=\"3\" cellpadding=\"10\" bgcolor=\"black\">\n\n\t\t");

    for (i = 0; i < pq.size; i++) {
        fprintf(fp, "<TR>\n\t\t");
        fprintf(fp, "<TD bgcolor=\"white\">%d</TD>\n\t\t", i+1);
        fprintf(fp, "<TD bgcolor=\"white\">%s</TD>\n\t\t", functions[pq.elements[i].from].funcName);
        fprintf(fp, "<TD bgcolor=\"white\">%.2f%%</TD>\n\t\t", pq.elements[i].percentage);
        fprintf(fp, "<TD bgcolor=\"white\">%.0f cycles</TD>\n\t\t", pq.elements[i].cycles);
        fprintf(fp, "</TR>\n\n\t\t");
    }

    fprintf(fp, "</TABLE>>];\n");
}

void write_graph_caption(FILE *fp) {
    time_t rawtime;
    struct tm *ltm;

    time(&rawtime);
    ltm = localtime(&rawtime);

    int hour = ltm->tm_hour;
    char am_pm[3];

    if (hour >= 12) {
        strcpy(am_pm, "PM");
        if (hour > 12) hour -= 12;
    } 
    else {
        strcpy(am_pm, "AM");
        if (hour == 0) hour = 12;
    }

    /* Print a caption */
    char header[250];
    sprintf(header, 
        "\tgraph [\n"
        "\t\tfontname = \"Helvetica-Oblique\",\n"
        "\t\tfontsize = 32,\n"
        "\t\tlabel = \"\\n\\n%s\\n%d/%d/%d @ %d:%02d %s\"\n"
        "\t];", progName, ltm->tm_mon+1, ltm->tm_mday, ltm->tm_year+1900, hour, ltm->tm_min, am_pm);

    fprintf(fp, "\n%s", header);
}

void create_dot_file(void) {
    unsigned long long int profileTotalCycles;
    FILE *fp;

    profileTotalCycles = profileEndTime - profileStartTime;

    fp = fopen("graph.dot", "w");
    if (fp == NULL) {
        printf("Couldn't open graph.dot\n");
        exit(0);
    }

    fprintf(fp, "digraph program {\n\n\t");

    /* Write the graph cluster */
    fprintf(fp, "subgraph cluster0 {\n\t\t"
                "ratio=fill;\n\t\t"
                "node [style=filled];\n\t\t"
                "peripheries=0;\n\n");

    write_node_shapes(fp, profileTotalCycles);
    write_call_graph(fp, profileTotalCycles);
    fprintf(fp, "\t}\n\n\t");

    /* Write the table cluster */
    fprintf(fp, "subgraph cluster1 {\n\t\t"
                "peripheries=0;\n\t\t"
                "fontname=\"Helvetica,Arial,sans-serif\";\n\t\t"
                "node [fontname=\"Helvetica,Arial,sans-serif\"]\n\t\t"
                "edge [fontname=\"Helvetica,Arial,sans-serif\"]\n\n");
    write_table(fp);
    fprintf(fp, "\t}\n\n");

    write_graph_caption(fp);

    fprintf(fp, "\n}\n");
    fclose(fp);
}

void calculate_total_profile_time(unsigned long long int cycle) {
    if(profileStartTime == 0)
        profileStartTime = cycle;
    
    profileEndTime = cycle;
}