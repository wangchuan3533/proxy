#include "define.h"
#include "http.h"

http_request_t *http_request_create()
{
    http_request_t *h = (http_request_t *)malloc(sizeof(http_request_t));
    if (NULL == h) {
        err_quit("malloc");
    }
    memset(h, 0, sizeof(http_request_t));
    return h;
}

void http_request_destroy(http_request_t *h)
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
http_request_state_t parse_http(struct evbuffer *b, http_request_t *h)
{
    char *line, *tmp;
    size_t n;
    while ((line = evbuffer_readln(b, &n, EVBUFFER_EOL_CRLF)) != NULL) {

        switch (h->state) {
        case HTTP_REQUEST_STATE_STARTED:
            // if no first line, goto error
            if (n == 0) {
                goto error;
            }

            // parse first line
            // method
            h->method = line;
            tmp = strchr(line, ' ');
            if (tmp == NULL) {
                goto error;
            }
            *tmp++ = '\0';

            // request_uri
            h->request_uri = tmp;
            tmp = strrchr(tmp, ' ');
            if (tmp == NULL) {
                goto error;
            }

            // http_version
            *tmp++ = '\0';
            h->http_version = tmp;

            // store the pointer for free()
            h->buffers[h->count++] = line;
            // first line parse finished
            h->state = HTTP_REQUEST_STATE_PARSED_FIRST_LINE;
            break;
        case HTTP_REQUEST_STATE_PARSED_FIRST_LINE:
            // skip to next state
            h->state = HTTP_REQUEST_STATE_HEADERS_PARSING;
        case HTTP_REQUEST_STATE_HEADERS_PARSING:
            if (n == 0) {
                goto finished;
            }

            // parse headers
            if (!h->host && strncasecmp(HTTP_HEADER_HOST, line, strlen(HTTP_HEADER_HOST)) == 0) {
                tmp = strchr(line, ' ');
                h->host = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->user_agent && strncasecmp(HTTP_HEADER_USER_AGENT, line, strlen(HTTP_HEADER_USER_AGENT)) == 0) {
                tmp = strchr(line, ' ');
                h->user_agent = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->connection && strncasecmp(HTTP_HEADER_CONNECTION, line, strlen(HTTP_HEADER_CONNECTION)) == 0) {
                tmp = strchr(line, ' ');
                h->connection = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->cookie && strncasecmp(HTTP_HEADER_COOKIE, line, strlen(HTTP_HEADER_COOKIE)) == 0) {
                tmp = strchr(line, ' ');
                h->cookie = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->upgrade && strncasecmp(HTTP_HEADER_UPGRADE, line, strlen(HTTP_HEADER_UPGRADE)) == 0) {
                tmp = strchr(line, ' ');
                h->upgrade = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->sec_websocket_key && strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_KEY, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_KEY)) == 0) {
                tmp = strchr(line, ' ');
                h->sec_websocket_key = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->sec_websocket_protocol && strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL)) == 0) {
                tmp = strchr(line, ' ');
                h->sec_websocket_protocol = tmp + 1;
                h->buffers[h->count++] = line;
            } else if (!h->sec_websocket_version && strncasecmp(HTTP_HEADER_SEC_WEBSOCKET_VERSION, line, strlen(HTTP_HEADER_SEC_WEBSOCKET_VERSION)) == 0) {
                tmp = strchr(line, ' ');
                h->sec_websocket_version = tmp + 1;
                h->buffers[h->count++] = line;
            } else {
                // ignore
                free(line);
            }
            break;
        case HTTP_REQUEST_STATE_FINISHED:
            // impossible
            goto finished;
        case HTTP_REQUEST_STATE_ERROR:
            // impossible
            goto error;
        }
    }
    return h->state;
error:
    free(line);
    h->state = HTTP_REQUEST_STATE_ERROR;
    return h->state;
finished:
    free(line);
    h->state = HTTP_REQUEST_STATE_FINISHED;
    return h->state;
}

void print_http_request(http_request_t *h)
{
    int i;

    printf("\n");
    printf("state:%d\n", h->state);
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

    printf("\nDUMP LINES:\n");
    for (i = 0; i < h->count; i++) {
       printf("[line %d]: %s\n", i, h->buffers[i]);
    }
    printf("\n");
}

int send_http_response(struct evbuffer *b, int response)
{
    const char *resp;

    switch (response) {
    case 200:
        resp = HTTP_RESPONSE_200;
        break;
    case 400:
        resp = HTTP_RESPONSE_400;
        break;
    case 404:
    default:
        resp = HTTP_RESPONSE_404;
    }

    if (evbuffer_add(b, resp, strlen(resp)) != 0) {
        return -1;
    }
    return 0;
}

