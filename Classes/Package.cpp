#include "Package.h"
#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <iostream>
using namespace std;

CPackage::CPackage(type_word size)
	:m_siSize(size)
	,m_sBuffSize(0)
	,m_siHead(0)
	,m_nRdptr(0)
	,m_nWrPtr(0)
{
	m_pBuff = new char[m_siSize];
	memset(m_pBuff, 0, sizeof(m_pBuff));
}

CPackage::~CPackage()
{
	delete [] m_pBuff;
}

void CPackage::ready(void)
{
	m_siHead = 0;
	m_nRdptr = 0;
	m_nWrPtr = 0;
	m_pBuff[0] = 0;
}

char * CPackage::buff(void)
{
	return m_pBuff;
}

char * CPackage::buffSurplus()
{
	return (m_pBuff + m_sBuffSize);
}

type_word CPackage::size(void)
{
	return m_siSize;
}
type_word CPackage::length(void)
{
	return m_nWrPtr;
}

void CPackage::pushHead(type_word head)
{
	m_siHead = head;
	pushWord(m_siHead);
}
type_word CPackage::readHead(void)
{
	m_siHead = readWord();
	return m_siHead;
}

void CPackage::pushByte(type_byte value)
{
	memcpy((void*)(buff()+m_nWrPtr), &value, BYTE_SIZE);
	m_nWrPtr += BYTE_SIZE;
}
type_byte CPackage::readByte(void)
{
	type_byte value = *(type_byte*)(buff()+m_nRdptr);
	m_nRdptr += BYTE_SIZE;
	return value;
}

void CPackage::pushByte(const char * data, type_word size)
{
	type_word len = strlen(data);
	memcpy((void*)(buff()+m_nWrPtr), data, len);
	memset((void*)(buff()+m_nWrPtr+len),0,size-len);
	m_nWrPtr += size;
}
const char * CPackage::readByte(type_word size)
{
	const char * value = (buff()+m_nRdptr);
	m_nRdptr += size;
	return value;
}

void CPackage::pushWord(type_word value)
{
	value = htons(value);
	memcpy((void*)(buff()+m_nWrPtr), &value, WORD_SIZE);
	m_nWrPtr += WORD_SIZE;
}
type_word CPackage::readWord(void)
{
	type_word value = *(type_word*)(buff()+m_nRdptr);
	value = ntohs(value);
	m_nRdptr += WORD_SIZE;
	return value;
}

void CPackage::pushDword(type_dword value)
{
	value = htonl(value);
	memcpy((void*)(buff()+m_nWrPtr), &value, DWORD_SIZE);
	m_nWrPtr += DWORD_SIZE;
}
type_dword CPackage::readDword(void)
{
	type_dword value = *(type_dword*)(buff()+m_nRdptr);
	value = ntohl(value);
	m_nRdptr += DWORD_SIZE;
	return value;
}

/** 重置读取包的起点*/
void CPackage::setOffset()
{
	m_nRdptr = 0;
}

/** 设置从系统缓存区读取包的大小*/
void CPackage::setBuffSize(type_word size)
{
	m_sBuffSize = size;
}

/** 是否读取到包尾部*/
bool CPackage::readFinish()
{
	if (m_nRdptr == m_sBuffSize)
	{
		return true;
	}

	return false;
}

void CPackage::reuse()
{
	m_nRdptr = 0;
	m_nWrPtr = 0;
}

void CPackage::resize(unsigned int mLength)
{
	int tmpLength = this->length();
	char *tmpContent = m_pBuff;

	m_siSize  += INCRSIZE ;
	while (m_siSize < mLength) {
		m_siSize += INCRSIZE;
	}

	m_pBuff = new char[m_siSize];
	memset(m_pBuff,0,m_siSize);
	memcpy(m_pBuff, tmpContent, tmpLength);

	delete[] tmpContent;
	tmpContent = NULL;
}

int CPackage::copy(const char *buf, int n)
{
	int _Len = this->length();
	if(_Len < n)
	{
		this->resize(n);
	}
	memcpy((void*)(buff()+m_nWrPtr), buf, n);
	m_nWrPtr += n;

	return 0;
}

//void CPackage::restructuring()
//{
//	int _size = sizeof(m_pBuff);
//	char* _buff = new char[m_siSize];
//	memset(_buff, 0, sizeof(_buff));
//	memcpy(_buff,m_pBuff,m_siSize);	
//	delete[] m_pBuff;
//
//	int _len = m_siOffset;
//	this->setOffset();
//	m_pBuff = new char[m_siSize + DWORD_SIZE];
//	memset(m_pBuff, 0, sizeof(m_pBuff));	
//	this->pushDword(_len);
//	memcpy((void*)(buff()+m_siOffset), _buff, _len);
//	m_siOffset += _len;
//	delete[] _buff;
//}