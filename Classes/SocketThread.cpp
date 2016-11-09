#include "SocketThread.h"
#include "NetService.h"

#include <string.h>
#include <errno.h>
#include <iostream>
#include <locale>
#include <codecvt>

#define SIZE 100

extern int on_log(const char *format, ...);
extern char * sock_ntop( const  struct  sockaddr *sa, socklen_t salen);

SocketThread::SocketThread(const char * hostname, const char * ip, const char * port, int mTag)
	:tag(mTag)
    , bufLength(MAXMSGSIZE)
    , isRunning(false)
    , isReceiveHeaderOK(false)
    , isReceiveOK(false)
	, isSendOver(false)
	, isRecvOver(false)
    , _socket(INVALID_SOCKET)
{
	_closeSocketByUser.clear();
	recvBuf = new char[bufLength];
	memset(recvBuf, 0, bufLength);

	memset(_ipaddr, 0, 128);
	receiveItem = new CPackage(256);
	//handl host info
	memset(_hostname, 0, 128);
	memcpy(_hostname, hostname, strlen(hostname));
	//handl ip info
	memset(_ip, 0, 128);
	memcpy(_ip, ip, strlen(ip));
	//handl port info
	memset(_port, 0, 16);
	memcpy(_port, port, strlen(port));
}

SocketThread::~SocketThread(void)
{
	//clearup buffer
	if (recvBuf != NULL) {
		delete[] recvBuf;
		recvBuf = NULL;
	}
	//delete receive item
	if (receiveItem) {
		delete receiveItem;
		receiveItem = NULL;
	}

	auto data = sendList.try_pop();
	while (data)
	{
		data.reset();
		data = sendList.try_pop();
	}
	data = recyleList.try_pop();
	while (data)
	{
		data.reset();
		data = recyleList.try_pop();
	}

	isRunning = false;
}

void SocketThread::startThread()
{
	isRunning = true;

	_connectTask = std::packaged_task<int()>(std::bind(&SocketThread::connectServer, this));
	_connect = _connectTask.get_future();
	_sendThread = thread(&SocketThread::sendThread, this);
	_sendThread.detach();
	_recvThread = thread(&SocketThread::recvThread, this);
	_recvThread.detach();
}

void SocketThread::stopThread()
{
	std::unique_lock<std::mutex> _lock(_mutex);
	isRunning = false;
	isReceiveHeaderOK = false;
	isReceiveOK = false;
	_closeSocketByUser.test_and_set();

	receiveItem->reuse();
	sendList.push_back(*receiveItem);
	//默认情况下，close()/closesocket() 会立即向网络中发送FIN包，不管输出缓冲区中是否还有数据，而shutdown() 会等输出缓冲区中的数据传输完毕再发送FIN包。
	//也就意味着，调用 close()/closesocket() 将丢失输出缓冲区中的数据，而调用 shutdown() 不会。
	//shutdown(_socket, SHUT_RDWR);

	closesocket(_socket);
	_socket=SOCKET_ERROR;
}

bool SocketThread::getIsRunning()
{
	return isRunning;
};

int SocketThread::getTag()
{
	return tag;
};

bool SocketThread::isThreadOver()
{
	return (isSendOver & isRecvOver);
}

void SocketThread::addToSendBuffer(const char * mData,unsigned int mDataLength,int mHeadType)
{
	std::shared_ptr<CPackage>_readBuffer = nullptr;

	if (isSendOver || isRecvOver)
	{
		return;
	}

	if (!recyleList.empty())
	{
		//printf("\n before -- recyleList size is %d ", recyleList.size());
		_readBuffer = recyleList.try_pop();
		_readBuffer->reuse();//must set to reuse.
		//printf("\n -- erase recyleList size is %d ", recyleList.size());
	}
	if (nullptr == _readBuffer)
	{
		_readBuffer = std::make_shared<CPackage>(mDataLength + AUGMENT_SIZE_LEN);// new CPackage(mDataLength + AUGMENT_SIZE_LEN);
	}

	_readBuffer->pushHead(mHeadType);
	_readBuffer->pushDword(mDataLength+WORD_SIZE);
	_readBuffer->pushWord(mHeadType);
	_readBuffer->copy(mData, mDataLength);

	sendList.push_back(*_readBuffer.get());
	std::cout << "count _readBuffer:" << _readBuffer.use_count() << std::endl;
}

