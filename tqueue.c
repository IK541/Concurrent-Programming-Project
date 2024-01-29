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


unsigned TQueueHash(TQueue *queue, pthread_t *thread);
void TQueueCleanUp(TQueue *queue);
void TQueuePrint(TQueue *tqueue);


#define DEFAULT_HASHMAP_SIZE 16
void TQueueCreateQueue(TQueue *queue, int *size){
    int hashmap_size = DEFAULT_HASHMAP_SIZE;
    TQueueCreateQueueHash(queue, size, &hashmap_size);
}

void TQueueCreateQueueHash(TQueue *queue, int *size, int *hashmap_size){
    queue->max_size = (unsigned)*size;
    queue->size = 0;
    queue->subscribers = 0;
    queue->hashmap_size = (unsigned)*hashmap_size;

    queue->hashmap = malloc(queue->hashmap_size*sizeof(TQueueThread*));
    for(unsigned i = 0; i < queue->hashmap_size; ++i)
        queue->hashmap[i] = NULL;

    queue->head = malloc(sizeof(TQueueMessage));
    queue->tail = queue->head;
    queue->head->message = NULL;
    queue->head->next = NULL;

    queue->destroyed = 0;
    queue->put_locked = 0;
    queue->get_locked = 0;

    pthread_cond_init(&queue->get_cond,NULL);
    pthread_cond_init(&queue->put_cond,NULL);
    pthread_mutex_init(&queue->lock,NULL);

    dbgprintf("NEW QUEUE\n");
    dbgTQueuePrint(queue);
}

int TQueueDestroyQueue(TQueue *queue){
    if (TQueueDestroyQueue_1(queue)) return -1;
    TQueueDestroyQueue_2(queue);
    return 0;
}

int TQueueDestroyQueue_1(TQueue *queue){
    int ret = -1;
    TQueueThread* hashmap_pos;
    TQueueThread* next_thread;
    TQueueMessage* node;


    pthread_mutex_lock(&queue->lock);

    dbgprintf("DESTROYING QUEUE (1)\n");
    dbgTQueuePrint(queue);

    if(queue->destroyed) goto end;
    queue->destroyed = 1;

    while(queue->get_locked){
        dbgprintf("REMOVING SUBSCRIBERS\n");
        pthread_cond_broadcast(&queue->get_cond);
        pthread_cond_wait(&queue->put_cond,&queue->lock);
        dbgprintf("RETRY DESTROYING (1)\n");
    }
    while(queue->put_locked){
        dbgprintf("REMOVING PUBLISHERS\n");
        pthread_cond_broadcast(&queue->put_cond);
        pthread_cond_wait(&queue->get_cond,&queue->lock);
        dbgprintf("RETRY DESTROYING (2)\n");
    }

    for(unsigned i = 0; i < queue->hashmap_size; ++i){
        hashmap_pos = queue->hashmap[i];
        while(hashmap_pos != NULL){
            next_thread = hashmap_pos->next;
            free(hashmap_pos);
            hashmap_pos = next_thread;
        }
    }
    free(queue->hashmap);

    node = queue->head;
    while(node != NULL){
        node = node->next;
        free(queue->head);
        queue->head = node;
    }

    pthread_cond_destroy(&queue->get_cond);
    pthread_cond_destroy(&queue->put_cond);

    ret = 0;

    end:
    pthread_mutex_unlock(&queue->lock);

    dbgprintf("DESTROYED QUEUE (1)\n");

    return ret;
}

void TQueueDestroyQueue_2(TQueue *queue){
    pthread_mutex_destroy(&queue->lock);
    dbgprintf("DESTROYED QUEUE (2)\n");
}

