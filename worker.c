#include "define.h"
#include "http.h"
#include "websocket.h"
#include "worker.h"

worker_t *worker_create()
{
    worker_t *w = (worker_t *)malloc(sizeof(worker_t));
    if (NULL == w) {
        err_quit("malloc");
    }
    memset(w, 0, sizeof(worker_t));
    w->base = event_base_new();
    if (NULL == w->base) {
        err_quit("event_base_new");
    }

    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, w->sockpair_dispatcher) < 0) {
        err_quit("socketpair");
    }

    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, w->sockpair_pusher) < 0) {
        err_quit("socketpair");
    }

    return w;
}

void worker_destroy(worker_t *w)
{
    // destroy event_base
    if (w) {
        // clear the hash
        HASH_CLEAR(hh, w->clients); 
        free(w);
    }
}

client_t *client_create()
{
    client_t *c = (client_t *)malloc(sizeof(client_t));
    if (NULL == c) {
        err_quit("malloc");
    }
    // memset
    memset(c, 0, sizeof(client_t));
    return c;
}

void client_destroy(client_t *c)
{
    if (c) {
        if (c->request)
            http_request_destroy(c->request);
        if (c->frame)
            websocket_frame_destroy(c->frame);
        free(c);
    }
}

client_index_t *client_index_create()
{
    client_index_t *c_i = (client_index_t *)malloc(sizeof(client_index_t));
    if (NULL == c_i) {
        err_quit("malloc");
    }
    memset(c_i, 0, sizeof(client_index_t));
    return c_i;
}

void client_index_destroy(client_index_t *c_i)
{
    if (c_i) {
        free(c_i);
    }
}

void worker_delete_from_hash(client_t *c)
{
    client_t *tmp;

    // if in the hash, delete it
    HASH_FIND(hh, c->worker->clients, &(c->user_id), sizeof(c->user_id), tmp);
    if (tmp && c == tmp) {
        HASH_DELETE(hh, c->worker->clients, c);
    }
}

int broadcast_raw(worker_t *w, void *data, size_t length)
{
    client_t *itr, *tmp;
    HASH_ITER(hh, w->clients, itr, tmp) {
        if (evbuffer_add(bufferevent_get_output(itr->bev), data, length) != 0) {
            err_quit("evbuffer_add");
        }
        bufferevent_enable(itr->bev, EV_WRITE);
        itr->notify_flag = 1;
    }
    return 0;
}


int websocket_broadcast(worker_t *w, void *data, size_t len)
{
    int ret;
    void *tmp;
    size_t length;
    cmd_t cmd = {0};
    struct evbuffer *buffer = evbuffer_new();

    ret = send_text_frame(buffer, data, len);
    if (ret != 0) {
        // TODO
    }

    length = evbuffer_get_length(buffer);
    tmp = malloc(length);
    if (evbuffer_remove(buffer, tmp, length) != length) {
        err_quit("evbuffer_remove");
    }
    evbuffer_free(buffer);

    cmd.cmd_no = CMD_BROADCAST;
    cmd.data = tmp;
    cmd.length = length;
    if (evbuffer_add(bufferevent_get_output(w->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
        err_quit("evbuffer_add");
    }

    return 0;
}

void peer_readcb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg;

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    switch (c->state) {
    case CLIENT_STATE_PEER_REQUEST_FINISHED:
        c->peer_data_length = evbuffer_get_length(bufferevent_get_input(c->peer));
        c->peer_data = malloc(c->peer_data_length);

        if (c->peer_data == NULL) {
            err_quit("malloc");
        }
        if (evbuffer_remove(bufferevent_get_input(c->peer), c->peer_data, c->peer_data_length) != c->peer_data_length) {
            err_quit("evbuffer_remove");
        }
#ifdef DUMP_DATA
        write(STDOUT_FILENO, c->peer_data, c->peer_data_length);
#endif
        c->state = CLIENT_STATE_PEER_RESPONSE_RECEIVED;
    case CLIENT_STATE_PEER_RESPONSE_RECEIVED:
        send_text_frame(bufferevent_get_output(c->bev), c->peer_data, c->peer_data_length);
        bufferevent_enable(c->bev, EV_WRITE);
        bufferevent_free(c->peer);
        free(c->peer_data);
        c->peer = NULL;
        c->peer_data = NULL;
        c->peer_data_length = 0;
        c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_SENT;
        break;
    default:
        err_quit("Opps");
    }
}

void peer_writecb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg;

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    switch (c->state) {
    case CLIENT_STATE_PEER_REQUEST_SENT:
        bufferevent_enable(c->peer, EV_READ);
        c->state = CLIENT_STATE_PEER_REQUEST_FINISHED;
        break;
    default:
        err_quit("Opps");
    }
}

