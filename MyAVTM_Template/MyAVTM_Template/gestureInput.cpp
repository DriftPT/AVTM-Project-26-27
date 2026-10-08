#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstdio>

#include "gestureInput.h"

#pragma comment(lib, "ws2_32.lib")

static SOCKET sock = INVALID_SOCKET;

bool gestureInput::init(unsigned short port)
{
	WSADATA w;
	if (WSAStartup(MAKEWORD(2, 2), &w) != 0)
	{
		printf("WSAStartup falhou\n");
		return false;
	}

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
		return false;

	u_long nonBlocking = 1;
	ioctlsocket(sock, FIONBIO, &nonBlocking); // define o socket como não bloqueante

	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);

	if (bind(sock, (sockaddr *)&a, sizeof(a)) == SOCKET_ERROR)
	{
		printf("Falha no bind UDP na porta %d\n", port);
		closesocket(sock);
		sock = INVALID_SOCKET;
		return false;
	}

	printf("A ouvir gestos em UDP 127.0.0.1:%d\n", port);
	return true;
}

GestureCmd gestureInput::poll()
{
	if (sock == INVALID_SOCKET)
		return GestureCmd::None;

	char buf[64];
	int n = recv(sock, buf, sizeof(buf) - 1, 0);
	if (n <= 0)
		return GestureCmd::None; // nada pendente (WSAEWOULDBLOCK) ou erro

	buf[n] = '\0';
	std::string cmd(buf);

	if (cmd == "CAM_NEXT") return GestureCmd::CamNext;
	if (cmd == "CAM_PREV") return GestureCmd::CamPrev;
	if (cmd == "PAUSE")    return GestureCmd::Pause;
	if (cmd == "RESUME")   return GestureCmd::Resume;
	return GestureCmd::None;
}

void gestureInput::shutdown()
{
	if (sock != INVALID_SOCKET)
	{
		closesocket(sock);
		sock = INVALID_SOCKET;
	}
	WSACleanup();
}