int TQueueSubscribe(TQueue *queue, pthread_t *thread){
    int ret = -1;
    TQueueThread* thread_ptr;
    TQueueThread* new_thread;
    unsigned hash;

    pthread_mutex_lock(&queue->lock);

    hash = TQueueHash(queue, thread);

    if(queue->destroyed) goto end;
    ret = 1;

    dbgprintf("BEFORE SUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    ++queue->subscribers;

    if(queue->subscribers > 0x40000000) TQueueCleanUp(queue);

    new_thread = malloc(sizeof(TQueueThread));
    new_thread->thread = thread;
    new_thread->next = NULL;

    thread_ptr = queue->hashmap[hash];
    if(thread_ptr == NULL) queue->hashmap[hash] = new_thread;
    else {
        while(thread_ptr->next != NULL && thread_ptr->thread != thread)
            thread_ptr = thread_ptr->next;
        if(thread_ptr->thread == thread) goto end;
        thread_ptr->next = new_thread;
    }
    new_thread->message_ptr = queue->tail;

    dbgprintf("AFTER SUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);
    ret = 0;

    end:
    pthread_mutex_unlock(&queue->lock);

    return ret;
}

int TQueueUnsubscribe(TQueue *queue, pthread_t *thread){
    int ret = -1;
    TQueueThread* thread_ptr;
    TQueueThread* last_ptr;
    TQueueMessage* message_ptr;
    unsigned hash;

    pthread_mutex_lock(&queue->lock);

    hash = TQueueHash(queue, thread);

    if(queue->destroyed) goto end;
    ret = 1;

    dbgprintf("BEFORE UNSUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);

    thread_ptr = queue->hashmap[hash];
    if(thread_ptr == NULL) goto end;
    if(thread_ptr->thread == thread){
        queue->hashmap[hash] = thread_ptr->next;
    }
    else{
        while(thread_ptr != NULL && thread_ptr->thread != thread){
            last_ptr = thread_ptr;
            thread_ptr = thread_ptr->next;
        } if(thread_ptr == NULL) goto end;
        last_ptr->next = thread_ptr->next;
    }

    message_ptr = thread_ptr->message_ptr;
    --message_ptr->count;
    ++message_ptr->unsubscribed;

    if(message_ptr == queue->head && !message_ptr->count){
        dbgprintf("REMOVING_UNSUB\n");
        while(!message_ptr->count && message_ptr->next != NULL){
            queue->head = message_ptr->next;
            queue->head->count -= message_ptr->unsubscribed;
            queue->head->unsubscribed += message_ptr->unsubscribed;
            free(message_ptr);
            --queue->size;
            message_ptr = queue->head;
        }
        pthread_cond_broadcast(&queue->put_cond);
    }

    free(thread_ptr);

    dbgprintf("AFTER UNSUBSCRIBE (%p)\n",thread);
    dbgTQueuePrint(queue);
    ret = 0;

    end:
    pthread_mutex_unlock(&queue->lock);

    return ret;
}

void* TQueueGet(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueMessage* message_ptr;
    void* msg = NULL;
    unsigned hash;
    
    pthread_mutex_lock(&queue->lock);

    hash = TQueueHash(queue,thread);

    if(queue->destroyed) goto end;

    dbgprintf("TRY GET (%p)\n",thread);
    dbgTQueuePrint(queue);

    thread_ptr = queue->hashmap[hash];
    while(thread_ptr != NULL && thread_ptr->thread != thread){
        thread_ptr = thread_ptr->next;
    } if(thread_ptr == NULL) goto end;

    while(thread_ptr->message_ptr->next == NULL){
        dbgprintf("FAIL GET (%p)\n",thread);
        ++queue->get_locked;
        pthread_cond_wait(&queue->get_cond,&queue->lock);
        --queue->get_locked;
        dbgprintf("RETRY GET (%p)\n",thread);
        if(queue->destroyed){
            pthread_cond_broadcast(&queue->put_cond);
            goto end;
        }
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

    dbgprintf("AFTER GET (%p)\n",thread);
    dbgTQueuePrint(queue);

    end:

    pthread_mutex_unlock(&queue->lock);

    return msg;
}

int TQueuePut(TQueue *queue, void *msg){
    int ret = -1;
    TQueueMessage* new_message;

    TQueueMessage* tail;

    pthread_mutex_lock(&queue->lock);

    if(queue->destroyed) goto end;

    dbgprintf("TRY PUT (%p)\n",msg);
    dbgTQueuePrint(queue);

    if(queue->subscribers == queue->tail->unsubscribed) {
        dbgprintf("NO SUBSCRIBERS\n");
        ret = 0;
        goto end;
    }
 
    while(queue->size >= queue->max_size){
        dbgprintf("FAIL PUT (%p)\n",msg);
        ++queue->put_locked;
        pthread_cond_wait(&queue->put_cond, &queue->lock);
        --queue->put_locked;
        dbgprintf("RETRY PUT (%p)\n",msg);
        if(queue->destroyed) {
            pthread_cond_broadcast(&queue->get_cond);
            goto end;
        }
    }

    dbgprintf("BEFORE PUT (%p)\n",msg);
    dbgTQueuePrint(queue);

    new_message = malloc(sizeof(TQueueMessage));
    new_message->next = NULL;
    new_message->unsubscribed = 0;
    new_message->count = 0;

    ++queue->size;
    tail = queue->tail;
    tail->message = msg;
    tail->count = tail->count + queue->subscribers;
    tail->next = new_message;
    queue->tail = tail->next;
    pthread_cond_broadcast(&queue->get_cond);

    dbgprintf("AFTER PUT (%p)\n",msg);
    dbgTQueuePrint(queue);
    ret = 0;

    end:
    pthread_mutex_unlock(&queue->lock);

    return ret;
}

int TQueueGetAvailable(TQueue *queue, pthread_t *thread){
    TQueueThread* thread_ptr;
    TQueueMessage* message_ptr;
    int available = -1;
    unsigned hash;

    pthread_mutex_lock(&queue->lock);

    hash = TQueueHash(queue,thread);

    if(queue->destroyed) goto end;
    available = -2;

    dbgprintf("BEFORE GET_AVAILABLE (%p)\n",thread);
    dbgTQueuePrint(queue);

    thread_ptr = queue->hashmap[hash];
    while(thread_ptr != NULL && thread_ptr->thread != thread){
        thread_ptr = thread_ptr->next;
    }
    if(thread_ptr == NULL) goto end;
    message_ptr = thread_ptr->message_ptr;
    
    available = 0;
    while(message_ptr->next != NULL){
        message_ptr = message_ptr->next;
        ++available;
    }

    dbgprintf("AFTER GET_AVAILABLE (%p)\n",thread);
    dbgTQueuePrint(queue);

    end:
    pthread_mutex_unlock(&queue->lock);

    return available;
}

int TQueueRemove(TQueue *queue, void *msg){
    int ret = -1;
    TQueueMessage* last_message;
    TQueueMessage* message_ptr;
    TQueueThread* thread_ptr;

    pthread_mutex_lock(&queue->lock);

    if(queue->destroyed) goto end;
    
    dbgprintf("BEFORE REMOVE (%p)\n",msg);
    dbgTQueuePrint(queue);

    message_ptr = queue->head;
    if(message_ptr->next == NULL) {
        ret = 0;
        goto end;
    }
    if(message_ptr->message == msg) {
        queue->head = message_ptr->next;
    }
    else{
        while(message_ptr->next != NULL && message_ptr->message != msg){
            last_message = message_ptr;
            message_ptr = message_ptr->next;
        }
        if(message_ptr->next == NULL) {
            ret = 0;
            goto end;
        }
        last_message->next = message_ptr->next;
    }
        
    message_ptr->next->count -= message_ptr->unsubscribed;
    message_ptr->next->unsubscribed += message_ptr->unsubscribed;

    for(unsigned i = 0; i < queue->hashmap_size; ++i){
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

    dbgprintf("AFTER REMOVE (%p)\n",msg);
    dbgTQueuePrint(queue);
    ret = 1;

    end:

    pthread_mutex_unlock(&queue->lock);

    return ret;
}

int TQueueSetSize(TQueue *queue, int *size){
    int ret = -1;
    TQueueMessage* to_remove;
    TQueueThread* thread_ptr;

    pthread_mutex_lock(&queue->lock);

    if(queue->destroyed) goto end;

    dbgprintf("BEFORE SET_SIZE (%i)\n",*size);
    dbgTQueuePrint(queue);

    queue->max_size = *size;

    while(queue->size > queue->max_size){
        to_remove = queue->head;
        dbgprintf("to remove: %p\n",to_remove);
        queue->head = to_remove->next;

        for(unsigned i = 0; i < queue->hashmap_size; ++i){
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
    ret = 0;

    end:
    pthread_mutex_unlock(&queue->lock);

    return ret;
}

int TQueueSetHashmapSize(TQueue *queue, int *hashmap_size){
    int ret = -1;
    unsigned old_size;
    TQueueThread** old_hashmap;
    TQueueThread* hashmap_pos;
    TQueueThread* next_thread;
    TQueueThread* thread_ptr;
    unsigned hash;

    pthread_mutex_lock(&queue->lock);

    if(queue->destroyed) goto end;

    old_size = queue->hashmap_size;
    old_hashmap = queue->hashmap;
    queue->hashmap_size = *hashmap_size;
    queue->hashmap = malloc(queue->hashmap_size*sizeof(TQueueThread*));

    for(int i = 0; i < old_size; ++i){
        hashmap_pos = old_hashmap[i];
        while(hashmap_pos != NULL){
            hash = TQueueHash(queue, hashmap_pos->thread);
            next_thread = hashmap_pos->next;
            hashmap_pos->next = NULL;
            thread_ptr = queue->hashmap[hash];
            if(thread_ptr == NULL) queue->hashmap[hash] = hashmap_pos;
            else {
                while(thread_ptr->next != NULL)
                    thread_ptr = thread_ptr->next;
                thread_ptr->next = hashmap_pos;
            }
            hashmap_pos = next_thread;
        }
    } free(old_hashmap);

    ret = 0;
    end:
    pthread_mutex_unlock(&queue->lock);

    return ret;
}



// non-interface functions:

unsigned TQueueHash(TQueue* queue, pthread_t *thread){
    unsigned long hash = 0xcbf29ce484222325UL;
    unsigned long prime = 0x100000001b3UL;
    unsigned long x = *((unsigned long*)thread);
    do {
        hash = (hash ^ (x & 0xff))*prime;
    } while(x >>= 8);
    return (unsigned)((hash) % (unsigned long)queue->hashmap_size);
}

void TQueueCleanUp(TQueue *queue){
    int total_unsubscribed = 0;
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
    printf("destroyed: %d\n", queue->destroyed);
    printf("get locked: %d\n", queue->get_locked);
    printf("put locked: %d\n", queue->put_locked);
    printf("hashmap:\n");
    for(int i = 0; i < queue->hashmap_size; ++i){
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
