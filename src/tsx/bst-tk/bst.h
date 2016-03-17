/*   
 *   File: bst.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: 
 *   bst.h is part of ASCYLIB
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

#ifndef _H_BST_TK_
#define _H_BST_TK_

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include <atomic_ops.h>
#include "lock_if.h"

#include "common.h"
#include "utils.h"
#include "measurements.h"
#include "ssalloc.h"
#include "ssmem.h"

static volatile int stop;
extern __thread ssmem_allocator_t* alloc;


typedef union tl32
{
  struct
  {
    volatile uint16_t version;
    volatile uint16_t ticket;
  };
  volatile uint32_t to_uint32;
} tl32_t;


typedef union tl
{
  tl32_t lr[2];
  uint64_t to_uint64;
} tl_t;

static inline int
tl_trylock_version(volatile tl_t* tl, volatile tl_t* tl_old, int right)
{
  int _xbegin_tries = 3;
  int t;
  for (t = 0; t < _xbegin_tries; t++)
  {
    long status;
    if ((status = _xbegin()) == _XBEGIN_STARTED)
    {
      if (likely(tl->lr[right].ticket == tl_old->lr[right].version))
      {
        tl->lr[right].ticket++;
        _xend();
        return 1;
      }
      else
      {
        _xabort(0xff);
      }
    }
    /*Transactionalization failed.*/
    else
    {
      if (status & _XABORT_EXPLICIT || !(status & _XABORT_RETRY))
      {
        break;
      }
      PAUSE;
    }
  }
  return 0;
}

#define TLN_REMOVED  0x0000FFFF0000FFFF0000LL

static inline int
tl_trylock_version_both(volatile tl_t* tl, volatile tl_t* tl_old)
{
  int _xbegin_tries = 3;
  int t;
  for (t = 0; t < _xbegin_tries; t++)
  {
    long status;
    if ((status = _xbegin()) == _XBEGIN_STARTED)
    {
      if (likely(tl_old->lr[0].version == tl->lr[0].ticket 
              && tl_old->lr[1].version == tl->lr[1].ticket))
      {
        tl->to_uint64 = TLN_REMOVED;
        _xend();
        return 1;
      }
      else
      {
        _xabort(0xff);
      }
    }
    /*Transactionalization failed.*/
    else
    {
      if (status & _XABORT_EXPLICIT || !(status & _XABORT_RETRY))
      {
        break;
      }
      PAUSE;
    }
  }
  return 0;
}

static inline void
tl_unlock(volatile tl_t* tl, int right)
{
  /* PREFETCHW(tl); */
#ifdef __tile__
  MEM_BARRIER;
#endif
  COMPILER_NO_REORDER(tl->lr[right].version++);
}

static inline void
tl_revert(volatile tl_t* tl, int right)
{
  /* PREFETCHW(tl); */
  COMPILER_NO_REORDER(tl->lr[right].ticket--);
}


typedef struct node
{
  skey_t key;
  union
  {
    sval_t val;
    volatile uint64_t leaf;
  };
  volatile struct node* left;
  volatile struct node* right;
  volatile tl_t lock;

  uint8_t padding[CACHE_LINE_SIZE - 40];
} node_t;

typedef ALIGNED(CACHE_LINE_SIZE) struct intset
{
  node_t* head;
} intset_t;

node_t* new_node(skey_t key, sval_t val, node_t* l, node_t* r, int initializing);
node_t* new_node_no_init();
intset_t* set_new();
void set_delete(intset_t* set);
int set_size(intset_t* set);
void node_delete(node_t* node);

#endif	/* _H_BST_TK_ */
