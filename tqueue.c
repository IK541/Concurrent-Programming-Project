#include <stdlib.h>
#include <stdio.h>

#include "tqueue.h"

#ifdef DEBUG
    #define dbgprintf(...) do {printf(__VA_ARGS__); fflush(stdout);} while(0)
    #define dbgTQueuePrint(x) TQueuePrint(x)
#else
    #define dbgprintf(...)
    #define dbgTQueuePrint(x)
#endif

void TQueueCreateQueue(TQueue *queue, int *size){
    queue->max_size = (unsigned)*size;
    queue->size = 0;
    queue->subscribers = 0;

    queue->hashmap = malloc(HASHMAP_SIZE*sizeof(TQueueThread*));
    for(unsigned i = 0; i < HASHMAP_SIZE; ++i)
        queue->hashmap[i] = NULL;

    queue->head = malloc(sizeof(TQueueMessage));
    queue->tail = queue->head;
    queue->head->message = NULL;
    queue->head->next = NULL;

    pthread_cond_init(&queue->get_cond,NULL);
    pthread_cond_init(&queue->put_cond,NULL);
    pthread_mutex_init(&queue->lock,NULL);

    dbgprintf("NEW QUEUE\n");
    dbgTQueuePrint(queue);
}

void TQueueDestroyQueue(TQueue *queue){
    pthread_mutex_lock(&queue->lock);

    dbgprintf("DESTROYED QUEUE\n");
    dbgTQueuePrint(queue);

    TQueueThread* hashmap_pos;
    TQueueThread* next_thread;
    for(unsigned i = 0; i < HASHMAP_SIZE; ++i){
        hashmap_pos = queue->hashmap[i];
        while(hashmap_pos != NULL){
            next_thread = hashmap_pos->next;
            free(hashmap_pos);
            hashmap_pos = next_thread;
        }
    }
    free(queue->hashmap);

    TQueueMessage* node = queue->head;
    while(node != NULL){
        node = node->next;
        free(queue->head);
        queue->head = node;
    }

    pthread_cond_destroy(&queue->get_cond);
    pthread_cond_destroy(&queue->put_cond);

    pthread_mutex_unlock(&queue->lock);
    pthread_mutex_destroy(&queue->lock);
}

