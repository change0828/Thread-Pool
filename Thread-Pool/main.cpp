#include "NetService.h"

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
	printf("bool testCMD::cmdHandle(CPackage * mCmd) mCmd head %d", mCmd->getHead());
	return true;
}

bool testCMD::notifyResponseState(CPackage * mCmd)
{
	printf("bool testCMD::notifyResponseState(CPackage * mCmd) mCmd head %d", mCmd->getHead());
	return true;
}

int main()
{
	testCMD *_Delegate = new testCMD;
	NetService::getInstance()->newSocket("","");
	NetService::getInstance()->addDelegate(_Delegate);
	return 0;
}