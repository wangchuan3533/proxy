#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
const char *websocket_request = 
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Origin: http://example.com\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";


int main(int argc, char **argv)
{
    struct sockaddr_in serv_addr;
    char buffer[128];
    int fd, ret, n, addr_len = sizeof serv_addr;
    
    ret = evutil_parse_sockaddr_port("127.0.0.1:8200", &serv_addr, &addr_len);
    if (ret) {
        perror("evutil_parse_sockaddr_port");
        exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }
    ret = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        perror("connect");
        exit(1);
    }

    n = send(fd, websocket_request, strlen(websocket_request), 0);
    if (n != strlen(websocket_request)) {
        printf("send failed\n");
        exit(1);
    }
    n = recv(fd, buffer, sizeof(buffer), 0);
    buffer[n] = 0;
    printf(buffer);
    close(fd);
}
