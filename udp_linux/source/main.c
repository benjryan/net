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

#define MAX_CLIENTS 1000
#define LISTEN_BUFFER_SIZE 4096

typedef struct {
    b8 online;
    struct sockaddr_in client_address;
} Client; 

typedef struct {
    s16 type;
    s16 to_id;
    s16 from_id;
} MSG_Data;

typedef struct {
    b8 online;
    int socket;
    Client clients[MAX_CLIENTS];
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

static void handle_msg_connect(Server* server, struct sockaddr_in client_address) {
    char* ip_address = inet_ntoa(client_address.sin_addr);
    Log("%s sent a connect message.", ip_address);

    s16 new_client_id = get_next_client_id(server);
    if (new_client_id >= 0) {
        server->clients[new_client_id].client_address = client_address;
    }

    MSG_Data msg = { MSG_ID, new_client_id, SERVER_ID };
    s32 bytes_sent = sendto(server->socket, &msg, sizeof(msg), 0, (struct sockaddr*)&client_address, sizeof(client_address));
    if (bytes_sent != sizeof(msg)) {
        LogError("Could not send MSG_ID to %hi.", new_client_id);
        return;
    }

    Log("Send MSG_ID %hi to client", new_client_id);
}

static void handle_msg_ping(Server* server, s16 client_id) {
    MSG_Data msg = { MSG_PING, client_id, SERVER_ID };
    struct sockaddr_in* client_address = &server->clients[client_id].client_address;
    s32 bytes_sent = sendto(server->socket, &msg, sizeof(msg), 0, (struct sockaddr*)client_address, sizeof(*client_address));
    if (bytes_sent != sizeof(msg)) {
        LogError("Could not send MSG_PING to %hi.", client_id);
        return;
    }

    Log("Send MSG_PING to client");
}

static void handle_msg_disconnect(Server* server, s16 client_id) {
    Log("Shutting down server.");
    server->online = FALSE;
}

static void server_read(Server* server, char* buffer, struct sockaddr_in client_address) {
    MSG_Data* msg = (MSG_Data*)buffer;

    switch (msg->type) {
        case MSG_CONNECT: 
            handle_msg_connect(server, client_address);
            break;
        case MSG_PING:
            handle_msg_ping(server, msg->from_id);
            break;
        case MSG_DISCONNECT:
            handle_msg_disconnect(server, msg->from_id);
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
    Server status;
    status.online = TRUE;

    status.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (status.socket == -1) {
        LogError("Could not create UDP socket.");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    MemZeroStruct(&server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NET_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int success = bind(status.socket, (struct sockaddr*)&server_address, sizeof(server_address));
    if (success == -1) {
        LogError("Could not bind the socket.");
        return EXIT_FAILURE;
    }

    u64 socket_settings = 1;
    success = ioctl(status.socket, FIONBIO, &socket_settings);
    if (success == -1) {
        LogError("Could not set socket to non-blocking I/O.");
        return EXIT_FAILURE;
    }

    pthread_t thread_handle;
    success = pthread_create(&thread_handle, NULL, server_listen, &status);
    if (success != 0) {
        LogError("Could not create thread.");
        return EXIT_FAILURE;
    }

    pthread_setname_np(thread_handle, "net_thread");

    mongoc_init();
    mongoc_client_t* mongo_client = mongoc_client_new("mongodb://localhost:27017");
    mongoc_database_t* mongo_db = mongoc_client_get_database(mongo_client, "test");
    
    while (status.online) {
    }

    mongoc_database_destroy(mongo_db);
    mongoc_client_destroy(mongo_client);
    mongoc_cleanup();

    success = close(status.socket);
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
