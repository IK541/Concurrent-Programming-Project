#pragma once
#include <pthread.h>

#define HASHMAP_SIZE 16U

typedef struct TQueueMessage TQueueMessage;
typedef struct TQueueThread TQueueThread;
typedef struct TQueue TQueue;

struct TQueueMessage{
    void* message;
    unsigned count;
    unsigned unsubscribed;
    TQueueMessage* next;
};

struct TQueueThread{
    TQueueMessage* message_ptr;
    pthread_t* thread;
    TQueueThread* next;
};

struct TQueue{
    unsigned size;
    unsigned max_size;
    unsigned subscribers;
    TQueueThread** hashmap;
    TQueueMessage* head;
    TQueueMessage* tail;
    pthread_cond_t get_cond;
    pthread_cond_t put_cond;
    pthread_mutex_t lock;
    unsigned char destroyed;
    unsigned put_locked;
    unsigned get_locked;
};

void TQueueCreateQueue(TQueue *queue, int *size);
void TQueueDestroyQueue(TQueue *queue);
void* TQueueGet(TQueue *queue, pthread_t *thread);

// return value: 0 - ok, -1 operation failed
int TQueueSubscribe(TQueue *queue, pthread_t *thread);
int TQueueUnsubscribe(TQueue *queue, pthread_t *thread);
int TQueuePut(TQueue *queue, void *msg);
int TQueueGetAvailable(TQueue *queue, pthread_t *thread);
int TQueueSetSize(TQueue *queue, int *size);

// returns 0 - no elements removed, 1 - one element removed, -1 - operation failed
int TQueueRemove(TQueue *queue, void *msg);


// non-interface functions
unsigned TQueueHash(pthread_t *thread);
void TQueueCleanUp(TQueue *queue);
void TQueuePrint(TQueue *tqueue);
