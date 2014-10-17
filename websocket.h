#ifndef  __WEBSOCKET_H_
#define  __WEBSOCKET_H_

/* Websocket protocol RFC6455 */
#define OPCODE_CONTINUATION_FRAME  0x0
#define OPCODE_TEXT_FRAME          0x1
#define OPCODE_BINARY_FRAME        0x2
#define OPCODE_CLOSE_FRAME         0x8
#define OPCODE_PING_FRAME          0x9
#define OPCODE_PONG_FRAME          0xa

#define MAX_WEBSOCKET_HEADER_LENGTH 14 //(2 + 8 + 4)

#define MAX_FRAME_SIZE 0x100000 // max frame size 1M bytes 

enum frame_state_e {
    FRAME_STATE_STARTED = 0,
    FRAME_STATE_OPCODE_PARSED,
    FRAME_STATE_LENGTH_PARSED,
    FRAME_STATE_MASK_KEY_PARSED,
    FRAME_STATE_DATA_READING,
    FRAME_STATE_FINISHED,
    FRAME_STATE_ERROR,

    FRAME_STATE_COUNT
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

#if 0
    // double linked list
    struct websocket_frame_s *next;
    struct websocket_frame_s *prev;
#endif

};

websocket_frame_t *websocket_frame_create();
void websocket_frame_destroy(websocket_frame_t *f);
void websocket_frame_clear(websocket_frame_t *f);
int check_websocket_request(http_request_t *h);
frame_state_t parse_frame(struct evbuffer *b, websocket_frame_t *f);

int send_handshake(struct evbuffer *b, const char *websocket_key);
int send_text_frame(struct evbuffer *b, const void *data, size_t len);
int send_ping_frame(struct evbuffer *b, const void *data, size_t len);
int send_pong_frame(struct evbuffer *b, const void *data, size_t len);
int send_binary_frame(struct evbuffer *b, const void *data, size_t len);
int send_close_frame(struct evbuffer *b, const void *data, size_t len);
#endif  //__WEBSOCKET_H_;
