#ifndef  __DISPATCHER_H_
#define  __DISPATCHER_H_

#include <pthread.h>
#include "define.h"

struct worker_s;
struct dispatcher_s {
    struct event_base *base;
    pthread_t thread_id;
    int stop;
};

dispatcher_t *dispatcher_create();
void dispatcher_destroy(dispatcher_t *d);
int dispatcher_start(dispatcher_t *d);
int dispatcher_stop(dispatcher_t *d);

#endif  //__DISPATCHER_H_