void TQueueSubscribe(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueThread* new_thread = malloc(sizeof(TQueueThread));
    new_thread->thread = thread;
    new_thread->next = NULL;

    pthread_mutex_lock(&queue->lock);

    dbgprintf("BEFORE SUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    ++queue->subscribers;

    if(queue->subscribers > 0x40000000) TQueueCleanUp(queue);

    thread_ptr = queue->hashmap[TQueueHash(thread)];
    if(thread_ptr == NULL) queue->hashmap[TQueueHash(thread)] = new_thread;
    else {
        while(thread_ptr->next != NULL)
            thread_ptr = thread_ptr->next;
        thread_ptr->next = new_thread;
    }
    new_thread->message_ptr = queue->tail;

    dbgprintf("AFTER SUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);
}

void TQueueUnsubscribe(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueThread* last_ptr;

    pthread_mutex_lock(&queue->lock);

    dbgprintf("BEFORE UNSUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    // FIX
    thread_ptr = queue->hashmap[TQueueHash(thread)];
    if(thread_ptr == NULL) goto end;
    if(thread_ptr->thread == thread){
        queue->hashmap[TQueueHash(thread)] = thread_ptr->next;
    }
    else{
        while(thread_ptr->next != NULL && thread_ptr->thread != thread){
            last_ptr = thread_ptr;
            thread_ptr = thread_ptr->next;
        } if(thread_ptr->next == NULL) goto end;
        last_ptr->next = thread_ptr->next;
    }

    --thread_ptr->message_ptr->count;
    ++thread_ptr->message_ptr->unsubscribed;

    free(thread_ptr);

    end:

    dbgprintf("AFTER UNSUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);
}

void TQueuePut(TQueue *queue, void *msg){
    TQueueMessage* new_message = malloc(sizeof(TQueueMessage));
    new_message->next = NULL;
    new_message->unsubscribed = 0;

    TQueueMessage* tail;

    pthread_mutex_lock(&queue->lock);

    dbgprintf("TRY PUT (%p)\n",msg);
    dbgTQueuePrint(queue);

    if(!queue->subscribers) {
        dbgprintf("NO SUBSCRIBERS\n");
        goto end;
    }
 
    while(queue->size >= queue->max_size){
        dbgprintf("FAIL PUT (%p)\n",msg);
        pthread_cond_wait(&queue->put_cond, &queue->lock);
        dbgprintf("RETRY PUT (%p)\n",msg);
    }

    dbgprintf("BEFORE PUT (%p)\n",msg);
    dbgTQueuePrint(queue);


    ++queue->size;
    tail = queue->tail;
    tail->message = msg;
    tail->count = queue->subscribers;
    tail->next = new_message;
    queue->tail = tail->next;
    pthread_cond_broadcast(&queue->get_cond);

    end:

    dbgprintf("AFTER PUT (%p)\n",msg);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);
}

void* TQueueGet(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueMessage* message_ptr;
    void* msg = NULL;
    
    pthread_mutex_lock(&queue->lock);

    dbgprintf("TRY GET (%p)\n",thread);
    dbgTQueuePrint(queue);

    thread_ptr = queue->hashmap[TQueueHash(thread)];
    while(thread_ptr != NULL && thread_ptr->thread != thread){
        thread_ptr = thread_ptr->next;
    } if(thread_ptr == NULL) goto end;

    while(thread_ptr->message_ptr->next == NULL){
        dbgprintf("FAIL GET (%p)\n",thread);
        pthread_cond_wait(&queue->get_cond,&queue->lock);
        dbgprintf("RETRY GET (%p)\n",thread);
    }

    dbgprintf("BEFORE GET (%p)\n",thread);
    dbgTQueuePrint(queue);
    
    message_ptr = thread_ptr->message_ptr;
    msg = message_ptr->message;
    thread_ptr->message_ptr = message_ptr->next;

    if(!--message_ptr->count){
        queue->head = message_ptr->next;
        queue->head->count -= message_ptr->unsubscribed;
        queue->head->unsubscribed += message_ptr->unsubscribed;
        free(message_ptr);
        --queue->size;
        pthread_cond_broadcast(&queue->put_cond);
    }

    end:

    dbgprintf("AFTER GET (%p)\n",thread);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);

    return msg;
}

unsigned TQueueGetAvailable(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueMessage* message_ptr;
    unsigned available = 0;

    pthread_mutex_lock(&queue->lock);

    dbgprintf("BEFORE GET_AVAILABLE (%p)\n",thread);
    dbgTQueuePrint(queue);

    thread_ptr = queue->hashmap[TQueueHash(thread)];
    while(thread_ptr != NULL && thread_ptr->thread != thread){
        thread_ptr = thread_ptr->next;
    } if(thread_ptr == NULL) return 0;
    message_ptr = thread_ptr->message_ptr;
    
    while(message_ptr->next != NULL){
        message_ptr = message_ptr->next;
        ++available;
    }

    dbgprintf("AFTER GET_AVAILABLE (%p)\n",thread);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);

    return available;
}

void TQueueRemove(TQueue *queue, void *msg){
    TQueueMessage* last_message;
    TQueueMessage* message_ptr;
    TQueueThread* thread_ptr;

    pthread_mutex_lock(&queue->lock);
    
    dbgprintf("BEFORE REMOVE (%p)\n",msg);
    dbgTQueuePrint(queue);

    message_ptr = queue->head;
    if(message_ptr->next == NULL) goto end;
    if(message_ptr->message == msg) {
        queue->head = message_ptr->next;
    }
    else{
        while(message_ptr->next != NULL && message_ptr->message != msg){
            last_message = message_ptr;
            message_ptr = message_ptr->next;
        }
        if(message_ptr->next == NULL) goto end;
        last_message->next = message_ptr->next;
    }
        
    message_ptr->next->count -= message_ptr->unsubscribed;
    message_ptr->next->unsubscribed += message_ptr->unsubscribed;

    dbgprintf("CHECKPOINT-1 (%p)\n",msg);

    for(unsigned i = 0; i < HASHMAP_SIZE; ++i){
            thread_ptr = queue->hashmap[i];
            while(thread_ptr != NULL){
                if(thread_ptr->message_ptr == message_ptr)
                    thread_ptr->message_ptr = message_ptr->next;
                thread_ptr = thread_ptr->next;
            }
        }

    free(message_ptr);
    --queue->size;
    pthread_cond_broadcast(&queue->put_cond);

    end:

    dbgprintf("AFTER REMOVE (%p)\n",msg);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);
}

