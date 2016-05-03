/*   
 *   File: harris.c
 *   Author: Vincent Gramoli <vincent.gramoli@sydney.edu.au>, 
 *  	     Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: Timothy L Harris. A Pragmatic Implementation 
 *   of Non-blocking Linked Lists. DISC 2001.
 *   harris.c is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "harris.h"

RETRY_STATS_VARS;
TSX_STATS_VARS;
TSX_ABORT_REASONS_VARS;

/*
 * The five following functions handle the low-order mark bit that indicates
 * whether a node is logically deleted (1) or not (0).
 *  - is_marked_ref returns whether it is marked, 
 *  - (un)set_marked changes the mark,
 *  - get_(un)marked_ref sets the mark before returning the node.
 */
inline int
is_marked_ref(long i) 
{
  /* return (int) (i & (LONG_MIN+1)); */
  return (int) (i & 0x1L);
}

inline long
unset_mark(long i)
{
  /* i &= LONG_MAX-1; */
  i &= ~0x1L;
  return i;
}

inline long
set_mark(long i) 
{
  /* i = unset_mark(i); */
  /* i += 1; */
  i |= 0x1L;
  return i;
}

inline long
get_unmarked_ref(long w) 
{
  /* return unset_mark(w); */
  return w & ~0x1L;
}

inline long
get_marked_ref(long w) 
{
  /* return set_mark(w); */
  return w | 0x1L;
}

/*
 * harris_search looks for value val, it
 *  - returns right_node owning val (if present) or its immediately higher 
 *    value present in the list (otherwise) and 
 *  - sets the left_node to the node owning the value immediately lower than val. 
 * Encountered nodes that are marked as logically deleted are physically removed
 * from the list, yet not garbage collected.
 */
node_t*
ege_search(intset_t *set, skey_t key, node_t **left_node) 
{
  node_t *prev;
  volatile node_t volatile * volatile next;
  prev = set->head;
  next = prev->next;
  while (next->key < key) 
    {
      prev = next;
      next = prev->next;
    }
  *left_node = prev;
  return next;
}

/*
 * harris_find returns whether there is a node in the list owning value val.
 */
sval_t
ege_find(intset_t *set, skey_t key) 
{
  node_t *prev, *next;
  prev = set->head;
  next = prev->next;
  while (next->key < key) 
    {
      prev = next;
      next = prev->next;
    }
  return (next->key == key) ? next->val : 0;
}

/*
 * harris_find inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int
ege_insert(intset_t *set, skey_t key, sval_t val) 
{
  int i=0;
  node_t *newnode = NULL, *prev, *next = NULL;
  do
    {
      if (i==1000000) 
        {
          int j, k;
          printf("Millionth time in the insertion loop\n");
          for (j=0; j<3; j++)
            for (k=0; k<7; k++)
              my_tsx_abort_reasons[j][k] = 0;
        }
      else if (i==2000000)
        {
          int j;
          printf("2Millionth time in the insertion loop\n");
          printf("Abort reasons: %-12s %-12s %-12s %-12s %-12s %-12s | %-12s\n",
                 "explicit", "conflict", "capacity", "debug trap", "nested txn", "other", "retry");
          for (j=0; j<TSX_STATS_DEPTH; j++)
            printf("Trial %d      : %-12lu %-12lu %-12lu %-12lu %-12lu %-12lu | %-12lu\n",
              j, my_tsx_abort_reasons[j][0], my_tsx_abort_reasons[j][1],
              my_tsx_abort_reasons[j][2], my_tsx_abort_reasons[j][3],
              my_tsx_abort_reasons[j][4], my_tsx_abort_reasons[j][5],
              my_tsx_abort_reasons[j][6]);
        }
      i++;
      UPDATE_TRY();
      next = ege_search(set, key, &prev);
      if (next->key == key)
        {
#if GC == 1
	  if (unlikely(newnode != NULL))
	    {
	      ssmem_free(alloc, (void*) newnode);
	    }
#endif
          return 0;
        }
      if (likely(newnode == NULL))
	{
	  newnode = new_node(key, val, next, 0);
	}
      else
	{
	  newnode->next = next;
	}

//      ATOMIC_CAS_MB(prev->next, 0, 1);
      TSX_CRITICAL_SECTION
        {
          /* The first condition is to check that they are still adjacent.
           * The second one is to make sure that 'prev' is not marked deleted. */
          if (unlikely(prev->next != next || prev->key > next->key))
            {
              TSX_ABORT;
            }
          prev->next = newnode;
          TSX_COMMIT;
          return 1;
        }
      TSX_AFTER;
      if (ATOMIC_CAS_MB(&prev->next, next, newnode))
        return 1;
    }
  while(1);
  return 0;
}

/*
 * harris_find deletes a node with the given value val (if the value is present) 
 * or does nothing (if the value is already present).
 * The deletion is logical and consists of setting the node mark bit to 1.
 */
sval_t
ege_delete(intset_t *set, skey_t key)
{
  int i=0;
  node_t *prev, *next = NULL, *next_next = NULL;
  do
    {
      if (i==1000000) 
        {
          int j, k;
          printf("Millionth time in the deletion loop\n");
          for (j=0; j<3; j++)
            for (k=0; k<7; k++)
              my_tsx_abort_reasons[j][k] = 0;
        }
      else if (i==2000000)
        {
          int j;
          printf("2Millionth time in the deletion loop\n");
          printf("Abort reasons: %-12s %-12s %-12s %-12s %-12s %-12s | %-12s\n",
                 "explicit", "conflict", "capacity", "debug trap", "nested txn", "other", "retry");
          for (j=0; j<TSX_STATS_DEPTH; j++)
            printf("Trial %d      : %-12lu %-12lu %-12lu %-12lu %-12lu %-12lu | %-12lu\n",
              j, my_tsx_abort_reasons[j][0], my_tsx_abort_reasons[j][1],
              my_tsx_abort_reasons[j][2], my_tsx_abort_reasons[j][3],
              my_tsx_abort_reasons[j][4], my_tsx_abort_reasons[j][5],
              my_tsx_abort_reasons[j][6]);
        }
      i++;
      UPDATE_TRY();
      next = ege_search(set, key, &prev);
      if (next->key != key)
        {
          return 0;
        }
      ATOMIC_CAS_MB(prev->next, 0, 1);
      ATOMIC_CAS_MB(next->next, 0, 1);
      TSX_CRITICAL_SECTION
        {
          if (unlikely(prev->next != next))
            {
              TSX_ABORT;
            }
          prev->next = next->next;
          next->next = prev;
          TSX_COMMIT;
#if GC == 1
          ssmem_free(alloc, (void*) next);
#endif
          return 1;
        }
      TSX_AFTER;
//      if (next_next == NULL)
//          next_next = SWAP_PTR(&next->next, prev);
      if (ATOMIC_CAS_MB(&prev->next, next, next->next))
        {
#if GC == 1
//          ssmem_free(alloc, (void*) next);
#endif
//          printf("happens\n");
//          fflush(stdout);
          return 1;
        }
    }
  while(1);
  return 0;
}

int
set_size(intset_t *set)
{
  int size = 0;
  node_t* node;

  /* We have at least 2 elements */
  node = (node_t*) get_unmarked_ref((long) set->head->next);
  while ((node_t*) get_unmarked_ref((long) node->next) != NULL)
    {
      if (!is_marked_ref((long) node->next)) size++;
      node = (node_t*) get_unmarked_ref((long) node->next);
    }
  return size;
}
