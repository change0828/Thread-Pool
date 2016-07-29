/**
 *  @brief: 数据包结构
 *  @Created by ZeroHong on 14-09-01
 *  @Copyright 2013 XmHQ. All rights reserved.
 */

#ifndef __PACKAGE_H__
#define __PACKAGE_H__

typedef unsigned char	type_byte;
typedef unsigned short	type_word;
typedef unsigned long	type_dword;

//32K 单个网络消息最大长度,(超过极易导致物理服务器收发队列阻塞)
#define MAXMSGSIZE (16 * 1024)
//2K 每次增量
#define INCRSIZE (2 * 1024)

/** 数据大小， 32位系统和64位系统可能会long 的类型不一致*/
#define BYTE_SIZE	1
#define WORD_SIZE	2
#define DWORD_SIZE	4

/** 扩充大小，包太小会出现内存溢出*/
#define AUGMENT_SIZE_LEN		16		// 包长度接收，默认是DWORD_SIZE，扩充到8为了保险
#define AUGMENT_SIZE_EXTRA		64		// 过小长度的包会造成数据溢出，扩充避免内存溢出


class  CPackage
{
public:
	CPackage(type_word size = 256);
	~CPackage();

	void ready(void);

	char * buff(void);
	char * buffSurplus();
	type_word size(void);
	type_word length(void);

	void pushHead(type_word head);
	type_word readHead(void);

	void pushByte(type_byte value);
	type_byte readByte(void);

	void pushByte(const char * data, type_word size);
	const char * readByte(type_word size);

	void pushWord(type_word value);
	type_word readWord(void);

	void pushDword(type_dword value);
	type_dword readDword(void);

	/** 重置读取包的起点*/
	void setOffset();
	/** 打印发送的包头*/
	type_word getHead(){return m_siHead;}

	/** 设置从系统缓存区读取包的大小*/
	void setBuffSize(type_word size);

	/** 是否读取到包尾部*/
	bool readFinish();

	/**set status*/
	void setStatus(char mStatus);
	/**get status*/
	char getStatus();

	/**resize buffer*/
	void resize(unsigned int mLength);
	/**reset rd_ptr and wr_ptr to 0*/
	void reuse();
	/**copy data and return 0*/
	int copy(const char *buf, int n);

	void setTag(int mTag) { _tag = mTag; }
	int getTag() { return _tag; }
private:
	char *		m_pBuff;
	type_word	m_siHead;
	/** 缓存区的大小*/
	type_word	m_siSize;
	//读取位置
	int   m_nRdptr;
	//写入位置
	int   m_nWrPtr;
	/** 从缓存区读取的包大小*/
	type_word  m_sBuffSize;
	/** socket tag */
	int			_tag;
	//command status
	char status;
};
#endif
