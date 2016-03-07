#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>

#define UTYPE unsigned int
#define PASTER2(x, y) x ## y
#define EVALUATE2(sz) PASTER2(CAS_U, sz)
#define CAS_UTYPE EVALUATE2(PTLOCK_SIZE)
typedef volatile UTYPE ptlock_t;
#define TAS_FREE 0
#define TAS_LCKD 1
#define TRUE 1
#define FALSE 0

#define _mm_pause() asm volatile ("nop")
#define PAUSE _mm_pause() 

__thread unsigned int fail_count=0, success_count=0, try_count=0; 
__thread ptlock_t locks[32];
__thread ptlock_t thread_lock;

static inline void
tas_init_tsx(ptlock_t* l)
{
  *l = TAS_FREE;
}

static inline unsigned int
tas_lock_tsx(ptlock_t* l)
{
  try_count++;
  int _xbegin_tries = 3;
  int t;
  for (t = 0; t < _xbegin_tries; t++)
  {
    while (*l != TAS_FREE)
    {
      PAUSE;
    }

    long status;
    if ((status = _xbegin()) == _XBEGIN_STARTED)
    {
      if (*l == TAS_FREE)
      {
        return 0;
      }
    _xabort(0xff);
    }
    else
    {
      if (status & _XABORT_EXPLICIT)
      {
        break;
      }
      PAUSE;
    }
  }

  fail_count++;
  while (__atomic_exchange_n(l, TAS_LCKD, __ATOMIC_ACQUIRE))
  {
    PAUSE;
  }

  return 0;
}

static inline int
tas_unlock_tsx(ptlock_t* l)
{
  if (*l == TAS_FREE)
  {
    _xend();
    success_count++;
    return TRUE;
  }
  else
  {
    __atomic_clear(l, __ATOMIC_RELEASE);
    return FALSE;
  }
}

static inline void
single_thread_multiple_locks(unsigned int number_of_locks, unsigned int repetitions)
{
  unsigned int i, j;
  int isTxn[repetitions][10];
  for (i=0; i<repetitions; i++)
  {
    for(j=1; j <= number_of_locks; j++)
    {
      switch(j)
      {
      case 1:
        tas_lock_tsx(&locks[0]);
        isTxn[i][0] = tas_unlock_tsx(&locks[0]);
        break;
      case 2:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][1] = tas_unlock_tsx(&locks[0]);
        break;
      case 3:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][2] = tas_unlock_tsx(&locks[0]);
        break;
      case 4:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][3] = tas_unlock_tsx(&locks[0]);
        break;
      case 5:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][4] = tas_unlock_tsx(&locks[0]);
        break;
      case 6:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_lock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][5] = tas_unlock_tsx(&locks[0]);
        break;
      case 7:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_lock_tsx(&locks[5]);
        tas_lock_tsx(&locks[6]);
        tas_unlock_tsx(&locks[6]);
        tas_unlock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][6] = tas_unlock_tsx(&locks[0]);
        break;
      case 8:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_lock_tsx(&locks[5]);
        tas_lock_tsx(&locks[6]);
        tas_lock_tsx(&locks[7]);
        tas_unlock_tsx(&locks[7]);
        tas_unlock_tsx(&locks[6]);
        tas_unlock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][7] = tas_unlock_tsx(&locks[0]);
        break;
      case 9:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_lock_tsx(&locks[5]);
        tas_lock_tsx(&locks[6]);
        tas_lock_tsx(&locks[7]);
        tas_lock_tsx(&locks[8]);
        tas_unlock_tsx(&locks[8]);
        tas_unlock_tsx(&locks[7]);
        tas_unlock_tsx(&locks[6]);
        tas_unlock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][8] = tas_unlock_tsx(&locks[0]);
        break;
      case 10:
        tas_lock_tsx(&locks[0]);
        tas_lock_tsx(&locks[1]);
        tas_lock_tsx(&locks[2]);
        tas_lock_tsx(&locks[3]);
        tas_lock_tsx(&locks[4]);
        tas_lock_tsx(&locks[5]);
        tas_lock_tsx(&locks[6]);
        tas_lock_tsx(&locks[7]);
        tas_lock_tsx(&locks[8]);
        tas_lock_tsx(&locks[9]);
        tas_unlock_tsx(&locks[9]);
        tas_unlock_tsx(&locks[8]);
        tas_unlock_tsx(&locks[7]);
        tas_unlock_tsx(&locks[6]);
        tas_unlock_tsx(&locks[5]);
        tas_unlock_tsx(&locks[4]);
        tas_unlock_tsx(&locks[3]);
        tas_unlock_tsx(&locks[2]);
        tas_unlock_tsx(&locks[1]);
        isTxn[i][9] = tas_unlock_tsx(&locks[0]);
        break;
      default:
	printf("Nesting %d locks is not implemented\n", number_of_locks);
      }
    }
  }
  /*Print out the results*/
  printf("/**************************************************/\n");
  printf("Single Core - Nesting Keys:\n");
  printf("Number of repetitions: %d\n", repetitions);
  for (j=0; j<number_of_locks; j++)
  {
    unsigned int txnal=0, locked=0;
    printf("Number of Keys: %d\n", j+1);
    for (i=0; i<repetitions; i++)
    {
      isTxn[i][j] ? txnal++ : locked++;
    }
    printf("%d transactional and %d locked executions\n", txnal, locked);
  }
  printf("/**************************************************/\n");
}

