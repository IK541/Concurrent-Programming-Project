#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "tqueue.h"

#define QUEUE_SIZE 16

#define PUBLISHERS 4
#define PUBLICATIONS 16
#define SUBSCRIBERS 16

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
    unsigned rand_state = id, rand_num;
    while(1){
        i = (i+1)%PUBLICATIONS;
        if(TQueuePut(tqueue,arr[i*PUBLISHERS+id])) break;
        printf("> thread %d put: %d\n",id,*arr[i*PUBLISHERS+id]);

        if(i) continue;

        rand_num = rand_r(&rand_state) % ARR_SIZE;
        if(TQueueRemove(tqueue,arr[rand_num]) < 0) break;
        printf("> thread %d removed: %d\n",id,*arr[i]);
        if(!(rand_num%16)){
            rand_num += 1;
            if(TQueueSetSize(tqueue,(int*)&rand_num)) break;
        }
    }

    printf("[PUBLISHER END]\n");

    return NULL;
}

void* subscriber(void* arg){

    pthread_t this_thread = pthread_self();
    TQueue* tqueue = ((subscriber_data*)arg)->tqueue;
    int id = ((subscriber_data*)arg)->id;

    TQueueSubscribe(tqueue,&this_thread);

    int* np;
    int n;
    while(1){
        np = (int*)TQueueGet(tqueue,&this_thread);
        if(np == NULL) break;
        n = *np;
        printf("> thread %d got: %d\n",id,n);
        if(!(n%16)){
            if(TQueueUnsubscribe(tqueue,&this_thread)) break;
            if(TQueueSubscribe(tqueue,&this_thread)) break;
        }
        n = TQueueGetAvailable(tqueue,&this_thread);
        if(n < 0) break;
        printf("> thread %d has available: %d\n",id,n);
    }

    TQueueUnsubscribe(tqueue,&this_thread);

    printf("[SUBSCRIBER END]\n");

    return NULL;
}

int main() {
    TQueue tqueue;
    int size = QUEUE_SIZE;
    TQueueCreateQueue(&tqueue,&size);
    int hashmap_size = 16;

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




    for(int i = 0; i < PUBLISHERS; ++i){
        pthread_create(publishers[i],NULL,publisher,pub_data_arr[i]);
    }
    for(int i = 0; i < SUBSCRIBERS; ++i){
        pthread_create(subscribers[i],NULL,subscriber,sub_data_arr[i]);
    }

    for(int i = 0; i < 4; ++i){
        sleep(1);
        hashmap_size *= 2;
        TQueueSetHashmapSize(&tqueue, &hashmap_size);
    }

    TQueueDestroyQueue(&tqueue);

    for(int i = 0; i < PUBLISHERS; ++i){
        pthread_join(*publishers[i],NULL);
    }
    for(int i = 0; i < SUBSCRIBERS; ++i){
        pthread_join(*subscribers[i],NULL);
    }

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

    return 0;    
}
