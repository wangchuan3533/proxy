#ifndef  __WORKER_H_
#define  __WORKER_H_
#include "define.h"

// client states
enum client_state_e {
    CLIENT_STATE_ACCEPTED = 0,
    CLIENT_STATE_HTTP_PARSE_STARTED,
    CLIENT_STATE_HANDSHAKE_STARTED,
    CLIENT_STATE_WEBSOCKET_FRAME_LOOP,
};

struct worker_s {

    // event base
    struct event_base *base;
    worker_t *next;
    pthread_t thread_id;
    int stop;

    // clients
    client_t *clients;

    // pipe to dispatcher
    int sockpair_dispatcher[2];
    struct bufferevent *bev_dispatcher[2];

    // pipe to pusher
    int sockpair_pusher[2];
    struct bufferevent *bev_pusher[2];

};

struct client_s {
    uint64_t user_id;
    struct bufferevent *bev;
    struct evbuffer *buffer;
    // http headers
    http_headers_t *headers;
    // websocket frame
    websocket_frame_t *frame;
    // client state
    client_state_t state;
    // pusher hash handle
    UT_hash_handle h1;
    // worker hash handle
    UT_hash_handle h2;
    // close_flag
    int close_flag;
    // fd
    int fd;
    // context
    worker_t *worker;
};

client_t *client_create();
void client_destroy(client_t *c);
worker_t *worker_create();
void worker_destroy(worker_t *s);
int broadcast(worker_t *w, void *data, size_t length);
int worker_start(worker_t *w);
int worker_stop(worker_t *w);

// private
void websocket_readcb(struct bufferevent *bev, void *arg);
void websocket_writecb(struct bufferevent *bev, void *arg);
void websocket_eventcb(struct bufferevent *bev, short error, void *arg);
#endif  //__WORKER_H_;
