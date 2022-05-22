#include "base.h"
#include "net_common.h"

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mmsystem.h>
#include <conio.h>
#include <synchapi.h>

#include "base.c"
#include "net_common.c"

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
    u16 local_seq;
    u16 remote_seq;
} Client;

static void client_read(Client* client) {
    Packet_Header* packet = (Packet_Header*)client->buffer;
    switch (packet->type) {
        case Packet_Type_Server_Ping:
            client->latency = timeGetTime() - client->ping_time;
            Log("Received %s. Latency: %lu", packet_type_to_string(packet->type), client->latency);
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
        } else {
            Sleep(1000);
        }
    }

    client->thread_active = FALSE;
}

static b8 send_packet(Client* client, Packet_Header* packet, s32 packet_size) {
    packet->from_id = client->id;
    packet->to_id = SERVER_ID;
    client->local_seq++;
    packet->seq = client->local_seq;
    packet->ack = client->remote_seq;

    s32 bytes_sent = sendto(client->socket, (char*)packet, packet_size, 0, (struct sockaddr*)&client->server_address, client->server_address_length);
    if (bytes_sent == 0) {
        LogError("Failed to send %s.", packet_type_to_string(packet->type));
        return FALSE;
    }

    return TRUE;
}

int main() {
    Arena* arena = make_arena(KB(10));
    Client client;
    MemZeroStruct(&client);
    client.id = INVALID_ID;
    client.local_seq = -1;
    client.remote_seq = -1;

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

    client.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

    char username[128];
    printf("Username: ");
    scanf_s("%s", username, (u32)sizeof(username));
    //char* password = getpass("Password: ");

    {
        Packet_Client_Login packet;
        MemZeroStruct(&packet);
        packet.header.type = Packet_Type_Client_Login;
        strncpy_s(packet.name, 16, username, 16);
        send_packet(&client, (Packet_Header*)&packet, sizeof(packet));
    }

    u32 send_time = timeGetTime();
    while (TRUE) {
        s32 bytes_read = recvfrom(client.socket, client.buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client.server_address, &client.server_address_length);
        if (bytes_read > 0) {
            break;
        } else {
            Sleep(1000);
        }

        if (timeGetTime() > send_time + TIMEOUT) {
            LogError("Packet_Type_Client_Login timeout.");
            return EXIT_FAILURE;
        }
    }

    Packet_Header* packet = (Packet_Header*)client.buffer;
    if (packet->type != Packet_Type_Server_Login_Success) {
        LogError("Received invalid message from server.");
        return EXIT_FAILURE;
    }

    if (packet->to_id == INVALID_ID) {
        LogError("Server is full.");
        return EXIT_FAILURE;
    }

    Log("Received %s with %hi.", packet_type_to_string(packet->type), packet->to_id);
    client.id = packet->to_id;
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
                Log("Disconnecting...");
                break;
            }
        }

        if (timeGetTime() >= client.ping_time + HEARTBEAT) {
            client.ping_time = timeGetTime();

            Packet_Header packet;
            MemZeroStruct(&packet);
            packet.type = Packet_Type_Client_Ping;
            send_packet(&client, &packet, sizeof(packet));
        }

        Sleep(1000);
    }

    if (client.online) {
        Packet_Header packet;
        MemZeroStruct(&packet);
        packet.type = Packet_Type_Client_Disconnect;
        send_packet(&client, &packet, sizeof(packet));

        client.online = FALSE;
        while (client.thread_active) {}
        closesocket(client.socket);
        WSACleanup();
    }

    free(arena);

    return 0;
}
