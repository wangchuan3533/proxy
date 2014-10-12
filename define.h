#ifndef  __DEFINE_H_
#define  __DEFINE_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "base64_enc.h"
#include "sha1.h"
#include "uthash.h"

#define err_quit(fmt, args...) do {\
    fprintf(stderr, "[file:%s line:%d]", __FILE__, __LINE__);\
    fprintf(stderr, fmt, ##args);\
    exit(1);\
} while (0)

#define WORKER_NUM 4
#define CLIENT_HIGH_WATERMARK 4096

struct dispatcher_s;
struct worker_s;
struct pusher_s;
struct notifier_s;
struct client_s;
struct global_s;
enum http_headers_state_s;
struct http_headers_s;
enum frame_state_e;
struct websocket_frame_s;
enum client_state_e;
enum cmd_no_e;
struct cmd_s;

typedef struct dispatcher_s dispatcher_t;
typedef struct worker_s worker_t;
typedef struct pusher_s pusher_t;
typedef struct client_s client_t;
typedef struct notifier_s notifier_t;
typedef struct global_s global_t;
typedef enum http_headers_state_s http_headers_state_t;
typedef struct http_headers_s http_headers_t;
typedef enum frame_state_e frame_state_t;
typedef struct websocket_frame_s websocket_frame_t;
typedef enum client_state_e client_state_t;
typedef enum cmd_no_e cmd_no_t;
typedef struct cmd_s cmd_t;

struct global_s {
    struct worker_s *workers;
    struct client_s *clients;
};

enum cmd_no_e {
    CMD_ADD_CLIENT = 0,
    CMD_DEL_CLIENT,
    CMD_NOTIFY,
    CMD_BROADCAST,
    CMD_HEATBEAT,
};

struct cmd_s {
    cmd_no_t cmd_no;
    client_t *client;
    void *data;
    size_t length; 
};

extern global_t global;
#endif  //__DEFINE_H_
