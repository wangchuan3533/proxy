#include "define.h"
#include "http.h"
#include "websocket.h"

websocket_frame_t *websocket_frame_create()
{
    websocket_frame_t *f = (websocket_frame_t *)malloc(sizeof(websocket_frame_t));
    if (NULL == f) {
        err_quit("malloc");
    }
    memset(f, 0, sizeof(websocket_frame_t));
    return f;
}

void websocket_frame_destroy(websocket_frame_t *f)
{
    if (f) {
        if (f->data) {
            free(f->data);
        }
        free(f);
    }
}

frame_state_t parse_frame(struct evbuffer *b, websocket_frame_t *f)
{
    uint8_t buf[MAX_WEBSOCKET_HEADER_LENGTH];
    size_t to_be_read, n, len;
    int i;

    len = evbuffer_get_length(b);
    switch (f->state) {
    case FRAME_STATE_STARTED:
        to_be_read = 2;
        if (len < to_be_read) {
            break;
        }
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

        // check 
        if (!f->fin || f->rsv1 || f->rsv2 || f->rsv3 || !f->mask) {
            f->state = FRAME_STATE_ERROR;
            break;
        }
        f->state = FRAME_STATE_OPCODE_PARSED;
        len -= to_be_read;
    case FRAME_STATE_OPCODE_PARSED:
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

        if (len < to_be_read) {
            break;
        }
        switch (to_be_read) {
        case 8:
            n = evbuffer_remove(b, buf, to_be_read);
            if (n != to_be_read) {
                err_quit("evbuffer_remove");
            }
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
        case 2:
            n = evbuffer_remove(b, buf, to_be_read);
            if (n != to_be_read) {
                err_quit("evbuffer_remove");
            }
            f->len_16 = ntohs(*(uint16_t *)buf);
            f->length = f->len_16;
            break;
        case 0:
            f->length = f->len_7;
            break;
        }

        // check
        if (to_be_read == 2 && f->length < 126) {
            f->state = FRAME_STATE_ERROR;
            break;
        }
        // max frame length is 1M
        if (to_be_read == 8 && (f->length <= 0xffff || f->length > MAX_FRAME_SIZE)) {
            f->state = FRAME_STATE_ERROR;
            break;
        }
        len -= to_be_read;
        f->state = FRAME_STATE_LENGTH_PARSED;
    case FRAME_STATE_LENGTH_PARSED:
        to_be_read = f->mask ? 4 : 0;
        if (len < to_be_read) {
            break;
        }
        if (to_be_read) {
            n = evbuffer_remove(b, buf, to_be_read);
            if (n != to_be_read) {
                err_quit("evbuffer_remove step 2");
            }
            memcpy(f->mask_key, buf, to_be_read);
            len -= to_be_read;
        }
        f->state = FRAME_STATE_MASK_KEY_PARSED;
    case FRAME_STATE_MASK_KEY_PARSED:
        // danger
        f->data = (uint8_t *)malloc((size_t)f->length);
        if (NULL == f->data) {
            err_quit("malloc");
        }
        f->cur = 0;
        f->state = FRAME_STATE_DATA_READING;
    case FRAME_STATE_DATA_READING:
        to_be_read = f->length - f->cur;
        to_be_read = to_be_read > len ? len : to_be_read;
        n = evbuffer_remove(b, f->data + f->cur, to_be_read);
        if (n != to_be_read) {
            err_quit("evbuffer_remove");
        }
        for (i = f->cur; i < f->cur + n; i++) {
            f->data[i] ^= f->mask_key[i % 4];
        }
        f->cur += n;

        if (f->cur == f->length) {
            f->state = FRAME_STATE_FINISHED;
        } else if (f->cur > f->length) {
            f->state = FRAME_STATE_ERROR;
            break;
        }
        break;
    case FRAME_STATE_ERROR:
    default:
        // imposible
        err_quit("Opps\n");
    }
    return f->state;
}

#if 0
int check_websocket_request(http_request_t *h)
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
#endif

int send_handshake(struct evbuffer *b, const char *websocket_key)
{
    static const char *_websocket_secret = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char tmp1[256], tmp2[256];
    int ret;

    snprintf(tmp1, sizeof(tmp1), "%s%s", websocket_key, _websocket_secret);
    sha1(tmp2, tmp1, strlen(tmp1) << 3);
    base64enc(tmp1, tmp2, 20);
    ret = evbuffer_add_printf(b, HTTP_RESPONSE_101, tmp1);
    if (ret != strlen(HTTP_RESPONSE_101) + strlen(tmp1) - 2) {
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
