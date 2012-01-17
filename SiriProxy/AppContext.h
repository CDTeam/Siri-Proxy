#pragma once
#include <string>
#include <vector>

using namespace std;
class AppContext
{
	string __plist_speak_tmpl;
	static AppContext* __sigleton;
	AppContext(void);
public:
	static AppContext* Current();
	bool GenFakePlistMessage(const string& content,vector<char>& ret);
	~AppContext(void);
};


