#include <pthread.h>

typedef struct TQueueMessage TQueueMessage;
typedef struct TQueueThread TQueueThread;
typedef struct TQueue TQueue;

struct TQueueMessage {
	void *message;
	int count;
	int unsubscribed;
	int num;
	TQueueMessage *next;
};

struct TQueueThread {
	TQueueMessage *message_ptr;
	pthread_t *thread;
	TQueueThread *next;
};

struct TQueue {
	unsigned size;
	unsigned max_size;
	int subscribers;
	unsigned hashmap_size;
	TQueueThread **hashmap;
	TQueueMessage *head;
	TQueueMessage *tail;
	pthread_cond_t get_cond;
	pthread_cond_t put_cond;
	pthread_mutex_t lock;
	unsigned char destroyed;
	unsigned put_locked;
	unsigned get_locked;
};

// queue creation and destruction functions
// non-void destroy functions return 0 on success
// and -1 if the queue has already been destroyed
void TQueueCreateQueue(TQueue * queue, int *size);
void TQueueCreateQueueHash(TQueue * queue, int *size, int *hashmap_size);
int TQueueDestroyQueue(TQueue * queue);
int TQueueDestroyQueue_1(TQueue * tqueue);
void TQueueDestroyQueue_2(TQueue * tqueue);

// returns:
// 0 on success
// -1 if the queue has already been destroyed
// 1 if the thread is already subscribed
int TQueueSubscribe(TQueue * queue, pthread_t * thread);

// returns:
// 0 on success
// -1 if the queue has already been destroyed
// 1 if the thread is not subscribed
int TQueueUnsubscribe(TQueue * queue, pthread_t * thread);

// if the queue is full at the moment, the function is blocking
// returns 0 on success and -1 if the queue has already been destroyed
int TQueuePut(TQueue * queue, void *msg);

// get function will return NULL if a thread is not subscribed,
// if no message is available at the moment, the function is blocking
// returns 0 on success and -1 if the queue has already been destroyed
void *TQueueGet(TQueue * queue, pthread_t * thread);

// returns:
// number of messages available on success
// -1 if the queue has already been destroyed
// -2 if the thread is not subscribed
int TQueueGetAvailable(TQueue * queue, pthread_t * thread);

// returns:
// 0 on success (an element has been removed)
// 1 if no element with given message is on the queue
// -1 if the queue has already been destroyed
// if the same message is duplicated on the queue,
// this function will remove the oldest instance
int TQueueRemoveMsg(TQueue * queue, void *msg);

// returns:
// 0 on success
// -1 if the queue has already been destroyed
int TQueueSetSize(TQueue * queue, int *size);

// returns:
// 0 on success
// -1 if the queue has already been destroyed
int TQueueSetHashmapSize(TQueue * queue, int *hashmap_size);
