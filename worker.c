#include "define.h"
#include "http.h"
#include "websocket.h"
#include "worker.h"

#if 0
client_state_t fsm_http_parse(client_t *c, client_state_t state, client_event_t event)
{
    assert(event == CLIENT_EVENT_READ);

    if (c->state == CLIENT_STATE_ACCEPTED) {
        c->state = CLIENT_STATE_HTTP_RESPONSE_FINISHED;
        // c->headers = http_request_create();
    }

    ret = parse_http(bufferevent_get_input(c->bev), c->headers);
    return 0;
}

fsm_func_t fsm_table[CLIENT_STATE_COUNT][CLIENT_EVENT_COUNT] = 
{
    {NULL}
};
#endif

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
    c->headers = http_request_create();
    c->frame = ws_frame_create();
    return c;
}

void client_destroy(client_t *c)
{
    if (c) {
        ws_frame_destroy(c->frame);
        http_request_destroy(c->headers);
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

// hooks
void proxy_readcb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg;
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    char *buf = malloc(len);

    if (buf == NULL) {
        err_quit("malloc");
    }
    if (evbuffer_remove(input, buf, len) != len) {
        err_quit("evbuffer_remove");
    }
#ifdef SEE_DATA
    write(STDOUT_FILENO, buf, len);
#endif
    send_text_frame(bufferevent_get_output(c->bev), buf, len);
    bufferevent_free(bev);
    free(buf);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void proxy_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void proxy_eventcb(struct bufferevent *bev, short events, void *arg)
{
    client_t *c = (client_t *)arg;
    const char *timeout = "timeout!";
    const char *not_found = "not found 404!";
    const char *closed = "connection closed!";
    const char *unknown = "unknown error!";

    if (events & BEV_EVENT_CONNECTED) {
        bufferevent_enable(bev, EV_READ | EV_WRITE);
    } else if (events & BEV_EVENT_TIMEOUT) {
        send_text_frame(bufferevent_get_output(c->bev), timeout, strlen(timeout));
        bufferevent_free(bev);
    } else if (events & BEV_EVENT_ERROR) {
        send_text_frame(bufferevent_get_output(c->bev), not_found, strlen(not_found));
        bufferevent_free(bev);
    } else if (events & BEV_EVENT_EOF) {
        send_text_frame(bufferevent_get_output(c->bev), closed, strlen(closed));
        bufferevent_free(bev);
    } else {
        send_text_frame(bufferevent_get_output(c->bev), unknown, strlen(unknown));
        bufferevent_free(bev);
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

int proxy_request(client_t *c, void *data, size_t length)
{
    struct sockaddr_in sin;
    struct bufferevent *bev;
    struct timeval timeout = {1, 0};

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(ECHO_SERVER_PORT);

    bev = bufferevent_socket_new(c->worker->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, proxy_readcb, proxy_writecb, proxy_eventcb, c);
    bufferevent_set_timeouts(bev, &timeout, &timeout);
    if (evbuffer_add(bufferevent_get_output(bev), data, length) != 0) {
        err_quit("evbuffer_add");
    }

    if (bufferevent_socket_connect(bev, (struct sockaddr *)&sin, sizeof sin) < 0) {
        bufferevent_free(bev);
        err_quit("bufferevent_socket_connect");
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

// FSM process
int client_process(client_t *c, client_event_t event)
{
    switch (c->state) {
    case CLIENT_STATE_ACCEPTED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_HTTP_REQUEST_PARSING:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_SENT:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_HANDSHAKE_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_REQUEST_PARSING:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_REQUEST_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_PROXY_CONNECTED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_PROXY_REQUEST_SENT:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_PROXY_REQUEST_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_PROXY_RESPONSE_PARSING:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_PROXY_RESPONSE_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_RESPONSE_SENT:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_RESPONSE_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_RECEIVED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_WEBSOCKET_CLOSE_FRAME_SENT:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_HTTP_RESPONSE_SENT:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    case CLIENT_STATE_HTTP_RESPONSE_FINISHED:
        switch (event) {
        case CLIENT_EVENT_READ:
            break;
        case CLIENT_EVENT_WRITE:
            break;
        case CLIENT_EVENT_TIMEOUT:
            break;
        case CLIENT_EVENT_ERROR:
            break;
        default:
            break;
        }
        break;
    }
    return 0;
}

int websocket_handle(client_t *c)
{
    websocket_frame_t *f = c->frame;
    cmd_t cmd = {0};

    switch (f->opcode) {
    case OPCODE_PING_FRAME:
        send_pong_frame(bufferevent_get_output(c->bev), f->data, f->length);
        // TODO write cb
        break;
    case OPCODE_TEXT_FRAME:
#if 0
        // websocket echo
        send_text_frame(bufferevent_get_output(c->bev), f->data, f->length);
#endif

#if 0
        {
            char str[128];
            // websocket broadcast
            sprintf(str, "[%lu]: ", c->user_id);
            strncat(str, f->data, f->length);
            websocket_broadcast(c->worker, str, strlen(str));
        }
#endif

#if 1
        // websocket proxy
        proxy_request(c, f->data, f->length);
#endif

        // TODO write cb
        break;
    case OPCODE_PONG_FRAME:
        // TODO
        break;
    case OPCODE_CONTINUATION_FRAME:
        // TODO not surpport
        break;
    case OPCODE_CLOSE_FRAME:
        if (c->close_flag) {

            worker_delete_from_hash(c);
            // notify the pusher
            cmd.cmd_no = CMD_DEL_CLIENT;
            cmd.data = (void *)c->user_id;
            if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                err_quit("evbuffer_add");
            }
            bufferevent_free(c->bev);
            client_destroy(c);
            break;
        }
        send_close_frame(bufferevent_get_output(c->bev), f->data, f->length);
        c->close_flag = 1;
        bufferevent_setcb(c->bev, websocket_readcb, websocket_writecb, websocket_eventcb, c);
        break;
    default:
        break;
    }
    return 0;
}

void websocket_readcb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg;
    struct evbuffer *input = bufferevent_get_input(bev);
    int ret;

    while (evbuffer_get_length(input)) {
        switch (c->state) {
        case CLIENT_STATE_ACCEPTED:
            c->state = CLIENT_STATE_HTTP_PARSE_STARTED;
        case CLIENT_STATE_HTTP_PARSE_STARTED:
            ret = parse_http(input, c->headers);
            // http parse error
            if (ret != 0) {
                // TODO error handling
                return;
            }
            if (c->headers->state != HTTP_REQUEST_STATE_FINISHED) {
                // TODO is this ok?
                return;
            }

            // parse finished
#ifdef TRACE
    print_http_request(c->headers);
#endif
            // check the websocket request
            ret = check_websocket_request(c->headers);
            if (ret == 0) {
                // asigned user_id
                send_handshake(bufferevent_get_output(c->bev), c->headers->sec_websocket_key);
                c->state = CLIENT_STATE_HANDSHAKE_STARTED;
                bufferevent_setcb(bev, websocket_readcb, websocket_writecb, websocket_eventcb, arg);
            // not websocket, send 200 ok, and close socket when finish
            } else {
                send_http_response(bufferevent_get_output(c->bev));
                c->close_flag = 1;
                bufferevent_setcb(bev, websocket_readcb, websocket_writecb, websocket_eventcb, arg);
            }
            break;
        case CLIENT_STATE_WEBSOCKET_FRAME_LOOP:
            ret = parse_frame(input, c->frame);
            if (ret != 0) {
                // TODO error handling
                return;
            }
            if (c->frame->state == FRAME_STATE_FINISHED) {
                // debug
                ret = websocket_handle(c);
                if (ret) {
                    // TODO error handling
                }
                // clear
                ws_frame_clear(c->frame);
            }
            // send client's frame to co worker

            break;
        case CLIENT_STATE_HANDSHAKE_STARTED:
            // waiting write to complete, do not read
            return;
        default:
            err_quit("Oops read client state:[%d]\n", c->state);
        }
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void websocket_writecb(struct bufferevent *bev, void *arg)
{
    client_t *c = (client_t *)arg, *tmp;
    cmd_t cmd = {0};

    // clear the write cb
    bufferevent_setcb(bev, websocket_readcb, NULL, websocket_eventcb, arg);

    switch (c->state) {
    case CLIENT_STATE_HANDSHAKE_STARTED:
        // handshaked , then add to hash
        // check if the user_id exist
        c->user_id = atoi(c->headers->request_uri + 1);
        HASH_FIND(hh, c->worker->clients, &(c->user_id), sizeof(c->user_id), tmp);
        if (tmp != NULL) {
            send_close_frame(bufferevent_get_output(bev), "already login", strlen("already login"));
            c->close_flag = 1;
            return;
        }
        
        HASH_ADD(hh, c->worker->clients, user_id, sizeof(c->user_id), c);
        // notify pusher
        cmd.cmd_no = CMD_ADD_CLIENT;
        cmd.data = (void *)c->user_id;
        if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
            err_quit("evbuffer_add");
        }

        // start the frame loop
        c->state = CLIENT_STATE_WEBSOCKET_FRAME_LOOP;
        break;
    default:
        break;
    }

    if (c->close_flag) {
        if (c->user_id) {
            // delete from hash
            worker_delete_from_hash(c);
            // notify the pusher
            cmd.cmd_no = CMD_DEL_CLIENT;
            cmd.data = (void *)c->user_id;
            if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
                err_quit("evbuffer_add");
            }
        }
        bufferevent_free(bev);
        client_destroy(c);
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void websocket_eventcb(struct bufferevent *bev, short error, void *arg)
{
    client_t *c = (client_t *)arg;
    cmd_t cmd = {0};
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        if (!c->close_flag) {
            printf("client:%lu EOF\n", c->user_id);
        }
        /* ... */
    } else if (error & BEV_EVENT_ERROR) {
        printf("client:%lu ERROR\n", c->user_id);
        /* check errno to see what error occurred */
        /* ... */
    } else if (error & BEV_EVENT_TIMEOUT) {
        printf("client:%lu TIMEOUT\n", c->user_id);
        /* ... */
    }
    // delete from hash
    worker_delete_from_hash(c);

    // notify the pusher
    if (c->user_id) {
        cmd.cmd_no = CMD_DEL_CLIENT;
        cmd.data = (void *)c->user_id;
        if (evbuffer_add(bufferevent_get_output(c->worker->bev_pusher[0]), &cmd, sizeof cmd) != 0) {
            err_quit("evbuffer_add");
        }
    }
    bufferevent_free(bev);
    client_destroy(c);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void worker_timer(int fd, short event, void *arg)
{
    worker_t *w = (worker_t *)arg;
    if (w->stop) {
        printf("worker stoped\n");
        event_base_loopexit(w->base, NULL);
    }
}

int broadcast(worker_t *w, void *data, size_t length)
{
    client_t *itr, *tmp;
    HASH_ITER(hh, w->clients, itr, tmp) {
        if (evbuffer_add(bufferevent_get_output(itr->bev), data, length) != 0) {
            err_quit("evbuffer_add");
        }
    }
    return 0;
}

void echo_readcb(struct bufferevent *bev, void *arg)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);

#ifdef SEE_DATA
    char *buf = malloc(evbuffer_get_length(input));
    evbuffer_copyout(input, buf, evbuffer_get_length(input));
    write(STDOUT_FILENO, buf, evbuffer_get_length(input));
    free(buf);
#endif

    evbuffer_add_buffer(output, input);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void echo_writecb(struct bufferevent *bev, void *arg)
{
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void echo_eventcb(struct bufferevent *bev, short error, void *arg)
{
    if (error | BEV_EVENT_TIMEOUT) {
        // TODO
    } else if (error | BEV_EVENT_ERROR) {
        // TODO
    }

    bufferevent_free(bev);
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void worker_dispatcher_readcb(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    struct bufferevent *bev_client;
    struct timeval timeout = {1000, 0};
    struct evbuffer *input = bufferevent_get_input(bev);
    client_t *c;
    cmd_t cmd = {0};

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
            bufferevent_setcb(bev_client, websocket_readcb, NULL, websocket_eventcb, c);
            bufferevent_setwatermark(bev_client, EV_READ, 0, CLIENT_HIGH_WATERMARK);
            bufferevent_enable(bev_client, EV_READ | EV_WRITE);
            bufferevent_set_timeouts(bev_client, &timeout, &timeout);
            break;
        default:
            break;
        }
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void worker_dispatcher_readcb_echo(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    struct bufferevent *bev_client;
    struct timeval timeout = {1000, 0};
    struct evbuffer *input = bufferevent_get_input(bev);
    cmd_t cmd = {0};

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
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void worker_dispatcher_eventcb(struct bufferevent *bev, short error, void *arg)
{
    printf("worker error\n");
    worker_t *w = (worker_t *)arg;
    if (error | BEV_EVENT_TIMEOUT) {
        // TODO
    } else if (error | BEV_EVENT_ERROR) {
        // TODO
    }
    w->stop = 1;
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
}

void worker_pusher_readcb(struct bufferevent *bev, void *arg)
{
    worker_t *w = (worker_t *)arg;
    uint64_t user_id;
    client_t *tmp;
    // push
    cmd_t cmd = {0};
    while (evbuffer_get_length(bufferevent_get_input(bev)) >= sizeof cmd) {
        if (evbuffer_remove(bufferevent_get_input(bev), &cmd, sizeof cmd) != sizeof cmd) {
            err_quit("evbuffer_remove");
        }
        switch (cmd.cmd_no) {
        case CMD_BROADCAST:
            broadcast(w, cmd.data, cmd.length);
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
            tmp->close_flag = 1;
            // TODO TODO
            break;
        case CMD_NOTIFY:
            user_id = (uint64_t)cmd.data;
            HASH_FIND(hh, w->clients, &user_id, sizeof(user_id), tmp);
            if (tmp == NULL) {
                break;
            }

            send_text_frame(bufferevent_get_output(tmp->bev), "message", strlen("message"));
            break;
        default:
            break;
        }
    }
#ifdef TRACE
    printf("%s\n", __FUNCTION__);
#endif
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

