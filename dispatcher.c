#include "define.h"
#include "dispatcher.h"
#include "worker.h"

global_t global = {NULL, NULL};
void dispatcher_timer(int fd, short event, void *arg)
{
    dispatcher_t *d = (dispatcher_t *)arg;
    if (d->stop) {
        printf("dispatcher stoped\n");
        event_base_loopexit(d->base, NULL);
    }
}

dispatcher_t *dispatcher_create()
{
    dispatcher_t *d = malloc(sizeof(dispatcher_t));
    if (NULL == d) {
        err_quit("malloc");
    }

    memset(d, 0, sizeof(dispatcher_t));
    d->base = event_base_new();
    if (NULL == d->base) {
        err_quit("event_base_new()");
    }

    return d;
}

void dispatcher_destroy(dispatcher_t *d)
{
    if (d) {
        free(d);
    }
}

// hooks
void dispatcher_worker_readcb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}
void dispatcher_worker_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}
void dispatcher_worker_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

int round_robin_dispatch(dispatcher_t *d, int fd)
{
    static worker_t *cur = NULL;
    client_t *c = client_create();
    cmd_t cmd;

    if (!cur) {
        cur = global.workers;
    }

    c->fd = fd;
    c->worker = cur;
    cmd.cmd_no = CMD_ADD_CLIENT;
    cmd.client = c;
    if (evbuffer_add(bufferevent_get_output(cur->bev_dispatcher[1]), &cmd, sizeof cmd) != 0) {
        err_quit("evbuffer_add");
    }
    cur = cur->next; 
    return 0;
}

void do_accept(int listener, short event, void *arg)
{
    dispatcher_t *d = (dispatcher_t *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    if (fd < 0) {
        err_quit("accept");
        //FD_SETSIZE
    } else {
        evutil_make_socket_nonblocking(fd);
        round_robin_dispatch(d, fd);
    }
}

void *dispatcher_loop(void *arg)
{
    dispatcher_t *d = (dispatcher_t *)arg;
    event_base_dispatch(d->base);
    return (void *)0;
}

int dispatcher_start(dispatcher_t *d)
{
    struct event *timer_event, *listener_event;
    struct timeval timeout = {1, 0};
    evutil_socket_t listener;
    struct sockaddr_in sin;
    worker_t *w;
    int ret;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
#ifdef ECHO_SERVER
    sin.sin_port = htons(8201);
#else
    sin.sin_port = htons(8200);
#endif

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_listen_socket_reuseable(listener);
    evutil_make_socket_nonblocking(listener);

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        err_quit("bind");
    }

    if (listen(listener, 128) < 0) {
        err_quit("listen");
    }

    listener_event = event_new(d->base, listener, EV_READ|EV_PERSIST, do_accept, d);
    if (NULL == listener_event) {
        err_quit("event_new");
    }
    event_add(listener_event, NULL);

    timer_event = event_new(d->base, -1, EV_PERSIST, dispatcher_timer, d);
    if (NULL == timer_event) {
        err_quit("event_new");
    }
    event_add(timer_event, &timeout);

    // create bev to workers
    for (w = global.workers; w != NULL; w = w->next) {
        w->bev_dispatcher[1] = bufferevent_socket_new(d->base, w->sockpair_dispatcher[1], BEV_OPT_CLOSE_ON_FREE);
        if (!w->bev_dispatcher[1]) {
            err_quit("bufferevent_socket_new");
        }
        bufferevent_setcb(w->bev_dispatcher[1], dispatcher_worker_readcb, dispatcher_worker_writecb, dispatcher_worker_eventcb, w);
        bufferevent_setwatermark(w->bev_dispatcher[1], EV_READ, sizeof(cmd_t), 0);
        bufferevent_enable(w->bev_dispatcher[1], EV_READ | EV_WRITE);
    }

    ret = pthread_create(&(d->thread_id), NULL, dispatcher_loop, d);
    if (ret != 0) {
        err_quit("pthread_create");
    }

    return 0;
}

int dispatcher_stop(dispatcher_t *d)
{
    d->stop = 1;
    return 0;
}
