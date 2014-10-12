#include <stdio.h>
#include "define.h"
#include "dispatcher.h"
#include "worker.h"
#include "pusher.h"
#include "notifier.h"

int main(int argc, char **argv)
{
    int ret, i;
    dispatcher_t *d;
    worker_t *w;
    pusher_t *p;
    notifier_t *n;

    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 0; i < WORKER_NUM; i++) {
        w = worker_create();
        w->next = global.workers;
        global.workers = w;
        ret = worker_start(w);
        if (ret != 0) {
            err_quit("worker_start");
        }
    }

    p = pusher_create();
    ret = pusher_start(p);
    if (ret != 0) {
        err_quit("pusher_start");
    }

    n = notifier_create();
    n->pusher = p;
    ret = notifier_start(n);
    if (ret != 0) {
        err_quit("notifier_start");
    }

    d = dispatcher_create();
    ret = dispatcher_start(d);
    if (ret != 0) {
        err_quit("dispatcher_start");
    }

    for (;;)
        sleep(1);
    return 0;
}
