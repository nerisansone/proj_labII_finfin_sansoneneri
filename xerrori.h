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

// termina programma
void termina(const char *s); 
void xtermina(const char *s, int linea, char *file); 

// operazioni su FILE *
FILE *xfopen(const char *path, const char *mode, int linea, char *file);

// operazioni su file descriptors
void xclose(int fd, int linea, char *file);

// operazioni su processi
pid_t xfork(int linea, char *file);
pid_t xwait(int *status, int linea, char *file);
// pipes
int xpipe(int pipefd[2], int linea, char *file);


// memoria condivisa POSIX
int xshm_open(const char *name, int oflag, mode_t mode, int linea, char *file);
int xshm_unlink(const char *name, int linea, char *file);
int xftruncate(int fd, off_t length, int linea, char *file);
void *simple_mmap(size_t length, int fd, int linea, char *file);
int xmunmap(void *addr, size_t length, int linea, char *file);

// semafori POSIX
sem_t *xsem_open(const char *name, int oflag, mode_t mode, unsigned int value, int linea, char *file);
int xsem_unlink(const char *name, int linea, char *file);
int xsem_close(sem_t *sem, int linea, char *file);
int xsem_init(sem_t *sem, int pshared, unsigned int value, int linea, char *file);
int xsem_destroy(sem_t *sem, int linea, char *file);
int xsem_post(sem_t *sem, int linea, char *file);
int xsem_wait(sem_t *sem, int linea, char *file);

// thread
void xperror(int en, char *msg);

int xpthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg, int linea, char *file);
int xpthread_join(pthread_t thread, void **retval, int linea, char *file);

// mutex 
int xpthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr, int linea, char *file);
int xpthread_mutex_destroy(pthread_mutex_t *mutex, int linea, char *file);
int xpthread_mutex_lock(pthread_mutex_t *mutex, int linea, char *file);
int xpthread_mutex_unlock(pthread_mutex_t *mutex, int linea, char *file);
