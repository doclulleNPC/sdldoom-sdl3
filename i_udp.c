// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	Clean-room Chocolate-Doom-compatible UDP/packet layer (see i_udp.h).
//	Stage 1: big-endian packet buffer + UDP transport + server QUERY, so we
//	can interoperate with Chocolate/Crispy peers on the wire.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef int socklen_t;
  #define CLOSESOCK closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <sys/select.h>
  #define CLOSESOCK close
  #define INVALID_SOCKET (-1)
#endif

#include "i_udp.h"

//
// Packet buffer.  All multi-byte integers are big-endian on the wire, matching
// Chocolate-Doom's net_packet.c.
//
net_packet_t* NET_NewPacket (int initial_size)
{
    net_packet_t* p = malloc (sizeof(*p));
    if (initial_size < 16) initial_size = 16;
    p->alloced = initial_size;
    p->data = malloc (initial_size);
    p->len = 0;
    p->pos = 0;
    return p;
}

void NET_FreePacket (net_packet_t* p)
{
    if (p) { free (p->data); free (p); }
}

static void NET_Grow (net_packet_t* p, size_t need)
{
    while (p->len + need > p->alloced)
    {
	p->alloced *= 2;
	p->data = realloc (p->data, p->alloced);
    }
}

void NET_WriteInt8 (net_packet_t* p, unsigned int i)
{
    NET_Grow (p, 1);
    p->data[p->len++] = i & 0xff;
}

void NET_WriteInt16 (net_packet_t* p, unsigned int i)
{
    NET_Grow (p, 2);
    p->data[p->len++] = (i >> 8) & 0xff;
    p->data[p->len++] = i & 0xff;
}

void NET_WriteInt32 (net_packet_t* p, unsigned int i)
{
    NET_Grow (p, 4);
    p->data[p->len++] = (i >> 24) & 0xff;
    p->data[p->len++] = (i >> 16) & 0xff;
    p->data[p->len++] = (i >> 8) & 0xff;
    p->data[p->len++] = i & 0xff;
}

void NET_WriteString (net_packet_t* p, const char* s)
{
    size_t n = strlen (s) + 1;
    NET_Grow (p, n);
    memcpy (p->data + p->len, s, n);
    p->len += n;
}

boolean NET_ReadInt8 (net_packet_t* p, unsigned int* i)
{
    if (p->pos + 1 > p->len) return false;
    *i = p->data[p->pos++];
    return true;
}

boolean NET_ReadInt16 (net_packet_t* p, unsigned int* i)
{
    if (p->pos + 2 > p->len) return false;
    *i = (p->data[p->pos] << 8) | p->data[p->pos+1];
    p->pos += 2;
    return true;
}

boolean NET_ReadInt32 (net_packet_t* p, unsigned int* i)
{
    if (p->pos + 4 > p->len) return false;
    *i = ((unsigned)p->data[p->pos] << 24) | (p->data[p->pos+1] << 16)
       | (p->data[p->pos+2] << 8) | p->data[p->pos+3];
    p->pos += 4;
    return true;
}

char* NET_ReadString (net_packet_t* p)
{
    static char	buf[1024];
    size_t	n = 0;
    while (p->pos < p->len && p->data[p->pos] != '\0' && n < sizeof(buf)-1)
	buf[n++] = p->data[p->pos++];
    if (p->pos >= p->len)		// no terminator -> malformed
	return NULL;
    p->pos++;				// skip the NUL
    buf[n] = 0;
    return buf;
}

//
// UDP transport.
//
#ifdef _WIN32
static SOCKET		sock = INVALID_SOCKET;
#else
static int		sock = INVALID_SOCKET;
#endif
static struct sockaddr_in destaddr;

boolean UDP_OpenClient (void)
{
#ifdef _WIN32
    WSADATA wsa;
    static int wsainit;
    if (!wsainit) { if (WSAStartup(MAKEWORD(2,2),&wsa)) return false; wsainit=1; }
#endif
    sock = socket (AF_INET, SOCK_DGRAM, 0);
    return sock != INVALID_SOCKET;
}

