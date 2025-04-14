#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#define MAX_LEN 8192


//MALLOC, STRDUP MACRO DEFINITION

#define MALLOC (ptr, size)                  \
    do{                                     \
        (ptr) = malloc(size);               \
        if((ptr) == NULL){                  \
            perror("Malloc failed\n");      \
            exit(EXIT_FAILURE);             \
        }                                   \
    }while(0)

#define STRDUP(dst, src)                    \
    do{                                     \
        (dst) = strdup(src);                \
        if((dst) == NULL){                  \
            perror("Strdup failed\n");      \
            exit(EXIT_FAILURE);             \
        }                                   \
    }while(0)


typedef struct Node{
    int seq;
    char *line;
    struct Node *next;
} Node;

Node *result_head = Null;
pthread_mutex_t res_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t res_cond = PTHREAD_COND_INITIALIZER;

int total_lines = 0;
int next_seq = 0;
int read_done = 0;

typedef struct Worker{
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char *line; //line string
    int seq;
    int job; //has_job = 1, no_job = 0
    int terminate;
} Worker;

Worker *workers = NULL;
int num_workers = 0;

void *worker_thread_func(void *arg);
void *receiver_thread_func(void *arg);

int main(int argc, char *argv[]){
    if(argc != 3){
        fprintf(stderr, "Use proper ComandLine Argument Format!\n");
        exit(EXIT_FAILURE);
    }
    if(strcmp(argv[1], "-n") != 0){
        fprintf(stderr, "Invalid parameter!\n");
        exit(EXIT_FAILURE);
    }

    num_workers = atoi(argv[2]);

    if(num_workers <= 0){
        fprintf(stderr, "Invalid number of threads!\n");
        exit(EXIT_FAILURE);
    }

    MALLOC(workers, num_workers * sizeof(Worker));
    for (int i = 0; i < num_workers; i++){
        pthread_mutex_init(&workers[i].lock, NULL);
        pthread_cond_init(&workers[i].cond, NULL);
        workers[i].line = NULL;
        workers[i].job = 0;
        workers[i].terminate = 0;
    }

    pthread_t *worker_threads = NULL;
    MALLOC(worker_threads, num_workers * sizeof(pthread_t));

    for(int i = 0; i < num_workers; i++){
        int *arg = NULL;
        MALLOC(arg, sizeof(int));
        *arg = i;

        if(pthread_create(&worker_thread[i], NULL, worker_thread_func, arg) != 0) {
            perror("pthread create");
            exit(EXIT_FAILURE);
        }
    }

    //Create Receiver Thread
