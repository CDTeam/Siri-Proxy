#include "AppContext.h"
#include "resource.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "libplist.h"

AppContext* AppContext::__sigleton=0x00;
#define EXCEPTION_CODE 1

extern void ConvertEndian16(void* lpMem );
extern void PlistFreeMem(void* memblock);

AppContext::AppContext(void)
{
	HRSRC hRes=FindResourceA(NULL,MAKEINTRESOURCEA(IDR_PLIST_RESPONSE),"PLIST_TMPL");
	DWORD dwSize=SizeofResource(NULL,hRes);
	HGLOBAL hGlobal=LoadResource(NULL,hRes);
	LPVOID lpRes=LockResource(hGlobal);
	char* buffer=(char*)lpRes;
	__plist_speak_tmpl.clear();
	__plist_speak_tmpl.append(buffer,dwSize);
	FreeResource(hGlobal);

	
}

AppContext::~AppContext(void)
{
}

AppContext* AppContext::Current()
{
	if(0x00==__sigleton)
	{
		__sigleton=new AppContext();
	}
	return __sigleton;
}

static size_t plist_xml_to_bin(const char* strXML,const size_t strLen,char** pplist_bin)
{
	__try{
		plist_t plist=NULL;
		plist_from_xml(strXML,strLen,&plist);
		char* plistbin=NULL;
		uint32_t dwLen=0;
		plist_to_bin(plist,&plistbin,&dwLen);
		char* ptr=new char[dwLen];
		memcpy(ptr,plistbin,dwLen);
		*pplist_bin=ptr;
		PlistFreeMem(plistbin);
		plist_free(plist);
		return dwLen;
	}__except(EXCEPTION_CODE){
		return 0;
	}
}

bool AppContext::GenFakePlistMessage(const string& content,vector<char>& ret)
{
	static const char header_0x020000[]={0x02,0x00,0x00};
	static const size_t SIZE_HEAD_BLOCK=3;

	string text=__plist_speak_tmpl;
	char* plistbin=NULL;
	uint32_t dwLen=0;
	const char* strXML=text.c_str();
	size_t strLen=text.size();
	dwLen=plist_xml_to_bin(strXML,strLen,&plistbin);

	if(!dwLen) return false;

	size_t nLen=0;
	char *cache=NULL;
	nLen=SIZE_HEAD_BLOCK+sizeof(short)+dwLen;
	ret.resize(nLen);
	short nextLen=(short)dwLen;
	ConvertEndian16(&nextLen);
	size_t offset=0;
	memcpy(&ret[offset],header_0x020000,SIZE_HEAD_BLOCK);
	offset+=SIZE_HEAD_BLOCK;
	memcpy(&ret[offset],&nextLen,sizeof(short));
	offset+=sizeof(short);
	memcpy(&ret[offset],plistbin,dwLen);

	delete[] plistbin;
	return true;
}
