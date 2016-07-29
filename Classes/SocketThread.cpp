#include "SocketThread.h"

#include <string.h>
#include <errno.h>
#include "NetService.h"
#include "cocos2d.h"

#define SIZE 100
/*
 * 打印错误信息到错误缓冲区(ut_error_message)，私有函数
 */
static int on_error(const char *format, ...)
{
	int bytes_written;
	va_list arg_ptr;
	
	va_start(arg_ptr, format);

	bytes_written = vfprintf(stderr, format, arg_ptr);
	
	va_end(arg_ptr);

	return bytes_written;
}

SocketThread::SocketThread(const char * mHost,const char * mPort,int mTag)
	:tag(mTag)
    , bufLength(MAXMSGSIZE)
    , isRunning(false)
    , isConnect(false)
    , isReceiveHeaderOK(false)
    , isReceiveOK(false)
	, closeSocketByUser(false)
	, isSendOver(false)
	, isRecvOver(false)
    , _socket(INVALID_SOCKET)
{
	recvBuf = new char[bufLength];
	memset(recvBuf, 0, bufLength);

	receiveItem = new CPackage(256);
	//handl host info
	memset(host, 0, 128);
	memcpy(host, mHost, strlen(mHost));
	//handl port info
	memset(port, 0, 16);
	memcpy(port, mPort, strlen(mPort));
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

	for (size_t i = 0; i < sendList.size(); i++) {
		CPackage * tmpArray = (CPackage *)sendList.at(i);
		if (tmpArray) {
			delete tmpArray;
			tmpArray = NULL;
		}
	}
	sendList.clear();

	for (size_t i = 0; i < recyleList.size(); i++) {
		CPackage * tmpArray = (CPackage *)recyleList.at(i);
		if (tmpArray) {
			delete tmpArray;
			tmpArray = NULL;
		}
	}
	recyleList.clear();

	isRunning = false;
}

void SocketThread::startThread()
{
	isRunning = true;

	this->connectServer();
	_sendThread = thread(&SocketThread::sendThread, this);
	_sendThread.detach();
	_recvThread = thread(&SocketThread::recvThread, this);
	_recvThread.detach();
}

