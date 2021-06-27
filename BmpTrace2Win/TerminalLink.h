#pragma once

#include "SwoPayload.h"


class CTerminalLink : public ISwoTarget
{
public:
	CTerminalLink(const ISwoFormatter &fmt);
	virtual ~CTerminalLink();

	virtual bool IsTargetActive() const override;
	virtual void Send(const SwoMessage &msg) override;
	virtual void Close() override {}

protected:
	const ISwoFormatter &m_Fmt;
};


