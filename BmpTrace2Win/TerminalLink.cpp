#include "stdafx.h"
#include "TerminalLink.h"


CTerminalLink::CTerminalLink(const ISwoFormatter &fmt, FILE *pLogFile)
	: m_Fmt(fmt)
	, m_pLogFile(pLogFile)
{
}


CTerminalLink::~CTerminalLink()
{
}


bool CTerminalLink::IsTargetActive() const
{
	return true;
}


static CStringA StripAnsiEscapes(const CStringA &s)
{
	CStringA out;
	out.Preallocate(s.GetLength());
	for (int i = 0; i < s.GetLength(); ++i)
	{
		char c = s[i];
		if (c == '\x1b')
		{
			// Skip CSI escape sequences (ESC [ ... m)
			if (i + 1 < s.GetLength() && s[i + 1] == '[')
			{
				while (i < s.GetLength() && s[i] != 'm')
					++i;
			}
		}
		else
		{
			out += c;
		}
	}
	return out;
}


void CTerminalLink::Send(const SwoMessage &msg)
{
	CStringA formatted = m_Fmt.Format(msg);
	fputs(formatted, stdout);
	if (m_pLogFile)
	{
		CStringA stripped = StripAnsiEscapes(formatted);
		fputs(stripped, m_pLogFile);
		fflush(m_pLogFile);
	}
}
