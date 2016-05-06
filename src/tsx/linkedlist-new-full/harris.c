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
 * harris_search looks for value val, it
 *  - returns right_node owning val (if present) or its immediately higher 
 *    value present in the list (otherwise) and 
 *  - sets the left_node to the node owning the value immediately lower than val. 
 * Encountered nodes that are marked as logically deleted are physically removed
 * from the list, yet not garbage collected.
 */
node_t*
new_search(intset_t *set, skey_t key, node_t **left_node) 
{
  node_t *prev;
  node_t *next;
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
new_find(intset_t *set, skey_t key) 
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
new_insert(intset_t *set, skey_t key, sval_t val) 
{
  node_t *newnode = NULL, *prev, *next = NULL;
  do
    {
retry:
      UPDATE_TRY();
      next = new_search(set, key, &prev);
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

      if (prev->key > next->key)
          continue;

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
          /* The first condition is to check that they are still adjacent.
           * The second one is to make sure that 'prev' is not marked deleted. */
          if (unlikely(prev->next != next))
            {
              TSX_ABORT;
            }
          prev->next = newnode;
          TSX_COMMIT;
          return 1;
        }
      TSX_END_EXPLICIT_ABORTS_GOTO(retry);

      if (ATOMIC_CAS_MB(&prev->next, next, newnode))
          return 1;
    }
  while(1);
}

/*
 * harris_find deletes a node with the given value val (if the value is present) 
 * or does nothing (if the value is already present).
 * The deletion is logical and consists of setting the node mark bit to 1.
 */
sval_t
new_delete(intset_t *set, skey_t key)
{
  node_t *prev, *next = NULL, *next_next = NULL;
  do
    {
retry:
      UPDATE_TRY();
      next = new_search(set, key, &prev);
      if (next->key != key)
        {
          return 0;
        }
      TSX_CRITICAL_SECTION
        {
          if (unlikely(prev->next != next || next->key > next->next->key))
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
      TSX_END_EXPLICIT_ABORTS_GOTO(retry);

      next_next = next->next;
      if (next->key > next_next->key
          || !ATOMIC_CAS_MB(&next->next, next_next, prev))
          continue;
      if (ATOMIC_CAS_MB(&prev->next, next, next_next))
        {
#if GC == 1
          ssmem_free(alloc, (void*) next);
#endif
          return 1;
        }
      else
        {
          next->next = next_next;
          next_next = NULL;
        }
    }
  while(1);
}

int
set_size(intset_t *set)
{
  int size = 0;
  node_t* node;

  /* We have at least 2 elements */
  node = (node_t*) set->head->next;
  while ((node_t*) node->next != NULL)
    {
      if (node->next) size++;
      node = (node_t*) node->next;
    }
  return size;
}
