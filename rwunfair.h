#define _GNU_SOURCE   
#include <stdio.h>    
#include <stdlib.h>   
#include <stdbool.h> 
#include <assert.h>   
#include <string.h>   
#include <errno.h>    
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>    
#include <fcntl.h>         
#include <pthread.h>

// Struct che utilizzo per l'accesso concorrente di lettori e scrittori alla hash table
typedef struct {
  int readersHT; 
	bool writingHT; 
  pthread_cond_t condHT;   
  pthread_mutex_t mutexHT; 
} rwHT;

// void rw_init(rwHT *z);
void read_lock(rwHT *z);
void read_unlock(rwHT *z);
void write_lock(rwHT *z);
void write_unlock(rwHT *z);