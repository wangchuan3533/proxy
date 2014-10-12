#include "define.h"
#include "websocket.h"

http_headers_t *http_headers_create()
{
    http_headers_t *h = (http_headers_t *)malloc(sizeof(http_headers_t));
    if (NULL == h) {
        err_quit("malloc");
    }
    memset(h, 0, sizeof(http_headers_t));
    return h;
}

void http_headers_destroy(http_headers_t *h)
{
    int i;
    if (h) {
        for (i = 0; i < h->count; i++) {
            if (h->buffers[i])
                free(h->buffers[i]);
        }
        free(h);
    }
}

// TODO filter invalid request
int parse_http(struct evbuffer *b, http_headers_t *h)
{
    char *line, *tmp;
    size_t n;
    while ((line = evbuffer_readln(b, &n, EVBUFFER_EOL_CRLF)) != NULL) {
        h->buffers[h->count++] = line;
        if (n == 0) {
            // finish
            h->state = HTTP_HEADERS_STATE_FINISHED;
            break;
        }
        if (h->state == HTTP_HEADERS_STATE_STEP_0) {
            h->method = line;

            tmp = strchr(line, ' ');
            if (tmp != NULL) {
                *tmp++ = '\0';
                h->request_uri = tmp;
                h->user_id = h->request_uri + 1;
                tmp = strrchr(tmp, ' ');
                if (tmp != NULL) {
                    *tmp++ = '\0';
                    h->http_version = tmp;
                }
            }
            h->state = HTTP_HEADERS_STATE_PARSED_REQUEST_LINE;
        }
        if (strncasecmp(HTTP_HEADER_HOST, line, strlen(HTTP_HEADER_HOST)) == 0) {
            tmp = strchr(line, ' ');
            h->host = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_USER_AGENT, line, strlen(HTTP_HEADER_USER_AGENT)) == 0) {
            tmp = strchr(line, ' ');
            h->user_agent = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_CONNECTION, line, strlen(HTTP_HEADER_CONNECTION)) == 0) {
            tmp = strchr(line, ' ');
            h->connection = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_COOKIE, line, strlen(HTTP_HEADER_COOKIE)) == 0) {
            tmp = strchr(line, ' ');
            h->cookie = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_UPGRADE, line, strlen(HTTP_HEADER_UPGRADE)) == 0) {
            tmp = strchr(line, ' ');
            h->upgrade = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_KEY, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_KEY)) == 0) {
            tmp = strchr(line, ' ');
            h->sec_websocket_key = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL)) == 0) {
            tmp = strchr(line, ' ');
            h->sec_websocket_protocol = tmp + 1;
        }
        if (strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_VERSION, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_VERSION)) == 0) {
            tmp = strchr(line, ' ');
            h->sec_websocket_version = tmp + 1;
        }
    }
    return 0;
}

void print_http_headers(http_headers_t *h)
{
    if (h->method)
        printf("[method]:%s\n", h->method);
    else
        printf("[method]:NULL\n");
    if (h->request_uri)
        printf("[request_uri]:%s\n", h->request_uri);
    else
        printf("[request_uri]:NULL\n");
    if (h->http_version)
        printf("[http_version]:%s\n", h->http_version);
    else
        printf("[http_version]:NULL\n");
    if (h->host)
        printf("[host]:%s\n", h->host);
    else
        printf("[host]:NULL\n");
    if (h->user_agent)
        printf("[user_agent]:%s\n", h->user_agent);
    else
        printf("[user_agent]:NULL\n");
    if (h->cookie)
        printf("[cookie]:%s\n", h->cookie);
    else
        printf("[cookie]:NULL\n");
    if (h->connection)
        printf("[connection]:%s\n", h->connection);
    else
        printf("[connection]:NULL\n");
    if (h->upgrade)
        printf("[upgrade]:%s\n", h->upgrade);
    else
        printf("[upgrade]:NULL\n");
    if (h->sec_websocket_key)
        printf("[sec_websocket_key]:%s\n", h->sec_websocket_key);
    else
        printf("[sec_websocket_key]:NULL\n");
    if (h->sec_websocket_protocol)
        printf("[sec_websocket_protocol]:%s\n", h->sec_websocket_protocol);
    else
        printf("[sec_websocket_protocol]:NULL\n");
    if (h->sec_websocket_version)
        printf("[sec_websocket_version]:%s\n", h->sec_websocket_version);
    else
        printf("[sec_websocket_version]:NULL\n");
}

websocket_frame_t *ws_frame_create()
{
    websocket_frame_t *f = (websocket_frame_t *)malloc(sizeof(websocket_frame_t));
    if (NULL == f) {
        err_quit("malloc");
    }
    memset(f, 0, sizeof(websocket_frame_t));
    return f;
}

void ws_frame_destroy(websocket_frame_t *f)
{
    if (f) {
        if (f->data) {
            free(f->data);
        }
        free(f);
    }
}

