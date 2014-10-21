#ifndef __HTTP_H_
#define __HTTP_H_

#define MAX_HTTP_REQUEST 128
#define HTTP_HEADER_HOST "Host:"
#define HTTP_HEADER_USER_AGENT "User-Agent:"
#define HTTP_HEADER_COOKIE "Cookie:"
#define HTTP_HEADER_CONNECTION "Connection:"
#define HTTP_HEADER_UPGRADE "Upgrade:"
#define HTTP_HEADER_SEC_WEBSOCKET_KEY "Sec-WebSocket-Key:"
#define HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL "Sec-WebSocket-Protocol:"
#define HTTP_HEADER_SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version:"

#define HTTP_RESPONSE_200 \
        "HTTP/1.1 200 OK\r\n"\
        "Server: WebSocket\r\n"\
        "Content-Length: 8\r\n"\
        "Connection: close\r\n"\
        "Content-Type: text/html\r\n\r\n"\
        "It works"
#define HTTP_RESPONSE_400 \
        "HTTP/1.1 400 Bad Reqeust\r\n"\
        "Server: WebSocket\r\n"\
        "Content-Length: 0\r\n"\
        "Connection: close\r\n"\
        "Content-Type: text/html\r\n\r\n"
#define HTTP_RESPONSE_403 \
        "HTTP/1.1 403 Forbidden\r\n"\
        "Server: WebSocket\r\n"\
        "Content-Length: 0\r\n"\
        "Connection: close\r\n"\
        "Content-Type: text/html\r\n\r\n"
#define HTTP_RESPONSE_404 \
        "HTTP/1.1 404 Not Found\r\n"\
        "Server: WebSocket\r\n"\
        "Content-Length: 0\r\n"\
        "Connection: close\r\n"\
        "Content-Type: text/html\r\n\r\n"
#define HTTP_RESPONSE_101 \
        "HTTP/1.1 101 Switching Protocols\r\n"\
        "Server: WebSocket\r\n"\
        "Upgrade: websocket\r\n"\
        "Connection: Upgrade\r\n"\
        "Sec-WebSocket-Accept: %s\r\n"\
        "\r\n"

enum http_request_state_s {
    HTTP_REQUEST_STATE_STARTED = 0,
    HTTP_REQUEST_STATE_PARSED_FIRST_LINE,
    HTTP_REQUEST_STATE_HEADERS_PARSING,
    HTTP_REQUEST_STATE_FINISHED,
    HTTP_REQUEST_STATE_ERROR,
};

struct http_request_s {
    //first line
    char *method;
    char *request_uri;
    char *http_version;

    // request
    char *host;
    char *user_agent;
    char *cookie;
    char *connection;
    char *upgrade;
    char *sec_websocket_key;
    char *sec_websocket_protocol;
    char *sec_websocket_version;

    // buffers & buffers count
    char *buffers[MAX_HTTP_REQUEST];
    size_t count;
    // state
    http_request_state_t state;
};

http_request_t *http_request_create();
void http_request_destroy(http_request_t *h);
void print_http_request(http_request_t *h);
http_request_state_t http_request_parse(struct evbuffer *b, http_request_t *h);
int send_http_response(struct evbuffer *b, int response);

#endif
