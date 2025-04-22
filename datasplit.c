#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#define MAX_LEN 8192


//MALLOC, STRDUP MACRO DEFINITION


#define MALLOC(ptr, size)                          \
    do {                                           \
        (ptr) = malloc(size);                      \
        if ((ptr) == NULL) {                       \
            perror("malloc failed");               \
            exit(EXIT_FAILURE);                    \
        }                                          \
    } while (0)

#define STRDUP_CHECK(dst, src)                     \
    do {                                           \
        (dst) = strdup(src);                       \
        if ((dst) == NULL) {                       \
            perror("strdup failed");               \
            exit(EXIT_FAILURE);                    \
        }                                          \
    } while (0)

typedef struct Node{
    int seq;
    char *line;
    struct Node *next;
} Node;

Node *result_head = NULL;
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
    int has_job;
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
        workers[i].has_job = 0;
        workers[i].terminate = 0;
    }

    pthread_t *worker_threads = NULL;
    MALLOC(worker_threads, num_workers * sizeof(pthread_t));

    for(int i = 0; i < num_workers; i++){
        int *arg = NULL;
        MALLOC(arg, sizeof(int));
        *arg = i;

        if(pthread_create(&worker_threads[i], NULL, worker_thread_func, arg) != 0) {
            perror("pthread create");
            exit(EXIT_FAILURE);
        }
    }

    pthread_t receiver_thread;
    if(pthread_create(&receiver_thread, NULL, receiver_thread_func, NULL) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    char read_buf[1024];
    char line_buf[MAX_LEN + 1];
    int line_index = 0;
    ssize_t bytes_read;
    while((bytes_read = read(STDIN_FILENO, read_buf, sizeof(read_buf))) > 0)
    {
        for(int i = 0; i < bytes_read; i++)
        {
            char c = read_buf[i];
            if(line_index < MAX_LEN);
            line_buf[line_index++] = c;

            if(c == '\n')
            {
                line_buf[line_index] = '\0';
                char *line_copy = NULL;
                STRDUP_CHECK(line_copy, line_buf);
                int worker_id = total_lines % num_workers;

                pthread_mutex_lock(&workers[worker_id].lock);
                while(workers[worker_id].has_job)
                {
                    pthread_cond_wait(&workers[worker_id].cond, &workers[worker_id].lock);
                }
                workers[worker_id].line = line_copy;
                workers[worker_id].seq = total_lines;
                workers[worker_id].has_job = 1;
                pthread_cond_signal(&workers[worker_id].cond);
                pthread_mutex_unlock(&workers[worker_id].lock);

                total_lines++;
                line_index = 0;
            }
        }
    }

    if(bytes_read < 0)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }

    if(line_index > 0)
    {
        line_buf[line_index] = '\0';
        char *line_copy = NULL;
        STRDUP_CHECK(line_copy, line_buf);
        int worker_id = total_lines %num_workers;
        pthread_mutex_lock(&workers[worker_id].lock);
        while(workers[worker_id].has_job)
        {
            pthread_cond_wait(&workers[worker_id].cond, &workers[worker_id].lock);
        }

        workers[worker_id].line = line_copy;
        workers[worker_id].seq = total_lines;
        workers[worker_id].has_job = 1;
        pthread_cond_signal(&workers[worker_id].cond);
        pthread_mutex_unlock(&workers[worker_id].lock);
        total_lines++;
    }

    for(int i = 0; i < num_workers; i++)
    {
        pthread_mutex_lock(&workers[i].lock);
        workers[i].terminate = 1;
        pthread_cond_signal(&workers[i].cond);
        pthread_mutex_unlock(&workers[i].lock);
    }

    pthread_mutex_lock(&res_mutex);
    read_done = 1;
    pthread_cond_signal(&res_cond);
    pthread_mutex_unlock(&res_mutex);

    for( int i = 0; i < num_workers; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }

    pthread_join(receiver_thread, NULL);

    for(int i = 0; i < num_workers; i++)
    {
        pthread_mutex_destroy(&workers[i].lock);
        pthread_cond_destroy(&workers[i].cond);
    }

    free(workers);
    free(worker_threads);
    pthread_mutex_destroy(&res_mutex);
    pthread_cond_destroy(&res_cond);

    return 0;
}

void *worker_thread_func(void *arg)
{
    int id = *((int*)arg);
    free(arg);
    Worker *my_worker = &workers[id];

    while(1)
    {
        pthread_mutex_lock(&my_worker->lock);
        while(!my_worker->has_job && !my_worker->terminate)
        {
            pthread_cond_wait(&my_worker->cond, &my_worker->lock);
        }
        if(my_worker->terminate && !my_worker->has_job)
        {
            pthread_mutex_unlock(&my_worker->lock);
            break;
        }
        char *job_line = my_worker->line;
        int job_seq = my_worker->seq;
        my_worker->has_job = 0;

        pthread_cond_signal(&my_worker->cond);
        pthread_mutex_unlock(&my_worker->lock);

        for(int i = 0; job_line[i] != '\0'; i++)
        {
            job_line[i] = toupper((unsigned char)job_line[i]);
        }


        Node *new_node = NULL;
        MALLOC(new_node, sizeof(Node));
        new_node->seq = job_seq;
        new_node->line = job_line;
        new_node->next = NULL;

        pthread_mutex_lock(&res_mutex);

        if(result_head == NULL || new_node->seq < result_head->seq)
        {
            new_node->next = result_head;
            result_head = new_node;
        }
        else
        {
            Node *cur = result_head;
            while(cur->next != NULL && cur->next->seq < new_node->seq)
            {
                cur = cur->next;
            }

            new_node->next = cur->next;
            cur->next = new_node;
        }
        pthread_cond_signal(&res_cond);
        pthread_mutex_unlock(&res_mutex);
    }

    return NULL;
}

void *receiver_thread_func(void *arg)
{
    (void)arg;
    while(1)
    {
        pthread_mutex_lock(&res_mutex);

        while((result_head == NULL || result_head->seq != next_seq) && (!read_done || next_seq < total_lines))
        {
            pthread_cond_wait(&res_cond, &res_mutex);
        }

        if(result_head != NULL && result_head->seq == next_seq)
        {
            Node* temp = result_head;
            result_head = result_head->next;
            pthread_mutex_unlock(&res_mutex);
            printf("%s", temp->line);
            free(temp->line);
            free(temp);
            next_seq++;
        }

        else if(read_done && next_seq == total_lines)
        {
            pthread_mutex_unlock(&res_mutex);
            break;
        }
        else
        {
            pthread_mutex_unlock(&res_mutex);
        }
    }

    return NULL;
}
