#pragma once

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
#include <future>
#include "threadsafe_list.h"
#include "Package.h"

using namespace std;

class SocketThread
{
public:
	SocketThread(const char * hostname,const char * port,int mTag);
	~SocketThread(void);

	/**start thread*/
	void startThread();
	/**stop thread*/
	void stopThread();

	/**thread is running or not*/
	bool getIsRunning();
	/**get socket tag*/
	int getTag();
	/**is thread over*/
	bool isThreadOver();

	//cmd action data
	void addToSendBuffer(const char * mData, unsigned int mDataLength, int mHeadType);
private:
	SocketThread();
	/**server operation ,return ture if connect is ok,else false*/
	int connectServer();
	/**handle error when socket has some exceptions*/
	void handleError();
	/**thread functions*/
	void sendThread();
	void recvThread();
private:
	//receive buffer,default size is MAXMSGSIZE
	char * recvBuf;
	char	_ipaddr[128];
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
	//receive header data ok or not
	bool isReceiveHeaderOK;
	//receive message ok or not
	bool isReceiveOK;
	//多线程判断链接状态
	std::shared_future<int> _connect;
	std::packaged_task<int()> _connectTask;

	//close socket by user.
	std::atomic_flag _closeSocketByUser;
	//send buffer
	threadsafe_list<CPackage> sendList;
	//recyle CPackage sended
	threadsafe_list<CPackage> recyleList;
	//message item
	CPackage * receiveItem;

	SOCKET _socket;
	//mutex used for thread synchronization
	mutex _mutex;
	//thread handle
	thread _sendThread;
	thread _recvThread;

	bool isSendOver;
	bool isRecvOver;
};

