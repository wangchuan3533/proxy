#ifndef  __WORKER_H_
#define  __WORKER_H_
#include "define.h"

// client states
enum client_state_e {
    CLIENT_STATE_ACCEPTED = 0,
    CLIENT_STATE_HTTP_REQUEST_PARSING,
    CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT,
    CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED,
    CLIENT_STATE_WEBSOCKET_REQUEST_PARSING,
    CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED,
    CLIENT_STATE_PROXY_CONNECTED,
    CLIENT_STATE_PROXY_REQUEST_SENT,
    CLIENT_STATE_PROXY_REQUEST_FINISHED,
    CLIENT_STATE_PROXY_RESPONSE_PARSING,
    CLIENT_STATE_PROXY_RESPONSE_FINISHED,
    CLIENT_STATE_WEBSOCKET_RESPONSE_SENT,
    CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED,
    CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED,
    CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED_AND_SENT,
    CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_SENT,
    CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_SENT_AND_RECEIVED,
    CLIENT_STATE_HTTP_RESPONSE_SENT,
    CLIENT_STATE_HTTP_RESPONSE_FINISHED,
    CLIENT_STATE_CLOSED,

    // count
    CLIENT_STATE_COUNT
};

enum client_event_e {
    CLIENT_EVENT_READ,
    CLIENT_EVENT_WRITE,
    CLIENT_EVENT_TIMEOUT,
    CLIENT_EVENT_ERROR,

    // count
    CLIENT_EVENT_COUNT
};

typedef client_state_t (*fsm_func_t)(client_t *, client_event_t);

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
    http_request_t *headers;
    // websocket frame
    websocket_frame_t *frame;
    // client state
    client_state_t state;
    // pusher hash handle
    UT_hash_handle hh;
    // close_flag
    int close_flag;
    // fd
    int fd;
    // context
    worker_t *worker;
};

struct client_index_s {
    uint64_t user_id;
    worker_t *worker;
    UT_hash_handle hh;
};

client_t *client_create();
void client_destroy(client_t *c);

client_index_t *client_index_create();
void client_index_destroy(client_index_t *c_i);

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
