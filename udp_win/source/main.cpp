#include "base.h"
#include "net_common.h"

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mmsystem.h>
#include <conio.h>

#define SERVER_ADDRESS "192.168.56.101"

#define HEARTBEAT 5000
#define TIMEOUT 10000
#define BUFFER_SIZE 4096

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

typedef struct {
    s16 id;
    b8 online;
    SOCKET socket;
    b8 thread_active;
    HANDLE thread_handle;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_address;
    s32 server_address_length;
    unsigned long ping_time;
    unsigned long latency;
} Client;

static void client_read(Client* client) {
    MSG_Base* msg = (MSG_Base*)client->buffer;
    switch (msg->type) {
        case MSG_TYPE_SERVER_PING:
            client->latency = timeGetTime() - client->ping_time;
            Log("Latency: %lu", client->latency);
            break;
        default:
            break;
    }
}

static void client_listen(void* p) {
    Client* client = (Client*)p;
    client->thread_active = TRUE;

    while (client->online) {
        s32 bytes_read = recvfrom(client->socket, client->buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client->server_address, &client->server_address_length);
        if (bytes_read > 0) {
            client_read(client);
        }
    }

    client->thread_active = FALSE;
}

int main() {
    Arena* arena = make_arena(KB(10));
    Client client;
    MemZeroStruct(&client);

    WSADATA wsa_data;
    u16 version = MAKEWORD(2, 0);
    int success = WSAStartup(version, &wsa_data);
    if (success != 0) {
        LogError("WSAStartup failed.");
        return EXIT_FAILURE;
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 0) {
        LogError("Incorrect WSA version.");
        return EXIT_FAILURE;
    }

    unsigned long buffer_size;
    WSAEnumProtocolsW(NULL, NULL, &buffer_size);

    Arena_Temp scratch = arena_temp_begin(arena);
    WSAPROTOCOL_INFOW* protocol_buffer = arena_push_zero(scratch.arena, buffer_size);
    int protocols[] = { IPPROTO_TCP, IPPROTO_UDP };

    success = WSAEnumProtocolsW(protocols, protocol_buffer, &buffer_size);
    if (success == SOCKET_ERROR) {
        LogError("Socket error.");
        return EXIT_FAILURE;
    }
    arena_temp_end(scratch);

    client.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client.socket == INVALID_SOCKET) {
        LogError("Unable to create socket.");
        return EXIT_FAILURE;
    }

    unsigned long socket_setting = 1;
    success = ioctlsocket(client.socket, FIONBIO, &socket_setting);
    if (success == SOCKET_ERROR) {
        LogError("Unable to bind socket.");
        return EXIT_FAILURE;
    }

    MemZeroStruct(&client.server_address);
    client.server_address.sin_family = AF_INET;
    client.server_address.sin_port = htons(NET_PORT);
    inet_pton(AF_INET, SERVER_ADDRESS, &client.server_address.sin_addr.s_addr);
    client.server_address_length = sizeof(struct sockaddr_in);

    MSG_Base msg_connect = { MSG_TYPE_CLIENT_LOGIN, SERVER_ID, INVALID_ID };
    s32 bytes_sent = sendto(client.socket, (char*)&msg_connect, sizeof(MSG_Base), 0, (struct sockaddr*)&client.server_address, client.server_address_length);
    if (bytes_sent == 0) {
        LogError("Failed to send MSG_TYPE_CLIENT_LOGIN.");
        return EXIT_FAILURE;
    }

    u32 send_time = timeGetTime();
    while (TRUE) {
        s32 bytes_read = recvfrom(client.socket, client.buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client.server_address, &client.server_address_length);
        if (bytes_read > 0) {
            break;
        }

        if (timeGetTime() > send_time + TIMEOUT) {
            LogError("MSG_TYPE_CLIENT_LOGIN timeout.");
            return EXIT_FAILURE;
        }
    }

    MSG_Base* msg = (MSG_Base*)client.buffer;
    if (msg->type != MSG_ID) {
        LogError("Received invalid message from server.");
        return EXIT_FAILURE;
    }

    if (msg->to_id == INVALID_ID) {
        LogError("Server is full.");
        return EXIT_FAILURE;
    }

    Log("Received MSG_TYPE_SERVER_LOGIN_SUCCESS with %hi.", msg->to_id);
    client.id = msg->to_id;
    client.online = TRUE;
    client.thread_active = FALSE;

    unsigned long thread_id;
    client.thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_listen, (void*)&client, 0, &thread_id);
    if (client.thread_handle == NULL) {
        LogError("Unable to create thread.");
        return EXIT_FAILURE;
    }
    
    client.latency = 0;
    client.ping_time = timeGetTime();

    while (TRUE) {
        if (_kbhit()) {
            char c = _getch();
            if (c == 'q') {
                break;
            }
        }

        if (timeGetTime() >= client.ping_time + HEARTBEAT) {
            client.ping_time = timeGetTime();
            MSG_Base msg = { MSG_TYPE_CLIENT_PING, SERVER_ID, client.id };
            sendto(client.socket, (char*)&msg, sizeof(MSG_Base), 0, (struct sockaddr*)&client.server_address, client.server_address_length);
        }
    }

    if (client.online) {
        MSG_Base msg = { MSG_TYPE_CLIENT_DISCONNECT, SERVER_ID, client.id };
        sendto(client.socket, (char*)&msg, sizeof(MSG_Base), 0, (struct sockaddr*)&client.server_address, client.server_address_length);
        client.online = FALSE;
        while (client.thread_active) {}
        closesocket(client.socket);
        WSACleanup();
    }

    free(arena);

    return 0;
}
