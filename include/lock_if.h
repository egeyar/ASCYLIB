/*   
 *   File: lock_if.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: 
 *   lock_if.h is part of ASCYLIB
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

#ifndef _LOCK_IF_H_
#define _LOCK_IF_H_

#include "utils.h"
#include "latency.h"

#define PREFETCHW_LOCK(lock)                            PREFETCHW(lock)

#if defined(MUTEX)
typedef pthread_mutex_t ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define PTLOCK_SIZE sizeof(ptlock_t)
#  define INIT_LOCK(lock)				pthread_mutex_init((pthread_mutex_t *) lock, NULL)
#  define DESTROY_LOCK(lock)			        pthread_mutex_destroy((pthread_mutex_t *) lock)
#  define LOCK(lock)					pthread_mutex_lock((pthread_mutex_t *) lock)
#  define TRYLOCK(lock)					pthread_mutex_trylock((pthread_mutex_t *) lock)
#  define UNLOCK(lock)					pthread_mutex_unlock((pthread_mutex_t *) lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				pthread_mutex_init((pthread_mutex_t *) lock, NULL)
#  define GL_DESTROY_LOCK(lock)			        pthread_mutex_destroy((pthread_mutex_t *) lock)
#  define GL_LOCK(lock)					pthread_mutex_lock((pthread_mutex_t *) lock)
#  define GL_TRYLOCK(lock)				pthread_mutex_trylock((pthread_mutex_t *) lock)
#  define GL_UNLOCK(lock)				pthread_mutex_unlock((pthread_mutex_t *) lock)
#elif defined(SPIN)		/* pthread spinlock */
typedef pthread_spinlock_t ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define PTLOCK_SIZE sizeof(ptlock_t)
#  define INIT_LOCK(lock)				pthread_spin_init((pthread_spinlock_t *) lock, PTHREAD_PROCESS_PRIVATE);
#  define DESTROY_LOCK(lock)			        pthread_spin_destroy((pthread_spinlock_t *) lock)
#  define LOCK(lock)					pthread_spin_lock((pthread_spinlock_t *) lock)
#  define UNLOCK(lock)					pthread_spin_unlock((pthread_spinlock_t *) lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				pthread_spin_init((pthread_spinlock_t *) lock, PTHREAD_PROCESS_PRIVATE);
#  define GL_DESTROY_LOCK(lock)			        pthread_spin_destroy((pthread_spinlock_t *) lock)
#  define GL_LOCK(lock)					pthread_spin_lock((pthread_spinlock_t *) lock)
#  define GL_UNLOCK(lock)				pthread_spin_unlock((pthread_spinlock_t *) lock)
#elif defined(TAS)			/* TAS */
#  if RETRY_STATS == 1
extern RETRY_STATS_VARS;
#    define PTLOCK_SIZE 32		/* choose 8, 16, 32, 64 */
typedef struct tticket
{
  union
  {
    volatile uint32_t whole;
    struct
    {
      volatile uint16_t tick;
      volatile uint16_t curr;
    };
  };
} tticket_t;
#  else
#    define PTLOCK_SIZE 32		/* choose 8, 16, 32, 64 */
#  endif
#  define PASTER(x, y, z) x ## y ## z
#  define EVALUATE(sz) PASTER(uint, sz, _t)
#  define UTYPE  EVALUATE(PTLOCK_SIZE)
#  define PASTER2(x, y) x ## y
#  define EVALUATE2(sz) PASTER2(CAS_U, sz)
#  define CAS_UTYPE EVALUATE2(PTLOCK_SIZE)
typedef volatile UTYPE ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define INIT_LOCK(lock)				tas_init(lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					tas_lock(lock)
#  define TRYLOCK(lock)					tas_trylock(lock)
#  define UNLOCK(lock)					tas_unlock(lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				tas_init(lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					tas_lock(lock)
#  define GL_TRYLOCK(lock)				tas_trylock(lock)
#  define GL_UNLOCK(lock)              			tas_unlock(lock)

#  define TAS_FREE 0
#  define TAS_LCKD 1

static inline void
tas_init(ptlock_t* l)
{
  *l = TAS_FREE;
#  if defined(__tile__)
  MEM_BARRIER;
#  endif
}

static inline uint32_t
tas_lock(ptlock_t* l)
{
  LOCK_TRY();
#if RETRY_STATS == 1
  volatile tticket_t* t = (volatile tticket_t*) l;
  volatile uint16_t tick = FAI_U16(&t->tick);

  uint16_t cur = t->curr;
  int16_t distance = tick - cur;
  if (distance < 0 ) { printf("distatnce is %d / %u\n", distance, distance); }

  LOCK_QUEUE(distance);

  while (t->curr != tick)
    {
      PAUSE;
    }

#else
  while (CAS_UTYPE(l, TAS_FREE, TAS_LCKD) == TAS_LCKD)
    {
      PAUSE;
    }
#endif
  return 0;
}

static inline uint32_t
tas_trylock(ptlock_t* l)
{
#if RETRY_STATS == 1
  LOCK_TRY_ONCE();
  volatile uint32_t tc = *(volatile uint32_t*) l;
  volatile tticket_t* tp = (volatile tticket_t*) &tc;

  if (tp->curr == tp->tick)
    {
      COMPILER_NO_REORDER(uint64_t tc_old = tc;);
      tp->tick++;
      int res = CAS_U32((uint32_t*) l, tc_old, tc) == tc_old;
      if (!res) 
	{
	  LOCK_QUEUE_ONCE(tp->tick - tp->curr);
	}
      return res;
    }
  else
    {
      LOCK_QUEUE_ONCE(tp->tick - tp->curr);
      return 0;
    }
#else
  return (CAS_UTYPE(l, TAS_FREE, TAS_LCKD) == TAS_FREE);
#endif

}

static inline uint32_t
tas_unlock(ptlock_t* l)
{
#  if defined(__tile__) 
  MEM_BARRIER;
#  endif

#if RETRY_STATS == 1
  volatile tticket_t* t = (volatile tticket_t*) l;
  PREFETCHW(t);
  COMPILER_NO_REORDER(t->curr++;);
#else
  COMPILER_NO_REORDER(*l = TAS_FREE;);
#endif
  return 0;
}

#elif defined(TTAS)			/* TTAS */
#  define PTLOCK_SIZE 32		/* choose 8, 16, 32, 64 */
#  define PASTER(x, y, z) x ## y ## z
#  define EVALUATE(sz) PASTER(uint, sz, _t)
#  define UTYPE  EVALUATE(PTLOCK_SIZE)
#  define PASTER2(x, y) x ## y
#  define EVALUATE2(sz) PASTER2(CAS_U, sz)
#  define CAS_UTYPE EVALUATE2(PTLOCK_SIZE)
typedef volatile UTYPE ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define INIT_LOCK(lock)				ttas_init(lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					ttas_lock(lock)
#  define TRYLOCK(lock)					ttas_trylock(lock)
#  define UNLOCK(lock)					ttas_unlock(lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				ttas_init(lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					ttas_lock(lock)
#  define GL_TRYLOCK(lock)				ttas_trylock(lock)
#  define GL_UNLOCK(lock)              			ttas_unlock(lock)

#  define TTAS_FREE 0
#  define TTAS_LCKD 1

static inline void
ttas_init(ptlock_t* l)
{
  *l = TTAS_FREE;
#  if defined(__tile__)
  MEM_BARRIER;
#  endif
}

static inline uint32_t
ttas_lock(ptlock_t* l)
{
  while (1)
    {
      while (*l == TTAS_LCKD)
	{
	  PAUSE;
	}

      if (CAS_UTYPE(l, TTAS_FREE, TTAS_LCKD) == TTAS_FREE)
	{
	  break;
	}
    }

  return 0;
}

static inline uint32_t
ttas_trylock(ptlock_t* l)
{
  return (CAS_UTYPE(l, TTAS_FREE, TTAS_LCKD) == TTAS_FREE);
}

static inline uint32_t
ttas_unlock(ptlock_t* l)
{
#  if defined(__tile__) 
  MEM_BARRIER;
#  endif
  COMPILER_NO_REORDER(*l = TTAS_FREE;);
  return 0;
}

#elif defined(TSX)			/* TSX lock */
#include <immintrin.h>
#  define PTLOCK_SIZE 32		/* choose 8, 16, 32, 64 */
#  define PASTER(x, y, z) x ## y ## z
#  define EVALUATE(sz) PASTER(uint, sz, _t)
#  define UTYPE  EVALUATE(PTLOCK_SIZE)
#  define PASTER2(x, y) x ## y
#  define EVALUATE2(sz) PASTER2(CAS_U, sz)
#  define CAS_UTYPE EVALUATE2(PTLOCK_SIZE)
typedef volatile UTYPE ptlock_t;
#  define INIT_LOCK(lock)                               tsx_init(lock)
#  define DESTROY_LOCK(lock)                    
#  define LOCK(lock)                                    tsx_lock(lock)
#  define TRYLOCK(lock)                                 tsx_trylock(lock)
#  define UNLOCK(lock)                                  tsx_unlock(lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)                            tsx_init(lock)
#  define GL_DESTROY_LOCK(lock)                 
#  define GL_LOCK(lock)                                 tsx_lock(lock)
#  define GL_TRYLOCK(lock)                              tsx_trylock(lock)
#  define GL_UNLOCK(lock)                               tsx_unlock(lock)

#  define TAS_FREE 0
#  define TAS_LCKD 1
#  define TSX_TRIES 3


#  define TSX_STATS_DEPTH 3
#  if defined(TSX_STATS) || defined(TSX_SMART)
#    define TSX_STATS_VARS \
__thread ticks my_tsx_trials[3] = {0, 0, 0},   \
               my_tsx_commits = 0,             \
               my_tsx_aborts[3] = {0, 0, 0}
#    define EXTERN_TSX_STATS_VARS \
extern __thread ticks my_tsx_trials[3], my_tsx_commits, my_tsx_aborts[3]

EXTERN_TSX_STATS_VARS;

#    define TSX_STATS_ABORTS(i)     if(i<TSX_STATS_DEPTH) my_tsx_aborts[i]++
#    define TSX_STATS_TRIALS(i)     if(i<TSX_STATS_DEPTH) my_tsx_trials[i]++
#    define TSX_STATS_COMMITS()     my_tsx_commits++
#  else /*if !defined TSX_STATS*/
#    define TSX_STATS_VARS
#    define EXTERN_TSX_STATS_VARS
#    define TSX_STATS_ABORTS(i) 
#    define TSX_STATS_TRIALS(i)
#    define TSX_STATS_COMMITS()
#  endif


#  ifdef TSX_SMART
#    define TSX_CONFLICT_TOO_HIGH                                      \
       ((my_tsx_commits<<1 < my_tsx_trials[0])                         \
         && (my_tsx_trials[0] > 100))
#  else
#    define TSX_CONFLICT_TOO_HIGH (0)
#  endif


#  ifdef TSX_ABORT_REASONS
#    define TSX_ABORT_REASONS_NUMBER 7
#    define TSX_ABORT_REASONS_VARS                                     \
            __thread ticks my_tsx_abort_reasons                        \
              [TSX_STATS_DEPTH][TSX_ABORT_REASONS_NUMBER]              \
            = {{0, 0, 0, 0, 0, 0, 0},                                  \
               {0, 0, 0, 0, 0, 0, 0},                                  \
               {0, 0, 0, 0, 0, 0, 0}}
#    define EXTERN_TSX_ABORT_REASONS_VARS                              \
            extern __thread ticks my_tsx_abort_reasons                 \
                     [TSX_STATS_DEPTH][TSX_ABORT_REASONS_NUMBER]
#    define TSX_ABORT_REASON(trial_no, status)                         \
       if (! (status & (_XABORT_EXPLICIT  | _XABORT_CAPACITY           \
                        | _XABORT_CONFLICT | _XABORT_DEBUG             \
                        | _XABORT_NESTED)))                            \
         my_tsx_abort_reasons[trial_no][5]++;                          \
       else                                                            \
       {                                                               \
         if (status & _XABORT_EXPLICIT)                                \
           my_tsx_abort_reasons[trial_no][0]++;                        \
         if (status & _XABORT_CONFLICT)                                \
           my_tsx_abort_reasons[trial_no][1]++;                        \
         if (status & _XABORT_CAPACITY)                                \
           my_tsx_abort_reasons[trial_no][2]++;                        \
         if (status & _XABORT_DEBUG)                                   \
           my_tsx_abort_reasons[trial_no][3]++;                        \
         if (status & _XABORT_NESTED)                                  \
           my_tsx_abort_reasons[trial_no][4]++;                        \
       }                                                               \
       if (status & _XABORT_RETRY)                                     \
         my_tsx_abort_reasons[trial_no][6]++;


EXTERN_TSX_ABORT_REASONS_VARS;

#  else /* if !defined TSX_ABORT_REASONS */
#    define TSX_ABORT_REASONS_VARS
#    define EXTERN_TSX_ABORT_REASONS_VARS
#    define TSX_ABORT_REASON(trial_no, reason)
#  endif


static __thread uint64_t tsx_depth = 0;
#  ifndef TSX_NESTED_TXN
#    define TSX_IN_TXN()                 (tsx_depth>0)
#    define TSX_DEPTH_INC()              (tsx_depth++)
#    define TSX_DEPTH_DEC()              (tsx_depth--)
#    define TSX_IS_COMMITTABLE()         (tsx_depth == 1)
#    define TSX_USE_CURRENT_TXN()        (tsx_depth > 1)
#  else /* if defined TSX_MULTIPLE_TXN */
#    define TSX_DEPTH_INC()              (tsx_depth++)
#    define TSX_DEPTH_DEC()              (tsx_depth--)
#    define TSX_IS_COMMITTABLE()         (1)
#    define TSX_USE_CURRENT_TXN()        (0)
#  endif


#  if TSX_PREFETCH==2
#    define TSX_PREFETCH_BEGIN()
#    define TSX_PREFETCH_FETCH_R(x)
#    define TSX_PREFETCH_FETCH_W(x)   ATOMIC_CAS_MB(x, 0, 1)
#    define TSX_PREFETCH_END()
#  elif TSX_PREFETCH==1
#    define TSX_PREFETCH_BEGIN()
#    define TSX_PREFETCH_FETCH_R(x)   asm volatile("prefetch %0" :: "m" (*(unsigned long *)x))
#    define TSX_PREFETCH_FETCH_W(x)   asm volatile("prefetchw %0" :: "m" (*(unsigned long *)x))
#    define TSX_PREFETCH_END()
#  else /*TSX_PREFETCH==0*/
#    define TSX_PREFETCH_BEGIN()
#    define TSX_PREFETCH_FETCH_R(x)
#    define TSX_PREFETCH_FETCH_W(x)
#    define TSX_PREFETCH_END()
#  endif


#  define TSX_CRITICAL_SECTION                 \
if (unlikely(!TSX_CONFLICT_TOO_HIGH))          \
{                                              \
  long status;                                 \
  int t;                                       \
  for (t = 0; t < TSX_TRIES; t++)              \
  {                                            \
    TSX_DEPTH_INC();                           \
    if (!TSX_USE_CURRENT_TXN())                \
    {                                          \
      TSX_STATS_TRIALS(t);                     \
    }                                          \
    if (TSX_USE_CURRENT_TXN() ||               \
      (status = _xbegin()) == _XBEGIN_STARTED)

#  define TSX_AFTER                            \
    else                                       \
    {                                          \
      TSX_DEPTH_DEC();                         \
      TSX_STATS_ABORTS(t);                     \
      TSX_ABORT_REASON(t, status);             \
      if (!(status & _XABORT_RETRY))           \
        break;                                 \
      pause_rep((1<<((1+t)*5)) & 255);         \
    }                                          \
  }                                            \
}

#  define TSX_END_EXPLICIT_ABORTS_GOTO(x)      \
    else                                       \
    {                                          \
      TSX_DEPTH_DEC();                         \
      TSX_STATS_ABORTS(t);                     \
      TSX_ABORT_REASON(t, status);             \
      if (status & _XABORT_EXPLICIT)           \
        goto x;                                \
      if (!(status & _XABORT_RETRY))           \
        break;                                 \
      pause_rep((1<<((1+t)*5)) & 255);         \
    }                                          \
  }                                            \
}

