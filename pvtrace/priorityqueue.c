/********************************************************************
 * File: priorityqueue.c
 *
 *
 * Author: Andress A Barajas
 * 
 */

#include <stdio.h>
#include "priorityqueue.h"

void pq_init(PriorityQueue *pq) {
    pq->size = 0;
}

int pq_is_full(PriorityQueue *pq) {
    return pq->size == MAX_SIZE;
}

int pq_is_empty(PriorityQueue *pq) {
    return pq->size == 0;
}

int pq_size(PriorityQueue *pq) {
    return pq->size;
}

void pq_insert(PriorityQueue *pq, int from, double percentage, double cycles) {
    if (pq_is_full(pq)) {
        if (percentage <= pq->elements[MAX_SIZE - 1].percentage) {
            // The new element has a lower percentage than the lowest in the queue, so we don't insert it
            return;
        }

        // Remove the element with the lowest percentage
        pq->size--;
    }

    // Find the index where the new element should be inserted
    int insert_index = 0;
    while (insert_index < pq->size && pq->elements[insert_index].percentage > percentage) {
        insert_index++;
    }

    // Shift the elements to the right of the insert_index
    for (int i = pq->size; i > insert_index; i--) {
        pq->elements[i] = pq->elements[i - 1];
    }

    // Insert the new element
    Element new_elem;
    new_elem.from = from;
    new_elem.cycles = cycles;
    new_elem.percentage = percentage;
    pq->elements[insert_index] = new_elem;
    pq->size++;
}