void UDP_Close (void)
{
    if (sock != INVALID_SOCKET) { CLOSESOCK (sock); sock = INVALID_SOCKET; }
}

boolean UDP_Resolve (const char* hostport, int default_port)
{
    char		host[256];
    int			port = default_port;
    char*		colon;
    struct addrinfo	hints, *res;

    strncpy (host, hostport, sizeof(host)-1);
    host[sizeof(host)-1] = 0;
    colon = strrchr (host, ':');
    if (colon) { *colon = 0; port = atoi (colon+1); }

    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo (host, NULL, &hints, &res) != 0 || !res)
	return false;

    memcpy (&destaddr, res->ai_addr, sizeof(struct sockaddr_in));
    destaddr.sin_port = htons (port);
    freeaddrinfo (res);
    return true;
}

boolean UDP_Send (net_packet_t* packet)
{
    int n = sendto (sock, (const char*)packet->data, packet->len, 0,
		    (struct sockaddr*)&destaddr, sizeof(destaddr));
    return n == (int)packet->len;
}

net_packet_t* UDP_Recv (int timeout_ms)
{
    fd_set		fds;
    struct timeval	tv;
    byte		buf[2048];
    int			n;

    FD_ZERO (&fds);
    FD_SET (sock, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select (sock+1, &fds, NULL, NULL, &tv) <= 0)
	return NULL;

    n = recvfrom (sock, (char*)buf, sizeof(buf), 0, NULL, NULL);
    if (n <= 0)
	return NULL;

    {
	net_packet_t* p = NET_NewPacket (n);
	memcpy (p->data, buf, n);
	p->len = n;
	p->pos = 0;
	return p;
    }
}

//
// Stage-1 milestone: query a Chocolate/Crispy server and print its info.
//
void I_QueryChocServer (const char* hostport)
{
    net_packet_t*	req;
    net_packet_t*	resp;
    unsigned		type, state, np, maxp, gm, gmis;
    char		version[256];	// NET_ReadString reuses a static buffer,
    char*		s;		//  so copy each string out before the next read
    char*		descr;
    int			tries;

    printf ("Querying %s (Chocolate/Crispy protocol)...\n", hostport);

    if (!UDP_OpenClient ())
    { printf ("  could not open UDP socket\n"); return; }
    if (!UDP_Resolve (hostport, NET_DEFAULT_PORT))
    { printf ("  could not resolve %s\n", hostport); UDP_Close (); return; }

    req = NET_NewPacket (4);
    NET_WriteInt16 (req, NET_PACKET_TYPE_QUERY);

    resp = NULL;
    for (tries = 0 ; tries < 3 && !resp ; tries++)
    {
	UDP_Send (req);
	resp = UDP_Recv (1000);
    }
    NET_FreePacket (req);

    if (!resp)
    { printf ("  no response (server down, firewalled, or wrong port?)\n"); UDP_Close (); return; }

    if (!NET_ReadInt16 (resp, &type) || type != NET_PACKET_TYPE_QUERY_RESPONSE)
    { printf ("  unexpected reply (type=%u)\n", type); NET_FreePacket (resp); UDP_Close (); return; }

    s = NET_ReadString (resp);
    strncpy (version, s ? s : "?", sizeof(version)-1);
    version[sizeof(version)-1] = 0;
    if (!NET_ReadInt8 (resp, &state) || !NET_ReadInt8 (resp, &np)
	|| !NET_ReadInt8 (resp, &maxp) || !NET_ReadInt8 (resp, &gm)
	|| !NET_ReadInt8 (resp, &gmis))
    { printf ("  malformed response\n"); NET_FreePacket (resp); UDP_Close (); return; }
    descr = NET_ReadString (resp);

    printf ("  server: \"%s\"\n", descr ? descr : "?");
    printf ("  version: %s\n", version);
    printf ("  players: %u/%u   state: %u   gamemode: %u   mission: %u\n",
	    np, maxp, state, gm, gmis);
    printf ("  -> wire-format interop OK.\n");

    NET_FreePacket (resp);
    UDP_Close ();
}
