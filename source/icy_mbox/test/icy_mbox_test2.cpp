//
// Simple listener.c program for UDP multicast
//
// Adapted from:
// http://ntrg.cs.tcd.ie/undergrad/4ba2/multicast/antony/example.html
//
// Changes:
// * Compiles for Windows as well as Linux
// * Takes the port and group on the command line
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifdef _WIN32
#include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h> // needed for ip_mreq definition for multicast
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MSGBUFSIZE 256
#pragma comment(lib, "ws2_32")
int wWinMain(HINSTANCE__*, HINSTANCE__*, wchar_t*, int)
{
    const char* group = "230.4.4.1";
    //argv[1]; // e.g. 239.255.255.250 for SSDP
    int port = atoi("1044"); // 0 if error, which is an invalid port


#ifdef _WIN32
    //
    // Initialize Windows Socket API with given VERSION.
    //
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        perror("WSAStartup");
        return 1;
    }
#endif

    // create what looks like an ordinary UDP socket
    //
    int fd = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (fd < 0)
    {
        perror("socket");
        return 1;
    }

    // allow multiple sockets to use the same PORT number
    //
    u_int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0 ) 
    {
        perror("Reusing ADDR failed");
        return 1;
    }
    auto null = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&null, sizeof(null)) < 0)
    {
        perror("SO_RCVBUF failed");
        return 1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&null, sizeof(null)) < 0)
    {
        perror("SO_RCVBUF failed");
        return 1;
    }
    timeval tv = {};

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
    {
        perror("SO_RCVTIMEO failed");
        return 1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
    {
        perror("SO_SNDTIMEO failed");
        return 1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    addr.sin_port = htons(port);

    // bind to receive address
    //
    if (bind(fd, (struct sockaddr*) & addr, sizeof(addr)) < 0) 
    {
        perror("bind");
        return 1;
    }

    // use setsockopt() to request that the kernel join a multicast group
    //
    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt");
        return 1;
    }

    // now just enter a read-print loop
    //
    while (1) {
        char msgbuf[MSGBUFSIZE];
        int addrlen = sizeof(addr);
        int nbytes = recvfrom(
            fd,
            msgbuf,
            MSGBUFSIZE,
            0,
            (struct sockaddr*) & addr,
            &addrlen
        );
        if (nbytes < 0) {
            perror("recvfrom");
            return 1;
        }
        msgbuf[nbytes] = '\0';
        puts(msgbuf);
    }

#ifdef _WIN32
    //
    // Program never actually gets here due to infinite loop that has to be
    // canceled, but since people on the internet wind up using examples
    // they find at random in their own code it's good to show what shutting
    // down cleanly would look like.
    //
    WSACleanup();
#endif

    return 0;
}