int SocketThread::connectServer()
{
	if (INVALID_SOCKET!=_socket)
	{
		closesocket(_socket);
		_socket=INVALID_SOCKET;
	}
	struct addrinfo hint, *curr; 
	addrinfo *_addrinfo = nullptr;

	const char* addr[2] = {_hostname, _ip};

	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_UNSPEC;		//
	hint.ai_socktype = SOCK_STREAM; //数据流

	int i = 0;
	do 
	{
		on_log("域名解析 host %s, port %s\n", addr[i], _port);
		int ret = getaddrinfo(addr[i], _port, &hint, &_addrinfo);
		if (SOCKET_OK != ret) {
			/** 域名解析失败*/
			on_log("域名解析失败 host %s, port %s\n", \
				addr[i], _port);
			//return ret;
			this->pushNetError(COM_DNS_ERROR, gai_strerrorA(ret), ret, addr[i], _addrinfo);
			freeaddrinfo(_addrinfo);
			continue;
		}
		else
			break;
	} while (++i<2);


	int _connect = -1;
	struct sockaddr_in  *sockaddr_ipv4 = nullptr;
	struct sockaddr_in6 *sockaddr_ipv6 = nullptr; 
	for (curr = _addrinfo; curr != nullptr; curr = curr->ai_next)
	{
		if((_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol))<0)
			continue;

		//IP 地址
		if (AF_UNSPEC == curr->ai_family)
		{
		} 
		else if (AF_INET == curr->ai_family)
		{
			sockaddr_ipv4 = reinterpret_cast<struct sockaddr_in *>( curr->ai_addr); 
#if CC_PLATFORM_WP8!=CC_TARGET_PLATFORM
			inet_ntop(curr->ai_family, &sockaddr_ipv4->sin_addr, _ipaddr, sizeof(_ipaddr));
#else
			inet_ntoa((in_addr)sockaddr_ipv4->sin_addr);
#endif
		}
		else if (AF_INET6 == curr->ai_family)
		{
			sockaddr_ipv6 = reinterpret_cast<struct sockaddr_in6 *>( curr->ai_addr); 
#if CC_PLATFORM_WP8!=CC_TARGET_PLATFORM
			inet_ntop(curr->ai_family, &sockaddr_ipv6->sin6_addr, _ipaddr, sizeof(_ipaddr));
#endif
		}
#if CC_PLATFORM_WP8!=CC_TARGET_PLATFORM
		on_log("\t ip :%s\n", _ipaddr);
#endif


		if((_connect = connect(_socket, curr->ai_addr, (int)curr->ai_addrlen))<0)
		{
			this->pushNetError(COM_CONNECT_FAILED, sock_ntop(curr->ai_addr,curr->ai_addrlen));
			on_log("trying %s failiure \n",sock_ntop(curr->ai_addr,curr->ai_addrlen));
			closesocket(_socket);
			continue;
		}else
			break;
	};

    freeaddrinfo(_addrinfo);
	on_log("connectServer host %s ip %s port %s", _hostname, _ip, _port);
	
	return _connect;
}

void SocketThread::handleError()
{
	if (!_closeSocketByUser.test_and_set())
	{
		receiveItem->reuse();
		receiveItem->setTag(tag);
		receiveItem->pushHead(COM_TCP);
		receiveItem->pushWord(COM_TCP);
		NetService::getInstance()->pushCmd(receiveItem->buff(), receiveItem->length(), COM_TCP, COM_TCP, tag, COM_SYS_ERROR);
		isRunning = false;
		isReceiveHeaderOK = false;
		isReceiveOK = false;
	}
}

void SocketThread::sendThread()
{
	_connectTask();
	auto localConnect = _connect;
	auto result_connect = localConnect.get();
	if (0==result_connect)
	{
		while (isRunning)
		{
			/////////////////////////////////////////// send start //////////////////////////////////////////

			bool isSendOK = false;
			long sendIndex = 0;
			int _length = 0;

			//线程等待获取发送数据
			auto data = sendList.wait_and_pop();
			if (!isRunning)
			{
				break;
			}

			while (!isSendOK)
			{

				_length = ::send(_socket, data->buff(), data->length(), 0);

				if (SOCKET_ERROR == _length)
				{
					on_log("发送失败 tag %d, head %d errorcode：%d\n", \
						tag, data->getHead(), _length);

					this->handleError();
					break;
				}

				sendIndex += _length;
				if (sendIndex > data->length())
				{
					on_log("send data error  _length > data->length()");
					break;
				}
				else if (sendIndex < data->length())
				{
					on_log("raw_send tag=%d: 数据未发送完成,剩余:%ld\n", tag, (data->length() - sendIndex));
				}
				else if (sendIndex == data->length())
				{
					isSendOK = true;
				}
			}
			//发送成功，清除缓存数据
			if (isSendOK)
			{
				on_log("\nsend head = %d, size = %d", (int)data->readHead(), (int)data->readDword());

				//printf("\n before ++ recyleList size is %d ", recyleList.size());
				recyleList.push_back(*data);
				//printf("\n ++ recyleList size is %d ", recyleList.size());
			}
			else
			{
				sendList.push_back(*data);
			}
			/////////////////////////////////////////// send end OK //////////////////////////////////////////
		}
	}

	isSendOver = true;
}

