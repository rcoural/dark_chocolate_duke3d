/*
 * duke3d-relay - dedicated relay server for Dark Chocolate Duke3D online play.
 *
 * The original Build netcode is peer-to-peer lockstep: every player must reach
 * every other player directly, which the internet's NAT makes impractical. This
 * relay fixes that without touching the lockstep game logic. Clients only ever
 * talk to this server (a single outbound connection, NAT-friendly), and the
 * server forwards each client's packets to the other. The server is NOT a
 * player - it runs no game engine, needs no SDL and no DUKE3D.GRP, so it builds
 * and runs on a bare headless box (even an ancient one) with just a C compiler.
 *
 * Phase 1: exactly 2 players. The handshake and packet layout match
 * Engine/src/mmulti.c (connect_to_server / wait_for_other_players) byte-for-byte.
 *
 * Handshake (leading 0,0,0 so the game never mistakes these for gameplay):
 *   client -> server : HELLO        (4 bytes)
 *   server -> client : WELCOME{idx} (assigns the 1-based player slot)
 * After both clients are welcomed the server reflects every packet from one
 * client to the other, verbatim, until the session goes idle or CTRL-C.
 *
 * Build:  gcc -O2 -Wall -o duke3d-relay duke3d-relay.c
 * Run:    ./duke3d-relay [port]        (default port 1635/udp)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT        1635
#define MAXPACKETSIZE       1024
#define HEADER_CLIENT_HELLO   246
#define HEADER_SERVER_WELCOME 247
#define NUMPLAYERS            2
#define WELCOME_REPEATS       10    /* resend WELCOME to ride out packet loss */
#define IDLE_RESET_SECONDS     60   /* free the slots once a match goes silent */

typedef struct {
    unsigned char dummy1, dummy2, dummy3;   /* 0,0,0 -> not a game packet. */
    unsigned char header;                   /* HEADER_CLIENT_HELLO */
} PacketClientHello;

typedef struct {
    unsigned char dummy1, dummy2, dummy3;   /* 0,0,0 -> not a game packet. */
    unsigned char header;                   /* HEADER_SERVER_WELCOME */
    unsigned char myindex;                  /* assigned 1-based player slot */
    unsigned char numplayers;
} PacketServerWelcome;

static volatile int ctrlc_pressed = 0;
static void siginthandler(int sig) { (void) sig; ctrlc_pressed = 1; }

static int packet_is_client_hello(const unsigned char *buf, int len)
{
    return (len == (int) sizeof (PacketClientHello))
        && (buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0)
        && (buf[3] == HEADER_CLIENT_HELLO);
}

static const char *addrstr(const struct sockaddr_in *a)
{
    static char s[32];
    snprintf(s, sizeof (s), "%s:%d", inet_ntoa(a->sin_addr), ntohs(a->sin_port));
    return s;
}

static int same_client(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return (a->sin_addr.s_addr == b->sin_addr.s_addr) && (a->sin_port == b->sin_port);
}

/* Block until a packet arrives or `secs` elapse. Returns >0 on data, 0 on
 * timeout, -1 on error/signal. Fills *from with the sender. */
static int recv_with_timeout(int sock, unsigned char *buf, int buflen,
                             struct sockaddr_in *from, int secs)
{
    fd_set fds;
    struct timeval tv;
    socklen_t fromlen = sizeof (*from);
    int rc;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec = secs;
    tv.tv_usec = 0;

    rc = select(sock + 1, &fds, NULL, NULL, &tv);
    if (rc <= 0)
        return rc;          /* 0 = timeout, -1 = interrupted */

    return (int) recvfrom(sock, buf, buflen, 0,
                          (struct sockaddr *) from, &fromlen);
}