void SocketThread::stopThread()
{
	//std::unique_lock<std::mutex> _lock(_mutex);
	closeSocketByUser = true;
	isRunning = false;
	isReceiveHeaderOK = false;
	isReceiveOK = false;
	isConnect = false;

	//发送线程可能等待通知
	waitNotify.notify_all();

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

bool SocketThread::getIsConnected()
{
	return isConnect;
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
	CPackage *_readBuffer = nullptr;

	std::unique_lock<std::mutex> _lock(_mutex);

	if (isSendOver || isRecvOver)
	{
		return;
	}

	if (!recyleList.empty())
	{
		//printf("\n before -- recyleList size is %d ", recyleList.size());
		_readBuffer = (CPackage*)(recyleList.front());
		_readBuffer->reuse();//must set to reuse.
		recyleList.erase(recyleList.begin());
		//printf("\n -- erase recyleList size is %d ", recyleList.size());

	}
	else
	{
		_readBuffer = new CPackage(mDataLength + AUGMENT_SIZE_LEN);
	}
	_readBuffer->pushHead(mHeadType);
	_readBuffer->pushDword(mDataLength+WORD_SIZE);
	_readBuffer->pushWord(mHeadType);
	_readBuffer->copy(mData, mDataLength);

	sendList.push_back(_readBuffer);
	_lock.unlock();

	waitNotify.notify_all();
}

int SocketThread::connectServer()
{
	std::unique_lock<std::mutex> _lock(_mutex);
	if (isConnect)
	{
		return 0;
	}
	if (INVALID_SOCKET!=_socket)
	{
		closesocket(_socket);
		_socket=INVALID_SOCKET;
	}
	struct addrinfo hint, *curr; 
	addrinfo *_addrinfo=nullptr;

	memset(&hint, 0, sizeof(hint)); 
	hint.ai_flags = AI_CANONNAME;	//获取域名
	hint.ai_family = AF_UNSPEC;		//
	hint.ai_socktype = SOCK_STREAM; //数据流
	CCLOG("域名解析 host %s, port %s\n",host,port);
	int ret = getaddrinfo(host, port, &hint, &_addrinfo); 
	if (SOCKET_OK!=ret) { 
		/** 域名解析失败*/
		CCLOG("域名解析失败 host %s, port %s\n", \
			host, port);
		return ret;
	} 
	int _connect = 0;
	struct sockaddr_in  *sockaddr_ipv4 = nullptr; 
	struct sockaddr_in6 *sockaddr_ipv6 = nullptr; 
	for (curr = _addrinfo; curr != NULL; curr = curr->ai_next) { 
		_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (SOCKET_ERROR==_socket)
		{
			CCLOG("创建套接字失败 host %s, port %s errorcode：%d\n", \
				host, port, _socket);
			continue;
		}

		CCLOG("socket is : %d", _socket);
		_connect = connect(_socket, curr->ai_addr, (int)curr->ai_addrlen);
		if (0!= _connect)
		{
			CCLOG("连接失败 host %s, port %s errorcode：%d\n", \
				host, port, _connect);
			continue;
		}
		isConnect = true;
		CCLOG("连接成功");
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
		CCLOG("analysis IP：%s\n",_ipaddr);
		break;
    }
    freeaddrinfo(_addrinfo);
	CCLOG("connectServer host %s port %s", host, port);
	
	return _connect;
}

void SocketThread::handleError()
{
	std::unique_lock<std::mutex> _lock(_mutex);
	if (!closeSocketByUser)
	{
		receiveItem->reuse();
		receiveItem->setTag(tag);
		receiveItem->pushHead(COM_TCP);
		receiveItem->pushWord(COM_TCP);
		NetService::getInstance()->pushCmd(receiveItem->buff(),receiveItem->length(),COM_TCP,COM_TCP,tag,COM_SYS_ERROR);
	}
	closeSocketByUser	= true;
	isRunning	= false;
	isReceiveHeaderOK	= false;
	isReceiveOK = false;
}

void SocketThread::sendThread()
{
	bool isExitThread = false;
	short comStatus = COM_CONNECTED;
	int retCode;

	while (isRunning)
	{
		if (!isConnect)
		{
			retCode = this->connectServer();
			if (retCode == -1) {
				comStatus = COM_CONNECT_FAILED ;
			}
			/////////////////////////////////////////
			if (comStatus == COM_CONNECT_FAILED)
			{
				isExitThread = true;

				///////send error info message //////////
				receiveItem->reuse();//must set to reuse.

				receiveItem->setTag(tag);
				receiveItem->pushHead(COM_TCP);

				receiveItem->pushDword(retCode);
				receiveItem->pushByte(host,128);
				receiveItem->pushByte(port,16);
				receiveItem->pushWord(tag);
				NetService::getInstance()->pushCmd(receiveItem->buff(),receiveItem->length(),COM_TCP,COM_TCP,tag,comStatus);

				break;// exit thread
			}
		}
/////////////////////////////////////////// send start ///////////////////////////////////////////
		if (sendList.size() > 0)
		{
			bool isSendOK = false;
			int sendIndex = 0;
			int _length = 0;

			//线程等待获取发送数据
			std::unique_lock<std::mutex> _lock(_mutex);
			CPackage* _data = *sendList.begin();
			_lock.unlock();

			while (!isSendOK)
			{

				_length = ::send(_socket, _data->buff(), _data->length(), 0);
				
				if (SOCKET_ERROR==_length)
				{
					CCLOG("发送失败 tag %d, head %d errorcode：%d\n", \
						tag, _data->getHead(), _length);

					this->handleError();
					break;
				}

				sendIndex += _length;
				if (sendIndex > _data->length())
				{
					CCLOG("send data error  _length > data->length()");
					break;
				}
				else if (sendIndex < _data->length())
				{
					CCLOG("raw_send tag=%d: 数据未发送完成,剩余:%ld\n",tag,(_data->length() - sendIndex));
				}
				else if (sendIndex == _data->length())
				{
					isSendOK=true;
				}
			}
			//发送成功，清除缓存数据
			if (isSendOK)
			{
				CCLOG("\nsend head = %d, size = %d", (int)_data->readHead(), (int)_data->readDword());

				std::unique_lock<std::mutex> _lock(_mutex);
				if (!sendList.empty())
				{
					sendList.erase(sendList.begin());
				}
				//printf("\n before ++ recyleList size is %d ", recyleList.size());
				recyleList.push_back(_data);
				//printf("\n ++ recyleList size is %d ", recyleList.size());
			}
			else
			{
			}
		}
		else
		{
			std::unique_lock<std::mutex> _lock(_mutex);
			waitNotify.wait(_lock);
		}
/////////////////////////////////////////// send end OK //////////////////////////////////////////
	}
	if (isExitThread)
	{
		NetService::getInstance()->removeSocket(tag);
	}
	isSendOver = true;
}

void SocketThread::recvThread()
{
	bool isExitThread = false;
	short comStatus = COM_CONNECTED;
	int retCode;

	while (isRunning)
	{
		if (!isConnect)
		{
			retCode = this->connectServer();
			if (retCode == -1) {
				comStatus = COM_CONNECT_FAILED ;
			}
			/////////////////////////////////////////
			if (comStatus == COM_CONNECT_FAILED)
			{
				isExitThread = true;

				///////send error info message //////////
				receiveItem->reuse();//must set to reuse.

				receiveItem->setTag(tag);
				receiveItem->pushHead(COM_TCP);

				receiveItem->pushDword(retCode);
				receiveItem->pushByte(host,128);
				receiveItem->pushByte(port,16);
				receiveItem->pushWord(tag);
				NetService::getInstance()->pushCmd(receiveItem->buff(),receiveItem->length(),COM_TCP,COM_TCP,tag,comStatus);

				break;// exit thread
			}
		}

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

			rev = ::recv(_socket, headerBuffer+curHeaderDataIndex, HEADER_BUFFER_SIZE, 0);

			if (SOCKET_ERROR == rev || 0 == rev)// 服务器中断
			{
				CCLOG("ReceiveHeader失败 tag %d, errorcode：%d\n", \
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
				CCLOG("---->conn thread tag=%d receive header left length =%ld\n",tag, HEADER_BUFFER_SIZE - curHeaderDataIndex);
			}
		}
		if (!isReceiveHeaderOK) {
			CCLOG("---->conn thread tag=%d receive header unsuccess \n",tag);
			continue; // continue for outer while loop /////
		}
		/////// receive header OK ////////////
		unsigned int dataLength = ntohl(*((int *)headerBuffer));
		if (dataLength>0)
		{
			memset(recvBuf, 0 , bufLength);
			if (dataLength>bufLength)
			{
				bufLength = dataLength + 16;
				char * swapBuf = new char[bufLength];
				memset(swapBuf, 0, bufLength);

				//release recv buffer
				delete [] recvBuf;
				recvBuf = NULL;

				//repoint to new buffer
				recvBuf = swapBuf;

				CCLOG("==conn thread tag=%d realloc length =%d\n",tag,bufLength);
			}

			rev = 0;
			isReceiveOK=false;
			int receivePackageIndex = 0;
			while (!isReceiveOK)
			{
				// 接收包的长度
				rev = ::recv(_socket, recvBuf+receivePackageIndex, dataLength, 0);

				if (SOCKET_ERROR == rev || 0 == rev)
				{
					CCLOG("ReceiveHeader失败 tag %d, dataLength %d, errorcode：%d\n", \
						tag, dataLength, rev);
					this->handleError();
					break;
				}

				receivePackageIndex += rev;

				dataLength -= rev;


				if (dataLength==0)
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

			}
		}
		else if (0==dataLength)
		{
			memset(recvBuf, 0, bufLength);
		}

/////////////////////////////////////// receive header ok //////////////////////////////////////////
	}
	isRecvOver = true;
}