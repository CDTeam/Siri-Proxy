#pragma once
#include "StdAfx.h"
class MessageProcessHelper
{
public:
	static int ProcessMessageStream(istream& istrm);
	static int InflateMessageStream(istream& istrm);
	static void ProcessStreamSocket(StreamSocket& sockIn,StreamSocket& sockOut,size_t nReadBuffer=1024);
	static void ProcessSocketStream(SocketStream& sstrmIn,SocketStream& sstrmOut);
};
