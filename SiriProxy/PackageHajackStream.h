#pragma once
#include "StdAfx.h"
class PackageProcessor;
//�����������ڽٳ�Siri�ͻ��˰�����̬��ͷ���
class PackageHajackStream
{
public:
	PackageHajackStream(void);
	~PackageHajackStream(void);
	bool WriteBlock(const char* data,size_t dataSize,size_t offset=0);
private:
	PackageProcessor *processor;
	
};
