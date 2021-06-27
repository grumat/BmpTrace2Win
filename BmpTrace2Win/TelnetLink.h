#pragma once

#include "SwoPayload.h"


class CTelnetLink : public ISwoTarget
{
public:
	enum 
	{
		READ_BUF_SIZE = 256,
	};
public:
	CTelnetLink();
	virtual ~CTelnetLink();

public:
	void Serve(unsigned int iPort);
	void Close();
	void PutMessage(const SwoMessage &msg);
	bool IsListening() const { return m_AcceptSock > 0; }

protected:
	void Listen(unsigned int iPort);
	virtual bool IsTargetActive() override
	{
		return IsListening();
	}
	virtual void Send(const SwoMessage &msg) override
	{
		PutMessage(msg);
	}

protected:
	CAtlList<SwoMessage> m_SwoMessages;

protected:
	SOCKET m_ListenSock;
	volatile SOCKET m_AcceptSock;
	volatile bool m_fEnableXmit;
	CComAutoCriticalSection m_Lock;
	typedef CComCritSecLock<CComAutoCriticalSection> CCritSecLock;

protected:
	DWORD StartWriteThread();
	static void __cdecl WriteThread(LPVOID pThis);
	void WriteThread();
	volatile uintptr_t m_hWriteThread;
	HANDLE m_hDoSend;

protected:
	DWORD StartReadThread();
	static void __cdecl ReadThread(LPVOID pThis);
	void ReadThread();
	HANDLE m_hReady;
	volatile uintptr_t m_hReadThread;
};