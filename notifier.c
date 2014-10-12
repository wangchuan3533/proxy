#include "define.h"
#include "notifier.h"
#include "pusher.h"

void notifier_timer(int fd, short event, void *arg)
{
    notifier_t *n = (notifier_t *)arg;

    if (n->stop) {
        printf("notifier stoped\n");
        event_base_loopexit(n->base, NULL);
    }
}

notifier_t *notifier_create()
{
    notifier_t *n = (notifier_t *)malloc(sizeof(notifier_t));
    if (NULL == n) {
        err_quit("malloc");
    }
    memset(n, 0, sizeof(notifier_t));
    n->base = event_base_new();
    if (NULL == n->base) {
        err_quit("event_base_new");
    }
    return n;
}

void notifier_destroy(notifier_t *n)
{
    if (n) {
        free(n);
    }
}

void notifier_pusher_readcb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void notifier_pusher_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}
void notifier_pusher_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void notifier_writecb(struct bufferevent *bev, void *arg)
{
    bufferevent_free(bev);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void notifier_eventcb(struct bufferevent *bev, short error, void *arg)
{
    bufferevent_free(bev);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void notifier_readcb(struct bufferevent *bev, void *arg)
{
    notifier_t *n = (notifier_t *)arg;
    evbuffer_add_buffer(bufferevent_get_output(n->pusher->bev_notifier[1]), bufferevent_get_input(bev));
    evbuffer_add_printf(bufferevent_get_output(bev), "OK");
    bufferevent_setcb(bev, notifier_readcb, notifier_writecb, notifier_eventcb, arg);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void notifier_accept(int listener, short event, void *arg)
{
    notifier_t *n = (notifier_t *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    struct bufferevent *bev;

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    if (fd < 0) {
        err_quit("accept");
    } else {
        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(n->base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, notifier_readcb, NULL, notifier_eventcb, n);
        bufferevent_enable(bev, EV_READ | EV_WRITE);
    }
}

void *notifier_loop(void *arg)
{
    notifier_t *n = (notifier_t *)arg;
    event_base_dispatch(n->base);
    return (void *)0;
}

int notifier_start(notifier_t *n)
{
    struct event *timer_event, *listener_event;
    struct timeval timeout = {10, 0};
    evutil_socket_t listener;
    struct sockaddr_in sin;
    int ret;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(8202);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_listen_socket_reuseable(listener);
    evutil_make_socket_nonblocking(listener);

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        err_quit("bind");
    }

    if (listen(listener, 128) < 0) {
        err_quit("listen");
    }

    listener_event = event_new(n->base, listener, EV_READ|EV_PERSIST, notifier_accept, n);
    if (NULL == listener_event) {
        err_quit("event_new");
    }
    event_add(listener_event, NULL);

    timer_event = event_new(n->base, -1, EV_PERSIST, notifier_timer, n);
    if (NULL == timer_event) {
        err_quit("event_new");
    }
    event_add(timer_event, &timeout);

    // create bev to pusher
    n->pusher->bev_notifier[1] = bufferevent_socket_new(n->base, n->pusher->sockpair_notifier[1], BEV_OPT_CLOSE_ON_FREE);
    if (!n->pusher->bev_notifier[1]) {
        err_quit("bufferevent_socket_new");
    }
    bufferevent_setcb(n->pusher->bev_notifier[1], notifier_pusher_readcb, notifier_pusher_writecb, notifier_pusher_eventcb, n);
    bufferevent_setwatermark(n->pusher->bev_notifier[1], EV_READ, sizeof(cmd_t), 0);
    bufferevent_enable(n->pusher->bev_notifier[1], EV_READ | EV_WRITE);

    // TODO create listener
    ret = pthread_create(&(n->thread_id), NULL, notifier_loop, n);
    if (ret != 0) {
        err_quit("pthread_create");
    }
    return 0;
}

int notifier_stop(notifier_t *n)
{
    n->stop = 1;
    return 0;
}
