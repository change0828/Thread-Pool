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

		for (short i = 0; i < 10; i++)
		{
			CPackage * tmpCmd = new CPackage(512);
			_instance->recyleBuffer.push_back(tmpCmd);
		}
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

void NetService::addDelegate(CmdHandleDelegate* mDelegate)
{
	std::unique_lock<std::mutex> _lock(_mutex);

	mDelegate->setReceiveCmd();
	delegateArray.push_back(mDelegate);
#ifdef COCOS2D_DEBUG
	printf("+++++ add to operateArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
#endif
}

void NetService::removeDelegate(CmdHandleDelegate * mDelegate)
{
	std::unique_lock<std::mutex> _lock(_mutex);

	mDelegate->setBlockCmd();


	vector<CmdHandleDelegate*>::iterator itr = delegateArray.begin();
	for (; itr != delegateArray.end();)
	{
		if (mDelegate == (*itr))
		{
			itr = delegateArray.erase(itr);
			break;
		}
		else {
			itr++;
		}
	}
#ifdef COCOS2D_DEBUG
	printf("---- remove to removedArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
#endif
}

void NetService::removeAllDelegates(void)
{
	std::unique_lock<std::mutex> _lock(_mutex);
	if (delegateArray.size() > 0)
		delegateArray.clear();
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

void NetService::handleThread()
{
	while (running)
	{
		// 处理第一条指令.
		CPackage *readCmd = NULL;
		readCmd = this->popCmd();
		if (readCmd != NULL)
		{
			bool isCmdHandled = this->handleDelegates(readCmd);
			if (isCmdHandled == false) {
				std::unique_lock<std::mutex> _lock(_mutex);
				cmdVector.insert(cmdVector.begin(), readCmd);
			}
			else {
				recyleBuffer.push_back(readCmd);
			}
		}
	}
}

bool NetService::handleDelegates(CPackage *mCmd)
{
	bool isCmdHandled = false;

	std::unique_lock<std::mutex> _lock(_mutex);
	vector<CmdHandleDelegate*>::iterator itr = delegateArray.begin();
	for (; itr != delegateArray.end(); itr++)
	{
		CmdHandleDelegate * object = (*itr);

		if (mCmd->getStatus() == COM_OK)
		{
			isCmdHandled = object->cmdHandle(mCmd);

			if (isCmdHandled)
			{
				break;
			}
		}
		else if (mCmd->getStatus() == COM_ERROR)
		{
			isCmdHandled = object->notifyResponseState(mCmd);

			if (isCmdHandled)
			{
				break;
			}
		}
	}

	return isCmdHandled;
}

void NetService::pushCmd(const char * mData, int mDataLength, int mCmdType, int mActionType, int mTag, char mStatus)
{
	std::unique_lock<std::mutex> _lock(_mutex);

	if (mData != NULL)
	{
		if (mDataLength >= 0) {
			CPackage * readedCmd = NULL;
			if (recyleBuffer.empty() == false)
			{
				readedCmd = (CPackage *)(recyleBuffer.front());
				readedCmd->reuse();//must set to reuse.
				recyleBuffer.erase(recyleBuffer.begin());
#ifdef COCOS2D_DEBUG
				printf("reuse message =%p \n", readedCmd);
#endif
			}
			else
			{
				readedCmd = new CPackage(mDataLength + 2);
			}

			if (mActionType != 0) {
				//readedCmd->setCmdType(mCmdType);
				readedCmd->pushHead(mActionType);
				readedCmd->setTag(mTag);
				readedCmd->setStatus(mStatus); // cmd status. 1 = success, 0 = false
				readedCmd->copy(mData, mDataLength);
				cmdVector.push_back(readedCmd);
#ifdef COCOS2D_DEBUG
				printf("ActionID =%d type=%d,status =%d,data length=%d \n", mCmdType, mActionType, mStatus, mDataLength);
#endif
			}
			else if (mActionType == 0)
			{
#ifdef COCOS2D_DEBUG
				printf("heart beat ActionID =%d ,data length=%d \n", mCmdType, mDataLength);
#endif
			}
		}
	}
}

void NetService::sendCmd(CPackage * mCmd)
{
	int _tag = mCmd->getTag();

	SocketThread *_socket = this->getSocketByTag(_tag);
	if (nullptr != _socket && _socket->getIsConnected())
	{
		_socket->addToSendBuffer(mCmd->buff(), mCmd->length(), mCmd->getHead());
	}
	else
	{
#ifdef COCOS2D_DEBUG
		printf("send command failed ,no valid socket item or not connect \n");
#endif
}
}

CPackage * NetService::popCmd()
{
	std::unique_lock<std::mutex> _lock(_mutex);

	CPackage * mCmd = NULL;
	if (cmdVector.empty() == false)
	{
		mCmd = (CPackage *)(cmdVector.front());
		cmdVector.erase(cmdVector.begin());
	}

	return mCmd;
}

void NetService::clearCmdVector()
{
	std::unique_lock<std::mutex> _lock(_mutex);

	for (vector<CPackage*>::iterator itr = cmdVector.begin(); itr != cmdVector.end(); itr++)
	{
		if ((*itr) != NULL)
		{
			CPackage* cmd = *itr;
			delete cmd; cmd = NULL;
		}
	}
	cmdVector.clear();

	//clear recycle buffer
	vector<CPackage*>::iterator itr2 = recyleBuffer.begin();
	for (; itr2 != recyleBuffer.end(); itr2++) {
		CPackage * mSendItem = (CPackage *)(*itr2);
		if (mSendItem != NULL) {
			delete mSendItem;
			mSendItem = NULL;
		}
	}
	recyleBuffer.clear();
}