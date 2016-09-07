#include "NetService.h"
#include <string.h>

using namespace std;


int on_log(const char *format, ...)
{
#ifdef COCOS2D_DEBUG
	int bytes_written;
	va_list arg_ptr;

	va_start(arg_ptr, format);

	bytes_written = vfprintf(stderr, format, arg_ptr);

	va_end(arg_ptr);

	return bytes_written;
#else
	return 0;
#endif
}


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
	on_log("+++++ add to operateArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
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
	on_log("---- remove to removedArray delegate =%p name=%s \n", (mDelegate), typeid(*mDelegate).name());
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

			on_log("socket tag=%d remove from NetService\n",mTag);
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
	auto readCmd = cmdVector.try_pop();
	if (readCmd != NULL)
	{
		bool isCmdHandled = this->handleDelegates(readCmd.get());
		if (isCmdHandled == false) {
			cmdVector.push_front(*readCmd.get());
		}
		else {
			recyleBuffer.push_back(*readCmd.get());
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
			std::shared_ptr<CPackage> readedCmd = nullptr;
			if (recyleBuffer.empty() == false)
			{
				readedCmd = recyleBuffer.try_pop();
				readedCmd->reuse();//must set to reuse.
				on_log("reuse message =%p \n", readedCmd.get());
			}
			else
			{
				readedCmd = std::make_shared<CPackage>(mDataLength + 2);
			}
			if (mActionType != 0) {
				//readedCmd->setCmdType(mCmdType);
				readedCmd->pushHead(mActionType);
				readedCmd->setTag(mTag);
				readedCmd->setStatus(mStatus); // cmd status. 1 = success, 0 = false
				readedCmd->copy(mData+2, mDataLength-2);	//扣除head 长度
				cmdVector.push_back(*readedCmd.get());
				on_log("recv ActionID =%d type=%d,status =%d,data length=%d \n", mCmdType, mActionType, mStatus, mDataLength);
			}
			else if (mActionType == 0)
			{
				readedCmd.reset();
				on_log("recv heart beat ActionID =%d ,data length=%d \n", mCmdType, mDataLength);
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
		on_log("send command failed ,no valid socket item or not connect \n");
	}
}

void NetService::clearCmdVector()
{
	auto data = cmdVector.try_pop();
	while (data)
	{
		data.reset();
		data = cmdVector.try_pop();
	}

	//clear recycle buffer
	data = recyleBuffer.try_pop();
	while (data)
	{
		data.reset();
		data = recyleBuffer.try_pop();
	}
}