void TQueueSetSize(TQueue *queue, int *size){
    TQueueMessage* to_remove;
    TQueueThread* thread_ptr;

    pthread_mutex_lock(&queue->lock);

    dbgprintf("BEFORE SET_SIZE (%i)\n",*size);
    dbgTQueuePrint(queue);

    queue->max_size = *size;

    while(queue->size > queue->max_size){
        to_remove = queue->head;
        dbgprintf("to remove: %p\n",to_remove);
        queue->head = to_remove->next;

        for(unsigned i = 0; i < HASHMAP_SIZE; ++i){
            thread_ptr = queue->hashmap[i];
            while(thread_ptr != NULL){
                if(thread_ptr->message_ptr == to_remove)
                    thread_ptr->message_ptr = to_remove->next;
                thread_ptr = thread_ptr->next;
            }
        }

        to_remove->next->count -= to_remove->unsubscribed;
        to_remove->next->unsubscribed += to_remove->unsubscribed;
        free(to_remove);
        --queue->size;
    }

    dbgprintf("AFTER SET_SIZE (%i)\n",*size);
    dbgTQueuePrint(queue);

    pthread_mutex_unlock(&queue->lock);
}

// non-interface functions:

unsigned TQueueHash(pthread_t *thread){
    unsigned long hash = 0xcbf29ce484222325;
    unsigned long prime = 0x100000001b3;
    unsigned long x = *((unsigned long*)thread);
    printf("HASH INIT: %ld\n",x);
    do {
        hash = (hash ^ (x & 0xff))*prime;
    } while(x >>= 8);
    return (unsigned)((hash) % (unsigned long)HASHMAP_SIZE);
}

void TQueueCleanUp(TQueue *queue){
    unsigned total_unsubscribed = 0;
    TQueueMessage* message_ptr = queue->head;
    do {
        message_ptr->count -= total_unsubscribed;
        total_unsubscribed += message_ptr->unsubscribed;
        message_ptr->unsubscribed = 0;
        message_ptr = message_ptr->next;
    } while(message_ptr != NULL);
    queue->subscribers -= total_unsubscribed;
}

#define ITER_LIMIT 1024
void TQueuePrint(TQueue *queue){
    
    TQueueThread* thread_ptr;
    TQueueMessage* message_ptr;

    printf("\nvvvvvvvv\n");
    printf("max size: %d\n", queue->max_size);
    printf("size: %d\n", queue->size);
    printf("subscribers: %d\n", queue->subscribers);
    printf("head: %p\n", queue->head);
    printf("tail: %p\n", queue->tail);
    printf("hashmap:\n");
    for(int i = 0; i < HASHMAP_SIZE; ++i){
        printf("[%d]",i);
        thread_ptr = queue->hashmap[i];
        while(thread_ptr != NULL){
            printf("->{%p|%p}",thread_ptr->thread,thread_ptr->message_ptr);
            thread_ptr = thread_ptr->next;
        } printf("\n");
    }
    printf("messages:\n");
    message_ptr = queue->head;
    for(int i = 0; i < ITER_LIMIT && message_ptr != NULL; ++i){
        printf("ptr: %p\tmes: %p\tcnt: %d\tusb: %d\n",message_ptr,message_ptr->message,message_ptr->count,message_ptr->unsubscribed);
        message_ptr = message_ptr->next;
    }
    printf("^^^^^^^^\n\n");
    
    fflush(stdout);
    
}