/*Reads up to max_reads items in critical sections and prints whether they were transactionalized.
 * \details Note that the iterator is read and written multple times in each txn.*/
static inline void
single_thread_multiple_reads(unsigned int max_reads, unsigned int repetitions, unsigned int allignment)
{
  unsigned int i, j, k;
  unsigned int data[max_reads*allignment], sum;
  /*Initilize the data*/
  for (j=0; j<max_reads; j++)
  {
    data[j*allignment]=j;
  }
  /*Execute the critical section*/
  int isTxn[repetitions][max_reads];
  for (i=0; i<repetitions; i++)
  {
    for(j=1; j <= max_reads; j++)
    {
      //Initilize the sum
      sum = 0;
      //Enter the critical section
      tas_lock_tsx(&locks[0]);
      //Read j many data items
      for(k=0; k <= j; k++)
      {
        sum += data[k*allignment];
      }
      //Exit the critical section
      isTxn[i][j] = tas_unlock_tsx(&locks[0]);
      printf("sum: %d.", sum);
    }
  }
  /*Print out the results*/
  printf("/**************************************************/\n");
  printf("Single Core - Multiple Reads:\n");
  printf("Number of repetitions: %d\n", repetitions);
  for (j=0; j<max_reads; j++)
  {
    unsigned int txnal=0, locked=0;
    printf("Number of Reads: %d\n", j+1);
    for (i=0; i<repetitions; i++)
    {
      isTxn[i][j] ? txnal++ : locked++;
    }
    printf("%d transactional and %d locked executions\n", txnal, locked);
  }
  printf("/**************************************************/\n");
}

/*Reads up to max_writes items in critical sections and prints whether they were transactionalized.
 * \details the iterator is also read and written in each txn. Each item is read before writing into them */
static inline void
single_thread_multiple_writes(unsigned int max_writes, unsigned int repetitions, unsigned int allignment)
{
  unsigned int i, j, k;
  unsigned int data[max_writes*allignment];
  /*Initilize the data*/
  for (j=0; j<max_writes; j++)
  {
    data[j*allignment]=j;
  }
  /*Execute the critical section*/
  int isTxn[repetitions][max_writes];
  for (i=0; i<repetitions; i++)
  {
    for(j=1; j <= max_writes; j++)
    {
      //Enter the critical section
      tas_lock_tsx(&locks[0]);
      //Read j many data items
      
      for(k=0; k <= j; k++)
      {
        data[k*allignment]++;
      }
      //Exit the critical section
      isTxn[i][j] = tas_unlock_tsx(&locks[0]);
    }
  }
  /*Print out the results*/
  printf("/**************************************************/\n");
  printf("Single Core - Multiple Writes:\n");
  printf("Number of repetitions: %d\n", repetitions);
  for (j=0; j<max_writes; j++)
  {
    unsigned int txnal=0, locked=0;
    printf("Number of Writes: %d\n", j+1);
    for (i=0; i<repetitions; i++)
    {
      isTxn[i][j] ? txnal++ : locked++;
    }
    printf("%d transactional and %d locked executions\n", txnal, locked);
  }
  printf("/**************************************************/\n");
}

int main (void)
{
  /*Nested locks*/
  single_thread_multiple_locks(10, 20);

  /*Multiple reads*/
//  single_thread_multiple_reads(9000, 20, 1); //locks towards the end of 7000s.
//  single_thread_multiple_reads(1200, 20, 8); //locks towards the end of 900s.
//  single_thread_multiple_reads(8000, 20, 16); //locks around 480s.
//  single_thread_multiple_reads(12000, 20, 32); //locks around ... 
//  single_thread_multiple_reads(12000, 20, 64); //locks around ...
//  single_thread_multiple_reads(12000, 20, 128); //locks around 7000-8000
//  single_thread_multiple_reads(6000, 20, 256); //locks around 4000
//  single_thread_multiple_reads(3000, 20, 512); //locks around 2000
//  single_thread_multiple_reads(1000, 20, 1024); //locks around the end of 900s
//  single_thread_multiple_reads(1000, 20, 2048); //locks around the end of 900s

  /*Multiple writes*/
//  single_thread_multiple_writes(9000, 20, 1); //occasional locks after 5500, all the locks after 6960.
//  single_thread_multiple_writes(1200, 20, 8); //some locks after 560, all the locks after 818.
//  single_thread_multiple_writes(1200, 20, 16); //some locks after 258, all the locks after 434.
//  single_thread_multiple_writes(2000, 20, 32); //some locks after 114, all the locks after 210.
//  single_thread_multiple_writes(2000, 20, 64); //some locks after 67, all the locks after 120.
//  single_thread_multiple_writes(2000, 20, 128); //some locks after 40, all the locks after 65.
//  single_thread_multiple_writes(2000, 20, 256); //some locks after 22, all the locks after 33.
//  single_thread_multiple_writes(2000, 20, 512); //some locks after 13, all the locks after 17.
//  single_thread_multiple_writes(2000, 20, 1024); //some locks after 7, all the locks after 9.
//  single_thread_multiple_writes(20, 20, 2048); //some locks after 7, all the locks after 9.
//  single_thread_multiple_writes(20, 20, 4096); //some locks after 7, all the locks after 9.
  return 0;
}
