/********************************************************************
 * File: priorityqueue.h
 *
 *
 * Author: Andress A Barajas
 * 
 */

#ifndef __PRIORITYQUEQUE_H
#define __PRIORITYQUEQUE_H

#define MAX_SIZE 5

typedef struct {
    int from;
    double percentage;
    double cycles;
} Element;

typedef struct {
    Element elements[MAX_SIZE];
    int size;
} PriorityQueue;

void pq_init(PriorityQueue *pq);

int pq_is_full(PriorityQueue *pq);

int pq_is_empty(PriorityQueue *pq);

int pq_size(PriorityQueue *pq);

void pq_insert(PriorityQueue *pq, int from, double percentage, double cycles);

#endif /* __PRIORITYQUEQUE_H */