void peer_eventcb(struct bufferevent *bev, short events, void *arg)
{
    client_t *c = (client_t *)arg;
    const char *timeout = "timeout!";
    const char *not_found = "not found 404!";
    const char *closed = "connection closed!";
    const char *unknown = "unknown error!";

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    if (events & BEV_EVENT_CONNECTED) {
        switch (c->state) {
        case CLIENT_STATE_PEER_CONNECT_SENT:
            bufferevent_enable(bev, EV_READ | EV_WRITE);
            c->state = CLIENT_STATE_PEER_CONNECT_FINISHED;
            if (evbuffer_add(bufferevent_get_output(c->peer), c->frame->data, c->frame->length) != 0) {
                err_quit("evbuffer_add");
            }
            bufferevent_enable(c->peer, EV_WRITE);
            c->state = CLIENT_STATE_PEER_REQUEST_SENT;
            break;
        default:
            err_quit("Opps");
        }
    } else {
        switch (c->state) {
        case CLIENT_STATE_PEER_RESPONSE_RECEIVED:
            free(c->peer_data);
            c->peer = NULL;
            c->peer_data = NULL;
            c->peer_data_length = 0;
        case CLIENT_STATE_PEER_CONNECT_SENT:
        case CLIENT_STATE_PEER_CONNECT_FINISHED:
        case CLIENT_STATE_PEER_REQUEST_SENT:
        case CLIENT_STATE_PEER_REQUEST_FINISHED:
            bufferevent_free(c->peer);
            break;
        default:
            err_quit("Opps");
        }
        if (events & BEV_EVENT_TIMEOUT) {
            send_text_frame(bufferevent_get_output(c->bev), timeout, strlen(timeout));
        } else if (events & BEV_EVENT_ERROR) {
            send_text_frame(bufferevent_get_output(c->bev), not_found, strlen(not_found));
        } else if (events & BEV_EVENT_EOF) {
            send_text_frame(bufferevent_get_output(c->bev), closed, strlen(closed));
        } else {
            send_text_frame(bufferevent_get_output(c->bev), unknown, strlen(unknown));
        }
        bufferevent_enable(c->bev, EV_WRITE);
        c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_SENT;
    }
}

