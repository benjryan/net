#ifndef NET_COMMON_H
#define NET_COMMON_H

#include "base.h"

#define NET_PORT 7531

#define SERVER_ID  -2
#define INVALID_ID -1

//#define MSG_TYPE_CLIENT_LOGIN         0
//#define MSG_TYPE_SERVER_LOGIN_SUCCESS 1
//#define MSG_TYPE_CLIENT_PING          2
//#define MSG_TYPE_SERVER_PING          3
//#define MSG_TYPE_CLIENT_DISCONNECT    4

typedef enum {
    Packet_Type_Client_Login,
    Packet_Type_Server_Login_Success,
    Packet_Type_Client_Ping,
    Packet_Type_Server_Ping,
    Packet_Type_Client_Disconnect,
} Packet_Type;

typedef struct {                        
    s16 type;
    s16 from_id;
    s16 to_id;
    u16 seq;
    u16 ack;
} Packet_Header;

typedef struct {
    Packet_Header header;
    char name[16];
} Packet_Client_Login;

function char* packet_type_to_string(Packet_Type type);

#endif // NET_COMMON_H
