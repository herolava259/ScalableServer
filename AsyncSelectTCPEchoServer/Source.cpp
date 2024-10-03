#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include<WinSock2.h>
#include<Windows.h>
#include<iostream>
#include<WS2tcpip.h>
#include<stdio.h>
#include<conio.h>
#include<WinUser.h>
#pragma comment(lib, "Ws2_32.lib")

#define WM_SOCKET WM_USER +1
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 5500
#define MAX_CLIENT 1024
#define BUFF_SIZE 2048
#define MAX_LOADSTRING 100

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);

LRESULT CALLBACK windoeProc(HWND, UINT, WPARAM, LPARAM);

SOCKET client[MAX_CLIENT];
SOCKET listenSock;

const char* clsName = "WSA Async Select TCP Server";
const char* wndName = "WSA Async Select TCP Window";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	MSG msg;
	HWND serverWindow;

	

	MyRegisterClass(hInstance);

	if ((serverWindow = InitInstance(hInstance, nCmdShow)) == NULL)
		return FALSE;

	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);

	if (WSAStartup(wVersion, &wsaData)) {
		MessageBox(serverWindow, "Winsock 2.2 is not supported.", "Error!", MB_OK);
		return 0;
 	}

	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	WSAAsyncSelect(listenSock, serverWindow, WM_SOCKET, FD_ACCEPT | FD_CLOSE | FD_READ);
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
		MessageBox(serverWindow, "Cannot place server socket in state LISTEN.", "Error!", MB_OK);
		return 0;
	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "WindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_WINLOGO);
	return RegisterClassEx(&wcex);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow) {


	HWND hWnd;

	int i;
	for (i = 0; i < MAX_CLIENT; i++) {
		client[i] = 0;
	}
	

	hWnd = CreateWindowEx(WS_EX_CLIENTEDGE,clsName,wndName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
		return FALSE;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	SOCKET connSock;
	sockaddr_in clientAddr;

	int ret, clientAddrLen = sizeof(clientAddr), i;

	char rcvBuff[BUFF_SIZE], sendBuff[BUFF_SIZE];

	switch (message) {
	case WM_SOCKET:
	{
		if (WSAGETSELECTERROR(lParam)) {
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == (SOCKET)wParam) {
					closesocket(client[i]);
					client[i] = 0;
					continue;
				}
			}
		}

		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT: 
		{
			connSock = accept((SOCKET)wParam, (sockaddr*)&clientAddr, &clientAddrLen);
			if (connSock == INVALID_SOCKET) {
				break;
			}
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == 0) {
					client[i] = connSock;

					WSAAsyncSelect(client[i], hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					break;
				}
			}
			if (i == MAX_CLIENT) {
				MessageBox(hWnd, "Too many ckients!", "Notice", MB_OK);
				closesocket(connSock);
			}
			break;
		}

		case FD_READ:
		{
			for (i = 0; i < MAX_CLIENT; i++) {
				if (client[i] == (SOCKET)wParam)
					break;
			}
			ret = recv(client[i], rcvBuff, BUFF_SIZE, 0);
			if (ret > 0) {
				memcpy(sendBuff, rcvBuff, ret);
				send(client[i], sendBuff, ret, 0);
			}
			break;
		}

		case FD_CLOSE:
		{
			for(i = 0; i < MAX_CLIENT; i++)
				if (client[i] == (SOCKET)wParam) {
					closesocket(client[i]);
					client[i] = 0;
					break;
				}
			break;
		}

		}
		break;
	}
	
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
		break;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}