#include "PackageHajackStream.h"
#include "PackageProcessor.h"
PackageHajackStream::PackageHajackStream(void)
{
	processor=new PackageProcessor(NULL);
}

PackageHajackStream::~PackageHajackStream(void)
{
	delete processor;
}

bool PackageHajackStream::WriteBlock( const char* data,size_t dataSize,size_t offset)
{
	return processor->ProcessBuffer(data,dataSize,offset);
}
