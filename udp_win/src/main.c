#include "base.h"
#include "net_common.h"

#include <winsock2.h>

#define SERVER_ADDRESS "192.168.56.101"

#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "winmm.lib")

int main() {
    Arena* arena = make_arena(KB(1));

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

    Log("buffer_size = %lu", buffer_size);

    Arena_Temp scratch = arena_temp_begin(arena);
    WSAPROTOCOL_INFOW* protocol_buffer = arena_push_zero(scratch.arena, buffer_size);
    int protocols[] = { IPPROTO_TCP, IPPROTO_UDP };

    success = WSAEnumProtocolsW(protocols, protocol_buffer, &buffer_size);
    if (success == SOCKET_ERROR) {
        LogError("Socket error.");
        return EXIT_FAILURE;
    }
    arena_temp_end(scratch);

    free(arena);

    return 0;
}
