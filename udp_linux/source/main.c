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
#include "net_common.c"

#define MAX_CLIENTS 1000
#define LISTEN_BUFFER_SIZE 4096

typedef struct {
    s16 id;
    b8 online;
    struct sockaddr_in client_address;
    char name[16];
    u16 local_seq;
    u16 remote_seq;
} Client; 

typedef struct {
    b8 online;
    u16 frame;
    int socket;
    Client clients[MAX_CLIENTS];
    mongoc_client_t* mongo_client;
    mongoc_database_t* mongo_db;
    mongoc_collection_t* mongo_col_characters;
    char listen_buffer[LISTEN_BUFFER_SIZE];
} Server;

static s16 get_next_client_id(Server* server) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (server->clients[i].online == FALSE) {
            server->clients[i].online = TRUE;
            server->clients[i].id = i;
            return i;
        }
    }

    return INVALID_ID;
}

static void client_reset(Client* client) {
    MemZeroStruct(client);
    client->id = INVALID_ID;
    client->local_seq = -1;
    client->remote_seq = -1;
}

static void packet_init(Packet_Header* header, s32 size, Packet_Type type) {
    MemZero(header, size);
    header->type = type;
}

static void packet_send(Server* server, Client* client, Packet_Header* packet, s32 packet_size) {
    packet->from_id = SERVER_ID;
    packet->to_id = client->id;
    client->local_seq++;
    packet->seq = client->local_seq;
    packet->ack = client->remote_seq;

    s32 bytes_sent = sendto(server->socket, packet, packet_size, 0, (struct sockaddr*)&client->client_address, sizeof(client->client_address));
    if (bytes_sent != packet_size) {
        LogError("Could not send %s to client id %hi.", packet_type_to_string(packet->type), client->id);
        return;
    }

    Log("Sent %s to client id %hi.", packet_type_to_string(packet->type), client->id);
}

static void receive_packet_client_login(Server* server, Packet_Client_Login* packet_in, struct sockaddr_in client_address) {
    char* ip_address = inet_ntoa(client_address.sin_addr);
    Log("%s sent a connect message.", ip_address);

    bson_t* filter = BCON_NEW("name", BCON_UTF8(packet_in->name));
    bson_t* opts = BCON_NEW("limit", BCON_INT64(1));
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(server->mongo_col_characters, filter, opts, NULL);

    Client* client = NULL;
    s16 client_id = INVALID_ID;
    const bson_t* document;
    if (mongoc_cursor_next(cursor, &document)) {
        client_id = get_next_client_id(server);
        if (client_id >= 0) {
            client = &server->clients[client_id];
            client->client_address = client_address;
            strncpy(client->name, packet_in->name, 16);

            //bson_iter_t iter;
            //bson_iter_t name;
            //if (bson_iter_init(&iter, document)) {
            //    while (bson_iter_next(&iter)) {
            //        const char* key = bson_iter_key(&iter);
            //        if (strcmp(key, "name") == 0) {
            //            strncpy(client->name, bson_iter_utf8(&iter, NULL), 16);
            //        }
            //    }
            //}
        } else {
            LogError("No available connections for client.");
            return;
        }
    } else {
        LogError("No character exists with name: %s.", packet_in->name);
        return;
    }


    mongoc_cursor_destroy(cursor);
    bson_destroy(opts);
    bson_destroy(filter);

    if (client_id == INVALID_ID) {
        LogError("Login failure.");
        return;
    }

    Log("%s has logged in.", client->name);
    Packet_Header packet_out;
    packet_init(&packet_out, sizeof(packet_out), Packet_Type_Server_Login_Success);
    packet_send(server, client, &packet_out, sizeof(packet_out));
}

static void receive_packet_client_ping(Server* server, Packet_Header* packet_in) {
    s16 client_id = packet_in->from_id;
    Packet_Header packet_out;
    packet_init(&packet_out, sizeof(packet_out), Packet_Type_Server_Ping);
    packet_send(server, &server->clients[client_id], &packet_out, sizeof(packet_out));
}

static void receive_packet_client_disconnect(Server* server, Packet_Header* msg) {
    Log("Shutting down server.");
    server->online = FALSE;
}

static void server_read(Server* server, struct sockaddr_in client_address) {
    Packet_Header* msg = (Packet_Header*)server->listen_buffer;

    Log("Received %s from %hi. SEQ=%hu, ACK=%hu.", packet_type_to_string(msg->type), msg->from_id, msg->seq, msg->ack);

    switch (msg->type) {
        case Packet_Type_Client_Login: 
            receive_packet_client_login(server, (Packet_Client_Login*)msg, client_address);
            break;
        case Packet_Type_Client_Ping:
            receive_packet_client_ping(server, msg);
            break;
        case Packet_Type_Client_Disconnect:
            receive_packet_client_disconnect(server, msg);
            break;
    }
}

