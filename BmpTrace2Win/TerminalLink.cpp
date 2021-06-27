#include "stdafx.h"
#include "TerminalLink.h"


CTerminalLink::CTerminalLink(const ISwoFormatter &fmt)
	: m_Fmt(fmt)
{
}


CTerminalLink::~CTerminalLink()
{
}


bool CTerminalLink::IsTargetActive() const
{
	return true;
}


void CTerminalLink::Send(const SwoMessage &msg)
{
	fputs(m_Fmt.Format(msg), stdout);
}
