#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "tqueue.h"

#define QUEUE_SIZE 256
#define PUBLISHERS 16
#define SUBSCRIBERS 64
#define PUBLICATIONS 256
#define ARR_SIZE (PUBLISHERS*PUBLICATIONS)

volatile int ready;
pthread_mutex_t lock;
pthread_cond_t cond;

typedef struct publisher_data{
    TQueue* tqueue;
    int** arr;
    int id;
} publisher_data;

typedef struct subscriber_data{
    TQueue* tqueue;
    int id;
} subscriber_data;

void* publisher(void* arg){
    int** arr = ((publisher_data*)arg)->arr;
    TQueue* tqueue = ((publisher_data*)arg)->tqueue;
    int id = ((publisher_data*)arg)->id;

    pthread_mutex_lock(&lock);
    while(ready < SUBSCRIBERS) pthread_cond_wait(&cond,&lock);
    pthread_mutex_unlock(&lock);

    for(int i = 0; i < PUBLICATIONS; ++i){
        TQueuePut(tqueue,arr[i*PUBLISHERS+id]);
        printf("> thread %d put: %d\n",id,*arr[i*PUBLISHERS+id]);
    }

    return NULL;
}

void* subscriber(void* arg){

    pthread_t this_thread = pthread_self();
    TQueue* tqueue = ((subscriber_data*)arg)->tqueue;
    int id = ((subscriber_data*)arg)->id;

    TQueueSubscribe(tqueue,&this_thread);
    pthread_mutex_lock(&lock);
    ++ready;
    pthread_mutex_unlock(&lock);

    int n;
    if(ready >= SUBSCRIBERS) {
        pthread_cond_broadcast(&cond);
    }
    for(int i = 0; i < ARR_SIZE; ++i){
        n = *(int*)TQueueGet(tqueue,&this_thread);
        printf("> thread %d got: %d\n",id,n);
    }
    TQueueUnsubscribe(tqueue,&this_thread);

    return NULL;
}


int main() {
    TQueue tqueue;
    int size = QUEUE_SIZE;
    TQueueCreateQueue(&tqueue,&size);

    int** arr = malloc(ARR_SIZE*sizeof(int*));
    for(int i = 0; i < ARR_SIZE; ++i){
        arr[i] = malloc(sizeof(int));
        *arr[i] = i;
    }

    pthread_t** publishers = malloc(PUBLISHERS*sizeof(pthread_t*));
    publisher_data** pub_data_arr = malloc(PUBLISHERS*(sizeof(publisher_data*)));
    for(int i = 0; i < PUBLISHERS; ++i){
        publishers[i] = malloc(sizeof(pthread_t));
        pub_data_arr[i] = malloc(sizeof(publisher_data));
        pub_data_arr[i]->arr = arr;
        pub_data_arr[i]->tqueue = &tqueue;
        pub_data_arr[i]->id = i;
    }

    pthread_t** subscribers = malloc(SUBSCRIBERS*sizeof(pthread_t*));
    subscriber_data** sub_data_arr = malloc(SUBSCRIBERS*(sizeof(subscriber_data*)));
    for(int i = 0; i < SUBSCRIBERS; ++i){
        subscribers[i] = malloc(sizeof(pthread_t));
        sub_data_arr[i] = malloc(sizeof(subscriber_data));
        sub_data_arr[i]->tqueue = &tqueue;
        sub_data_arr[i]->id = i;
    }

    ready = 0;
    pthread_mutex_init(&lock,NULL);
    pthread_cond_init(&cond,NULL);

    for(int i = 0; i < PUBLISHERS; ++i){
        pthread_create(publishers[i],NULL,publisher,pub_data_arr[i]);
    }
    for(int i = 0; i < SUBSCRIBERS; ++i){
        pthread_create(subscribers[i],NULL,subscriber,sub_data_arr[i]);
    }

    for(int i = 0; i < PUBLISHERS; ++i){
        pthread_join(*publishers[i],NULL);
    }
    for(int i = 0; i < SUBSCRIBERS; ++i){
        pthread_join(*subscribers[i],NULL);
    }

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    for(int i = 0; i < PUBLISHERS; ++i){
        free(publishers[i]);
        free(pub_data_arr[i]);
    }

    for(int i = 0; i < PUBLISHERS; ++i){
        free(subscribers[i]);
        free(sub_data_arr[i]);
    }

    free(pub_data_arr);
    free(sub_data_arr);
    free(publishers);
    free(subscribers);

    for(int i = 0; i < ARR_SIZE; ++i) free(arr[i]);
    free(arr);

    TQueueDestroyQueue(&tqueue);

    return 0;
}
