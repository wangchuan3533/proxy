#include <unistd.h>
#include <event2/event.h>
#include <event2/buffer.h>
int main(int argc, char **argv)
{
    struct evbuffer *buf = evbuffer_new();
    size_t n;
    const char *line, *str = "GET / HTTP/1.1\r\n"
        "Host: hello.world\r\n"
        "\r\n";
    evbuffer_add(buf, str, strlen(str));
    while (1) {
        line = evbuffer_readln(buf, &n, EVBUFFER_EOL_CRLF);
        printf("line=[%s], address=[%#x], n=%d\n", line, (unsigned int)line, n);
        if (NULL == line) {
            break;
        }
        free(line);
    }
    return 0;
}
