#pragma once


//! A single SWO message
struct SwoMessage
{
	//! Channel number
	int chan;
	//! The timestamp of the message
	DWORD time;
	//! Stores message contents
	CStringA msg;

	//! Clears the object
	void Clear()
	{
		chan = -1;
		time = GetTickCount() - m_TimeBase;
		msg.Empty();
	}
	//! Checks if message is empty
	bool IsClear() const
	{
		return msg.GetLength() == 0;
	}
	//! Restart time base
	static void InitTimeBase()
	{
		m_TimeBase = GetTickCount();
	}
protected:
	//! A relative time base
	static DWORD m_TimeBase;
};


//! Interface class for a message target
class ISwoTarget
{
public:
	virtual ~ISwoTarget() {}
	//! Checks if message transmission is still active
	virtual bool IsTargetActive() const = 0;
	//! Sends a SWo message to the target
	virtual void Send(const SwoMessage &msg) = 0;
	//! Closes target, freeing resources
	virtual void Close() = 0;
};


//! Interface class for a text formatter
class ISwoFormatter
{
public:
	//! Formats the message before it is dispatched
	virtual CStringA Format(const SwoMessage &swo) const = 0;
};

