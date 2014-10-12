#include "define.h"
#include "pusher.h"
#include "worker.h"

int broadcast_to_worker(void *data, size_t length)
{
    worker_t *w;
    cmd_t cmd;
    //printf("pusher timer\n");
    for (w = global.workers; w != NULL; w = w->next) {
        cmd.cmd_no = CMD_BROADCAST;
        cmd.data = malloc(length);
        memcpy(cmd.data, data, length);
        cmd.length = length;
        if (evbuffer_add(bufferevent_get_output(w->bev_pusher[1]), &cmd, sizeof cmd) != 0) {
            err_quit("evbuffer_add");
        }
    }
    return 0;
}

void pusher_timer(int fd, short event, void *arg)
{
    pusher_t *p = (pusher_t *)arg;

    if (p->stop) {
        printf("pusher stoped\n");
        event_base_loopexit(p->base, NULL);
    }
}

pusher_t *pusher_create()
{
    pusher_t *p = (pusher_t *)malloc(sizeof(pusher_t));
    if (NULL == p) {
        err_quit("malloc");
    }
    memset(p, 0, sizeof(pusher_t));
    p->base = event_base_new();
    if (NULL == p->base) {
        err_quit("event_base_new");
    }

    // create pipe to notifier
    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, p->sockpair_notifier) < 0) {
        err_quit("socketpair");
    }
    return p;
}
void pusher_destroy(pusher_t *p)
{
    if (p) {
        free(p);
    }
}

void pusher_delete_from_hash(client_t *c)
{
    client_t *tmp;

    // if in the hash, delete it
    HASH_FIND(h2, global.clients, &(c->user_id), sizeof(c->user_id), tmp);
    if (tmp && c == tmp) {
        HASH_DELETE(h2, global.clients, c);
    }
}

void pusher_worker_readcb(struct bufferevent *bev, void *arg)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    worker_t *w = (worker_t *)arg;
    client_t *c, *tmp;
    cmd_t cmd;

    while (evbuffer_get_length(input) >= sizeof cmd) {
        if (evbuffer_remove(input, &cmd, sizeof cmd) != sizeof cmd) {
            err_quit("evbuffer_remove");
        }
        switch (cmd.cmd_no) {
        case CMD_ADD_CLIENT:
#ifdef TRACE
    printf("client added\n");
#endif
            c = cmd.client;
            // check if the client exists
            HASH_FIND(h2, global.clients, &(c->user_id), sizeof(c->user_id), tmp);
            // found, reject it
            if (tmp != NULL) {
                cmd.cmd_no = CMD_DEL_CLIENT;
                if (evbuffer_add(bufferevent_get_output(w->bev_pusher[1]), &cmd, sizeof cmd) != 0) {
                    err_quit("evbuffer_add");
                }
                break;
            }
            HASH_ADD(h2, global.clients, user_id, sizeof(c->user_id), c);
            break;
        case CMD_DEL_CLIENT:
#ifdef TRACE
    printf("client deleted\n");
#endif
            c = cmd.client;
            pusher_delete_from_hash(c);
            client_destroy(c);
            break;
        case CMD_BROADCAST:
            broadcast_to_worker(cmd.data, cmd.length);
            free(cmd.data);
            break;
        default:
            break;
        }
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

// hooks
void pusher_worker_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}
void pusher_worker_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void pusher_notifier_readcb(struct bufferevent *bev, void *arg)
{
    char str_user_id[16]; 
    uint64_t user_id;
    client_t *tmp;
    size_t len;
    cmd_t cmd;

    len = evbuffer_get_length(bufferevent_get_input(bev));
    if (evbuffer_remove(bufferevent_get_input(bev), str_user_id, len) != len) {
        err_quit("evbuffer_remove");
    }
    str_user_id[len] = '\0';
    user_id = atoi(str_user_id);

    // find the client
    HASH_FIND(h1, global.clients, &user_id, sizeof(user_id), tmp);
    if (tmp) {
        cmd.cmd_no = CMD_NOTIFY;
        cmd.client = tmp;
        if (evbuffer_add(bufferevent_get_output(tmp->worker->bev_pusher[1]), &cmd, sizeof(cmd)) != 0) {
            err_quit("evbuffer_add");
        }
    } else {
        // ignore
    }

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void pusher_notifier_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}
void pusher_notifier_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}


void *pusher_loop(void *arg)
{
    pusher_t *p = (pusher_t *)arg;
    event_base_dispatch(p->base);
    return (void *)0;
}

int pusher_start(pusher_t *p)
{
    struct event *timer_event;
    struct timeval timeout = {10, 0};
    worker_t *w;
    int ret;

    timer_event = event_new(p->base, -1, EV_PERSIST, pusher_timer, p);
    if (NULL == timer_event) {
        err_quit("event_new");
    }
    event_add(timer_event, &timeout);

    // create bev to notifier
    p->bev_notifier[0] = bufferevent_socket_new(p->base, p->sockpair_notifier[0], BEV_OPT_CLOSE_ON_FREE);
    if (!p->bev_notifier[0]) {
        err_quit("bufferevent_socket_new");
    }
    bufferevent_setcb(p->bev_notifier[0], pusher_notifier_readcb, pusher_notifier_writecb, pusher_notifier_eventcb, p);
    //bufferevent_setwatermark(p->bev_notifier[0], EV_READ, sizeof(cmd_t), 0);
    bufferevent_enable(p->bev_notifier[0], EV_READ | EV_WRITE);

    // create bev to workers
    for (w = global.workers; w != NULL; w = w->next) {
        w->bev_pusher[1] = bufferevent_socket_new(p->base, w->sockpair_pusher[1], BEV_OPT_CLOSE_ON_FREE);
        if (!w->bev_pusher[1]) {
            err_quit("bufferevent_socket_new");
        }
        bufferevent_setcb(w->bev_pusher[1], pusher_worker_readcb, pusher_worker_writecb, pusher_worker_eventcb, w);
        bufferevent_setwatermark(w->bev_pusher[1], EV_READ, sizeof(cmd_t), 0);
        bufferevent_enable(w->bev_pusher[1], EV_READ | EV_WRITE);
    }

    ret = pthread_create(&(p->thread_id), NULL, pusher_loop, p);
    if (ret != 0) {
        err_quit("pthread_create");
    }
    return 0;
}

int pusher_stop(pusher_t *p)
{
    p->stop = 1;
    return 0;
}
