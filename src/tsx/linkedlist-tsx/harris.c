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
#ifdef TSX_STATS
TSX_STATS_VARS;
#endif

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
  node_t *prev, *next;
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
  node_t *newnode = NULL, *prev, *next = NULL;
  do
    {
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
      
      TSX_CRITICAL_SECTION
        {
          if (unlikely(prev->next != next))
            {
              TSX_ABORT;
            }
          prev->next = newnode;
          TSX_COMMIT;
          return 1;
        }
      TSX_AFTER;
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
  node_t *prev, *next = NULL;
  do
    {
      UPDATE_TRY();
      next = ege_search(set, key, &prev);
      if (next->key != key)
        {
          return 0;
        }      
      TSX_CRITICAL_SECTION
        {
          if (unlikely(prev->next != next))
            {
              TSX_ABORT;
            }
          prev->next = next->next;
          next->next = set->head;
          TSX_COMMIT;
#if GC == 1
          ssmem_free(alloc, (void*) next);
#endif
          return 1;
        }
      TSX_AFTER;
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