#  define TSX_ABORT  _xabort(0xff);

#  define TSX_COMMIT                           \
if (TSX_IS_COMMITTABLE()) {                    \
  _xend();                                     \
  TSX_STATS_COMMITS();                         \
}                                              \
TSX_DEPTH_DEC();

static ptlock_t global_tsx_lock = TAS_FREE; 
#  define TSX_LOCK()           tsx_lock(&global_tsx_lock)

#  define TSX_UNLOCK(commit)                                   \
if (global_tsx_lock == TAS_FREE)                               \
  {                                                            \
    if (commit) {TSX_COMMIT;}                                  \
    else {TSX_ABORT;}                                          \
  }                                                            \
else                                                           \
  {                                                            \
    __atomic_clear(&global_tsx_lock, __ATOMIC_RELEASE);        \
  }

#  define TSX_FALLBACK_LOCK()    tsx_fallback_lock(&global_tsx_lock);
#  define TSX_FALLBACK_UNLOCK()  tsx_fallback_unlock(&global_tsx_lock);

extern __thread void *tsx_markedNodes[5];
extern __thread uint64_t tsx_markedNo;
#  define TSX_WITH_FALLBACK_VARS               \
     __thread void *tsx_markedNodes[5];        \
     __thread uint64_t tsx_markedNo;


