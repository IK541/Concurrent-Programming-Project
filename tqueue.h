#pragma once
#include <pthread.h>

#define HASHMAP_SIZE 4U

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
};

void TQueueCreateQueue(TQueue *queue, int *size);
void TQueueDestroyQueue(TQueue *queue);
void TQueueSubscribe(TQueue *queue, pthread_t *thread);
void TQueueUnsubscribe(TQueue *queue, pthread_t *thread);
void TQueuePut(TQueue *queue, void *msg);
void* TQueueGet(TQueue *queue, pthread_t *thread);
unsigned TQueueGetAvailable(TQueue *queue, pthread_t *thread);
void TQueueRemove(TQueue *queue, void *msg);
void TQueueSetSize(TQueue *queue, int *size);

unsigned TQueueHash(pthread_t *thread);
// static void TQueueCleanUp(TQueue *queue)
void TQueuePrint(TQueue *tqueue);
