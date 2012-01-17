#include "SiriClientServiceHandler.h"
#include "MessageProcessHelper.h"
#include "SiriTokenProvider.h"
#include "SiriTcpSvr.h"
#include "DbHelpSQL.h"
#include "SafeProcessPlist.h"
#include "AppContext.h"
#include "PluginAdapter.h"
#include <plist/plist++.h>
const string SiriClientServiceHandler::SIRI_SERVICE_HOST="17.174.4.4";//"guzzoni.apple.com";//"17.174.4.9";
const unsigned short SiriClientServiceHandler::SIRI_SERVICE_PORT=443;
bool SiriClientServiceHandler::m_bLogging=true;

#define MAX_ACE_HTTP_HEAD 1024L
#define SLEEP_MIN_SECS 10
#define SOCKET_BUFFER_SIZE 8192*5

const Timestamp::TimeDiff diff=30*Timestamp::resolution();

SiriClientServiceHandler::SiriClientServiceHandler(const SecureStreamSocket& sock)
{
	pTokenMgr=NULL;
	/* 初始化解压缩流*/
	inflatestrm_push.zalloc = Z_NULL;
	inflatestrm_push.zfree = Z_NULL;
	inflatestrm_push.opaque = Z_NULL;
	inflatestrm_push.avail_in = 0;
	inflatestrm_push.next_in = Z_NULL;
	inflateInit(&inflatestrm_push);

	deflatestrm_push.zalloc = Z_NULL;
	deflatestrm_push.zfree = Z_NULL;
	deflatestrm_push.opaque = Z_NULL;
	deflateInit(&deflatestrm_push, Z_DEFAULT_COMPRESSION);

	unpack_push=new PackageProcessor(this);
	s1=sock;
	state_push=PUSH_STATE_PROCESS_HTTP_HEAD;
	state_pull=PULL_STATE_PROCESS_HTTP_HEAD;
	
	//s1.setNoDelay(false);
	
	inflatestrm_pull.zalloc = Z_NULL;
	inflatestrm_pull.zfree = Z_NULL;
	inflatestrm_pull.opaque = Z_NULL;
	inflatestrm_pull.avail_in = 0;
	inflatestrm_pull.next_in = Z_NULL;
	inflateInit(&inflatestrm_pull);

	deflatestrm_pull.zalloc = Z_NULL;
	deflatestrm_pull.zfree = Z_NULL;
	deflatestrm_pull.opaque = Z_NULL;
	deflateInit(&deflatestrm_pull, Z_DEFAULT_COMPRESSION);
	unpack_pull=new PackageProcessor(this,false);

	mon=new SocketReactor();
	nAgentReadSleep=0;
	nSiriReadSleep=0;
}

SiriClientServiceHandler::~SiriClientServiceHandler(void)
{	
	try
	{
		mon->stop();
		delete mon;

		if(pTokenMgr && !pTokenMgr->iPhone4s)
		{
			SiriTokenProvider::RecycleTicket(pTokenMgr->getTicket());
		}
		if(pTokenMgr!=NULL) delete pTokenMgr;

		(void)inflateEnd(&inflatestrm_pull);
		(void)deflateEnd(&deflatestrm_pull);
		delete unpack_pull;

		(void)inflateEnd(&inflatestrm_push);
		(void)deflateEnd(&deflatestrm_push);
		delete unpack_push;

		#ifdef _DEBUG
		if(m_bLogging)
		{
			InternalLogInfo(format("-------------------->释放线程:%d<--------------------",m_ThreadId));
		}
		#endif // _DEBUG
		s2.close();
	}
	catch(Poco::Exception& e)
	{
		#ifdef _DEBUG
		InternalWarning("~:"+e.displayText());
		#endif // _DEBUG
		
	}
}