int main(int argc, char **argv)
{
    int sock, port = DEFAULT_PORT;
    int one = 1;
    struct sockaddr_in bindaddr;

    if (argc > 1)
        port = atoi(argv[1]);
    if ((port <= 0) || (port > 65535)) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        return 1;
    }

    signal(SIGINT,  siginthandler);
    signal(SIGTERM, siginthandler);
    signal(SIGPIPE, SIG_IGN);

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { perror("socket"); return 1; }
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

    memset(&bindaddr, 0, sizeof (bindaddr));
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons((unsigned short) port);
    if (bind(sock, (struct sockaddr *) &bindaddr, sizeof (bindaddr)) < 0) {
        perror("bind");
        return 1;
    }

    setbuf(stdout, NULL);
    printf("duke3d-relay: listening on 0.0.0.0:%d/udp (2 players). CTRL-C to stop.\n", port);

    /* Each outer iteration hosts one match, then resets for the next pair. */
    while (!ctrlc_pressed) {
        struct sockaddr_in client[NUMPLAYERS], from;
        PacketServerWelcome welcome;
        unsigned char buf[MAXPACKETSIZE];
        int n = 0, i, reps, rc;
        time_t last_activity;

        memset(client, 0, sizeof (client));
        memset(&welcome, 0, sizeof (welcome));
        welcome.header = HEADER_SERVER_WELCOME;
        welcome.numplayers = NUMPLAYERS;

        printf("relay: waiting for %d players...\n", NUMPLAYERS);

        /* Phase A: collect two distinct clients by their HELLO. */
        while ((n < NUMPLAYERS) && (!ctrlc_pressed)) {
            rc = recv_with_timeout(sock, buf, sizeof (buf), &from, 5);
            if (rc <= 0)
                continue;
            if (!packet_is_client_hello(buf, rc))
                continue;

            int known = 0;
            for (i = 0; i < n; i++)
                if (same_client(&client[i], &from))
                    known = 1;
            if (!known) {
                client[n] = from;
                printf("relay: player #%d = %s\n", n + 1, addrstr(&from));
                n++;
            }
        }
        if (ctrlc_pressed)
            break;

        /* Tell each client its slot. */
        for (reps = 0; reps < WELCOME_REPEATS; reps++) {
            for (i = 0; i < NUMPLAYERS; i++) {
                welcome.myindex = (unsigned char) (i + 1);
                sendto(sock, &welcome, sizeof (welcome), 0,
                       (struct sockaddr *) &client[i], sizeof (client[i]));
            }
            usleep(20 * 1000);
        }
        printf("relay: both players in, relaying.\n");
        last_activity = time(NULL);

        /* Phase B: reflect packets between the two clients until idle / CTRL-C. */
        while (!ctrlc_pressed) {
            rc = recv_with_timeout(sock, buf, sizeof (buf), &from, 5);
            if (rc < 0)
                continue;
            if (rc == 0) {  /* timeout: reset if the match went quiet. */
                if (time(NULL) - last_activity >= IDLE_RESET_SECONDS) {
                    printf("relay: session idle for %ds, resetting.\n", IDLE_RESET_SECONDS);
                    break;
                }
                continue;
            }

            /* Only traffic from the two known clients counts as activity and gets
             * relayed. Packets from anyone else (stray scans, a stale session's
             * dead sockets) are ignored and must NOT keep the session alive, or
             * the slots would never free up for the next pair. */
            if (packet_is_client_hello(buf, rc)) {   /* lost welcome -> resend it. */
                for (i = 0; i < NUMPLAYERS; i++)
                    if (same_client(&client[i], &from)) {
                        welcome.myindex = (unsigned char) (i + 1);
                        sendto(sock, &welcome, sizeof (welcome), 0,
                               (struct sockaddr *) &client[i], sizeof (client[i]));
                        last_activity = time(NULL);
                    }
                continue;
            }

            if (same_client(&from, &client[0])) {
                sendto(sock, buf, rc, 0,
                       (struct sockaddr *) &client[1], sizeof (client[1]));
                last_activity = time(NULL);
            } else if (same_client(&from, &client[1])) {
                sendto(sock, buf, rc, 0,
                       (struct sockaddr *) &client[0], sizeof (client[0]));
                last_activity = time(NULL);
            }
            /* packets from anyone else are ignored. */
        }
    }

    printf("\nduke3d-relay: shutting down.\n");
    close(sock);
    return 0;
}
