#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

const char *websocket_request = 
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Origin: http://example.com\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

void readcb(struct bufferevent *bev, void *arg)
{
    int length = evbuffer_get_length(bufferevent_get_input(bev));
    void *data = malloc(length);
    if (evbuffer_remove(bufferevent_get_input(bev), data, length) != length) {
        printf("evbuffer_remove");
        exit(1);
    }
    fwrite(data, 1, length, stdout);
    free(data);
}

void writecb(struct bufferevent *bev, void *arg)
{
//    evbuffer_add_printf(bufferevent_get_output(bev), websocket_request);
}

void eventcb(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_CONNECTED) {
        printf("connected\n");
        bufferevent_enable(bev, EV_READ | EV_WRITE);
    } else if (events & BEV_EVENT_TIMEOUT) {
        printf("timeout");
        evbuffer_add_printf(bufferevent_get_output(bev), websocket_request);
        bufferevent_enable(bev, EV_READ | EV_WRITE);
    } else if (events & BEV_EVENT_TIMEOUT) {
    } else {
        bufferevent_free(bev);
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_in serv_addr;
    struct event_base *base;
    struct bufferevent *bev, *bev_stdout;
    int i, addr_len = sizeof serv_addr;
    struct timeval timeout = {1, 0};
    
    base = event_base_new();
    if (evutil_parse_sockaddr_port("127.0.0.1:8200", (struct sockaddr *)&serv_addr, &addr_len)) {
        printf("evutil_parse_sockaddr_port");
        exit(1);
    }


    for (i = 0; i < 10; i++) {

        bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, readcb, writecb, eventcb, NULL);
        bufferevent_enable(bev, EV_READ | EV_WRITE);
        bufferevent_set_timeouts(bev, &timeout, &timeout);
        evbuffer_add_printf(bufferevent_get_output(bev), websocket_request);

        if (bufferevent_socket_connect(bev, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            bufferevent_free(bev);
            printf("bufferevent_socket_connect");
            exit(1);
        }
    }

    event_base_dispatch(base);
}
