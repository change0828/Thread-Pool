﻿#pragma once

//socket
#ifdef WIN32

#include <WinSock2.h>
#include <WS2tcpip.h>

#ifndef INVALID_SOCKET
#include <winsock2.h>
#endif

#define SHUT_RDWR 2

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket(a) close(a)

#endif

#define SOCKET_OK 0

// thread

#include <thread>
#include <mutex>
#include <condition_variable>

#include <vector>

#include "Package.h"

using namespace std;

class SocketThread
{
public:
	explicit SocketThread(const char * hostname,const char * port,int mTag);
	virtual ~SocketThread(void);

	/**start thread*/
	void startThread();
	/**stop thread*/
	void stopThread();

	/**thread is running or not*/
	bool getIsRunning();
	/**socket is connect or not*/
	bool getIsConnected();
	/**get socket tag*/
	int getTag();

	//cmd action data
	void addToSendBuffer(const char * mData,unsigned int mDataLength,int mHeadType);
private:
	/**server operation ,return 0 if connect is ok,else -1*/
	int connectServer();
	/**handle error when socket has some exceptions*/
	void handleError();
	/**thread functions*/
	void sendThread();
	void recvThread();
private:
	//receive buffer,default size is MAXMSGSIZE
	char * recvBuf;
	//buffer length
	unsigned int bufLength;

	// socket tag to be unique
	int tag;
	//host address
	char host[128];
	//host port number
	char port[16];
	//thread running or not
	bool isRunning;
	//connect or not
	bool isConnect;
	//receive header data ok or not
	bool isReceiveHeaderOK;
	//receive message ok or not
	bool isReceiveOK;

	//send buffer
	vector<CPackage*> sendList;
	//recyle CPackage sended
	vector<CPackage*> recyleList;

	SOCKET _socket;
	//mutex used for thread synchronization
	mutex _mutex;
	// 条件开启发送线程
	std::condition_variable waitNotify; 
	//thread handle
	thread _sendThread;
	thread _recvThread;

};