SiriClientServiceHandler* SiriClientServiceHandler::Create( const SecureStreamSocket& sock,bool catchToken)
{
	SiriClientServiceHandler* p=new SiriClientServiceHandler(sock);
	p->bCatchToken=catchToken;
	Logger& logger =Application::instance().logger();
	p->pLogger =&logger;
	p->m_ThreadId=Thread::current()->id();

	try
	{
		SocketAddress addr(SIRI_SERVICE_HOST,SIRI_SERVICE_PORT); 
		p->s2.connect(addr);

#ifdef _SOCKET_KEEP_ALIVE
		p->s1.setSendBufferSize(SOCKET_BUFFER_SIZE);
		p->s1.setReceiveBufferSize(SOCKET_BUFFER_SIZE);
		p->s2.setSendBufferSize(SOCKET_BUFFER_SIZE);
		p->s2.setReceiveBufferSize(SOCKET_BUFFER_SIZE);

		SetSocketKeepAlive(p->s1.impl()->sockfd());
		SetSocketKeepAlive(p->s2.impl()->sockfd());
		p->s1.setKeepAlive(true);
		p->s2.setKeepAlive(true);
#endif // _SOCKET_KEEP_ALIVE
	}
	catch(Poco::Exception& e)
	{
		string msg=format("Create,socketfd:%d,",(int)p->s2.impl()->sockfd())+e.displayText();
		InternalWarning(msg);

		delete p;

		return NULL;
	}

	p->mon->addEventHandler(p->s1,Observer<SiriClientServiceHandler,ReadableNotification>(*p,&SiriClientServiceHandler::onSiriSocketReadEvent));
	p->mon->addEventHandler(p->s1,Observer<SiriClientServiceHandler,ErrorNotification>(*p,&SiriClientServiceHandler::onError));
	p->mon->addEventHandler(p->s1,Observer<SiriClientServiceHandler,IdleNotification>(*p,&SiriClientServiceHandler::onSocketIdle));

	p->mon->addEventHandler(p->s2,Observer<SiriClientServiceHandler,ReadableNotification>(*p,&SiriClientServiceHandler::onAgentSocketReadEvent));
	p->mon->addEventHandler(p->s2,Observer<SiriClientServiceHandler,ErrorNotification>(*p,&SiriClientServiceHandler::onError));
	p->mon->addEventHandler(p->s2,Observer<SiriClientServiceHandler,IdleNotification>(*p,&SiriClientServiceHandler::onSocketIdle));

	return p;
}

void SiriClientServiceHandler::onSiriSocketReadEvent( ReadableNotification* pNf )
{
	pNf->release();
	OnSiriSocketReadable();
}

void SiriClientServiceHandler::onAgentSocketReadEvent( ReadableNotification* pNf )
{
	pNf->release();
	OnAgentSocketReadable();
}

void SiriClientServiceHandler::onError( ErrorNotification* pNf )
{
	poco_socket_t fd=pNf->socket().impl()->sockfd();
	pNf->release();
	string msg;
	Thread* tts=Thread::current();
	int tid=tts->id();
	if(fd==s1.impl()->sockfd())
	{
		msg=format("Siri客户端套接字发生错误,错误代号:%d,线程Id:%d",(int)fd,tid);
	}
	else if(fd==s2.impl()->sockfd())
	{
		msg=format("代理客户端套接字发生错误,错误代号:%d,线程Id:%d",(int)fd,tid);
	}
	InternalLogInfo(msg);
	Finalize();
}

void SiriClientServiceHandler::onSocketIdle( IdleNotification* pNf )
{
	pNf->release();
	if(t1.isElapsed(diff) || t2.isElapsed(diff))
	{
		Finalize();
	}
	else
	{
		Thread::sleep(SLEEP_MIN_SECS);
	}
}

void SiriClientServiceHandler::OnSiriSocketReadable()
{
	try
	{
		ProcessPushNetworkStream();
	}
	catch(Poco::Exception& e)
	{
		InternalLogInfo("OnSiriSocketReadable"+e.displayText());
		Finalize();
	}
}

void SiriClientServiceHandler::OnAgentSocketReadable()
{
	try
	{
		ProcessPullNetworkStream();
	}
	catch(Poco::Exception& e)
	{
		InternalLogInfo("OnAgentSocketReadable"+e.displayText());
		Finalize();
	}
}

