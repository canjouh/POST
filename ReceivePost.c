#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096

// 简单的URL解码函数
void url_decode(char *str) {
    char *src = str;
    char *dst = str;
    while (*src) {
        if (*src == '+') {
            *dst = ' ';
        } else if (*src == '%' && src[1] && src[2]) {
            int hex;
            sscanf(src + 1, "%2x", &hex);
            *dst = (char)hex;
            src += 2;
        } else {
            *dst = *src;
        }
        src++;
        dst++;
    }
    *dst = '\0';
}

// 从表单数据中提取指定字段的值
int get_form_value(const char *body, const char *key, char *value, int max_len) {
    char search[256];
    snprintf(search, sizeof(search), "%s=", key);

    char *start = strstr(body, search);
    if (start == NULL) return 0;

    start += strlen(search);
    char *end = strchr(start, '&');

    int len;
    if (end != NULL) {
        len = (int)(end - start);
    } else {
        len = (int)strlen(start);
    }

    if (len >= max_len) len = max_len - 1;
    strncpy(value, start, len);
    value[len] = '\0';
    url_decode(value);
    return 1;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);
    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    listen(serverSocket, 5);
    printf("Server running on http://localhost:%d\n", PORT);

    for (;;) {
        struct sockaddr_in clientAddress;
        int clientAddressSize = sizeof(clientAddress);
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressSize);

        char buffer[BUFFER_SIZE] = {0};
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            closesocket(clientSocket);
            continue;
        }

        // 检查是否是POST请求
        if (strncmp(buffer, "POST", 4) == 0) {
            // 找到请求体（空行之后）
            char *body = strstr(buffer, "\r\n\r\n");
            if (body != NULL) {
                body += 4; // 跳过 \r\n\r\n
                printf("Received POST data: %s\n", body);

                // 提取表单字段
                char title[256] = {0};
                char content[1024] = {0};
                get_form_value(body, "title", title, sizeof(title));
                get_form_value(body, "content", content, sizeof(content));

                printf("Parsed title: %s\n", title);
                printf("Parsed content: %s\n", content);

                // 构造JSON响应
                char json_response[BUFFER_SIZE];
                snprintf(json_response, sizeof(json_response),
                    "{\"status\":\"success\",\"message\":\"数据接收成功\",\"data\":{\"title\":\"%s\",\"content\":\"%s\"}}",
                    title, content);

                // 发送HTTP响应（JSON格式）
                char http_response[BUFFER_SIZE];
                snprintf(http_response, sizeof(http_response),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json; charset=utf-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    (int)strlen(json_response), json_response);

                send(clientSocket, http_response, (int)strlen(http_response), 0);
            }
        }

        closesocket(clientSocket);
    }

    WSACleanup();
    return 0;
}
