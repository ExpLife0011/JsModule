#include "pips.h"
#include <iostream>
#include "v8pp/convert.hpp"
#include "v8pp/call_v8.hpp"
#include "v8.h"
#include <windows.h>


namespace common{
	CPips::CPips()
	{
		m_bCancel = false;
	}


	CPips::~CPips()
	{
	}

	static void v8_arg_count(v8::FunctionCallbackInfo<v8::Value> const& args)
	{
		v8::HandleScope handle_scope(args.GetIsolate());

		for (int i = 0; i < args.Length(); ++i)
		{
			if (i > 0) std::cout << ' ';
			v8::String::Utf8Value str(args[i]);
			string s = *str;

			continue;
		}


		args.GetReturnValue().Set(args.Length());
	}

	void Convert(const char* strIn, char* strOut, int sourceCodepage, int targetCodepage)
	{
		int len = strlen(strIn);
		int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strIn, -1, NULL, 0);
		wchar_t* pUnicode;
		pUnicode = new wchar_t[unicodeLen + 1];
		memset(pUnicode, 0, (unicodeLen + 1)*sizeof(wchar_t));
		MultiByteToWideChar(sourceCodepage, 0, strIn, -1, (LPWSTR)pUnicode, unicodeLen);
		BYTE * pTargetData = NULL;
		int targetLen = WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char *)pTargetData, 0, NULL, NULL);
		pTargetData = new BYTE[targetLen + 1];
		memset(pTargetData, 0, targetLen + 1);
		WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char *)pTargetData, targetLen, NULL, NULL);
		strcpy(strOut, (char*)pTargetData);
		delete pUnicode;
		delete pTargetData;
	}

	std::string MbToUtf8(string strIn)
	{
		char chOut2[10240] = { 0 };
		Convert(strIn.c_str(), chOut2, CP_ACP, CP_UTF8);
		string strOut = chOut2;
		return strOut;
	}

	std::string Utf8ToMb(string strIn)
	{
		char chOut2[10240] = { 0 };
		Convert(strIn.c_str(), chOut2, CP_UTF8, CP_ACP);
		string strOut = chOut2;
		return strOut;
	}

	int CPips::PipsCallback(void *clientp, const char* szContent)
	{
		///////////////////////
		CPips* dd = (CPips*)clientp;
		if (dd && !dd->m_strCallbackName.empty())
		{
			v8::Isolate* isolate = dd->m_isolate;
			v8::Local<v8::Context> context = isolate->GetCurrentContext();

			v8::Local<v8::String> funName = v8::String::NewFromUtf8(isolate, dd->m_strCallbackName.c_str(), v8::NewStringType::kNormal).ToLocalChecked();

			v8::Context::Scope context_scope(context);

			v8::Local<v8::Object> gObj = context->Global();
			v8::Local<v8::Value> value = gObj->Get(funName);
			v8::Local<v8::Function> fun_execute = v8::Local<v8::Function>::Cast(value);

			//
			v8::Local<v8::Value> args[1];
			
			string strUtf8 = MbToUtf8(szContent);

			v8::Local<v8::String> str = v8::String::NewFromUtf8(isolate, strUtf8.c_str(), v8::NewStringType::kNormal).ToLocalChecked();

			args[0] = str;
			fun_execute->Call(gObj, 1, args);
		}

		return 0;
	}


	int CPips::start(const char* szParam1, const char* szParam2)
	{
		/////////////
		SECURITY_ATTRIBUTES sa;//����һ����ȫ���Եı���
		HANDLE hRead, hWrite; //�ܵ��Ķ�д�������
		sa.nLength = sizeof(SECURITY_ATTRIBUTES); //��ȡ��ȫ���Եĳ���
		sa.lpSecurityDescriptor = NULL;  //ʹ��ϵͳĬ�ϵİ�ȫ������ 
		sa.bInheritHandle = TRUE;  //�����Ľ�������̳о��

		if (!CreatePipe(&hRead, &hWrite, &sa, 0))  //���������ܵ�
		{
			return 1;
		}
		STARTUPINFOA si; //������Ϣ�ṹ�����
		PROCESS_INFORMATION pi;//��Ҫ����Ľ�����Ϣ�ı���

		ZeroMemory(&si, sizeof(STARTUPINFOA)); //��ΪҪ�����������������ոñ���
		si.cb = sizeof(STARTUPINFOA); //�ṹ��ĳ���
		GetStartupInfoA(&si);
		si.hStdError = hWrite;
		si.hStdOutput = hWrite;  //�´������̵ı�׼�������д�ܵ�һ��
		si.wShowWindow = SW_HIDE;  //���ش��� 
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

		//////////////////////
		string strParam1 = Utf8ToMb(szParam1);
		string strParam2 = Utf8ToMb(szParam2);

		char cmdline[200] = { 0 };
		sprintf(cmdline, "cmd /C %s %s", strParam1.c_str(), strParam2.c_str());

		if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi))  //�����ӽ���
		{
			return 2;
		}
		CloseHandle(hWrite);  //�رչܵ����

		char buffer[4096] = { 0 };
		DWORD bytesRead;

		while (true)
		{
			if (ReadFile(hRead, buffer, 4095, &bytesRead, NULL) == NULL)  //��ȡ�ܵ����ݵ�buffer��  
				break;

			OutputDebugStringA(buffer);
			OutputDebugStringA("\r\n");

			
			PipsCallback(this, buffer);

		}
		CloseHandle(hRead); //�رն�ȡ���

		return 0;
	}

	void CPips::setCallback(v8::Local<v8::Function> js_callback)
	{
		v8::Local<v8::Value> vFuncName = js_callback->GetName();
		v8::String::Utf8Value vStrName(vFuncName);
		const char* strName = *vStrName;
		m_strCallbackName = strName;

		m_isolate = js_callback->GetIsolate();

		return;
	}

};//namespace common