#  ifndef NO_TSX
#    define TSX_WITH_FALLBACK_BEGIN()            twf_begin()
#    define TWF_SLOW_PATH_END()                                \
     int tsx_i;                                                \
     for(tsx_i = 0; tsx_i < tsx_markedNo; tsx_i++)             \
     {                                                         \
       ((DS_NODE*)tsx_markedNodes[tsx_i])->locked = 0;         \
     }                                                         \
     tsx_markedNo=0;

#    define TSX_PROTECT_NODE(np, failPath)                     \
     if (TSX_IN_TXN())                                         \
     {                                                         \
       if (np->locked)                                         \
       {                                                       \
         TSX_ABORT;                                            \
       }                                                       \
     }                                                         \
     else                                                      \
     {                                                         \
       if(!CAS_U32_bool(&np->locked, 0, 1))                   \
       {                                                       \
         TWF_SLOW_PATH_END();                                  \
         goto failPath;                                        \
       }                                                       \
       else                                                    \
       {                                                       \
         tsx_markedNodes[tsx_markedNo] = (void *) np;          \
         tsx_markedNo++;                                       \
       }                                                       \
     }

#    define TSX_VALIDATE(cond, failPath)                       \
     if (!(cond))                                              \
     {                                                         \
       if (TSX_IN_TXN())                                       \
       {                                                       \
         TSX_ABORT;                                            \
       }                                                       \
       else                                                    \
       {                                                       \
         TWF_SLOW_PATH_END();                                  \
         goto failPath;                                        \
       }                                                       \
     }

