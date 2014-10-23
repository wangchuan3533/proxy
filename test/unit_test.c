#include "define.h"
#include "minunit.h"
#include "http.h"
#include "websocket.h"

MU_TEST(test_http_1)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf, "HELLO world");
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_STARTED);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_2)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf, "HELLO\r\n");
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_ERROR);
    evbuffer_free(buf);
    http_request_destroy(req);
}
MU_TEST(test_http_3)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf, "GET /\r\n");
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_ERROR);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_4)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf, "GET / HTTP/1.1\r\n");
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_PARSED_FIRST_LINE);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_5)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf,
            "GET / HTTP/1.1\r\n"
            "Host: websocket"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_PARSED_FIRST_LINE);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_6)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf,
            "GET / HTTP/1.1\r\n"
            "Host: websocket\r\n"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_HEADERS_PARSING);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_7)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf,
            "GET / HTTP/1.1\r\n"
            "Host: websocket\r\n"
            "hello\r\n"
            "world\r\n"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_HEADERS_PARSING && req->count == 2);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_8)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf,
            "GET / HTTP/1.1\r\n"
            "Host: websocket\r\n"
            "\r\n"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_FINISHED && req->count == 2);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST(test_http_9)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf,
            "GET / HTTP/1.1\r\n"
            "Host: websocket\r\n"
            "Connection: Close\r\n"
            "\r\n"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_FINISHED && req->count == 3);
    evbuffer_free(buf);
    http_request_destroy(req);
}


MU_TEST(test_http_finish)
{
    struct evbuffer *buf = evbuffer_new();
    http_request_state_t state;
    http_request_t *req = http_request_create();
    evbuffer_add_printf(buf, 
            "GET / HTTP/1.1\r\n"
            "Host: Websocket\r\n"
            "Connection: Upgrade\r\n"
            "\r\n"
            );
    state = http_request_parse(buf, req);
#ifdef DUMP_DATA
    print_http_request(req);
#endif
    mu_check(state == HTTP_REQUEST_STATE_FINISHED);
    evbuffer_free(buf);
    http_request_destroy(req);
}

MU_TEST_SUITE(test_http)
{
    MU_RUN_TEST(test_http_1);
    MU_RUN_TEST(test_http_2);
    MU_RUN_TEST(test_http_3);
    MU_RUN_TEST(test_http_4);
    MU_RUN_TEST(test_http_5);
    MU_RUN_TEST(test_http_6);
    MU_RUN_TEST(test_http_7);
    MU_RUN_TEST(test_http_8);
    MU_RUN_TEST(test_http_9);
    MU_RUN_TEST(test_http_finish);
}

MU_TEST(test_websocket_handshake)
{
    struct evbuffer *buf = evbuffer_new();
    const char *key = "helloworld";
    char result[1024];
    char dst[1024];
    size_t len, n;

    snprintf(result, sizeof(result), HTTP_RESPONSE_101, "k+Z5KOa4w2VXpclg7J8hZv3SDE8=");
    send_handshake(buf, key);
    len = evbuffer_get_length(buf);
    n = evbuffer_remove(buf, dst, len);
    mu_check(n == len && strncmp(dst, result, len) == 0);
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame1)
{
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_STARTED);
    evbuffer_free(buf);
    websocket_frame_destroy(f);
}

