/*   
 *   File: fraser.c
 *   Author: Vincent Gramoli <vincent.gramoli@sydney.edu.au>, 
 *  	     Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: Lock-based skip list implementation of the Fraser algorithm
 *   "Practical Lock Freedom", K. Fraser, 
 *   PhD dissertation, September 2003
 *   fraser.c is part of ASCYLIB
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

#include "fraser.h"

RETRY_STATS_VARS;
TSX_STATS_VARS;
TSX_ABORT_REASONS_VARS;
TSX_WITH_FALLBACK_VARS;
#define DS_NODE sl_node_t

#include "latency.h"
#if LATENCY_PARSING == 1
__thread size_t lat_parsing_get = 0;
__thread size_t lat_parsing_put = 0;
__thread size_t lat_parsing_rem = 0;
#endif	/* LATENCY_PARSING == 1 */

extern ALIGNED(CACHE_LINE_SIZE) unsigned int levelmax;

#define FRASER_MAX_MAX_LEVEL 64 /* covers up to 2^64 elements */

void
fraser_search(sl_intset_t *set, skey_t key, sl_node_t **left_list, sl_node_t **right_list, int lowest_level)
{
  int i;
  sl_node_t *left, *left_next;

  PARSE_TRY();

  left = set->head;
  for (i = levelmax - 1; i >= lowest_level; i--)
    {
      left_next = left->next[i];
      while (1)
        {
          if (left_next->key >= key)
	    {
	      break;
	    }
          left = left_next;
          left_next = left_next->next[i];
        }

      if (left_list != NULL)
	{
	  left_list[i] = left;
	}
      if (right_list != NULL)	
	{
	  right_list[i] = left_next;
	}
    }
}

sval_t
fraser_find(sl_intset_t *set, skey_t key)
{
  int i;
  sl_node_t *left, *left_next;

  PARSE_START_TS(0);
  left = set->head;
  for (i = levelmax - 1; i >= 0; i--)
    {
      left_next = left->next[i];
      while (1)
        {
          if (left_next->key >= key)
	    {
              if (left_next->key == key)
                {
                  return left_next->val;
                }
	      break;
	    }
          left = left_next;
          left_next = left_next->next[i];
        }
    }
  PARSE_END_TS(0, lat_parsing_get++);
  return 0;
}

sval_t
fraser_remove(sl_intset_t *set, skey_t key)
{
  sl_node_t *curr, *curr_next;
  sl_node_t* preds[FRASER_MAX_MAX_LEVEL];
  sl_node_t* succs[FRASER_MAX_MAX_LEVEL];
  int i;
  sval_t result = 0;

  PARSE_START_TS(2);
retry1:
  UPDATE_TRY();
  fraser_search(set, key, preds, succs, 0);
  PARSE_END_TS(2, lat_parsing_rem++);

  /* If k-node does not exist or marked deleted, the operation fails */
  curr = succs[0];
  if (unlikely(curr->key != key || curr->deleted))
    {
      return 0;
    }
  
  TSX_WITH_FALLBACK_BEGIN();
  TSX_PROTECT_NODE(curr, retry1);
  TSX_VALIDATE(!curr->deleted, end);
  curr->deleted = 1;
  TSX_WITH_FALLBACK_END();

  for (i = curr->toplevel-1; i >= 0; i--) 
    {
nextlevel:
      while(1) 
	{
          TSX_WITH_FALLBACK_BEGIN();
          TSX_PROTECT_NODE(preds[i], retry2);
          TSX_PROTECT_NODE(curr, retry2);
          if (i < curr->toplevel)
            {
              TSX_VALIDATE(preds[i]->next[i] == curr, retry2);
              preds[i]->next[i] = curr->next[i];
              curr->next[i] = preds[i];
            }
          TSX_WITH_FALLBACK_END();
          if (--i >= 0)
            {
              goto nextlevel;
            }
          else
            {
              goto success;
            }
retry2:
          fraser_search(set, key, preds, succs, i);
        }
    }
success:
  result = curr->val;
#if GC == 1
  ssmem_free(alloc, (void*) curr);
#endif
end:
  return result;
}

int
fraser_insert(sl_intset_t *set, skey_t key, sval_t val) 
{
  sl_node_t *new = NULL, *new_next;
  sl_node_t *succs[FRASER_MAX_MAX_LEVEL], *preds[FRASER_MAX_MAX_LEVEL];
  int i;
  int result = 0;

  PARSE_START_TS(1);
retry1: 	
  UPDATE_TRY();

  fraser_search(set, key, preds, succs, 0);
  PARSE_END_TS(1, lat_parsing_put);

  if (succs[0]->key == key) 
    {				/* Value already in list */
      result = 0;
#if GC == 1
      if (unlikely(new != NULL))
        {
          sl_delete_node(new);
        }
#endif
      goto end;
    }

  if (new == NULL)
    {
      new = sl_new_simple_node(key, val, get_rand_level(), 0);
    }

  for (i = 0; i < new->toplevel; i++)
    {
      new->next[i] = succs[i];
    }

#if defined(__tile__)
  MEM_BARRIER;
#endif

  TSX_WITH_FALLBACK_BEGIN();
  TSX_PROTECT_NODE(preds[0], retry1);
  TSX_PROTECT_NODE(new, retry1);
  TSX_VALIDATE(preds[0]->next[0] == succs[0], retry1);
  preds[0]->next[0] = new;
  TSX_WITH_FALLBACK_END();

  for (i = 1; i < new->toplevel; i++) 
    {
nextlevel:
      while (1)
	{
	  /* Update the forward pointer if it is stale */
	  new_next = new->next[i];
	  if (new->deleted) /* If new is deleted*/
	    {
              new->toplevel = i;
	      goto success;
	    }

          TSX_WITH_FALLBACK_BEGIN();
          TSX_PROTECT_NODE(preds[i], retry2);
          TSX_PROTECT_NODE(new, retry2);
          TSX_VALIDATE(preds[i]->next[i] == succs[i], retry2);
          if (new->next[i] != succs[i])
            {
              new->next[i] = succs[i];
            }
          preds[i]->next[i] = new;
          TSX_WITH_FALLBACK_END();
          if (++i < new->toplevel)
            {
              goto nextlevel;
            }
          else
            {
              goto success;
            }
retry2:
          fraser_search(set, key, preds, succs, i);
	}
    }

 success:
  result = 1;
 end:
  PARSE_END_INC(lat_parsing_put);
  return result;
}