void SiriClientServiceHandler::ListenNetworkEvents()
{
	mon->run();
}

void SiriClientServiceHandler::VectorBufferCutLeft( vector<char>& buffer,const size_t& count )
{
	if(0==count)
		return;
	vector<char> tmp=buffer;
	int size=tmp.size()-(count);
	buffer.clear();
	buffer.resize(size);
	if(size>0)
	{
		memcpy(&buffer[0],&tmp[count],size);
	}
}

void SiriClientServiceHandler::SiriSocketReadAll()
{
	int next=0;
	char *data=new char[READ_SIZE];
	try
	{
		do 
		{
			int len= s1.receiveBytes(data,READ_SIZE);

			if(len>0)
			{
				int size=m_buffer_push.size();
				m_buffer_push.resize(size+len);
				memcpy(&m_buffer_push[size],data,len);
				t1=Timestamp();
			}
			else
			{
				if(t1.isElapsed(diff))
				{
					Finalize();
				}
				else
				{
					Thread::sleep(SLEEP_MIN_SECS);
				}
			}

			next=s1.available();
		} while (next!=0);
		delete[] data;
	}
	catch(Poco::Exception& e)
	{
		Finalize();
		delete[] data;
		#ifdef _DEBUG
		InternalLogInfo("SiriSocketReadAll:"+e.displayText());
		#endif // _DEBUG
	}
}

void SiriClientServiceHandler::ProcessPushNetworkStream()
{
	switch(state_push)
	{
	case PUSH_STATE_PROCESS_HTTP_HEAD:
		{
			SiriSocketReadAll();

			if(m_buffer_push.size())
			{
				string ss;
				ss.append(&m_buffer_push[0],m_buffer_push.size());
				int index=ss.find("\r\n\r\n");
				if(index!=-1)
				{//表明这已经包含了头部
					string header=ss.substr(0,index+4);
					pTokenMgr=SiriTokenProvider::FromAceHeader(header);
					if(NULL==pTokenMgr) return Finalize();
		
					string ip_addr=s1.peerAddress().host().toString();
					pTokenMgr->m_Ticket.ip_address=ip_addr;

					const char* data=pTokenMgr->m_http_string.data();
					size_t size=pTokenMgr->m_http_string.size();

					AgentSocketSendBytes(data,size);
					VectorBufferCutLeft(m_buffer_push,index+4);

					if(m_buffer_push.size()>=4)
					{
						AgentSocketSendBytes(&m_buffer_push[0],4);
						VectorBufferCutLeft(m_buffer_push,4);//剔除前4个字节
						state_push=PUSH_STATE_PROCESS_ZIP_DATA;
					}
					else
					{
						state_push=PUSH_STATE_PROCESS_0xAACCEE02;
					}
				}
				else if(m_buffer_push.size()>MAX_ACE_HTTP_HEAD)
				{//这是不正确的Http请求,恶意的，假冒的？
					InternalLogInfo("这是不正确的Http请求,恶意的，假冒的？---------------------------------->");
					Finalize();
				}
			}
		}
		break;
	case PUSH_STATE_PROCESS_0xAACCEE02:
		{
			SiriSocketReadAll();
			if(m_buffer_push.size()>0)
			{
				if(m_buffer_push.size()>=4)
				{
					AgentSocketSendBytes(&m_buffer_push[0],4);
					VectorBufferCutLeft(m_buffer_push,4);//剔除前4个字节
					state_push=PUSH_STATE_PROCESS_ZIP_DATA;

					if(!GetPushPackageFromData())
					{
						InternalLogInfo("GetPackageFromData失败在STATE_PROCESS_0xAACCEE02");
						Finalize();
					}
				}
			}
		}
		break;
	case PUSH_STATE_PROCESS_ZIP_DATA:
		{
			SiriSocketReadAll();
			if (m_buffer_push.size()>0)
			{
				if(!GetPushPackageFromData())
				{
					InternalLogInfo("GetPackageFromData失败在STATE_PROCESS_ZIP_DATA");
					Finalize();
				}
			}
		}
	}
}