int peer_connect(client_t *c)
{
    struct sockaddr_in sin;
    struct timeval timeout = {1, 0};

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(ECHO_SERVER_PORT);

    c->peer = bufferevent_socket_new(c->worker->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(c->peer, peer_readcb, peer_writecb, peer_eventcb, c);
    bufferevent_disable(c->peer, EV_READ | EV_WRITE);
    bufferevent_set_timeouts(c->peer, &timeout, &timeout);

    if (bufferevent_socket_connect(c->peer, (struct sockaddr *)&sin, sizeof sin) < 0) {
        bufferevent_free(c->peer);
        err_quit("bufferevent_socket_connect");
    }
    return 0;
}

void client_readcb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg, *tmp;
    struct evbuffer *input = bufferevent_get_input(bev);
    cmd_t cmd = {0};

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    while (evbuffer_get_length(input)) {
        switch (c->state) {
        case CLIENT_STATE_ACCEPTED:
            c->request = http_request_create();
            c->state = CLIENT_STATE_HTTP_REQUEST_PARSING;
        case CLIENT_STATE_HTTP_REQUEST_PARSING:
            http_request_parse(bufferevent_get_input(c->bev), c->request);
            if (c->request->state == HTTP_REQUEST_STATE_ERROR) {
                c->state = CLIENT_STATE_CLOSED;
                goto closed;
            } else if (c->request->state != HTTP_REQUEST_STATE_FINISHED) {
                goto finished; 
            }
            bufferevent_disable(c->bev, EV_READ);
            c->state = CLIENT_STATE_HTTP_REQUEST_PARSE_FINISHED;
#ifdef DUMP_DATA
            print_http_request(c->request);
#endif
        case CLIENT_STATE_HTTP_REQUEST_PARSE_FINISHED:

            // check the request, and dispatch it
            if (!c->request->method || strncasecmp(c->request->method, "GET", strlen("GET"))) {
                send_http_response(bufferevent_get_output(c->bev), 400);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_HTTP_RESPONSE_SENT;
            } else if (!c->request->connection || strncasecmp(c->request->connection, "Upgrade", strlen("Upgrade")) || !c->request->sec_websocket_key) {
                send_http_response(bufferevent_get_output(c->bev), 200);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_HTTP_RESPONSE_SENT;
            } if (!c->request->request_uri || c->request->request_uri[0] != '/' || atoi(c->request->request_uri + 1) <= 0) {
                send_http_response(bufferevent_get_output(c->bev), 403);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_HTTP_RESPONSE_SENT;
            } else {
                c->user_id = atoi(c->request->request_uri + 1);
                // check if the user_id dose not exist, then add to hash
                HASH_FIND(hh, c->worker->clients, &(c->user_id), sizeof(c->user_id), tmp);
                if (tmp != NULL) {
                    send_http_response(bufferevent_get_output(c->bev), 403);
                    bufferevent_enable(c->bev, EV_WRITE);
                    c->state = CLIENT_STATE_HTTP_RESPONSE_SENT;
                } else {
                    HASH_ADD(hh, c->worker->clients, user_id, sizeof(c->user_id), c);
                    // notify pusher
                    cmd.cmd_no = CMD_ADD_CLIENT;
                    cmd.data = (void *)c->user_id;
                    if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                        err_quit("evbuffer_add");
                    }

                    // send websocket handshake
                    send_handshake(bufferevent_get_output(c->bev), c->request->sec_websocket_key);
                    bufferevent_enable(c->bev, EV_WRITE);
                    c->state = CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT;
                }
            }
            goto finished;
        case CLIENT_STATE_WEBSOCKET_REQUEST_PARSING:
            parse_frame(bufferevent_get_input(c->bev), c->frame);
            if (c->frame->state == FRAME_STATE_ERROR) {
                c->state = CLIENT_STATE_CLOSED;
                goto websocket_closed;
            } else if (c->frame->state != FRAME_STATE_FINISHED) {
                goto finished;
            }
            bufferevent_disable(c->bev, EV_READ);
            c->state = CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED;
        case CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED:
            switch (c->frame->opcode) {
            case OPCODE_PING_FRAME:
                send_pong_frame(bufferevent_get_output(c->bev), c->frame->data, c->frame->length);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_SENT;
                goto finished;
            case OPCODE_CLOSE_FRAME:
                if (c->close_frame_sent) {
                    c->state = CLIENT_STATE_CLOSED;
                    goto websocket_closed;
                }
                send_close_frame(bufferevent_get_output(c->bev), c->frame->data, c->frame->length);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED_AND_SENT;
                goto finished;
            case OPCODE_TEXT_FRAME:
#ifdef WEBSOCKET_ECHO
                // websocket echo
                send_text_frame(bufferevent_get_output(c->bev), c->frame->data, c->frame->length);
                bufferevent_enable(c->bev, EV_WRITE);
                c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_SENT;
#endif
#ifdef WEBSOCKET_BROADCAST
                // websocket broadcast
                {
                    char str[128];
                    sprintf(str, "[%lu]: ", c->user_id);
                    strncat(str, c->frame->data, c->frame->length);
                    websocket_broadcast(c->worker, str, strlen(str));

                    send_text_frame(bufferevent_get_output(c->bev), "ok", 2);
                    bufferevent_enable(c->bev, EV_WRITE);
                    c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_SENT;
                }
#endif
#ifdef WEBSOCKET_PROXY
                // websocket peer
                peer_connect(c);
                c->state = CLIENT_STATE_PEER_CONNECT_SENT;
#endif
                goto finished;
            default:
                err_quit("Opps");
            }
            goto finished;
        default:
            err_quit("Oops read client state:[%d]\n", c->state);
        }
    }

