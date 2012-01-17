#include "MessageProcessHelper.h"
#include "PackageHajackStream.h"
#include "zlib.h"
#define CHUNK 256

int MessageProcessHelper::ProcessMessageStream( istream& istrm )
{
	PackageHajackStream hajack;
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* 初始化压缩状态 */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	/* 解压，直到流的结尾 */
	do {
		istrm.read((char*)in,CHUNK);
		strm.avail_in = istrm.gcount();
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			if(!hajack.WriteBlock((const char*)out,have,0))
			{
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

	} while (ret != Z_STREAM_END);

	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}


int MessageProcessHelper::InflateMessageStream( istream& istrm )
{
	char head[4];
	istrm.read(head,4);

	std::streamsize count=0;

	FileOutputStream fs("c:\\123.out");
	InflatingInputStream infStrem(istrm);
	
	char* cache=new char[CHUNK];
	while (infStrem.good())
	{
		infStrem.read(cache,CHUNK);
		count=infStrem.gcount();
		fs.write(cache,count);
	}

	fs.flush();
	delete[] cache;
	return 0;
}

void MessageProcessHelper::ProcessStreamSocket( StreamSocket& sockIn,StreamSocket& sockOut,size_t nReadBuffer )
{
	
}

void MessageProcessHelper::ProcessSocketStream( SocketStream& sstrmIn,SocketStream& sstrmOut )
{
	
}