#    define TSX_WITH_FALLBACK_END()                            \
     if (TSX_IN_TXN())                                         \
     {                                                         \
       TSX_COMMIT;                                             \
     }                                                         \
     else                                                      \
     {                                                         \
       TWF_SLOW_PATH_END();                                    \
     }

#  else
#    define TSX_WITH_FALLBACK_BEGIN()
#    define TWF_SLOW_PATH_END()                                \
     int tsx_i;                                                \
     for(tsx_i = 0; tsx_i < tsx_markedNo; tsx_i++)             \
     {                                                         \
       ((DS_NODE*)tsx_markedNodes[tsx_i])->locked = 0;         \
     }                                                         \
     tsx_markedNo=0;

#    define TSX_PROTECT_NODE(np, failPath)                     \
     if(!CAS_U32_bool(&np->locked, 0, 1))                      \
     {                                                         \
       TWF_SLOW_PATH_END();                                    \
       goto failPath;                                          \
     }                                                         \
     else                                                      \
     {                                                         \
       tsx_markedNodes[tsx_markedNo] = (void *) np;            \
       tsx_markedNo++;                                         \
     }

#    define TSX_VALIDATE(cond, failPath)                       \
     if (!(cond))                                              \
     {                                                         \
       TWF_SLOW_PATH_END();                                    \
       goto failPath;                                          \
     }