websocket_closed:
    if (c->user_id) {
        // delete from hash
        worker_delete_from_hash(c);
        if (!c->conflict_flag) {
            // notify the pusher
            cmd.cmd_no = CMD_DEL_CLIENT;
            cmd.data = (void *)c->user_id;
            if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                err_quit("evbuffer_add");
            }
        }
    }
closed:
    bufferevent_free(c->bev);
    client_destroy(c);
finished:
    return;
}

void client_writecb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg;
    cmd_t cmd = {0};

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    // clear the write cb
    bufferevent_disable(bev, EV_WRITE);

    if (c->close_frame_sent) {
        goto finished;
    }
    if (c->notify_flag) {
        c->notify_flag = 0;
        goto finished;
    }
    switch (c->state) {
    case CLIENT_STATE_HTTP_RESPONSE_SENT:
        c->state = CLIENT_STATE_HTTP_RESPONSE_FINISHED;
    case CLIENT_STATE_HTTP_RESPONSE_FINISHED:
        c->state = CLIENT_STATE_CLOSED;
        goto closed;
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT:
        c->state = CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED;
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED:
        c->frame = websocket_frame_create();
        bufferevent_enable(c->bev, EV_READ);
        c->state = CLIENT_STATE_WEBSOCKET_REQUEST_PARSING;
        goto finished;
    case CLIENT_STATE_WEBSOCKET_RESPONSE_SENT:
        websocket_frame_destroy(c->frame);
        c->frame = NULL;
        c->state = CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED;
    case CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED:
        c->frame = websocket_frame_create();
        bufferevent_enable(c->bev, EV_READ);
        c->state = CLIENT_STATE_WEBSOCKET_REQUEST_PARSING;
        goto finished;
    case CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED_AND_SENT:
        c->state = CLIENT_STATE_CLOSED;
        goto websocket_closed;
    default:
        err_quit("Opps");
    }

websocket_closed:
    if (c->user_id) {
        // delete from hash
        worker_delete_from_hash(c);
        if (!c->conflict_flag) {
            // notify the pusher
            cmd.cmd_no = CMD_DEL_CLIENT;
            cmd.data = (void *)c->user_id;
            if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                err_quit("evbuffer_add");
            }
        }
    }
closed:
    bufferevent_free(c->bev);
    client_destroy(c);
finished:
    return;
}

void client_eventcb(struct bufferevent *bev, short error, void *arg)
{
    client_t *c = (client_t *)arg;
    cmd_t cmd = {0};

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        printf("client:%lu EOF\n", c->user_id);
    } else if (error & BEV_EVENT_ERROR) {
        printf("client:%lu ERROR\n", c->user_id);
        /* check errno to see what error occurred */
    } else if (error & BEV_EVENT_TIMEOUT) {
        printf("client:%lu TIMEOUT\n", c->user_id);
    }
#endif

    switch (c->state) {
    case CLIENT_STATE_ACCEPTED:
    case CLIENT_STATE_HTTP_REQUEST_PARSING:
    case CLIENT_STATE_HTTP_REQUEST_PARSE_FINISHED:
        goto closed;
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT:
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED:
    case CLIENT_STATE_WEBSOCKET_REQUEST_PARSING:
    case CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED:
    case CLIENT_STATE_PEER_CONNECT_SENT:
    case CLIENT_STATE_PEER_CONNECT_FINISHED:
    case CLIENT_STATE_PEER_REQUEST_SENT:
    case CLIENT_STATE_PEER_REQUEST_FINISHED:
    case CLIENT_STATE_PEER_RESPONSE_RECEIVED:
    case CLIENT_STATE_WEBSOCKET_RESPONSE_SENT:
    case CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED:
    case CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED_AND_SENT:
        goto websocket_closed;
    default:
        err_quit("Opps");
    }

