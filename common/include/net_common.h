#ifndef NET_COMMON_H
#define NET_COMMON_H

#include "base.h"

#define NET_PORT 7531

#define SERVER_ID  -2
#define INVALID_ID -1

#define MSG_CONNECT 1000
#define MSG_ID 1001
#define MSG_PING 1002
#define MSG_DISCONNECT 1003

typedef struct {                        
  s16 type;              
  s16 to_id;              
  s16 from_id;            
} Net_Message_Header;

#endif // NET_COMMON_H
