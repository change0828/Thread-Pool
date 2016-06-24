#include "NetServer.h"

#include <string.h>


using namespace std;


NetServer *NetServer::_instance = nullptr;

NetServer* NetServer::getInstance()
{
	if (nullptr==_instance)
	{
		_instance = new NetServer();
	}
	return _instance;
}

void NetServer::purge()
{
	if (nullptr!=_instance)
	{
		delete _instance;
		_instance = nullptr;
	}
}

NetServer::NetServer(void)
{
}

NetServer::~NetServer(void)
{
	_cleanNet();
}

void NetServer::newSocket(const char *hostname, const char* port, int mTag)
{
	SocketThread* _Socket = this->getSocketByTag(mTag);
	if (nullptr==_Socket)
	{
		_Socket = new SocketThread(hostname, port, mTag);
		_Socket->startThread();

		sokcetArray.push_back(_Socket);
	}
}

SocketThread* NetServer::getSocketByTag(int mTag)
{
	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end(); itr++)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT && _SocketT->getTag() == mTag)
		{
			return _SocketT;
		}
	}
	return nullptr;
}

void NetServer::addSocket(SocketThread * mSocket,int mTag)
{
	if (this->getSocketByTag(mTag) == NULL) {
		sokcetArray.push_back(mSocket);
	}
}

void NetServer::removeSocket(int mTag)
{
	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end(); itr++)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT && _SocketT->getTag() == mTag)
		{
			_SocketT->stopThread();

			delete _SocketT;
			_SocketT = nullptr;

			sokcetArray.erase(itr);

#ifdef COCOS2D_DEBUG
			printf("socket tag=%d remove from NetService\n",mTag);
#endif
			break;
		}
	}
}

void NetServer::clearSockets()
{
	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end(); itr++)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT)
		{
			_SocketT->stopThread();

			delete _SocketT;
			_SocketT = nullptr;
		}
	}

	sokcetArray.clear();
}

void NetServer::sendData(CPackage *mData)
{
	int _tag = mData->getTag();

	SocketThread *_socket = this->getSocketByTag(_tag);
	if (nullptr!=_socket && _socket->getIsConnected())
	{
		_socket->addToSendBuffer(mData->buff(), mData->length(), mData->getHead());
	}
	else
	{
#ifdef COCOS2D_DEBUG
		printf("send command failed ,no valid socket item or not connect \n");
#endif
	}
}
