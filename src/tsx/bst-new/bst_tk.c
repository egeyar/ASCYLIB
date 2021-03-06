/*   
 *   File: bst_tk.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: Asynchronized Concurrency: The Secret to Scaling Concurrent
 *    Search Data Structures, Tudor David, Rachid Guerraoui, Vasileios Trigonakis,
 *   ASPLOS '15
 *   bst_tk.c is part of ASCYLIB
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

#include "bst_tk.h"

RETRY_STATS_VARS;
TSX_STATS_VARS;
TSX_ABORT_REASONS_VARS;

sval_t
bst_tk_find(intset_t* set, skey_t key) 
{
  PARSE_TRY();
  node_t* curr = set->head;

  while (likely(!curr->leaf))
    {
      if (key < curr->key)
	{
	  curr = (node_t*) curr->left;
	}
      else
	{
	  curr = (node_t*) curr->right;
	}
    }

  if (curr->key == key)
    {
      return curr->val;
    }  

  return 0;
}

int
bst_tk_insert(intset_t* set, skey_t key, sval_t val) 
{
  node_t *curr, *pred = NULL;
  node_t *nn = NULL, *nr;
  uint64_t right = 0;

retry:
  PARSE_TRY();
  UPDATE_TRY();

  curr = set->head;
  do
    {
      pred = curr;
      if (key < curr->key)
	{
	  right = 0;
	  curr = (node_t*) curr->left;
	}
      else
	{
	  right = 1;
	  curr = (node_t*) curr->right;
	}
    }
  while(likely(!curr->leaf));

  if (curr->key == key)
    {
#if GC == 1
      if (unlikely(nn != NULL))
        {
          ssmem_free(alloc, nn);
          ssmem_free(alloc, nr);
        }
#endif
      return 0;
    }

  /*Not sure if this improves the performance*/
  if (unlikely((right && pred->right != curr)
           || (!right && pred->left  != curr)))
    goto retry;

  if (likely(nn == NULL))
    {
      nn = new_node(key, val, NULL, NULL, 0);
      nr = new_node_no_init();
    }

  if (key < curr->key)
    {
      nr->key = curr->key;
      nr->left = nn;
      nr->right = curr;
    }
  else
    {
      nr->key = key;
      nr->left = curr;
      nr->right = nn;
    }

#if defined(__tile__)
  /* on tilera you may have store reordering causing the pointer to a new node
     to become visible, before the contents of the node are visible */
  MEM_BARRIER;
#endif	/* __tile__ */

  if (right)
    {
      if (!ATOMIC_CAS_MB(&pred->right, curr, nr))
        goto retry;
    }
  else
    {
      if (!ATOMIC_CAS_MB(&pred->left, curr, nr))
        goto retry;
    }

  return 1;
}


#define NEW_DELETE(branchx, branchy, branchnoty)                       \
TSX_CRITICAL_SECTION                                                   \
  {                                                                    \
    if (unlikely(ppred->branchx != pred || pred->branchy != curr       \
                 || pred == set->head                                  \
                 || pred->branchnoty == set->head ))                   \
      {                                                                \
        TSX_ABORT;                                                     \
      }                                                                \
    ppred->branchx = pred->branchnoty;                                 \
    pred->left = set->head;                                            \
    pred->right = set->head;                                           \
    TSX_COMMIT;                                                        \
    goto success;                                                      \
  }                                                                    \
TSX_END_EXPLICIT_ABORTS_GOTO(retry);                                   \
sibling = (node_t *) pred->branchnoty;                                 \
node_t *leftchild  = right ? sibling : curr,                           \
       *rightchild = right ? curr : sibling;                           \
if (unlikely(pred==set->head || sibling == set->head                   \
    || !ATOMIC_CAS_MB(&pred->left, leftchild, set->head)))             \
    goto retry;                                                        \
if (unlikely(!ATOMIC_CAS_MB(&pred->right, rightchild, set->head)))     \
  {                                                                    \
    pred->left = leftchild;                                            \
    goto retry;                                                        \
  }                                                                    \
if (unlikely(!ATOMIC_CAS_MB(&ppred->branchx, pred, sibling)))          \
  {                                                                    \
    pred->left = leftchild;                                            \
    pred->right = rightchild;                                          \
    goto retry;                                                        \
  }                                                                    \
  goto success;


sval_t
bst_tk_delete(intset_t* set, skey_t key)
{
  node_t *curr, *pred = NULL, *ppred = NULL, *sibling;
  uint64_t right = 0, pright = 0;

retry:
  PARSE_TRY();
  UPDATE_TRY();

  curr = set->head;
  do
    {
      ppred = pred;
      pright = right;
      pred = curr;

      if (key < curr->key)
	{
	  right = 0;
	  curr = (node_t*) curr->left;
	}
      else
	{
	  right = 1;
	  curr = (node_t*) curr->right;
	}
    }
  while(likely(!curr->leaf));


  if (curr->key != key)
    {
      return 0;
    }

  if (pright)
    {
      if (right)
	{
          NEW_DELETE(right, right, left);
	}
      else
	{
          NEW_DELETE(right, left, right);
	}
    }
  else
    {
      if (right)
	{
          NEW_DELETE(left, right, left);
	}
      else
	{
          NEW_DELETE(left, left, right);
	}
    }

success:
#if GC == 1
  ssmem_free(alloc, curr);
  ssmem_free(alloc, pred);
#endif

  return curr->val;
} 