websocket_closed:
    if (c->user_id) {
        // delete from hash
        worker_delete_from_hash(c);
        // notify the pusher
        if (!c->conflict_flag) {
            cmd.cmd_no = CMD_DEL_CLIENT;
            cmd.data = (void *)c->user_id;
            if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                err_quit("evbuffer_add");
            }
        }
    }
closed:
    bufferevent_free(c->bev);
    client_destroy(c);
}

void worker_timer(int fd, short event, void *arg)
{
    worker_t *w = (worker_t *)arg;
    if (w->stop) {
        printf("worker stoped\n");
        event_base_loopexit(w->base, NULL);
    }
}

void echo_readcb(struct bufferevent *bev, void *arg)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif

#ifdef DUMP_DATA
    char *buf = malloc(evbuffer_get_length(input));
    evbuffer_copyout(input, buf, evbuffer_get_length(input));
    write(STDOUT_FILENO, buf, evbuffer_get_length(input));
    free(buf);
#endif

    evbuffer_add_buffer(output, input);
}

void echo_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void echo_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    if (error | BEV_EVENT_TIMEOUT) {
        // TODO
    } else if (error | BEV_EVENT_ERROR) {
        // TODO
    }

    bufferevent_free(bev);
}

void worker_dispatcher_readcb(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    struct bufferevent *bev_client;
    struct timeval timeout = {1000, 0};
    struct evbuffer *input = bufferevent_get_input(bev);
    client_t *c;
    cmd_t cmd = {0};

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    while (evbuffer_get_length(input) >= sizeof cmd) {
        if (evbuffer_remove(input, &cmd, sizeof cmd) != sizeof cmd) {
            err_quit("evbuffer_remove");
        }
        switch (cmd.cmd_no) {
        case CMD_ADD_CLIENT:
            c = client_create();
            c->fd = (int)(uint64_t)cmd.data;
            c->worker = w;
            bev_client = bufferevent_socket_new(w->base, c->fd, BEV_OPT_CLOSE_ON_FREE);
            if (!bev_client) {
                err_quit("bufferevent_socket_new");
            }
            c->bev = bev_client;
            bufferevent_setcb(bev_client, client_readcb, client_writecb, client_eventcb, c);
            bufferevent_setwatermark(bev_client, EV_READ, 0, CLIENT_HIGH_WATERMARK);
            bufferevent_enable(bev_client, EV_READ);
            bufferevent_disable(bev_client, EV_WRITE);
            bufferevent_set_timeouts(bev_client, &timeout, &timeout);
            break;
        default:
            err_quit("Opps");
        }
    }
}

void worker_dispatcher_readcb_echo(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    struct bufferevent *bev_client;
    struct timeval timeout = {1000, 0};
    struct evbuffer *input = bufferevent_get_input(bev);
    cmd_t cmd = {0};

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    while (evbuffer_get_length(input) >= sizeof cmd) {
        if (evbuffer_remove(input, &cmd, sizeof cmd) != sizeof cmd) {
            err_quit("evbuffer_remove");
        }
        switch (cmd.cmd_no) {
        case CMD_ADD_CLIENT:
            bev_client = bufferevent_socket_new(w->base, (int)(uint64_t)cmd.data, BEV_OPT_CLOSE_ON_FREE);
            bufferevent_setcb(bev_client, echo_readcb, NULL, echo_eventcb, NULL);
            bufferevent_setwatermark(bev_client, EV_READ, 0, CLIENT_HIGH_WATERMARK);
            bufferevent_enable(bev_client, EV_READ | EV_WRITE);
            bufferevent_set_timeouts(bev_client, &timeout, &timeout);
            break;
        default:
            break;
        }
    }
}