bool SiriClientServiceHandler::GetPushPackageFromData()
{
	if (m_buffer_push.size() == 0)
		return true;

	int ret=0;
	unsigned have=0;

	unsigned char* in=new unsigned char[CHUNK_SIZE];
	unsigned char* out=new unsigned char[CHUNK_SIZE];
	MemoryInputStream ms(&m_buffer_push[0],m_buffer_push.size());


	do {
		ms.read((char*)in,CHUNK_SIZE);
		inflatestrm_push.avail_in = ms.gcount();
		if (inflatestrm_push.avail_in == 0)
			break;
		inflatestrm_push.next_in = in;

		do {
			inflatestrm_push.avail_out = CHUNK_SIZE;
			inflatestrm_push.next_out = out;
			ret = inflate(&inflatestrm_push, Z_NO_FLUSH);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				{
					delete[] in;
					delete[] out;
					(void)inflateEnd(&inflatestrm_push);
					return false;
				}
			}
			have = CHUNK_SIZE - inflatestrm_push.avail_out;
			if(!unpack_push->ProcessBuffer((const char*)out,have,0))
			{
				(void)inflateEnd(&inflatestrm_push);
				return false;
			}
		} while (inflatestrm_push.avail_out == 0);

	} while (ms.good());

	delete[] in;
	delete[] out;

	m_buffer_push.clear();
	return true;
}

bool SiriClientServiceHandler::WritePushPackageToData( const char* package,const size_t& packageLen,vector<char>& data )
{
	if((0==packageLen)||(NULL==package))
		return true;

	MemoryInputStream ms(package,packageLen);

	unsigned char* in=new unsigned char[CHUNK_SIZE];
	unsigned char* out=new unsigned char[CHUNK_SIZE];
	int ret, flush;
	unsigned have;

	/* compress until end of file */
    do {
		ms.read((char*)in,CHUNK_SIZE);
		streamsize readsize=ms.gcount();
		if(0==readsize)
			break;
        deflatestrm_push.avail_in = readsize;
   
        flush =Z_FULL_FLUSH;
        deflatestrm_push.next_in = in;

        do {
            deflatestrm_push.avail_out = CHUNK_SIZE;
            deflatestrm_push.next_out = out;
            ret = deflate(&deflatestrm_push, flush);    /* no bad return value */
            
			if(ret==Z_DATA_ERROR)
			{
				delete[] in;
				delete[] out;
				return false;
			}

            have = CHUNK_SIZE - deflatestrm_push.avail_out;
            
			if(have>0)
			{
				size_t sz=data.size();
				data.resize(sz+have);
				memcpy(&data[sz],out,have);
			}
        } while (deflatestrm_push.avail_out == 0);
        
    } while (ms.good());

	delete[] in;
	delete[] out;

	return true;
}

void SiriClientServiceHandler::OnPingPackage( const char* pingData,const size_t& dataLen,bool bPush )
{
	int nLen=SIZE_HEAD_BLOCK+SIZE_SHORT;
	char *cache=new char[nLen];
	memcpy(cache,header_0x030000,SIZE_HEAD_BLOCK);
	short nextLen;
	memcpy(&nextLen,pingData,2);
	ConvertEndian16(&nextLen);
	memcpy(cache+SIZE_HEAD_BLOCK,&nextLen,SIZE_SHORT);

	vector<char> data;
	if(bPush)
	{
		#ifdef _DEBUG
		static int index_ping=0;
		InternalLogInfo(format("ping包-->序号:%d",++index_ping));
		#endif // _DEBUG
		WritePushPackageToData(cache,nLen,data);
		if(data.size()>0)
		{
			AgentSocketSendBytes(&data[0],data.size());
		}
	}
	else
	{
		#ifdef _DEBUG
		static int index_pong=0;
		InternalLogInfo(format("pong包-->序号:%d",++index_pong));
		#endif // _DEBUG
		WritePullPackageToData(cache,nLen,data);
		if(data.size()>0)
		{
			SiriSocketSendBytes(&data[0],data.size());
		}
	}

	delete[] cache;
	
}

