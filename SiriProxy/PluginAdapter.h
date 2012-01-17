// Copyright (c) 2011,cd-team.org.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
#pragma once
#include <vector>
#include <string>
#include <plist/plist++.h>
#include "StdAfx.h"
using namespace std;
struct PluginInfo;
struct PlistPackageArgs;
struct CommandCallResult;

//定义插件初始化函数指针原型
typedef bool (__stdcall*PluginFuncInitialize)();

////定义插件释放时函数指针原型
typedef void (__stdcall*PluginFuncUnInitialize)();

//定义释放PluginInfo*的函数指针原型
typedef void (__stdcall*FuncPluginInfoFree)(PluginInfo*);

//释放插件动态数组的函数指针原型
typedef void (__stdcall*FuncPluginInfoArrayFree)(PluginInfo**);

//定义枚举可用插件的函数指针原型
//返回值:PluginInfo*的动态数组
//lpArraySize:如果返回的动态数组不为空，该值保存动态数组的长度
typedef PluginInfo** (__stdcall*FuncEnumAvailablePlugins)(size_t* lpArraySize);

//定义当命令匹配时调用的函数的函数指针的原型
//args:封装从服务器解包plist而来的相关重要参数,请参见:PlistPackageArgs结构
//返回:封装返回给服务器的结果,请参见CommandCallResult结构
typedef CommandCallResult* (__stdcall*PluginFuncCommandCall)(PlistPackageArgs* args);

//定义释放CommandCallResult*指针的函数指针原型
typedef void (__stdcall*FuncFreeCommandCallResult)(CommandCallResult*);

//定义插件主入口函数指针原型
typedef PluginInfo* (*FnPluginInitMain)();

//命令匹配时，封装相关信息
struct PlistPackageArgs 
{
	//匹配的plist包的AceId字段的值，固定长度为37个字节,一个guid字符串
	char aceId[37];

	//匹配的plist包的refId字段的值，固定长度为37个字节,一个guid字符串
	char refId[37];

	//发送的文本的第一个单词
	char* sendText;

	//Siri服务对该次请求的原始响应文本
	char* responseText;
};

//CommandCallResult结果类型
typedef enum ResultType
{
	//结果是纯文本
	RESULT_TEXT,

	//结果是xml形式的plist
	RESULT_PLIST_XML
};

struct CommandCallResult
{
	//CommandCallResult结果类型
	ResultType type;

	//一个c形式的字符串，表示对命令响应的处理结果
	char* result_text;

	//指示用什么函数来释放为CommandCallResult*动态分配的内存
	FuncFreeCommandCallResult FreeCommandCallResult;
};

//定义插件信息结构
struct PluginInfo
{
	//形如:'D10C80A1-6160-455e-96C2-1D40029BFF95'的字符串，表示插件的Id
	char PluginId[37];

	//c形式的字符串，表示插件名称，对插件进行描述
	char* PluginName;

	//命令文本,当发送的首个单词,或者者服务器不可识别的一段文本等于该文本时，该plist包将会被捕捉，然后发送到插件处理模块
	char* CommandText;

	//命令的正则匹配字符串,当命令的首个单词，或者服务器不可识别的一段文本与该正则匹配时,list包将会被捕捉，然后发送到插件处理模块
	//该字段的优先级高于CommandText
	char* CommandMatchPattern;

	//当命令命中时，需要调用的插件模块的函数的指针
	PluginFuncCommandCall  OnCommandCall;

	//初始化插件模块需要调用的函数的指针
	PluginFuncInitialize   InitPluginModule;

	//反初始化插件模块需要调用的函数的指针,当前没有任何地方实际调用该函数，保留用
	PluginFuncUnInitialize UnInitPluginModule;

	//当需要释放由插件模块返回PluginInfo*的指针时，使用该字段提供的函数指针来释放动态分配的内存
	FuncPluginInfoFree	   PluginInfoFree;
};

//PluginInfo的c++友好封装，仅供模块内部使用
struct PluginInfoInternal
{
	string PluginId;
	string PluginName;
	string CommandText;
	string CommandMatchPattern;
	PluginFuncCommandCall  OnCommandCall;
	PluginFuncInitialize   InitPluginModule;
	PluginFuncUnInitialize UnInitPluginModule;
};

//插件管理器类
class PluginMgr
{
	static PluginMgr* __sigleton;
	vector<PluginInfoInternal> plugins;
	Poco::FastMutex _mutex;
	PluginMgr();
public:
	vector<PluginInfoInternal> GetPlugins();
	vector<PluginInfoInternal> FindPluginMatch(const string& text);
	void PushbackPluginInternal(const PluginInfoInternal& info);
	static PluginMgr* Current();
	void FilterPlistBin(char* plistbin,const size_t& dataLen,void* context);
	void LoadPlugin();
};

extern FuncEnumAvailablePlugins pfnEnumAvailablePlugins;
extern FuncPluginInfoArrayFree  pfnPluginInfoArrayFree;