#    define TSX_WITH_FALLBACK_END()                            \
     TWF_SLOW_PATH_END();
#  endif


static inline void
tsx_init(ptlock_t* l)
{
  *l = TAS_FREE;
#  if defined(__tile__)
  MEM_BARRIER;
#  endif
}

static inline void
twf_begin()
{
  TSX_CRITICAL_SECTION
  {
    return;
  }
  TSX_AFTER;
}

static inline uint32_t
tsx_CAS(volatile size_t * addr, volatile size_t oldValue, volatile size_t newValue)
{
  TSX_PREFETCH_BEGIN();
  TSX_PREFETCH_FETCH_W(addr);
  TSX_PREFETCH_END();

  TSX_CRITICAL_SECTION
  {
    if (*addr != oldValue)
    {
      TSX_ABORT;
    }
    *addr = newValue;
    TSX_COMMIT;
    return 1;
  }
  TSX_AFTER;
  /*Transactionalization of the CAS failed. Apply actual CAS*/
  return ATOMIC_CAS_MB(addr, oldValue, newValue);
}

static inline uint64_t
tsx_CAS_PTR(volatile uint64_t * addr, volatile uint64_t oldValue, volatile uint64_t newValue)
{
  TSX_PREFETCH_BEGIN();
  TSX_PREFETCH_FETCH_W(addr);
  TSX_PREFETCH_END();

  TSX_CRITICAL_SECTION
  {
    if (*addr != oldValue)
    {
      TSX_ABORT;
    }
    *addr = newValue;
    TSX_COMMIT;
    return oldValue;
  }
  TSX_AFTER;
  /*Transactionalization of the CAS failed. Apply actual CAS*/
  return CAS_U64(addr, oldValue, newValue);
}