void __stdcall DefaultFreePlistData(char* plistData)
{
	delete[] plistData;
}

void SiriClientServiceHandler::OnPlistPackage( const char* plistData,const size_t& dataLen,bool bPush )
{
	#ifdef _SAVE_PLIST
	SiriTokenProvider::SavePlistXML(plistData,dataLen,bPush);
	#endif // _SAVE_PLIST

	bool iphone4s=this->pTokenMgr->iPhone4s;
	if(bPush)
	{
		vector<char> plistFix;
		if(!iphone4s || (iphone4s&&bCatchToken))
		{
			bool processed=false;
			if(!iphone4s)
			{
				processed=this->pTokenMgr->processed;
			}
			else
			{//强制iphone4s每次分析包
				processed=false;
				this->pTokenMgr->processed=false;
			}
			FIX_PLIST_RESULT code=fix_plist_xml(plistData,dataLen,plistFix,&pTokenMgr->m_Ticket,pTokenMgr->iPhone4s,pTokenMgr->processed);
			if(FIX_RESULT_SUCCESS!=code)
			{
				Finalize();
				return;
			}
			if(!processed && iphone4s && this->pTokenMgr->processed)
			{
				this->pTokenMgr->iPhone4sRefreshTicket();
			}
		}

		size_t fixSize=plistFix.size();
		if (fixSize)
		{
			WriteSocketPlistBin(&plistFix[0],fixSize,true);
		}
		else
		{
			WriteSocketPlistBin(plistData,dataLen,true);
		}
	}
	else
	{
		HOST_PLIST_RESULT hr=check_host_plist_result(plistData,dataLen,&pTokenMgr->m_Ticket);
		if(RESULT_SESSION_EPRIED==hr)
		{
			pTokenMgr->m_Ticket.expired++;
			if(iphone4s)
			{
				pTokenMgr->iPhone4sRefreshTicket();
			}
			else
			{
				pTokenMgr->iPhone4RefreshTicket();
			}
			if(!pTokenMgr->m_Ticket.assistantId.empty())
			{
				string msg=format("收到服务器响应:AssistantId为%s的密钥已经过期!",pTokenMgr->m_Ticket.assistantId);
				InternalLogInfo(msg);
			}
			Finalize();
		}
		else if(RESULT_ASSISTANT_LOADED==(hr&RESULT_ASSISTANT_LOADED))
		{
			if(!iphone4s)
			{
				pTokenMgr->m_Ticket.expired=0;//重置
				pTokenMgr->iPhone4RefreshTicket();
			}
		}

		if(iphone4s)
		{
			if(RESULT_ASSISTANT_CREATE==(hr&RESULT_ASSISTANT_CREATE))
			{
				pTokenMgr->m_Ticket.expired=0;
				pTokenMgr->iPhone4sRefreshTicket();
			}
		}

		PluginMgr::Current()->FilterPlistBin(const_cast<char*>(plistData),dataLen,this);

	}
}

void SiriClientServiceHandler::OnPackageParseError( const string& msg,bool bPush )
{
	if(bPush)
	{
		InternalLogInfo(format("解析发送包时发生了错误:%s",msg));
	}
	else
	{
		InternalLogInfo(format("解析接收包时发生了错误:%s",msg));
	}
}

