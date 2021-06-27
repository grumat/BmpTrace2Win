#pragma once

#include "SwoPayload.h"


// Using an ATL map, instead of std::set, as STL keeps always being part of 
// the problem and not the solution. Just another reason for C++ downfall...
typedef CRBMap<GUID, GUID> SetOfGuids;


class CBmpSwo
{
public:
	CBmpSwo();
	virtual ~CBmpSwo();
	enum
	{
		VID = 0x1d50,
		PID = 0x6018,
		MI = 0x05,
		READ_BUFFER_BYTES = 4096
	};

public:
	void Open();
	void Close();
	void DoTrace(ISwoTarget &itf);

protected:
	CString GetDevice();
	bool CollectGuidInterfaces(SetOfGuids &guids);
	CString GetStringDescriptor(UCHAR index);

protected:
	WinUSB::CDevice m_Device;
	USB_DEVICE_DESCRIPTOR m_DeviceDescriptor;
	USB_CONFIGURATION_DESCRIPTOR m_ConfigDescriptor;
	UCHAR m_AlternateSetting;
	UCHAR m_PipeIn;
	UCHAR m_PipeOut;
	CString m_StringMfg;
	CString m_StringDescriptor;
	CString m_StringSerial;

protected:
	enum ProtoState { ITM_IDLE, ITM_SYNCING, ITM_TS, ITM_SWIT };
	void PumpData(ISwoTarget &itf, const BYTE c);
	void HandleTS(ISwoTarget &itf);
	void HandleSWIT(ISwoTarget &itf);
	void Flush(ISwoTarget &itf);

	ProtoState m_PState;
	size_t m_TargetCount;
	size_t m_CurCount;
	int m_Addr;
	BYTE m_RxPacket[5];
	SwoMessage m_CurMessage;
};
