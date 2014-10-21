#ifndef  __WORKER_H_
#define  __WORKER_H_
#include "define.h"

// client states
enum client_state_e {
    CLIENT_STATE_ACCEPTED = 0,

    CLIENT_STATE_HTTP_REQUEST_PARSING,
    CLIENT_STATE_HTTP_REQUEST_PARSE_FINISHED,

    CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT,
    CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED,

    CLIENT_STATE_WEBSOCKET_REQUEST_PARSING,
    CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED,

    CLIENT_STATE_PEER_CONNECT_SENT,
    CLIENT_STATE_PEER_CONNECT_FINISHED,

    CLIENT_STATE_PEER_REQUEST_SENT,
    CLIENT_STATE_PEER_REQUEST_FINISHED,

    CLIENT_STATE_PEER_RESPONSE_RECEIVED,

    CLIENT_STATE_WEBSOCKET_RESPONSE_SENT,
    CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED,

    CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED_AND_SENT,

    CLIENT_STATE_HTTP_RESPONSE_SENT,
    CLIENT_STATE_HTTP_RESPONSE_FINISHED,

    CLIENT_STATE_CLOSED,

    // count
    CLIENT_STATE_COUNT
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
    struct bufferevent *peer;
    struct evbuffer *buffer;
    // http request
    http_request_t *request;
    // websocket frame
    websocket_frame_t *frame;
    // client state
    client_state_t state;
    // pusher hash handle
    UT_hash_handle hh;
    // fd
    int fd;
    // context
    worker_t *worker;
    void *peer_data;
    size_t peer_data_length;

    // websocket close handshake
    uint8_t close_frame_sent;

    // notify send mark
    uint8_t notify_frame_sent;
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
void client_readcb(struct bufferevent *bev, void *arg);
void client_writecb(struct bufferevent *bev, void *arg);
void client_eventcb(struct bufferevent *bev, short error, void *arg);
#endif  //__WORKER_H_;