static inline uint32_t
tsx_lock(ptlock_t* l)
{
  while (*l != TAS_FREE)
  {
    PAUSE;
  }
  TSX_CRITICAL_SECTION
  {
    if (*l == TAS_FREE)
    {
      return 0;
    }
    TSX_ABORT;
  }
  TSX_AFTER;

  while (__atomic_exchange_n(l, TAS_LCKD, __ATOMIC_ACQUIRE))
  {
    PAUSE;
  }
  return 0;
}

static inline uint32_t
tsx_fallback_lock(ptlock_t* l)
{
  while (*l != TAS_FREE || __atomic_exchange_n(l, TAS_LCKD, __ATOMIC_ACQUIRE))
  {
    PAUSE;
  }
  return 0;
}

static inline uint32_t
tsx_trylock(ptlock_t* l)
{
  return ((ptlock_t*)tsx_CAS_PTR((uint64_t*)l, (uint64_t)TAS_FREE, (uint64_t)TAS_LCKD) == TAS_FREE);
}

static inline uint32_t
tsx_unlock(ptlock_t* l)
{
  if (*l == TAS_FREE)
  {
    TSX_COMMIT;
  }
  else
  {
    __atomic_clear(l, __ATOMIC_RELEASE);
  }
  return 0;
}

static inline uint32_t
tsx_fallback_unlock(ptlock_t* l)
{
  __atomic_clear(l, __ATOMIC_RELEASE);
  return 0;
}


#elif defined(TICKET)			/* ticket lock */

struct ticket_st
{
  uint32_t ticket;
  uint32_t curr;
  /* char padding[40]; */
};