void ws_frame_clear(websocket_frame_t *f)
{
    if (f) {
        if (f->data) {
            free(f->data);
        }
        memset(f, 0, sizeof(websocket_frame_t));
    }
}

int parse_frame(struct evbuffer *b, websocket_frame_t *f)
{
    uint8_t buf[MAX_WEBSOCKET_HEADER_LENGTH];
    size_t to_be_read, n, len;
    int i;

    len = evbuffer_get_length(b);
    switch (f->state) {
    case FRAME_STATE_STEP_0:
        to_be_read = 2;
        if (len >= to_be_read) {
            n = evbuffer_remove(b, buf, to_be_read);
            if (n != to_be_read) {
                err_quit("evbuffer_remove step 0");
            }
            // fist byte
            f->fin = (buf[0] >> 7) & 0x1;
            f->rsv1 = (buf[0] >> 6) & 0x1;
            f->rsv2 = (buf[0] >> 5) & 0x1;
            f->rsv3 = (buf[0] >> 4) & 0x1;
            f->opcode = buf[0] & 0xf;

            // second byte
            f->mask = (buf[1] >> 7) & 0x1;
            f->len_7 = buf[1] & 0x7f;
            f->state = FRAME_STATE_STEP_1;
            len -= to_be_read;
        } else {
            return 0;
        }
    case FRAME_STATE_STEP_1:
        switch (f->len_7) {
        case 127:
            to_be_read = 8;
            break;
        case 126:
            to_be_read = 2;
            break;
        default:
            to_be_read = 0;
        }
        if (len >= to_be_read) {
            if (to_be_read) {
                n = evbuffer_remove(b, buf, to_be_read);
                switch (n) {
                case 2:
                    f->len_16 = ntohs(*(uint16_t *)buf);
                    f->length = f->len_16;
                    break;
                case 8:
                    f->len_64 = buf[0];
                    f->len_64 = (f->len_64 << 8) + buf[1];
                    f->len_64 = (f->len_64 << 8) + buf[2];
                    f->len_64 = (f->len_64 << 8) + buf[3];
                    f->len_64 = (f->len_64 << 8) + buf[4];
                    f->len_64 = (f->len_64 << 8) + buf[5];
                    f->len_64 = (f->len_64 << 8) + buf[6];
                    f->len_64 = (f->len_64 << 8) + buf[7];
                    f->length = f->len_64;
                    break;
                default:
                    err_quit("Oops evbuffer_remove step 1");
                }
            } else {
                f->length = f->len_7;
            }
            len -= to_be_read;
            f->state = FRAME_STATE_STEP_2;
        } else {
            return 0;
        }
    case FRAME_STATE_STEP_2:
        to_be_read = f->mask ? 4 : 0;
        if (len >= to_be_read) {
            if (to_be_read) {
                n = evbuffer_remove(b, buf, to_be_read);
                if (n != to_be_read) {
                    err_quit("evbuffer_remove step 2");
                }
                memcpy(f->mask_key, buf, to_be_read);
            }
            len -= to_be_read;
            f->state = FRAME_STATE_STEP_3;
            f->data = (uint8_t *)malloc((size_t)f->length);
            if (NULL == f->data) {
                err_quit("malloc");
            }
            f->cur = 0;
        } else {
            return 0;
        }
    case FRAME_STATE_STEP_3:
        to_be_read = f->length - f->cur;
        if (to_be_read > len) {
            to_be_read = len;
        }
        n = evbuffer_remove(b, f->data + f->cur, to_be_read);
        //n = evbuffer_remove(b, f->data + f->cur, len);
        f->cur += n;
        if (f->cur == f->length) {
            for (i = 0; i < f->cur; i++) {
                f->data[i] ^= f->mask_key[i % 4];
            }
            f->state = FRAME_STATE_FINISHED;
        } else if (f->cur > f->length) {
            // impossible
            err_quit("Opps\n");
        }
        return 0;
    default:
        return -1;
    }
}

int check_websocket_request(http_headers_t *h)
{
    if (!h->method || strncasecmp(h->method, "GET", strlen("GET")))
        return -1;
    if (!h->connection || strncasecmp(h->connection, "Upgrade", strlen("Upgrade")))
        return -1;
    if (!h->sec_websocket_key)
        return -1;
    if (!h->request_uri || h->request_uri[0] != '/')
        return -1;
    if (!h->user_id || atoi(h->user_id) <= 0)
        return -1;
    return 0;
}

int send_200_ok(struct evbuffer *b)
{
    const char *http_200_ok_response = 
        "HTTP/1.1 200 OK\r\n"
        "Server: WebSocket\r\n"
        "Content-Length: 8\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html\r\n\r\n"
        "It Works";
    size_t len = strlen(http_200_ok_response);

    if (evbuffer_add(b, http_200_ok_response, len) != 0) {
        return -1;
    }
    return 0;
}

