#pragma once


struct SwoMessage
{
	int ctx;
	CStringA msg;

	void Clear()
	{
		ctx = -1;
		msg.Empty();
	}
	bool IsClear() const
	{
		return msg.IsEmpty();
	}
};


class ISwoTarget
{
public:
	virtual bool IsTargetActive() = 0;
	virtual void Send(const SwoMessage &msg) = 0;
};

