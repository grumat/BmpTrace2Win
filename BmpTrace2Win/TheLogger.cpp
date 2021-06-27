#include "stdafx.h"
#include "TheLogger.h"

namespace Logger
{

CLogger *CLogger::m_pLogger = NULL;


CLogger::CLogger()
{
#ifdef _DEBUG
	m_nLevel = DETAIL_LEVEL;
#else
	m_nLevel = INFO_LEVEL;
#endif
	m_dwStartTick = ::GetTickCount();
	m_dwMainThreadID = GetCurrentThreadId();
	m_Lock.Init();
}


CLogger::~CLogger()
{
}


void CLogger::Dispose()
{
	delete m_pLogger;
	m_pLogger = nullptr;
}

CLogger & CLogger::GetLogger()
{
	static bool registered = false;
	if (!m_pLogger)
	{
		m_pLogger = new CStdioLogger();
		if(!registered)
		{
			registered = true;
			atexit(Dispose);
		}
	}
	return *m_pLogger;
}


void Debug(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(DEBUG_LEVEL, msg, vargs);
	va_end(vargs);
}


void Error(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(ERROR_LEVEL, msg, vargs);
	va_end(vargs);
}


void Warning(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(WARN_LEVEL, msg, vargs);
	va_end(vargs);
}


void Info(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(INFO_LEVEL, msg, vargs);
	va_end(vargs);
}


void Detail(const TCHAR *msg, ...)
{
	va_list vargs;
	va_start(vargs, msg);
	TheLogger().Log(DETAIL_LEVEL, msg, vargs);
	va_end(vargs);
}


void CLogger::Log(Level_e level, const TCHAR *msg, va_list vargs)
{
	if(OnTestLevel(level))
	{
		CComCritSecLock<CComCriticalSection> lock(m_Lock);
		CAtlString s;
		s.FormatV(msg, vargs);
		OnLog(level, s);
	}
}


void CStdioLogger::OnLog(Level_e level, const TCHAR *msg)
{
	CAtlString s;
	DWORD dif = ::GetTickCount() - m_dwStartTick;
	s.Format(_T("%03d.%03d %c> "), dif / 1000, dif % 1000, "TM"[GetCurrentThreadId() == m_dwMainThreadID]);
	if(level == INFO_LEVEL)
	{
		_fputts(s, stdout);
		_fputts(msg, stdout);
	}
	else
	{
		_fputts(s, stderr);
		_fputts(msg, stderr);
		ATLTRACE(_T("%s%s"), (LPCTSTR)s, (LPCTSTR)msg);
	}
}


}	// namespace Logger
