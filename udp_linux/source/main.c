#define _GNU_SOURCE

#include "base.h"
#include "net_common.h"

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <mongoc/mongoc.h>

// NOTE(bryan): Unity build
#include "base.c"

#define MAX_CLIENTS 1000
#define LISTEN_BUFFER_SIZE 4096

typedef struct {
    b8 online;
    struct sockaddr_in client_address;
} Client; 

typedef struct {
    s16 type;
    s16 from_id;
} MSG_Header;

typedef struct {
    MSG_Header header;
    char name[16];
} MSG_Connect;

typedef struct {
    b8 online;
    int socket;
    Client clients[MAX_CLIENTS];
    mongoc_client_t* mongo_client;
    mongoc_database_t* mongo_db;
} Server;

static s16 get_next_client_id(Server* server) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (server->clients[i].online == FALSE) {
            server->clients[i].online = TRUE;
            return i;
        }
    }

    return -1;
}

static void handle_msg_client_login(Server* server, MSG_Base* msg, struct sockaddr_in client_address) {
    char* ip_address = inet_ntoa(client_address.sin_addr);
    Log("%s sent a connect message.", ip_address);

    s16 new_client_id = get_next_client_id(server);
    if (new_client_id >= 0) {
        server->clients[new_client_id].client_address = client_address;
    }

    MSG_Base reply = { MSG_TYPE_SERVER_LOGIN_SUCCESS, SERVER_ID, new_client_id };
    s32 bytes_sent = sendto(server->socket, &reply, sizeof(reply), 0, (struct sockaddr*)&client_address, sizeof(client_address));
    if (bytes_sent != sizeof(reply)) {
        LogError("Could not send MSG_TYPE_SERVER_LOGIN_SUCCESS to %hi.", new_client_id);
        return;
    }

    Log("Send MSG_ID %hi to client", new_client_id);
}

static void handle_msg_client_ping(Server* server, MSG_Base* msg) {
    s16 client_id = msg->from_id;
    MSG_Base reply = { MSG_TYPE_SERVER_PING, SERVER_ID, client_id };
    struct sockaddr_in* client_address = &server->clients[client_id].client_address;
    s32 bytes_sent = sendto(server->socket, &reply, sizeof(reply), 0, (struct sockaddr*)client_address, sizeof(*client_address));
    if (bytes_sent != sizeof(reply)) {
        LogError("Could not send MSG_TYPE_SERVER_PING to %hi.", client_id);
        return;
    }

    Log("Send MSG_PING to client");
}

static void handle_msg_client_disconnect(Server* server, MSG_Base* msg) {
    Log("Shutting down server.");
    server->online = FALSE;
}

static void server_read(Server* server, char* buffer, struct sockaddr_in client_address) {
    MSG_Base* msg = (MSG_Base*)buffer;

    switch (msg->type) {
        case MSG_TYPE_CLIENT_LOGIN: 
            handle_msg_client_login(server, msg, client_address);
            break;
        case MSG_TYPE_CLIENT_PING:
            handle_msg_client_ping(server, msg);
            break;
        case MSG_TYPE_CLIENT_DISCONNECT:
            handle_msg_client_disconnect(server, msg);
            break;
    }
}

static void* server_listen(void* p) {
    Server* server = (Server*)p;
    struct sockaddr_in client_address;
    u32 client_length = sizeof(client_address);
    char buffer[LISTEN_BUFFER_SIZE];

    while (server->online) {
        int bytes_read = recvfrom(server->socket, buffer, LISTEN_BUFFER_SIZE, 0, (struct sockaddr*)&client_address, &client_length);
        if (bytes_read > 0)
        {
            server_read(server, buffer, client_address);
        }
    }

    return 0;
}

int main() {
    Server server;
    server.online = TRUE;

    server.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server.socket == -1) {
        LogError("Could not create UDP socket.");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    MemZeroStruct(&server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NET_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int success = bind(server.socket, (struct sockaddr*)&server_address, sizeof(server_address));
    if (success == -1) {
        LogError("Could not bind the socket.");
        return EXIT_FAILURE;
    }

    u64 socket_settings = 1;
    success = ioctl(server.socket, FIONBIO, &socket_settings);
    if (success == -1) {
        LogError("Could not set socket to non-blocking I/O.");
        return EXIT_FAILURE;
    }

    pthread_t thread_handle;
    success = pthread_create(&thread_handle, NULL, server_listen, &server);
    if (success != 0) {
        LogError("Could not create thread.");
        return EXIT_FAILURE;
    }

    pthread_setname_np(thread_handle, "net_thread");

    mongoc_init();
    server.mongo_client = mongoc_client_new("mongodb://localhost:27017");
    server.mongo_db = mongoc_client_get_database(server.mongo_client, "test");
    
    while (server.online) {
    }

    mongoc_database_destroy(server.mongo_db);
    mongoc_client_destroy(server.mongo_client);
    mongoc_cleanup();

    success = close(server.socket);
    if (success != 0) {
        LogError("Unable to close socket.");
    }

    return 0;
}

//int main() {
//    unsigned int i = 0;
//    unsigned long l = 0;
//    printf("unsigned int=%lu", sizeof(i));
//    printf("unsigned long=%lu", sizeof(l));
//}