void SocketThread::recvThread()
{
	auto localConnect = _connect;
	localConnect.wait();
	auto result_connect = localConnect.get();
	if (0==result_connect)
	{
		while (isRunning)
		{
			/////////////////////////////////////// receive header data ///////////////////////////////////////
			const int HEADER_BUFFER_SIZE = 4;
			char headerBuffer[HEADER_BUFFER_SIZE];
			memset(headerBuffer, '\0', HEADER_BUFFER_SIZE);

			int rev = 0;
			int curHeaderDataIndex = 0;
			isReceiveHeaderOK = false;

			///////////// receive header data ///////////////
			while (!isReceiveHeaderOK)
			{

				rev = ::recv(_socket, headerBuffer + curHeaderDataIndex, HEADER_BUFFER_SIZE, 0);

				if (SOCKET_ERROR == rev || 0 == rev)// 服务器中断
				{
					on_log("ReceiveHeader失败 tag %d, errorcode：%d\n", \
						tag, rev);
					this->handleError();
					break;
				}
				curHeaderDataIndex += rev;
				if (HEADER_BUFFER_SIZE - curHeaderDataIndex == 0) {
					isReceiveHeaderOK = true;
				}
				else
				{
					on_log("---->conn thread tag=%d receive header left length =%ld\n", tag, HEADER_BUFFER_SIZE - curHeaderDataIndex);
				}
			}
			if (!isReceiveHeaderOK) {
				on_log("---->conn thread tag=%d receive header unsuccess \n", tag);
				continue; // continue for outer while loop /////
			}
			/////// receive header OK ////////////
			unsigned int dataLength = ntohl(*((int *)headerBuffer));
			if (dataLength > 0)
			{
				memset(recvBuf, 0, bufLength);
				if (dataLength > bufLength)
				{
					bufLength = dataLength + 16;
					char * swapBuf = new char[bufLength];
					memset(swapBuf, 0, bufLength);

					//release recv buffer
					delete[] recvBuf;
					recvBuf = NULL;

					//repoint to new buffer
					recvBuf = swapBuf;

					on_log("==conn thread tag=%d realloc length =%d\n", tag, bufLength);
				}

				rev = 0;
				isReceiveOK = false;
				int receivePackageIndex = 0;
				while (!isReceiveOK)
				{
					// 接收包的长度
					rev = ::recv(_socket, recvBuf + receivePackageIndex, dataLength, 0);

					if (SOCKET_ERROR == rev || 0 == rev)
					{
						on_log("Receive package 失败 tag %d, dataLength %d, errorcode：%d\n", \
							tag, dataLength, rev);
						this->handleError();
						break;
					}

					receivePackageIndex += rev;

					dataLength -= rev;


					if (dataLength == 0)
					{
						isReceiveOK = true;
					}
				}
				if (isReceiveOK)
				{
					NetService::getInstance()->pushCmd(recvBuf, receivePackageIndex, 0, ntohs(*(type_word*)(recvBuf)), 0, COM_OK);
				}
				else
				{
					NetService::getInstance()->pushCmd(recvBuf, receivePackageIndex, 0, ntohs(*(type_word*)(recvBuf)), 0, COM_ERROR);
				}
			}
			else if (0 == dataLength)
			{
				memset(recvBuf, 0, bufLength);
			}

			/////////////////////////////////////// receive header ok //////////////////////////////////////////
		}
	}
	isRecvOver = true;
}

void SocketThread::pushNetError(int COM_STATUS, char * errorinfo/* =nullptr */, int error/* =0 */, const char * host/* =nullptr */, void * pointPath /* = nullptr */)
{
	///////send error info message //////////
	receiveItem->reuse();//must set to reuse.

	receiveItem->setTag(tag);
	receiveItem->pushHead(COM_TCP);

	std::string strerror(errorinfo);

	receiveItem->pushWord(1);//client 填充兼容
	receiveItem->pushDword(strerror.length());
	receiveItem->pushByte(strerror.c_str(), strerror.length());
	receiveItem->pushDword(error);
	receiveItem->pushByte(host, 128);
	NetService::getInstance()->pushCmd(receiveItem->buff(), receiveItem->length(), COM_TCP, COM_TCP, tag, COM_STATUS);
}