typedef struct ticket_st ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define INIT_LOCK(lock)				ticket_init((volatile ptlock_t*) lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					ticket_lock((volatile ptlock_t*) lock)
#  define TRYLOCK(lock)					ticket_trylock((volatile ptlock_t*) lock)
#  define UNLOCK(lock)					ticket_unlock((volatile ptlock_t*) lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				ticket_init((volatile ptlock_t*) lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					ticket_lock((volatile ptlock_t*) lock)
#  define GL_TRYLOCK(lock)				ticket_trylock((volatile ptlock_t*) lock)
#  define GL_UNLOCK(lock)				ticket_unlock((volatile ptlock_t*) lock)

static inline void
ticket_init(volatile ptlock_t* l)
{
  l->ticket = l->curr = 0;
#  if defined(__tile__)
  MEM_BARRIER;
#  endif
}

#  define TICKET_BASE_WAIT 512
#  define TICKET_MAX_WAIT  4095
#  define TICKET_WAIT_NEXT 128

static inline uint32_t
ticket_lock(volatile ptlock_t* l)
{
  uint32_t ticket = FAI_U32(&l->ticket);

#  if defined(OPTERON_OPTIMIZE)
  uint32_t wait = TICKET_BASE_WAIT;
  uint32_t distance_prev = 1;
  while (1)
    {
      PREFETCHW(l);
      uint32_t cur = l->curr;
      if (cur == ticket)
	{
	  break;
	}
      uint32_t distance = (ticket > cur) ? (ticket - cur) : (cur - ticket);

      if (distance > 1)
      	{
	  if (distance != distance_prev)
	    {
	      distance_prev = distance;
	      wait = TICKET_BASE_WAIT;
	    }

	  nop_rep(distance * wait);
	  wait = (wait + TICKET_BASE_WAIT) & TICKET_MAX_WAIT;
      	}
      else
	{
	  nop_rep(TICKET_WAIT_NEXT);
	}

      if (distance > 20)
      	{
      	  sched_yield();
      	}
    }

#  else  /* !OPTERON_OPTIMIZE */
  PREFETCHW(l);
  while (ticket != l->curr)
    {
      PREFETCHW(l);
      PAUSE;
    }

#  endif
  return 0;
}

static inline uint32_t
ticket_trylock(volatile ptlock_t* l)
{
  volatile uint64_t tc = *(volatile uint64_t*) l;
  volatile struct ticket_st* tp = (volatile struct ticket_st*) &tc;

  if (tp->curr == tp->ticket)
    {
      COMPILER_NO_REORDER(uint64_t tc_old = tc;);
      tp->ticket++;
      return CAS_U64((uint64_t*) l, tc_old, tc) == tc_old;
    }
  else
    {
      return 0;
    }
}

static inline uint32_t
ticket_unlock(volatile ptlock_t* l)
{
#  if defined(__tile__)
  MEM_BARRIER;
#  endif
  PREFETCHW(l);
  COMPILER_NO_REORDER(l->curr++;);
  return 0;
}

#elif defined(HTICKET)		/* Hierarchical ticket lock */

#  include "htlock.h"

#  define INIT_LOCK(lock)				init_alloc_htlock((htlock_t*) lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					htlock_lock((htlock_t*) lock)
#  define UNLOCK(lock)					htlock_release((htlock_t*) lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				init_alloc_htlock((htlock_t*) lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					htlock_lock((htlock_t*) lock)
#  define GL_UNLOCK(lock)				htlock_release((htlock_t*) lock)

#elif defined(CLH)		/* CLH lock */

#  include "clh.h"

#  define INIT_LOCK(lock)				init_alloc_clh((clh_lock_t*) lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					clh_local_p.my_pred = \
    clh_acquire((volatile struct clh_qnode **) lock, clh_local_p.my_qnode);
#  define UNLOCK(lock)					clh_local_p.my_qnode = \
    clh_release(clh_local_p.my_qnode, clh_local_p.my_pred);
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				init_alloc_clh((clh_lock_t*) lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					clh_local_p.my_pred = \
    clh_acquire((volatile struct clh_qnode **) lock, clh_local_p.my_qnode);
#  define GL_UNLOCK(lock)				clh_local_p.my_qnode = \
    clh_release(clh_local_p.my_qnode, clh_local_p.my_pred);

#elif defined(MCS)		/* MCS lock */

#  include "mcs.h"

typedef mcs_lock_t ptlock_t;
#define LOCK_LOCAL_DATA                                 __thread mcs_lock_local_t __mcs_local

#  define INIT_LOCK(lock)				mcs_lock_init((mcs_lock_t*) lock, NULL)
#  define DESTROY_LOCK(lock)			        mcs_lock_destroy((mcs_lock_t*) lock)
#  define LOCK(lock)					mcs_lock_lock((mcs_lock_t*) lock)
#  define UNLOCK(lock)					mcs_lock_unlock((mcs_lock_t*) lock)     
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				mcs_lock_init((mcs_lock_t*) lock, NULL) 
#  define GL_DESTROY_LOCK(lock)				mcs_lock_destroy((mcs_lock_t*) lock)	  
#  define GL_LOCK(lock)					mcs_lock_lock((mcs_lock_t*) lock)	  
#  define GL_UNLOCK(lock)				mcs_lock_unlock((mcs_lock_t*) lock)     

#elif defined(NONE)			/* no locking */

struct none_st
{
  uint32_t nothing;
};

typedef struct none_st ptlock_t;
#  define LOCK_LOCAL_DATA                                
#  define INIT_LOCK(lock)				none_init((volatile ptlock_t*) lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)					none_lock((volatile ptlock_t*) lock)
#  define TRYLOCK(lock)					none_trylock((volatile ptlock_t*) lock)
#  define UNLOCK(lock)					none_unlock((volatile ptlock_t*) lock)
/* GLOBAL lock */
#  define GL_INIT_LOCK(lock)				none_init((volatile ptlock_t*) lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)					none_lock((volatile ptlock_t*) lock)
#  define GL_TRYLOCK(lock)				none_trylock((volatile ptlock_t*) lock)
#  define GL_UNLOCK(lock)				none_unlock((volatile ptlock_t*) lock)

static inline void
none_init(volatile ptlock_t* l)
{
}

static inline uint32_t
none_lock(volatile ptlock_t* l)
{
  return 0;
}

static inline uint32_t
none_trylock(volatile ptlock_t* l)
{
  return 1;
}

static inline uint32_t
none_unlock(volatile ptlock_t* l)
{
  return 0;
}

#endif



/* --------------------------------------------------------------------------------------------------- */
/* GLOBAL LOCK --------------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------------------------- */


#if defined(LL_GLOBAL_LOCK)
#  define ND_GET_LOCK(nd)                 nd /* LOCK / UNLOCK are not defined in any case ;-) */

#  undef INIT_LOCK
#  undef DESTROY_LOCK
#  undef LOCK
#  undef UNLOCK
#  undef PREFETCHW_LOCK

#  define INIT_LOCK(lock)
#  define DESTROY_LOCK(lock)			
#  define LOCK(lock)
#  define UNLOCK(lock)
#  define PREFETCHW_LOCK(lock)

#  define INIT_LOCK_A(lock)       GL_INIT_LOCK(lock)
#  define DESTROY_LOCK_A(lock)    GL_DESTROY_LOCK(lock)			
#  define LOCK_A(lock)            GL_LOCK(lock)
#  define TRYLOCK_A(lock)         GL_TRYLOCK(lock)
#  define UNLOCK_A(lock)          GL_UNLOCK(lock)
#  define PREFETCHW_LOCK_A(lock)  

/* optik */
#  define OPTIK_WITHOUT_GL_DO(a)             
#  define OPTIK_WITH_GL_DO(a)                 a

#else  /* !LL_GLOBAL_LOCK */
#  define ND_GET_LOCK(nd)                 &nd->lock

#  undef GL_INIT_LOCK
#  undef GL_DESTROY_LOCK
#  undef GL_LOCK
#  undef GL_UNLOCK

#  define GL_INIT_LOCK(lock)
#  define GL_DESTROY_LOCK(lock)			
#  define GL_LOCK(lock)
#  define GL_UNLOCK(lock)

#  define INIT_LOCK_A(lock)       INIT_LOCK(lock)
#  define DESTROY_LOCK_A(lock)    DESTROY_LOCK(lock)			
#  define LOCK_A(lock)            LOCK(lock)
#  define TRYLOCK_A(lock)         TRYLOCK(lock)
#  define UNLOCK_A(lock)          UNLOCK(lock)
#  define PREFETCHW_LOCK_A(lock)  PREFETCHW_LOCK(lock)

/* optik */
#  define OPTIK_WITHOUT_GL_DO(a)              a
#  define OPTIK_WITH_GL_DO(a)                 


#endif

#endif	/* _LOCK_IF_H_ */
