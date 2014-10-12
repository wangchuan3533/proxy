#ifndef  __WEBSOCKET_H_
#define  __WEBSOCKET_H_

#define MAX_HTTP_HEADERS 128
#define HTTP_HEADER_HOST "Host:"
#define HTTP_HEADER_USER_AGENT "User-Agent:"
#define HTTP_HEADER_COOKIE "Cookie:"
#define HTTP_HEADER_CONNECTION "Connection:"
#define HTTP_HEADER_UPGRADE "Upgrade:"
#define HTTP_HEADER_SEC_WEBSOCKET_KEY "Sec-WebSocket-Key:"
#define HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL "Sec-WebSocket-Protocol:"
#define HTTP_HEADER_SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version:"

/* Websocket protocol RFC6455 */
#define OPCODE_CONTINUATION_FRAME  0x0
#define OPCODE_TEXT_FRAME          0x1
#define OPCODE_BINARY_FRAME        0x2
#define OPCODE_CLOSE_FRAME         0x8
#define OPCODE_PING_FRAME          0x9
#define OPCODE_PONG_FRAME          0xa

#define MAX_WEBSOCKET_HEADER_LENGTH 14 //(2 + 8 + 4)

enum http_headers_state_s {
    HTTP_HEADERS_STATE_STEP_0 = 0,
    HTTP_HEADERS_STATE_PARSED_REQUEST_LINE,
    HTTP_HEADERS_STATE_FINISHED
};

struct http_headers_s {
    //first line
    char *method;
    char *request_uri;
    char *http_version;

    // headers
    char *host;
    char *user_agent;
    char *cookie;
    char *connection;
    char *upgrade;
    char *sec_websocket_key;
    char *sec_websocket_protocol;
    char *sec_websocket_version;

    // special
    char *user_id;

    // buffers & buffers count
    char *buffers[MAX_HTTP_HEADERS];
    size_t count;
    // state
    http_headers_state_t state;
};

enum frame_state_e {
    FRAME_STATE_STEP_0 = 0,
    FRAME_STATE_STEP_1,
    FRAME_STATE_STEP_2,
    FRAME_STATE_STEP_3,
    FRAME_STATE_FINISHED
};

struct websocket_frame_s {
    //first byte
    unsigned fin:1;
    unsigned rsv1:1;   // must be 0
    unsigned rsv2:1;   // must be 0
    unsigned rsv3:1;   // must be 0
    unsigned opcode:4; // 

    // second type
    unsigned mask:1;
    unsigned len_7:7;
    uint16_t len_16;
    uint64_t len_64;

    // mask
    uint8_t mask_key[4];

    // state
    frame_state_t state;

    // data;
    uint8_t *data;
    uint64_t length;
    uint64_t cur;

    // double linked list
    struct websocket_frame_s *next;
    struct websocket_frame_s *prev;

};

http_headers_t *http_headers_create();
void http_headers_destroy(http_headers_t *h);
void print_http_headers(http_headers_t *h);
int parse_http(struct evbuffer *b, http_headers_t *h);

websocket_frame_t *ws_frame_create();
void ws_frame_destroy(websocket_frame_t *f);
void ws_frame_clear(websocket_frame_t *f);
int check_websocket_request(http_headers_t *h);
int parse_frame(struct evbuffer *b, websocket_frame_t *f);

int send_200_ok(struct evbuffer *b);
int send_handshake(struct evbuffer *b, const char *websocket_key);
int send_text_frame(struct evbuffer *b, const void *data, size_t len);
int send_ping_frame(struct evbuffer *b, const void *data, size_t len);
int send_pong_frame(struct evbuffer *b, const void *data, size_t len);
int send_binary_frame(struct evbuffer *b, const void *data, size_t len);
int send_close_frame(struct evbuffer *b, const void *data, size_t len);
#endif  //__WEBSOCKET_H_;
