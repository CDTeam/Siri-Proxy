#pragma once
#include "StdAfx.h"
class PackageProcessor;
//定义流，用于劫持Siri客户端包，动态解和发包
class PackageHajackStream
{
public:
	PackageHajackStream(void);
	~PackageHajackStream(void);
	bool WriteBlock(const char* data,size_t dataSize,size_t offset=0);
private:
	PackageProcessor *processor;
	
};
