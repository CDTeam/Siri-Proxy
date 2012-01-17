#include "stdafx.h"
typedef int (*FuncServerMain)(int argc, char** argv);

int main(int argc, char* argv[])
{
	HMODULE hMod=LoadLibraryA("SiriProxy.dll");
	if(hMod)
	{
		FuncServerMain pfn=(FuncServerMain)GetProcAddress(hMod,"ServerMain");
		if(pfn)
		{
			pfn(argc,argv);
		}
	}
	return 0;
}

