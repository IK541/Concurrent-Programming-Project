#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "tqueue.h"

#define QUEUE_SIZE 16

#define PUBLISHERS 4
#define PUBLICATIONS 16
#define SUBSCRIBERS 16
#define REMOVERS 1

#define ARR_SIZE (PUBLISHERS*PUBLICATIONS)

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

    int i = 0;
    while(1){
        i = (i+1)%PUBLICATIONS;
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

    int n;
    while(1){
        n = *(int*)TQueueGet(tqueue,&this_thread);
        printf("> thread %d got: %d\n",id,n);
        // if(!(n%16)){
        //     TQueueUnsubscribe(tqueue,&this_thread);
        //     TQueueSubscribe(tqueue,&this_thread);
        // }
    }

    TQueueUnsubscribe(tqueue,&this_thread);

    return NULL;
}

void* remover(void* arg){

    int** arr = ((publisher_data*)arg)->arr;
    TQueue* tqueue = ((publisher_data*)arg)->tqueue;
    int id = ((publisher_data*)arg)->id;

    int i = 0;
    while(1){
        i = (i+1)%ARR_SIZE;
        TQueueRemove(tqueue,arr[i]);
        printf("> thread %d removed: %d\n",id,*arr[i]);
    }

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

    pthread_t** removers = malloc(REMOVERS*sizeof(pthread_t*));
    publisher_data** rem_data_arr = malloc(REMOVERS*(sizeof(publisher_data*)));
    for(int i = 0; i < PUBLISHERS; ++i){
        removers[i] = malloc(sizeof(pthread_t));
        rem_data_arr[i] = malloc(sizeof(publisher_data));
        rem_data_arr[i]->arr = arr;
        rem_data_arr[i]->tqueue = &tqueue;
        rem_data_arr[i]->id = i;
    }



    for(int i = 0; i < PUBLISHERS; ++i){
        pthread_create(publishers[i],NULL,publisher,pub_data_arr[i]);
    }
    for(int i = 0; i < SUBSCRIBERS; ++i){
        pthread_create(subscribers[i],NULL,subscriber,sub_data_arr[i]);
    }
    for(int i = 0; i < REMOVERS; ++i){
        pthread_create(removers[i],NULL,remover,rem_data_arr[i]);
    }

    while(1);
    
}