int send_handshake(struct evbuffer *b, const char *websocket_key)
{
    const char *_websocket_secret = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const char *handshake_template =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n";
    char tmp1[256], tmp2[256];
    int ret;

    snprintf(tmp1, sizeof(tmp1), "%s%s", websocket_key, _websocket_secret);
    sha1(tmp2, tmp1, strlen(tmp1) << 3);
    base64enc(tmp1, tmp2, 20);
    ret = evbuffer_add_printf(b, handshake_template, tmp1);
    //debug
    //printf(handshake_template, tmp1);
    if (ret != strlen(handshake_template) + strlen(tmp1) - 2) {
        return -1;
    }
    return 0;
}

int send_frame(struct evbuffer *b, websocket_frame_t *f)
{
    int ret, cur = 0;
    uint8_t header[MAX_WEBSOCKET_HEADER_LENGTH];
    
    memset(header, 0, sizeof(header));
    // length
    if (f->length < 126) {
        f->len_7 = f->length;
    } else if (f->length <= 0xffff) {
        f->len_7 = 126;
        f->len_16 = f->length;
    } else {
        f->len_7 = 127;
        f->len_64 = f->length;
    }

    // first byte
    header[cur]  = (f->fin << 7)  & 0xff;
    header[cur] |= (f->rsv1 << 6) & 0xff;
    header[cur] |= (f->rsv2 << 5) & 0xff;
    header[cur] |= (f->rsv3 << 4) & 0xff;
    header[cur] |= (f->opcode)    & 0xff;
    cur++;

    // second byte
    header[cur]  = (f->mask << 7) & 0xff;
    header[cur] |= (f->len_7)     & 0xff;
    cur++;

    // payload length
    switch (f->len_7) {
    case 127:
        header[cur++] = (f->len_64 >> 56) & 0xff;
        header[cur++] = (f->len_64 >> 48) & 0xff;
        header[cur++] = (f->len_64 >> 40) & 0xff;
        header[cur++] = (f->len_64 >> 32) & 0xff;
        header[cur++] = (f->len_64 >> 24) & 0xff;
        header[cur++] = (f->len_64 >> 16) & 0xff;
        header[cur++] = (f->len_64 >> 8)  & 0xff;
        header[cur++] = (f->len_64)       & 0xff;
        break;
    case 126:
        header[cur++] = (f->len_16 >> 8)  & 0xff;
        header[cur++] = (f->len_16)       & 0xff;
        break;
    default:
        break;
    }

    // mask
    if (f->mask) {
        memcpy(header + cur, f->mask_key, 4);
        cur += 4;
    }

    ret = evbuffer_add(b, header, cur);
    if (ret != 0)
        return -1;
    ret = evbuffer_add(b, f->data, f->length);
    if (ret != 0)
        return -1;
    return ret;
}

int send_text_frame(struct evbuffer *b, const void *data, size_t len)
{
    websocket_frame_t f;

    memset(&f, 0, sizeof(f));

    f.fin = 1;
    f.rsv1 = 0;
    f.rsv2 = 0;
    f.rsv3 = 0;
    f.opcode = OPCODE_TEXT_FRAME;

    f.mask = 0;
    f.length = len;
    f.data = (void *)data;

    return send_frame(b, &f);
}

int send_binary_frame(struct evbuffer *b, const void *data, size_t len)
{
    websocket_frame_t f;

    memset(&f, 0, sizeof(f));

    f.fin = 1;
    f.rsv1 = 0;
    f.rsv2 = 0;
    f.rsv3 = 0;
    f.opcode = OPCODE_BINARY_FRAME;

    f.mask = 0;
    f.length = len;
    f.data = (void *)data;

    return send_frame(b, &f);
}

int send_close_frame(struct evbuffer *b, const void *data, size_t len)
{
    websocket_frame_t f;

    memset(&f, 0, sizeof(f));

    f.fin = 1;
    f.rsv1 = 0;
    f.rsv2 = 0;
    f.rsv3 = 0;
    f.opcode = OPCODE_CLOSE_FRAME;;

    f.mask = 0;
    f.length = len;
    f.data = (void *)data;

    return send_frame(b, &f);
}

int send_ping_frame(struct evbuffer *b, const void *data, size_t len)
{
    websocket_frame_t f;

    memset(&f, 0, sizeof(f));

    f.fin = 1;
    f.rsv1 = 0;
    f.rsv2 = 0;
    f.rsv3 = 0;
    f.opcode = OPCODE_PING_FRAME;;

    f.mask = 0;
    f.length = len;
    f.data = (void *)data;

    return send_frame(b, &f);
}

int send_pong_frame(struct evbuffer *b, const void *data, size_t len)
{
    websocket_frame_t f;

    memset(&f, 0, sizeof(f));

    f.fin = 1;
    f.rsv1 = 0;
    f.rsv2 = 0;
    f.rsv3 = 0;
    f.opcode = OPCODE_PONG_FRAME;;

    f.mask = 0;
    f.length = len;
    f.data = (void *)data;

    return send_frame(b, &f);
}
