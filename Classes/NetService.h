﻿/**
 *  @Created by change on 16-06-08
 *  @Copyright 2016 icoole. All rights reserved.
 */
#ifndef __NetService_H__
#define __NetService_H__

#include "Package.h"
#include "SocketThread.h"
#include <vector>

//communication type for http,tcp,and http downloading
enum COM_TYPE {
	COM_HTTP = 1,
	COM_TCP = 2,
	COM_HTTP_DOWNLOADING = 3
};

//communication status including http,tcp,and http status
enum COM_STATUS {
	COM_ERROR = 0,//communication is well,but action ID < 0
	COM_OK = 1,//communication is ok
	COM_CONNECTED = 2 ,//socket connceted
	COM_CONNECT_FAILED  = 3 ,//socket connect failed
	COM_SYS_ERROR = 4,//communication error
	COM_DOWNLOADING = 5,//http downloading
	COM_DOWNLOADING_OK=  6,//http downloading ok
	COM_DOWNLOADING_FAILED =7//http downloading failed
};

/**declare CmdHandleDelegate*/
class CmdHandleDelegate
{
public:
	CmdHandleDelegate();
	virtual ~CmdHandleDelegate();

public:
	/**set receive message from NetService dispatcher*/
	void setReceiveCmd();
	/**set denied to receive message from NetService dispatcher*/
	void setBlockCmd();
	/**get is receiving or not*/
	bool getReceiveCmd() { return isReceiveCmd; }

	/**add to NetService dispatcher*/
	virtual void IntoDealNetCommand(void);
	/**leave to NetService dispatcher*/
	virtual void LeaveNetCommand(void);

	/**message handler and response state*/
	virtual bool cmdHandle(CPackage * mCmd);
	virtual bool notifyResponseState(CPackage * mCmd);

private:
	//check whether receive hanlder or not
	bool isReceiveCmd;
};


class SocketThread;

class NetService
{
public:
	static NetService* getInstance();
	static void purge();

	/**main Thread*/
	void handleThread();
	/**handle delegates in function(handleThread)*/
	bool handleDelegates(CPackage *mCmd);

	/**remove all delegates*/
	void removeAllDelegates(void);
	/**add delegate*/
	void addDelegate(CmdHandleDelegate* mDelegate);
	/**remove delegate*/
	void removeDelegate(CmdHandleDelegate * mDelegate);

	/**clear all socket handle*/
	void clearSockets();
	/**new socket and spawn a new SocketThread instance*/
	void newSocket(const char * hostname,const char * port,int mTag=0);
	/**get SocketThread instance by tag*/
	SocketThread * getSocketByTag(int mTag);
	/**add SocketThread instance with tag*/
	void addSocket(SocketThread * mSocket,int mTag);
	/**remove SocketThread instance by tag*/
	void removeSocket(int mTag);

	//cmd
public:
	/**push cmd from socket received*/
	void pushCmd(const char * mData, int mDataLength, int mCmdType, int mActionType, int mTag, char mStatus);
	/**send cmd to socket*/
	void sendCmd(CPackage * mCmd);
	/**pop command from command vector*/
	CPackage * popCmd();
	/**get command size*/
	int cmdVectorSize();
	/**clear message vector*/
	void clearCmdVector();

private:
	NetService(void);
	~NetService(void);

	/** 断开网络*/
	void _cleanNet();
private:
	static NetService *_instance;

	//handle loop is running or not,default is true
	bool running;
	//mutex used for thread synchronization
	mutex _mutex;

	//all delegate array
	vector<CmdHandleDelegate*> delegateArray;
	//socket array
	std::vector<SocketThread*> sokcetArray;

	//command already receive from server
	vector<CPackage*> cmdVector;
	//recyle ByteArray used
	vector<CPackage*> recyleBuffer;
};
#endif