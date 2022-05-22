#include "net_common.h"

function char* packet_type_to_string(Packet_Type type) {
    switch (type) {
        case Packet_Type_Client_Disconnect:
            return "Packet_Type_Client_Disconnect";
        case Packet_Type_Client_Login:
            return "Packet_Type_Client_Login";
        case Packet_Type_Client_Ping:
            return "Packet_Type_Client_Ping";
        case Packet_Type_Server_Ping:
            return "Packet_Type_Server_Ping";
        case Packet_Type_Server_Login_Success:
            return "Packet_Type_Server_Login_Success";
    }

    return "Packet_Type_Invalid";
}

