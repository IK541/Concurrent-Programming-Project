# Multi-thread publish-subscribe queue

The project implements a mutex-potected publish-subscribe queue with a set maximum size. Any thread can subscribe to the queue to be able to read new messages. Subscribing mechanism is implemented using a hashmap (using FNV hash function and chaining). Threads can also add new messages and perform basic operations on the queue.

## Interface functions

The available operations are as following (the prefix ```TQueue``` is used to avoid naming conflicts):

```void TQueueCreateQueue(TQueue * queue, int *size)``` - creates a queue with given size for messages and default hashmap size for subscribers

```void TQueueCreateQueue(TQueue * queue, int *size, int *hashmap_size)``` - creates a queue with given size for messages and given hashmap size for subscribers

```int TQueueDestroyQueue(TQueue * queue)``` - destroy queue, if the user cannot guarantee that no new operations will be performed on the queue it is advised to use functions ```TQueueDestroyQueue_1(TQueue *queue)``` and ```TQueueDestroyQueue_2(TQueue *queue)```; returns 0 on sucess, -1 if the queue has already been destroyed

```int TQueueDestroyQueue_1(TQueue * queue)``` - destroys all of the queue except for the mutex and sets destroy variable to 1; this causes all operations acessing the queue to fail and return information that the queue has been destroyed which can be used for synchronization as shown in example file ```main.c```; returns 0 on sucess, -1 if the queue has already been destroyed

```void TQueueDestroyQueue_2(TQueue * queue)``` - destroys the mutex, trying to access the queue afterwards will result in undefined behaviour

```int TQueueSubscribe(TQueue * queue, pthread_t * thread)``` - adds thread ```thread``` as subscriber, the thread will be able to read messages added after this operation; returns 0 on sucess, -1 if the queue has already been destroyed, 1 if the thread is already subscribed

```int TQueueUnsubscribe(TQueue * queue, pthread_t * thread)``` - removes thread ```thread``` as subscriber, all its unread messages will be treaded as if they were read;

```int TQueuePut(TQueue * queue, void *msg)``` - adds message ```msg``` to the queue, this operation is blocking if the queue is full; returns 0 on sucess, -1 if the queue has already been destroyed

```void *TQueueGet(TQueue * queue, pthread_t * thread)``` - reads and returns a single message from the queue, if no messages are available the operation is blocking, if the thread is not subscribed it returns NULL, if all subscribers who have been subscribed at the time of message publication have read the message, the message is removed from the queue

```int TQueueGetAvailable(TQueue * queue, pthread_t * thread)``` - returns the number of messages available to the thread ```thread```; returns -1 if the queue has already been destroyed, -2 if the thread is not subscribed

```int TQueueRemoveMsg(TQueue * queue, void *msg)``` - removes message ```msg``` from the queue, if the same message is duplicated on the queue, this function will remove the oldest instance; returns 0 on sucess, -1 if the queue has already been destroyed, 1 if the message is not present in the queue

```int TQueueSetSize(TQueue * queue, int *size)``` - sets maximum size of the queue to ```size```, if the new size exceeds the former one, the oldest messages are removed; returns 0 on sucess, -1 if the queue has already been destroyed

```int TQueueSetHashmapSize(TQueue * queue, int *hashmap_size)``` - sets hashmap size for subscribers to ```hashmap_size```; returns 0 on sucess, -1 if the queue has already been destroyed

## Files

The project contains following files:

```tqueue.h``` - definitions for the publish-subscribe queue

```tqueue.c``` - implementations for the publish-subscribe queue (if this file is compiled with DEBUG macro it will print all operations on the queue and state of the queue before and after each operation to stdout)

```main.c``` - example of use of the publish-subscribe queue
