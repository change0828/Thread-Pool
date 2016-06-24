#include "SocketThread.h"

#include <string.h>
#include <errno.h>

#include "NetService.h"

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

SocketThread::~SocketThread(void)
{
	//clearup buffer
	if (recvBuf != NULL) {
		delete[] recvBuf;
		recvBuf = NULL;
	}

	for (size_t i=0; i<sendList.size(); i++) {
		CPackage * tmpArray = (CPackage *) sendList.at(i);
		if (tmpArray) {
			delete tmpArray;
			tmpArray = NULL;
		}
	}
	sendList.clear();

	for (size_t i=0; i<recyleList.size(); i++) {
		CPackage * tmpArray = (CPackage *) recyleList.at(i);
		if (tmpArray) {
			delete tmpArray;
			tmpArray = NULL;
		}
	}
	recyleList.clear();

	isRunning		= false;
}

SocketThread::SocketThread(const char * mHost,const char * mPort,int mTag)
	:tag(mTag)
{
	recvBuf = new char[MAXMSGSIZE];
	memset(recvBuf, 0, MAXMSGSIZE);
	bufLength = MAXMSGSIZE;

	isRunning	= false;
	isConnect	= false;
	//handl host info
	memset(host, 0, 128);
	memcpy(host, mHost, strlen(mHost));
	//handl port info
	memset(port, 0, 16);
	memcpy(port, mPort, strlen(mPort));

	_socket=INVALID_SOCKET;
}


void SocketThread::startThread()
{
	isRunning = true;

	_sendThread = thread(&SocketThread::sendThread, this);
	_sendThread.detach();
	_recvThread = thread(&SocketThread::recvThread, this);
	_recvThread.detach();
}

void SocketThread::stopThread()
{
	isRunning = false;
	isReceiveHeaderOK= false;
	isReceiveOK = false;

	//默认情况下，close()/closesocket() 会立即向网络中发送FIN包，不管输出缓冲区中是否还有数据，而shutdown() 会等输出缓冲区中的数据传输完毕再发送FIN包。
	//也就意味着，调用 close()/closesocket() 将丢失输出缓冲区中的数据，而调用 shutdown() 不会。
	//shutdown(_socket, SHUT_RDWR);

	closesocket(_socket);
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

void SocketThread::addToSendBuffer(const char * mData,unsigned int mDataLength,int mHeadType)
{
	CPackage *_readBuffer = nullptr;
	if (!recyleList.empty())
	{
		_readBuffer = (CPackage*)(recyleList.front());
		_readBuffer->reuse();//must set to reuse.
		recyleList.erase(recyleList.begin());;
	}
	else
	{
		_readBuffer = new CPackage(mDataLength + AUGMENT_SIZE_LEN);
	}
	_readBuffer->pushHead(mHeadType);
	_readBuffer->pushDword(mDataLength);
	_readBuffer->copy(mData, mDataLength);

	std::unique_lock<std::mutex> _lock(_mutex);
	sendList.push_back(_readBuffer);

	waitNotify.notify_all();
	_lock.unlock();
}

int SocketThread::connectServer()
{
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


	int ret = getaddrinfo(host, port, &hint, &_addrinfo); 
	if (SOCKET_OK!=ret) { 
		/** 域名解析失败*/
		char buf[SIZE];
		strerror_s(buf, SIZE, errno);
		on_error("域名解析失败 host %s, port %s errorcode：%s\n", \
			host, port, buf);
		return ret;
	} 
	
	int _connect = 0;
	for (curr = _addrinfo; curr != NULL; curr = curr->ai_next) { 
		_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if (SOCKET_ERROR==_socket)
		{
			char buf[SIZE];
			strerror_s(buf, SIZE, errno);
			on_error("域名解析失败 host %s, port %s errorcode：%s\n", \
				host, port, buf);
			continue;
		}
	
		_connect = connect(_socket, curr->ai_addr, curr->ai_addrlen);
		if (0!= _connect)
		{
			char buf[SIZE];
			strerror_s(buf, SIZE, errno);
			on_error("域名解析失败 host %s, port %s errorcode：%s\n", \
				host, port, buf);
			continue;
		}
		isConnect = true;
#ifdef COCOS2D_DEBUG
		printf("socket is : %d", _socket);
#endif
		break;
	}
	
	return _connect;
}

void SocketThread::handleError()
{
	char buf[SIZE];
	strerror_s(buf, SIZE, errno);
	on_error("域名解析失败 host %s, port %s errorcode：%s\n", \
		host, port, buf);

	isRunning	= false;
	isReceiveHeaderOK= false;
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
			}
			///////send error info message //////////
			//break to exit thread
			if (isExitThread) {
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
					this->handleError();
					break;
				}

				sendIndex += _length;
				if (sendIndex > _data->length())
				{
#ifdef COCOS2D_DEBUG
					printf("send data error  _length > data->length()");
#endif
					break;
				}
				else if (sendIndex < _data->length())
				{
#ifdef COCOS2D_DEBUG
					printf("raw_send tag=%d: 数据未发送完成,剩余:%ld\n",tag,(_data->length() - sendIndex));
#endif
				}
				else if (sendIndex == _data->length())
				{
					isSendOK=true;
				}
			}
			//发送成功，清除缓存数据
			if (isSendOK)
			{
#ifdef COCOS2D_DEBUG
				printf("Account send head = %d", (int)_data->getHead());
#endif
				std::unique_lock<std::mutex> _lock(_mutex);
				if (!sendList.empty())
				{
					sendList.erase(sendList.begin());
				}
				recyleList.push_back(_data);
				_lock.unlock();
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
}

void SocketThread::recvThread()
{
	//short comStatus = COM_CONNECTED;
	while (isRunning)
	{
		if (!isConnect)
		{
			//retCode = this->connectServer();
			//if (retCode == -1)
			//{
			//}
			continue;
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
				this->handleError();
				break;
			}
			curHeaderDataIndex += rev;
			if (HEADER_BUFFER_SIZE - curHeaderDataIndex == 0) {
				isReceiveHeaderOK = true;
			}
			else
			{
#ifdef COCOS2D_DEBUG
				printf("---->conn thread tag=%d receive header left length =%ld\n",tag, HEADER_BUFFER_SIZE - curHeaderDataIndex);
#endif
			}
		}
		if (!isReceiveHeaderOK) {
#ifdef COCOS2D_DEBUG
			//printf("---->conn thread tag=%d receive header unsuccess \n",tag);
#endif
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

#ifdef COCOS2D_DEBUG
				printf("==conn thread tag=%d realloc length =%d\n",tag,bufLength);
#endif
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
}