void SiriClientServiceHandler::ProcessPullNetworkStream()
{
	switch(state_pull)
	{
	case PULL_STATE_PROCESS_HTTP_HEAD:
		{
			AgentSocketReadAll();
			
			if(m_buffer_pull.size())
			{
				string ss;
				ss.append(&m_buffer_pull[0],m_buffer_pull.size());
				int index=ss.find("\r\n\r\n");
				if(index!=-1)
				{//表明这已经包含了头部
					string header=ss.substr(0,index+4);
			
					//s1.sendBytes(&m_buffer_pull[0],index+4);//发送响应头
					SiriSocketSendBytes(&m_buffer_pull[0],m_buffer_pull.size());
					VectorBufferCutLeft(m_buffer_pull,index+4);

					if(m_buffer_pull.size()>=4)
					{
						//s1.sendBytes(&m_buffer_pull[0],4);
						SiriSocketSendBytes(&m_buffer_pull[0],4);
						VectorBufferCutLeft(m_buffer_pull,4);//剔除前4个字节
						state_pull=PULL_STATE_PROCESS_ZIP_DATA;
					}
					else
					{
						state_pull=PULL_STATE_PROCESS_0xAACCEE02;
					}
				}
				else if(m_buffer_pull.size()>MAX_ACE_HTTP_HEAD)
				{//这是不正确的Http请求,恶意的，假冒的？
					Finalize();
				}
			}
		}
		break;
	case PULL_STATE_PROCESS_0xAACCEE02:
		{
			AgentSocketReadAll();
			if(m_buffer_pull.size()>=4)
			{
				//s1.sendBytes(&m_buffer_pull[0],4);
				SiriSocketSendBytes(&m_buffer_pull[0],4);
				VectorBufferCutLeft(m_buffer_pull,4);//剔除前4个字节
				state_pull=PULL_STATE_PROCESS_ZIP_DATA;

				if(!GetPullPackageFromData())
				{
					InternalLogInfo("GetPackageFromData失败在STATE_PROCESS_0xAACCEE02");
					Finalize();
				}
			}
			else
			{
				Thread::sleep(SLEEP_MIN_SECS);
			}
		}
		break;
	case PULL_STATE_PROCESS_ZIP_DATA:
		{
			AgentSocketReadAll();
			if (m_buffer_pull.size()>0)
			{
				if(!GetPullPackageFromData())
				{
					InternalLogInfo("GetPullPackageFromData失败在STATE_PROCESS_ZIP_DATA");
					Finalize();
				}
				t2=Timestamp();
			}
			else
			{
				if(t2.isElapsed(diff))
				{
					Finalize();
				}
				else
				{
					Thread::sleep(SLEEP_MIN_SECS);
				}
			}
		}
	}
}

bool SiriClientServiceHandler::GetPullPackageFromData()
{
	if (m_buffer_pull.size() == 0)
		return true;

	int ret=0;
	unsigned have=0;

	unsigned char* in=new unsigned char[CHUNK_SIZE];
	unsigned char* out=new unsigned char[CHUNK_SIZE];
	MemoryInputStream ms(&m_buffer_pull[0],m_buffer_pull.size());


	do {
		ms.read((char*)in,CHUNK_SIZE);
		inflatestrm_pull.avail_in = ms.gcount();
		if (inflatestrm_pull.avail_in == 0)
			break;
		inflatestrm_pull.next_in = in;

		do {
			inflatestrm_pull.avail_out = CHUNK_SIZE;
			inflatestrm_pull.next_out = out;
			ret = inflate(&inflatestrm_pull, Z_NO_FLUSH);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				{
					delete[] in;
					delete[] out;
					(void)inflateEnd(&inflatestrm_pull);
					return false;
				}
			}
			have = CHUNK_SIZE - inflatestrm_pull.avail_out;
			if(!unpack_pull->ProcessBuffer((const char*)out,have,0))
			{
				(void)inflateEnd(&inflatestrm_pull);
				return false;
			}
		} while (inflatestrm_pull.avail_out == 0);

	} while (ms.good());

	delete[] in;
	delete[] out;

	m_buffer_pull.clear();
	return true;
}

void SiriClientServiceHandler::AgentSocketReadAll()
{
	int next=0;
	char *data=new char[READ_SIZE];
	try
	{
		do 
		{
			int len= s2.receiveBytes(data,READ_SIZE);

			if(len>0)
			{
				int size=m_buffer_pull.size();
				m_buffer_pull.resize(size+len);
				memcpy(&m_buffer_pull[size],data,len);
			}
			else
			{//这说明客户端已经断开连接了,触发了SELEXT_READ
				Finalize();
			}

			next=s2.available();
		} while (next!=0);
		delete[] data;
	}
	catch(Poco::Exception& e)
	{
		Finalize();
		delete[] data;
		#ifdef _DEBUG
		InternalLogInfo("AgentSocketReadAll:"+e.displayText());
		#endif // _DEBUG
	}
}