//static void* server_listen(void* p) {
//    Server* server = (Server*)p;
//    struct sockaddr_in client_address;
//    u32 client_length = sizeof(client_address);
//    char buffer[LISTEN_BUFFER_SIZE];
//
//    while (server->online) {
//        int bytes_read = recvfrom(server->socket, buffer, LISTEN_BUFFER_SIZE, 0, (struct sockaddr*)&client_address, &client_length);
//        if (bytes_read > 0) {
//            server_read(server, buffer, client_address);
//        } else {
//            usleep(200*1000); // 200ms
//        }
//    }
//
//    return 0;
//}

function s32 server_init(Server* server) {
    server->online = TRUE;

    MemZeroArray(server->clients);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        client_reset(&server->clients[i]);
    }

    server->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server->socket == -1) {
        LogError("Could not create UDP socket.");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    MemZeroStruct(&server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NET_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int success = bind(server->socket, (struct sockaddr*)&server_address, sizeof(server_address));
    if (success == -1) {
        LogError("Could not bind the socket.");
        return EXIT_FAILURE;
    }

    u64 socket_settings = 1;
    success = ioctl(server->socket, FIONBIO, &socket_settings);
    if (success == -1) {
        LogError("Could not set socket to non-blocking I/O.");
        return EXIT_FAILURE;
    }

    //pthread_t thread_handle;
    //success = pthread_create(&thread_handle, NULL, server_listen, &server);
    //if (success != 0) {
    //    LogError("Could not create thread.");
    //    return EXIT_FAILURE;
    //}

    //pthread_setname_np(thread_handle, "net_thread");

    mongoc_init();
    server->mongo_client = mongoc_client_new("mongodb://localhost:27017");
    server->mongo_col_characters = mongoc_client_get_collection(server->mongo_client, "test", "characters");

    return 0;
}

function void net_read(Server* server) {
    struct sockaddr_in client_address;
    u32 client_length = sizeof(client_address);

    while (TRUE) {
        int bytes_read = recvfrom(server->socket, server->listen_buffer, LISTEN_BUFFER_SIZE, 0, (struct sockaddr*)&client_address, &client_length);
        if (bytes_read <= 0) {
            return;
        }

        server_read(server, client_address);
    }
}

function void server_update(Server* server) {
    net_read(server);
}

function void server_shutdown(Server* server) {
    mongoc_collection_destroy(server->mongo_col_characters);
    mongoc_client_destroy(server->mongo_client);
    mongoc_cleanup();

    int success = close(server->socket);
    if (success != 0) {
        LogError("Unable to close socket.");
    }
}

struct timespec diff_timespec(const struct timespec *time1, const struct timespec *time0) {
  assert(time1 && time1->tv_nsec < 1000000000);
  assert(time0 && time0->tv_nsec < 1000000000);
  struct timespec diff = {.tv_sec = time1->tv_sec - time0->tv_sec, .tv_nsec =
      time1->tv_nsec - time0->tv_nsec};
  if (diff.tv_nsec < 0) {
    diff.tv_nsec += 1000000000;
    diff.tv_sec--;
  }
  return diff;
}

#define TICK_NS 200000000L //200ms

int main() {
    Server server;
    MemZeroStruct(&server);
    server_init(&server);

    s64 acc = 0;
    s64 busywait_count = 0;
    struct timespec frame_start;
    struct timespec frame_end;
    struct timespec loop_start;
    struct timespec loop_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &loop_start);
    while (server.online) {
        //Log("diff.tv_sec = %li, diff.tv_nsec = %li", diff.tv_sec, diff.tv_nsec);
        //Log("dt = %li", dt);
        //Log("acc = %li", acc);
        while (acc >= TICK_NS) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &frame_start);
            server_update(&server);
            server.frame++;
            acc -= TICK_NS;
            clock_gettime(CLOCK_MONOTONIC_RAW, &frame_end);
            struct timespec frame_diff = diff_timespec(&frame_end, &frame_start);
            s64 frame_dt = 1000000000L * frame_diff.tv_sec + frame_diff.tv_nsec;
            Log("Frame = %hu. Duration = %fms.", server.frame, frame_dt / 1000000.0);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &loop_end);
        struct timespec loop_diff = diff_timespec(&loop_end, &loop_start);
        s64 loop_dt = 1000000000L * loop_diff.tv_sec + loop_diff.tv_nsec;
        acc += loop_dt;
        loop_start = loop_end;

        busywait_count++;

        //s64 remaining_microseconds = (TICK_NS - acc) / 1000;
        //s64 to_sleep = remaining_microseconds - 10;
        //if (to_sleep > 0) {
        //    usleep(to_sleep);
        //}
    }

    server_shutdown(&server);

    return 0;
}
