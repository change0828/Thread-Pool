#include "NetService.h"

#ifdef _DEBUG
#define DEBUG_CLIENTBLOCK new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
#define DEBUG_CLIENTBLOCK
#endif  // _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif  // _DEBUG

#include <random>

class testCMD : public CmdHandleDelegate
{
public:
	testCMD();
	~testCMD();

	virtual bool cmdHandle(CPackage * mCmd);
	virtual bool notifyResponseState(CPackage * mCmd);
private:

};

testCMD::testCMD()
{
}

testCMD::~testCMD()
{
}

bool testCMD::cmdHandle(CPackage * mCmd)
{
	printf("bool testCMD::cmdHandle(CPackage * mCmd) mCmd head %d \n", mCmd->getHead());
	return true;
}

bool testCMD::notifyResponseState(CPackage * mCmd)
{
	printf("bool testCMD::notifyResponseState(CPackage * mCmd) mCmd head %d \n", mCmd->getHead());
	return true;
}

// 发送聊天数据
#define SENDERCHITCHAT(pack) CGame::sendChitchatData(pack);
// 创建发送包
#define NEWPACK CPackage *pack = new CPackage();

#define LEN_NAME 20
#define LEN_PASS 33

bool isRunning = true;

void sendThread();

int main()
{
	testCMD *_Delegate = new testCMD;
	NetService::getInstance()->newSocket("192.168.1.120", "7000");
	//NetService::getInstance()->newSocket("180.150.185.60", "7000");
	NetService::getInstance()->addDelegate(_Delegate);
	
	for (int i = 0; i < 1; i++)
	{
		thread thread1(sendThread);
		thread1.detach();
	}

	while (true)
	{

	}
	isRunning = false;

	this_thread::sleep_for(std::chrono::seconds(10));

	delete _Delegate;
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	return 0;
}

void sendThread()
{
	while (isRunning)
	{
		this_thread::sleep_for(std::chrono::milliseconds(rand()%100));
		NEWPACK
		pack->pushHead(1005);
		pack->pushWord(1000);
		pack->pushByte("130001", 64);
		pack->pushByte("123456", 64);
		NetService::getInstance()->sendCmd(pack);
		delete pack;
	}
}