void worker_dispatcher_eventcb(struct bufferevent *bev, short error, void *arg)
{
    printf("worker error\n");
    worker_t *w = (worker_t *)arg;

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    if (error | BEV_EVENT_TIMEOUT) {
        // TODO
    } else if (error | BEV_EVENT_ERROR) {
        // TODO
    }
    w->stop = 1;
}

void worker_pusher_readcb(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    uint64_t user_id;
    client_t *tmp;

#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
    cmd_t cmd = {0};
    while (evbuffer_get_length(bufferevent_get_input(bev)) >= sizeof cmd) {
        if (evbuffer_remove(bufferevent_get_input(bev), &cmd, sizeof cmd) != sizeof cmd) {
            err_quit("evbuffer_remove");
        }
        switch (cmd.cmd_no) {
        case CMD_BROADCAST:
            broadcast_raw(w, cmd.data, cmd.length);
            free(cmd.data);
            break;
        case CMD_DEL_CLIENT:
            user_id = (uint64_t)cmd.data;
            HASH_FIND(hh, w->clients, &user_id, sizeof(user_id), tmp);
            if (tmp == NULL) {
                // imposible
                err_quit("imposible");
            }

            send_close_frame(bufferevent_get_output(tmp->bev), "already login", strlen("already login"));
            bufferevent_enable(tmp->bev, EV_WRITE);
            tmp->close_frame_sent = 1;
            tmp->conflict_flag = 1;
            break;
        case CMD_NOTIFY:
            user_id = (uint64_t)cmd.data;
            HASH_FIND(hh, w->clients, &user_id, sizeof(user_id), tmp);
            if (tmp == NULL) {
                break;
            }

            send_text_frame(bufferevent_get_output(tmp->bev), "message", strlen("message"));
            bufferevent_enable(tmp->bev, EV_WRITE);
            tmp->notify_flag = 1;
            break;
        default:
            err_quit("Opps");
        }
    }
}

void worker_pusher_eventcb(struct bufferevent *bev, short error, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void *worker_loop(void *arg)
{
    worker_t *w = (worker_t *)arg;
    struct event *timer_event;
    struct timeval timeout = {1, 0};

    // bufferevent to dispatcher
    w->bev_dispatcher[0] = bufferevent_socket_new(w->base, w->sockpair_dispatcher[0], BEV_OPT_CLOSE_ON_FREE);
    if (!w->bev_dispatcher[0]) {
        err_quit("bufferevent_socket_new");
    }
#ifdef ECHO_SERVER
    bufferevent_setcb(w->bev_dispatcher[0], worker_dispatcher_readcb_echo, NULL, worker_dispatcher_eventcb, w);
#else
    bufferevent_setcb(w->bev_dispatcher[0], worker_dispatcher_readcb, NULL, worker_dispatcher_eventcb, w);
#endif
    bufferevent_setwatermark(w->bev_dispatcher[0], EV_READ, sizeof(cmd_t), 0);
    bufferevent_enable(w->bev_dispatcher[0], EV_READ | EV_WRITE);

    // bufferevent to pusher
    w->bev_pusher[0] = bufferevent_socket_new(w->base, w->sockpair_pusher[0], BEV_OPT_CLOSE_ON_FREE);
    if (!w->bev_pusher[0]) {
        err_quit("bufferevent_socket_new");
    }
    bufferevent_setcb(w->bev_pusher[0], worker_pusher_readcb, NULL, worker_pusher_eventcb, w);
    bufferevent_setwatermark(w->bev_pusher[0], EV_READ, sizeof(cmd_t), 0);
    bufferevent_enable(w->bev_pusher[0], EV_READ | EV_WRITE);

    timer_event = event_new(w->base, -1, EV_PERSIST, worker_timer, w);
    if (NULL == timer_event) {
        err_quit("event_new");
    }
    event_add(timer_event, &timeout);
    event_base_dispatch(w->base);
    return (void *)0;
}

int worker_start(worker_t *w)
{
    int ret;
    ret = pthread_create(&(w->thread_id), NULL, worker_loop, w);
    if (ret != 0) {
        err_quit("pthread_create");
    }
    return 0;
}

int worker_stop(worker_t *w)
{
    w->stop = 1;
    return 0;
}
