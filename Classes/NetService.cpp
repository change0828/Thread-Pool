#include "NetService.h"
#include "SocketThread.h"

#include <string.h>

using namespace std;

CmdHandleDelegate::CmdHandleDelegate()
{
}

CmdHandleDelegate::~CmdHandleDelegate()
{
}

void CmdHandleDelegate::setReceiveCmd()
{
	isReceiveCmd = true;
}

void CmdHandleDelegate::IntoDealNetCommand(void)
{
	NetService::getInstance()->addDelegate(this);
}

void CmdHandleDelegate::LeaveNetCommand(void)
{
	NetService::getInstance()->removeDelegate(this);
}

bool CmdHandleDelegate::cmdHandle(CPackage * mCmd)
{
	return false;
}

bool CmdHandleDelegate::notifyResponseState(CPackage * mCmd)
{
	return false;
}


void CmdHandleDelegate::setBlockCmd()
{
	isReceiveCmd = false;
}

NetService *NetService::_instance = nullptr;

NetService* NetService::getInstance()
{
	if (nullptr==_instance)
	{
		_instance = new NetService();
	}
	return _instance;
}

void NetService::purge()
{
	if (nullptr!=_instance)
	{
		delete _instance;
		_instance = nullptr;
	}
}

NetService::NetService(void)
{
}

NetService::~NetService(void)
{
	//_cleanNet();
}

void NetService::newSocket(const char *hostname, const char* port, int mTag)
{
	SocketThread* _Socket = this->getSocketByTag(mTag);
	if (nullptr==_Socket)
	{
		_Socket = new SocketThread(hostname, port, mTag);
		_Socket->startThread();

		sokcetArray.push_back(_Socket);
	}
}

SocketThread* NetService::getSocketByTag(int mTag)
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

void NetService::addSocket(SocketThread * mSocket,int mTag)
{
	if (this->getSocketByTag(mTag) == NULL) {
		sokcetArray.push_back(mSocket);
	}
}

void NetService::removeSocket(int mTag)
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

void NetService::clearSockets()
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

void NetService::sendData(CPackage *mData)
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