bool SiriClientServiceHandler::WritePullPackageToData( const char* package,const size_t& packageLen,vector<char>& data )
{
	if((0==packageLen)||(NULL==package))
		return true;

	MemoryInputStream ms(package,packageLen);

	unsigned char* in=new unsigned char[CHUNK_SIZE];
	unsigned char* out=new unsigned char[CHUNK_SIZE];
	int ret, flush;
	unsigned have;

	do {
		ms.read((char*)in,CHUNK_SIZE);
		streamsize readsize=ms.gcount();
		if(0==readsize)
			break;
		deflatestrm_pull.avail_in = readsize;

		flush =Z_FULL_FLUSH;
		deflatestrm_pull.next_in = in;

		do {
			deflatestrm_pull.avail_out = CHUNK_SIZE;
			deflatestrm_pull.next_out = out;
			ret = deflate(&deflatestrm_pull, flush);    /* no bad return value */

			if(ret==Z_DATA_ERROR)
			{
				delete[] in;
				delete[] out;
				return false;
			}

			have = CHUNK_SIZE - deflatestrm_pull.avail_out;

			if(have>0)
			{
				size_t sz=data.size();
				data.resize(sz+have);
				memcpy(&data[sz],out,have);
			}
		} while (deflatestrm_pull.avail_out == 0);

	} while (ms.good());

	delete[] in;
	delete[] out;

	return true;
}

void SiriClientServiceHandler::AgentSocketSendBytes( const char* buffer,const size_t& len )
{
	try
	{
		s2.sendBytes(buffer,len);
	}
	catch(Exception& e)
	{
		InternalWarning("AgentSocketSendBytes:"+e.displayText());
		Finalize();
	}
}

bool SiriClientServiceHandler::CheckHeaderBufferSize(const size_t& size)
{
	if(size<=1024)
	{
		return true;
	}
	else
	{
		InternalWarning("警告:数据包异常,超出预计的缓冲区大小,可能是敌意的客户端连接,该连接将会被强制断开,请检查!");
		Finalize();
		return false;
	}
}

void SiriClientServiceHandler::SiriSocketSendBytes( const char* buffer,const size_t& len )
{
	try
	{
		if(s1.poll(Timespan(250000),Socket::SELECT_WRITE))
		{
			s1.sendBytes(buffer,len);
		}
		else
		{
			Thread::sleep(SLEEP_MIN_SECS);
		}
	}
	catch(Exception& e)
	{
		#ifdef _DEBUG
		InternalWarning("SiriSocketSendBytes:"+e.displayText());
		#endif // _DEBUG
		Finalize();
	}
}

void SiriClientServiceHandler::Finalize()
{
	mon->stop();
}

bool SiriClientServiceHandler::WriteSocketPlistBin( const char* plistbin,const size_t& dataLen,bool bPush )
{
	size_t nLen=SIZE_HEAD_BLOCK+sizeof(short)+dataLen;
	char* cache=new char[nLen];
	short nextLen=(short)dataLen;
	ConvertEndian16(&nextLen);
	size_t offset=0;
	memcpy(cache+offset,header_0x020000,SIZE_HEAD_BLOCK);
	offset+=SIZE_HEAD_BLOCK;
	memcpy(cache+offset,&nextLen,sizeof(short));
	offset+=sizeof(short);
	memcpy(cache+offset,plistbin,dataLen);

	if(bPush)
	{
		vector<char> data;
		if(!WritePushPackageToData(cache,nLen,data))
		{
			delete[] cache;
			return false;
		}
		if(data.size()>0)
		{
			AgentSocketSendBytes(&data[0],data.size());
		}
	}
	else
	{
		vector<char> data;
		if(!WritePullPackageToData(cache,nLen,data))
		{
			delete[] cache;
			return false;
		}
		if(data.size()>0)
		{
			SiriSocketSendBytes(&data[0],data.size());
		}
	}

	delete[] cache;

	return true;
}

