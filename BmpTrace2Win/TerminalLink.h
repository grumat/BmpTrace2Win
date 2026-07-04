#pragma once

#include "SwoPayload.h"


class CTerminalLink : public ISwoTarget
{
public:
	CTerminalLink(const ISwoFormatter &fmt, FILE *pLogFile = NULL);
	virtual ~CTerminalLink();

	virtual bool IsTargetActive() const override;
	virtual void Send(const SwoMessage &msg) override;
	virtual void Close() override {}

protected:
	const ISwoFormatter &m_Fmt;
	FILE *m_pLogFile;
};


