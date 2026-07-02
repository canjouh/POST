#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 4096

/* 传递给线程的客户端信息 */
typedef struct {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

/* 工作线程：处理单个客户端 */
void* handle_client(void* arg)
{
    client_info_t* info = (client_info_t*)arg;
    SOCKET client_socket = info->client_socket;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &info->client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("[+] Client connected: %s:%d\n", client_ip, ntohs(info->client_addr.sin_port));

    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("[<] Received from %s: %s\n", client_ip, buffer);

            /* 回显（Echo）给客户端 */
            send(client_socket, buffer, bytes_received, 0);
        }
        else if (bytes_received == 0) {
            printf("[-] Client disconnected: %s\n", client_ip);
            break;
        }
        else {
            printf("[!] recv error: %d\n", WSAGetLastError());
            break;
        }
    }

    closesocket(client_socket);
    free(info);
    return NULL;
}

int main()
{
    /* 初始化 Winsock */
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    /* 创建监听 socket */
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* 绑定地址和端口 */
    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    /* 开始监听 */
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("[*] Server listening on port %d...\n", PORT);

    /* 主循环：接受客户端连接，为每个客户端创建线程 */
    while (1) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            printf("[!] Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        /* 分配客户端信息结构体（在线程中释放） */
        client_info_t* info = (client_info_t*)malloc(sizeof(client_info_t));
        if (!info) {
            printf("[!] malloc failed\n");
            closesocket(client_socket);
            continue;
        }
        info->client_socket = client_socket;
        info->client_addr   = client_addr;

        /* 为每个客户端创建独立线程 */
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void*)info) != 0) {
            printf("[!] Thread creation failed\n");
            closesocket(client_socket);
            free(info);
            continue;
        }
        pthread_detach(thread);   /* 线程结束后自动回收资源 */
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
