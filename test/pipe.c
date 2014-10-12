/***************************************************************************
 * 
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file pipe.c
 * @author wangchuan02(com@baidu.com)
 * @date 2014/09/24 09:05:45
 * @brief 
 *  
 **/
#include <pthread.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#define err_quit(fmt, args...) do {\
    fprintf(stderr, fmt, ##args);\
    exit(1);\
} while (0)


void readcb(struct bufferevent *bev, void *arg)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output((struct bufferevent *)arg);
    int len;
    char buffer[1024];
    len = evbuffer_remove(input, buffer, sizeof(buffer));
    evbuffer_add(output, buffer, len);

}

void writecb(struct bufferevent *bev, void *arg)
{
}

void errorcb(struct bufferevent *bev, short error, void *arg)
{
    printf("error\n");
}

void *dispatcher_loop(void *arg)
{
    struct event_base *base = NULL;
    struct bufferevent *bev = NULL;
    int fd = (int)arg;

    base = event_base_new();
    if (base == NULL) {
        err_quit("event_base_new");
    }

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        err_quit("bufferevent_socket_new");
    }
    bufferevent_setcb(bev, readcb, writecb, errorcb, bev);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    event_base_dispatch(base);
}

void *worker_loop(void *arg)
{
    struct event_base *base = NULL;
    struct bufferevent *bev = NULL;
    int fd = (int)arg;

    base = event_base_new();
    if (base == NULL) {
        err_quit("event_base_new");
    }

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        err_quit("bufferevent_socket_new");
    }
    bufferevent_setcb(bev, readcb, writecb, errorcb, bev);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    event_base_dispatch(base);
}


int main(int argc, char **argv)
{
    evutil_socket_t fd[2];
    int ret;
    void *status;
    pthread_t dispatcher, worker;

    ret = pthread_create(&dispatcher, NULL, dispatcher_loop, (void *)fd[0]);
    if (ret) {
        err_quit("pthread_create");
    }
    ret = pthread_create(&worker, NULL, worker_loop, (void *)fd[1]);
    if (ret) {
        err_quit("pthread_create");
    }

    ret = evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    ret = evutil_make_socket_nonblocking(fd[0]);
    ret = evutil_make_socket_nonblocking(fd[1]);

    pthread_join(dispatcher, &status);
    pthread_join(worker, &status);
    return 0;
}
