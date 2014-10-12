#ifndef __NOTIFIER_H_
#define __NOTIFIER_H_
#include "define.h"

struct notifier_s {
    struct event_base *base;
    pthread_t thread_id;
    int stop;
    pusher_t *pusher;
};

notifier_t *notifier_create();
void notifier_destroy(notifier_t *n);
int notifier_start(notifier_t *n);
int notifier_stop(notifier_t *n);
#endif
