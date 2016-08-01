#include "NetService.h"
#include "SocketThread.h"
#include "CCPlatform.h"

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

void NetService::clear()
{
	this->clearSockets();
	this->clearCmdVector();
}

NetService::NetService(void)
	:isRunning(true)
{
	for (short i = 0; i < 10; i++)
	{
		CPackage * tmpCmd = new CPackage(512);
		_instance->recyleBuffer.push_back(tmpCmd);
	}
#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);
#endif
}

NetService::~NetService(void)
{
	isRunning = false;
	this->clearSockets();
	this->clearCmdVector();
	this->removeAllDelegates();
}

void NetService::stopThread()
{
	isRunning = false;

	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end(); itr++)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr != _SocketT)
		{
			_SocketT->stopThread();
		}
	}
}

void NetService::addDelegate(CmdHandleDelegate* mDelegate)
{
	mDelegate->setReceiveCmd();
	delegateArray.push_back(mDelegate);
	NET_LOG("+++++ add to operateArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
}

void NetService::removeDelegate(CmdHandleDelegate * mDelegate)
{
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
	NET_LOG("---- remove to removedArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
}

void NetService::removeAllDelegates(void)
{
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
	std::unique_lock<std::mutex> _lock(_mutex);
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
	std::unique_lock<std::mutex> _lock(_mutex);
	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end(); itr++)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT && _SocketT->getTag() == mTag)
		{
			_SocketT->stopThread();

			//delete _SocketT;
			//_SocketT = nullptr;
			removeArray.push_back(_SocketT);
			sokcetArray.erase(itr);

			NET_LOG("socket tag=%d remove from NetService\n",mTag);
			break;
		}
	}
}

void NetService::clearSockets()
{
	vector<SocketThread*>::iterator itr = sokcetArray.begin();
	for (; itr != sokcetArray.end();)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT)
		{
			_SocketT->stopThread();

			removeArray.push_back(_SocketT);
		}
		itr = sokcetArray.erase(itr);
	}

	sokcetArray.clear();
}

void NetService::handleLoop(float mTime)
{
	if (!isRunning)
	{
		return;
	}
	//处理移除
	this->handleRemoveArray();
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

void NetService::handleRemoveArray()
{
	vector<SocketThread*>::iterator itr = removeArray.begin();
	for (; itr != removeArray.end();)
	{
		SocketThread *_SocketT = *itr;
		if (nullptr!=_SocketT && _SocketT->isThreadOver())
		{
			itr = removeArray.erase(itr);

			delete _SocketT;
			_SocketT = nullptr;
		}
		else 
		{
			itr++;
		}
	}
}

bool NetService::handleDelegates(CPackage *mCmd)
{
	bool isCmdHandled = false;

	vector<CmdHandleDelegate*>::iterator itr = delegateArray.begin();
	for (; itr != delegateArray.end(); itr++)
	{
		CmdHandleDelegate * object = (*itr);

		if (mCmd->getHead() == COM_TCP)
		{
			if (mCmd->getStatus() == COM_CONNECT_FAILED)
			{
				this->removeSocket(mCmd->getTag());
			}
			//if (mCmd->getStatus() == COM_SYS_ERROR)
			//{
			//	this->removeSocket(mCmd->getTag());
			//}

			isCmdHandled = object->notifyResponseState(mCmd);

			if (isCmdHandled)
			{
				break;
			}
		}
		else
		{
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
	}

	return isCmdHandled;
}

void NetService::pushCmd(const char * mData, int mDataLength, int mCmdType, int mActionType, int mTag, char mStatus)
{
	if (mData != NULL)
	{
		if (mDataLength >= 0) {
			CPackage * readedCmd = NULL;
			std::unique_lock<std::mutex> _lock(_mutex);
			if (recyleBuffer.empty() == false)
			{
				readedCmd = (CPackage *)(recyleBuffer.front());
				readedCmd->reuse();//must set to reuse.
				recyleBuffer.erase(recyleBuffer.begin());
				NET_LOG("reuse message =%p \n", readedCmd);
			}
			else
			{
				readedCmd = new CPackage(mDataLength + 2);
			}
			_lock.unlock();
			if (mActionType != 0) {
				//readedCmd->setCmdType(mCmdType);
				readedCmd->pushHead(mActionType);
				readedCmd->setTag(mTag);
				readedCmd->setStatus(mStatus); // cmd status. 1 = success, 0 = false
				readedCmd->copy(mData+2, mDataLength-2);	//扣除head 长度
				cmdVector.push_back(readedCmd);
				NET_LOG("ActionID =%d type=%d,status =%d,data length=%d \n", mCmdType, mActionType, mStatus, mDataLength);
			}
			else if (mActionType == 0)
			{
                delete readedCmd;
                readedCmd = nullptr;
				NET_LOG("heart beat ActionID =%d ,data length=%d \n", mCmdType, mDataLength);
			}
		}
	}
}

void NetService::sendCmd(CPackage * mCmd)
{
	if (!isRunning)
	{
		return;
	}

	int _tag = mCmd->getTag();

	SocketThread *_socket = this->getSocketByTag(_tag);
	if (nullptr != _socket && _socket->getIsRunning())
	{
		_socket->addToSendBuffer(mCmd->buff(), mCmd->length(), mCmd->getHead());
	}
	else
	{
		NET_LOG("send command failed ,no valid socket item or not connect \n");
	}
}

CPackage * NetService::popCmd()
{
	CPackage * mCmd = NULL;

	std::unique_lock<std::mutex> _lock(_mutex);
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