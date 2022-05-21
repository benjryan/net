#ifndef NET_COMMON_H
#define NET_COMMON_H

#include "base.h"

#define NET_PORT 7531

#define SERVER_ID  -2
#define INVALID_ID -1

#define MSG_TYPE_CLIENT_LOGIN         0
#define MSG_TYPE_SERVER_LOGIN_SUCCESS 1
#define MSG_TYPE_CLIENT_PING          2
#define MSG_TYPE_SERVER_PING          3
#define MSG_TYPE_CLIENT_DISCONNECT    4

typedef struct {                        
    s16 type;
    s16 from_id;
    s16 to_id;
} MSG_Base;

typedef struct {
    MSG_Base base;
    char name[16];
} MSG_Client_Login;

#endif // NET_COMMON_H