MU_TEST(test_websocket_parse_frame2)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    evbuffer_add(buf, data, 1);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_STARTED);
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame3)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x81;
    data[1] = 0xff; // length

    evbuffer_add(buf, data, 2);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_OPCODE_PARSED
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_TEXT_FRAME
            && f->mask == 1
            && f->len_7 == 127
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame4)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x72; // must be fin
    data[1] = 0xff; // length
    data[2] = 0xff;

    evbuffer_add(buf, data, 3);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_ERROR
            && f->fin == 0
            && f->rsv1 == 1
            && f->rsv2 == 1
            && f->rsv2 == 1
            && f->opcode == OPCODE_BINARY_FRAME
            && f->mask == 1
            && f->len_7 == 127
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame5)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x88;
    data[1] = 0xfd; // length

    evbuffer_add(buf, data, 2);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_LENGTH_PARSED
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_CLOSE_FRAME
            && f->mask == 1
            && f->length == 125
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame6)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x89;
    data[1] = 0xfe; // length

    data[2] = 0xff;
    data[3] = 0xff;

    evbuffer_add(buf, data, 5);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_LENGTH_PARSED
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_PING_FRAME
            && f->mask == 1
            && f->length == 0xffff
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame7)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x8a;
    data[1] = 0xff; // length

    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0xff;
    data[8] = 0xff;
    data[9] = 0xff;

    evbuffer_add(buf, data, 10);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_ERROR
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_PONG_FRAME
            && f->mask == 1
            && f->length == 0xffffff
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame8)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x80;
    data[1] = 0xff; // length

    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x0f;
    data[8] = 0xff;
    data[9] = 0xff;

    data[10] = 0xff;
    data[11] = 0xff;
    data[12] = 0xff;
    data[13] = 0xff;

    evbuffer_add(buf, data, 13);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_LENGTH_PARSED
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_CONTINUATION_FRAME
            && f->mask == 1
            && f->length == 0xfffff
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame9)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x88;
    data[1] = 0xff; // length

    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x0f;
    data[8] = 0xff;
    data[9] = 0xff;

    data[10] = 0xff;
    data[11] = 0xff;
    data[12] = 0xff;
    data[13] = 0xff;

    evbuffer_add(buf, data, 14);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_DATA_READING
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_CLOSE_FRAME
            && f->mask == 1
            && f->length == 0xfffff
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_parse_frame10)
{
    uint8_t data[128];
    struct evbuffer *buf = evbuffer_new();
    frame_state_t state;
    websocket_frame_t *f = websocket_frame_create();

    data[0] = 0x88;
    data[1] = 0xff; // length

    data[2] = 0x00;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x0f;
    data[8] = 0xff;
    data[9] = 0xff;

    data[10] = 0xff;
    data[11] = 0xff;
    data[12] = 0xff;
    data[13] = 0xff;

    evbuffer_add(buf, data, 14);
    state = parse_frame(buf, f);
    mu_check(state == FRAME_STATE_DATA_READING
            && f->fin == 1
            && f->rsv1 == 0
            && f->rsv2 == 0
            && f->rsv2 == 0
            && f->opcode == OPCODE_CLOSE_FRAME
            && f->mask == 1
            && f->length == 0xfffff
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_send_text_frame)
{
    struct evbuffer *buf = evbuffer_new();
    uint8_t data[1024];
    size_t len;

    len = sprintf(data, "hello world");
    send_text_frame(buf, data, len);
    len = evbuffer_remove(buf, data, sizeof(data));
    mu_check(len == 13
            && data[0] == 0x81
            && data[1] == 0x0b
            && strncmp(data + 2, "hello world", 11) == 0
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_send_binary_frame)
{
    struct evbuffer *buf = evbuffer_new();
    uint8_t data[1024] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00};
    size_t len = 10;

    send_binary_frame(buf, data, len);
    len = evbuffer_remove(buf, data, sizeof(data));
    mu_check(len == 12
            && data[0] == 0x82
            && data[1] == 0x0a
            && data[2] == 0x01
            && data[3] == 0x02
            && data[4] == 0x03
            && data[5] == 0x04
            && data[6] == 0x05
            && data[7] == 0x06
            && data[8] == 0x07
            && data[9] == 0x08
            && data[10] == 0x09
            && data[11] == 0x00
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_send_close_frame)
{
    struct evbuffer *buf = evbuffer_new();
    uint8_t data[1024] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00};
    size_t len = 10;

    send_close_frame(buf, data, len);
    len = evbuffer_remove(buf, data, sizeof(data));
    mu_check(len == 12
            && data[0] == 0x88
            && data[1] == 0x0a
            && data[2] == 0x01
            && data[3] == 0x02
            && data[4] == 0x03
            && data[5] == 0x04
            && data[6] == 0x05
            && data[7] == 0x06
            && data[8] == 0x07
            && data[9] == 0x08
            && data[10] == 0x09
            && data[11] == 0x00
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_send_ping_frame)
{
    struct evbuffer *buf = evbuffer_new();
    uint8_t data[1024] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00};
    size_t len = 10;

    send_ping_frame(buf, data, len);
    len = evbuffer_remove(buf, data, sizeof(data));
    mu_check(len == 12
            && data[0] == 0x89
            && data[1] == 0x0a
            && data[2] == 0x01
            && data[3] == 0x02
            && data[4] == 0x03
            && data[5] == 0x04
            && data[6] == 0x05
            && data[7] == 0x06
            && data[8] == 0x07
            && data[9] == 0x08
            && data[10] == 0x09
            && data[11] == 0x00
            );
    evbuffer_free(buf);
}

MU_TEST(test_websocket_send_pong_frame)
{
    struct evbuffer *buf = evbuffer_new();
    uint8_t data[1024] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00};
    size_t len = 10;

    send_pong_frame(buf, data, len);
    len = evbuffer_remove(buf, data, sizeof(data));
    mu_check(len == 12
            && data[0] == 0x8a
            && data[1] == 0x0a
            && data[2] == 0x01
            && data[3] == 0x02
            && data[4] == 0x03
            && data[5] == 0x04
            && data[6] == 0x05
            && data[7] == 0x06
            && data[8] == 0x07
            && data[9] == 0x08
            && data[10] == 0x09
            && data[11] == 0x00
            );
    evbuffer_free(buf);
}

MU_TEST_SUITE(test_websocket)
{
    MU_RUN_TEST(test_websocket_handshake);
    MU_RUN_TEST(test_websocket_parse_frame1);
    MU_RUN_TEST(test_websocket_parse_frame2);
    MU_RUN_TEST(test_websocket_parse_frame3);
    MU_RUN_TEST(test_websocket_parse_frame4);
    MU_RUN_TEST(test_websocket_parse_frame5);
    MU_RUN_TEST(test_websocket_parse_frame6);
    MU_RUN_TEST(test_websocket_parse_frame7);
    MU_RUN_TEST(test_websocket_parse_frame8);
    MU_RUN_TEST(test_websocket_parse_frame9);
    MU_RUN_TEST(test_websocket_parse_frame10);

    MU_RUN_TEST(test_websocket_send_text_frame);
    MU_RUN_TEST(test_websocket_send_binary_frame);
    MU_RUN_TEST(test_websocket_send_close_frame);
    MU_RUN_TEST(test_websocket_send_ping_frame);
    MU_RUN_TEST(test_websocket_send_pong_frame);
}

int main()
{
    MU_RUN_SUITE(test_http);
    MU_RUN_SUITE(test_websocket);
    MU_REPORT();
    return 0;
}
