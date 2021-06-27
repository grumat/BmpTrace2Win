#include "stdafx.h"
#include "BmpSwo.h"
#include "TheLogger.h"


DWORD SwoMessage::m_TimeBase;


using namespace Logger;


CBmpSwo::CBmpSwo()
	: m_AlternateSetting(0)
	, m_PipeIn(0)
	, m_PipeOut(0)
	, m_PState(ITM_IDLE)
	, m_TargetCount(0)
	, m_CurCount(0)
	, m_Addr(0)
	, m_fLFSeen(false)
{
	ZeroMemory(&m_DeviceDescriptor, sizeof(m_DeviceDescriptor));
	ZeroMemory(&m_ConfigDescriptor, sizeof(m_ConfigDescriptor));
	SwoMessage::InitTimeBase();
}


CBmpSwo::~CBmpSwo()
{
	Close();
}


bool CBmpSwo::CollectGuidInterfaces(SetOfGuids &guids)
{
	CRegKey dev_reg_root;
	// Open Black Magic Probe's Trace interface device key
	LONG err = dev_reg_root.Open(
		HKEY_LOCAL_MACHINE
		, _T("SYSTEM\\CurrentControlSet\\Enum\\USB\\VID_1D50&PID_6018&MI_05")
		, KEY_READ);
	// Sorry, not installed (use Zadig)
	if(err != ERROR_SUCCESS)
		return false;
	CStringArray arr;
	// Scan all existing profiles, as Zadig creates more and more interface GUIDs,
	// if you repeat installations
	for (DWORD i = 0; ; ++i)
	{
		CString buf;
		DWORD size = MAX_PATH + 1;
		err = dev_reg_root.EnumKey(i, CStrBuf(buf, MAX_PATH), &size);
		if (err)
			break;
		arr.Add(buf);
	}
	// Sorry, nothing found
	if (arr.GetCount() == 0)
		return false;
	// Scan all installed device (including those not connected)
	for (size_t i = 0; i < arr.GetCount(); ++i)
	{
		// Build the path for the sub-key
		CString path;
		path.Format(_T("SYSTEM\\CurrentControlSet\\Enum\\USB\\VID_%04X&PID_%04X&MI_%02X\\%s\\Device Parameters")
					 , VID
					 , PID
					 , MI
					 , arr[i].GetString()
					);
		CRegKey dev_inst;
		// Access the device key
		err = dev_inst.Open(HKEY_LOCAL_MACHINE, path, KEY_READ);
		if(err == ERROR_SUCCESS)
		{
			/*
			** Windows allows interface GUID's to be stored in DeviceInterfaceGUID and 
			** DeviceInterfaceGUIDs. Here we are going to load them.
			*/
			// First a buffer that it's big enough for GUIDs
			CAtlArray<BYTE> big_buf;
			big_buf.SetCount(4096 * sizeof(TCHAR));

			// Establishes handy aliases for the buffer
			ULONG size = (ULONG)big_buf.GetCount();
			LPTSTR pBuf = (LPTSTR)big_buf.GetData();
			// Zadig usually stores in Multi-string values
			err = dev_inst.QueryMultiStringValue(_T("DeviceInterfaceGUIDs"), pBuf, &size);
			if (err == ERROR_SUCCESS)
			{
				// Scan string set (even if usually just one is stored)
				while (*pBuf)
				{
					CString tmp;
					while (*pBuf)
					{
						/*
						** Brave Microsoft! UuidFromString doesn't recognize the common 
						** "representation form" created by yourselves!.
						** Again, Forbe's top-10 stealing my hard earned time!
						*/
						if (*pBuf != _T('{') && *pBuf != _T('}'))
							tmp += *pBuf;
						++pBuf;
					}
					// Parse the string
					GUID guid;
					if (UuidFromString((RPC_WSTR)tmp.GetString(), &guid) == RPC_S_OK)
						guids.SetAt(guid, guid);
					++pBuf;
				}
			}
			// Single string reg entry
			size = (ULONG)big_buf.GetCount();
			err = dev_inst.QueryStringValue(_T("DeviceInterfaceGUID"), pBuf, &size);
			if (err == ERROR_SUCCESS)
			{
				// Just parse the string
				GUID guid;
				if (UuidFromString((RPC_WSTR)pBuf, &guid) == RPC_S_OK)
					guids.SetAt(guid, guid);
			}
		}
	}
	return true;
}


CString CBmpSwo::GetDevice()
{
	SetOfGuids guids;
	if (!CollectGuidInterfaces(guids))
		return _T("");

	Info(_T("Opening Black Magic Probe interface\n"));
	CStringArray all_dev_names;
	for (POSITION pos = guids.GetHeadPosition(); pos;)
	{
		const SetOfGuids::CPair *pair = guids.GetNext(pos);
		WinUSB::StringArray dev_names_for_interf;
		if (!WinUSB::CDevice::EnumerateDevices(&pair->m_key, VID, PID, dev_names_for_interf))
			AtlThrowLastWin32();
		all_dev_names.Append(dev_names_for_interf);
	}
	Debug(_T("  Found %d ICDI JTAG devices\n"), all_dev_names.GetCount());
	if (all_dev_names.GetCount() == 0)
	{
		Error(_T("No ICDI device found VID=%04X/PID=%04X\n"), VID, PID);
		AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
	}
	Debug(_T("Using device '%s'\n"), all_dev_names[0].GetString());
	return all_dev_names[0];
}


CString CBmpSwo::GetStringDescriptor(UCHAR index)
{
	WinUSB::String res;
	UCHAR stringDescriptor[1024];
	ULONG nTransferred = 0;
	if (m_Device.GetDescriptor(USB_STRING_DESCRIPTOR_TYPE, index, 0, stringDescriptor, sizeof(stringDescriptor), &nTransferred) != 0
		&& (nTransferred >= sizeof(USB_STRING_DESCRIPTOR)))
	{
		USB_STRING_DESCRIPTOR *pStringDescriptor = reinterpret_cast<USB_STRING_DESCRIPTOR *>(&stringDescriptor);
		pStringDescriptor->bString[(pStringDescriptor->bLength - 2) / sizeof(WCHAR)] = L'\0';
		res = CT2WEX<512>(pStringDescriptor->bString);
	}
	return res;
}


void CBmpSwo::Open()
{
	WinUSB::String name = GetDevice();
	if (!m_Device.Initialize(name.GetString()))
	{
		DWORD dw = GetLastError();
		if (dw == E_INVALIDARG)
			Error(_T("This utility requires a WinUSB driver for the ICDI device\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}
	m_PipeIn = m_PipeOut = 0;
	// Let's query for the device descriptor
	Detail(_T("Getting device descriptor\n"));
	memset(&m_DeviceDescriptor, 0, sizeof(m_DeviceDescriptor));
	ULONG nTransferred = 0;
	if (!m_Device.GetDescriptor(USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, reinterpret_cast<PUCHAR>(&m_DeviceDescriptor), sizeof(m_DeviceDescriptor), &nTransferred))
		AtlThrowLastWin32();
	if (m_DeviceDescriptor.iManufacturer)
		m_StringMfg = GetStringDescriptor(m_DeviceDescriptor.iManufacturer);
	if (m_DeviceDescriptor.iProduct)
		m_StringDescriptor = GetStringDescriptor(m_DeviceDescriptor.iProduct);
	if (m_DeviceDescriptor.iSerialNumber)
		m_StringSerial = GetStringDescriptor(m_DeviceDescriptor.iSerialNumber);
	if (IsDebugLevel())
	{
		Debug(_T("  bLength: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bLength));
		Debug(_T("  bDescriptorType: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDescriptorType));
		Debug(_T("  bcdUSB: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.bcdUSB));
		Debug(_T("  bDeviceClass: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceClass));
		Debug(_T("  bDeviceSubClass: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceSubClass));
		Debug(_T("  bDeviceProtocol: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.bDeviceProtocol));
		Debug(_T("  bMaxPacketSize0: 0x%02X bytes\n"), static_cast<int>(m_DeviceDescriptor.bMaxPacketSize0));
		Debug(_T("  idVendor: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.idVendor));
		Debug(_T("  idProduct: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.idProduct));
		Debug(_T("  bcdDevice: 0x%04X\n"), static_cast<int>(m_DeviceDescriptor.bcdDevice));
		Debug(_T("  iManufacturer: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iManufacturer));
		if (m_StringMfg.IsEmpty())
			Warning(_T("    No manufacturer name was provided\n"));
		else
			Debug(_T("    Manufacturer name: %s\n"), m_StringMfg.GetString());
		Debug(_T("  iProduct: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iProduct));
		if (m_StringDescriptor.IsEmpty())
			Warning(_T("    No product name was provided\n"));
		else
			Debug(_T("    Product name: %s\n"), m_StringDescriptor.GetString());
		Debug(_T("  iSerialNumber: 0x%02X\n"), static_cast<int>(m_DeviceDescriptor.iSerialNumber));
		if (m_StringSerial.IsEmpty())
			Warning(_T("    No serial number was provided\n"));
		else
			Debug(_T("    Serial Number: %s\n"), m_StringSerial.GetString());
	}

	// Let's query for the configuration descriptor
	Detail(_T("Getting configuration descriptor\n"));
	memset(&m_ConfigDescriptor, 0, sizeof(m_ConfigDescriptor));
	nTransferred = 0;
	if (!m_Device.GetDescriptor(USB_CONFIGURATION_DESCRIPTOR_TYPE, 0, 0, reinterpret_cast<PUCHAR>(&m_ConfigDescriptor), sizeof(m_ConfigDescriptor), &nTransferred))
		AtlThrowLastWin32();
	if (IsDebugLevel())
	{
		Debug(_T("  bLength: %d\n"), static_cast<int>(m_ConfigDescriptor.bLength));
		Debug(_T("  bDescriptorType: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bDescriptorType));
		Debug(_T("  wTotalLength: 0x%04X\n"), static_cast<int>(m_ConfigDescriptor.wTotalLength));
		Debug(_T("  bNumInterfaces: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bNumInterfaces));
		Debug(_T("  bConfigurationValue: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bConfigurationValue));
		Debug(_T("  iConfiguration: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.iConfiguration));
		if (m_ConfigDescriptor.iConfiguration)
		{
			CString s = GetStringDescriptor(m_ConfigDescriptor.iConfiguration);
			Debug(_T("    Configuration name: %s\n"), s.GetString());
		}
		Debug(_T("  bmAttributes: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.bmAttributes));
		Debug(_T("  MaxPower: 0x%02X\n"), static_cast<int>(m_ConfigDescriptor.MaxPower));
	}

	// Let's query the device for info
	if (IsDebugLevel())
	{
		Debug(_T("Querying device speed\n"));
		UCHAR deviceSpeed = 0;
		ULONG nBufferLen = sizeof(deviceSpeed);
		if (!m_Device.QueryDeviceInformation(DEVICE_SPEED, &nBufferLen, &deviceSpeed))
			AtlThrowLastWin32();
		switch (deviceSpeed)
		{
		case LowSpeed:
			Debug(_T("  The device is operating at low speed\n"));
			break;
		case FullSpeed:
			Debug(_T("  The device is operating at full speed\n"));
			break;
		case HighSpeed:
			Debug(_T("  The device is operating at high speed\n"));
			break;
		default:
			Debug(_T("  The device is operating at unknown %d speed\n"), (int)deviceSpeed);
			break;
		}
	}

	// Current alternate setting
	m_AlternateSetting = 0;
	if (!m_Device.GetCurrentAlternateSetting(&m_AlternateSetting))
		AtlThrowLastWin32();
	Debug(_T("Current alternate setting %d\n"), static_cast<int>(m_AlternateSetting));

	//Let's query for interface and pipe information
	Detail(_T("Querying device interfaces and pipes\n"));
	for (UCHAR interfaceNumber = 0; interfaceNumber <= 0xFF; ++interfaceNumber)
	{
		USB_INTERFACE_DESCRIPTOR interfaceDescriptor;
		memset(&interfaceDescriptor, 0, sizeof(interfaceDescriptor));
		if (!m_Device.QueryInterfaceSettings(interfaceNumber, &interfaceDescriptor))
		{
			DWORD err = GetLastError();
			if (err == ERROR_NO_MORE_ITEMS)
				break;
			AtlThrow(HRESULT_FROM_WIN32(err));
		}
		else
		{
			Debug(_T("  Interface %d details\n"), static_cast<int>(interfaceNumber));
			Debug(_T("    bLength: %d\n"), static_cast<int>(interfaceDescriptor.bLength));
			Debug(_T("    bDescriptorType: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bDescriptorType));
			Debug(_T("    bInterfaceNumber: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceNumber));
			Debug(_T("    bAlternateSetting: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bAlternateSetting));
			Debug(_T("    bNumEndpoints: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bNumEndpoints));
			Debug(_T("    bInterfaceClass: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceClass));
			Debug(_T("    bInterfaceSubClass: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceSubClass));
			Debug(_T("    bInterfaceProtocol: 0x%02X\n"), static_cast<int>(interfaceDescriptor.bInterfaceProtocol));
			Debug(_T("    iInterface: 0x%02X\n"), static_cast<int>(interfaceDescriptor.iInterface));

		}
		for (UCHAR pipeNumber = 0; pipeNumber <= 0xFF; ++pipeNumber)
		{
			WINUSB_PIPE_INFORMATION pipeInformation;
			memset(&pipeInformation, 0, sizeof(pipeInformation));
			if (!m_Device.QueryPipe(interfaceNumber, pipeNumber, &pipeInformation))
			{
				DWORD err = GetLastError();
				if (err == ERROR_NO_MORE_ITEMS)
					break;
				AtlThrow(HRESULT_FROM_WIN32(err));
			}
			else
			{
				WINUSB_PIPE_INFORMATION_EX pipeInformationEx;
				memset(&pipeInformationEx, 0, sizeof(pipeInformationEx));
				if (!(m_Device.QueryPipEx(interfaceNumber, pipeNumber, &pipeInformationEx)))
					AtlThrowLastWin32();
				switch (pipeInformation.PipeType)
				{
				case UsbdPipeTypeControl:
					Debug(_T("      Control Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				case UsbdPipeTypeIsochronous:
					Debug(_T("      Isochronous Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				case UsbdPipeTypeBulk:
					if (USB_ENDPOINT_DIRECTION_IN(pipeInformation.PipeId))
					{
						Debug(_T("      Bulk IN Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
						if (m_PipeIn)
						{
							Error(_T("Too many input pipes found. Only one was expected.\n"));
							AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
						}
						m_PipeIn = pipeInformation.PipeId;
					}
					if (USB_ENDPOINT_DIRECTION_OUT(pipeInformation.PipeId))
					{
						Debug(_T("      Bulk OUT Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
						if (m_PipeOut)
						{
							Error(_T("Too many input pipes found. Only one was expected.\n"));
							AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
						}
						m_PipeOut = pipeInformation.PipeId;
					}
					break;
				case UsbdPipeTypeInterrupt:
					Debug(_T("      Interrupt Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				default:
					Debug(_T("      Unknown [%d] Pipe, ID:0x%02X, Max Packet Size:%d, Interval:%d, Max bytes per interval:%d\n"), static_cast<int>(pipeInformation.PipeType), static_cast<int>(pipeInformation.PipeId), static_cast<int>(pipeInformation.MaximumPacketSize), static_cast<int>(pipeInformation.Interval), static_cast<int>(pipeInformationEx.MaximumBytesPerInterval));
					break;
				}
			}
		}
	}
	// Validate pipes
	if (m_PipeIn == 0 /*|| m_PipeOut != 0*/)
	{
		Error(_T("Communication pipes could not be found. Device can't be used!\n"));
		AtlThrow(HRESULT_FROM_WIN32(ERROR_BAD_UNIT));
	}

	long val = 10;
	m_Device.SetPipePolicy(m_PipeIn, PIPE_TRANSFER_TIMEOUT, sizeof(val), &val);
	// Time base
	SwoMessage::InitTimeBase();
}


void CBmpSwo::Close()
{
	m_Device.Free();
}


void CBmpSwo::Flush(ISwoTarget &itf)
{
	if(!m_CurMessage.IsClear())
	{
		itf.Send(m_CurMessage);
		m_CurMessage.Clear();
	}
	m_fLFSeen = false;
}


void CBmpSwo::HandleTS(ISwoTarget &itf)
{
}


void CBmpSwo::HandleSWIT(ISwoTarget &itf)
{
	if (!m_CurMessage.IsClear() && m_Addr != m_CurMessage.chan)
		Flush(itf);
	m_CurMessage.chan = m_Addr;
	for (size_t i = 0; i < m_CurCount; ++i)
	{
		char ch = m_RxPacket[i];
		if (ch == '\r' || ch == '\n')
			m_fLFSeen = true;
		else if (m_fLFSeen)
			Flush(itf);
		if(ch)
			m_CurMessage.msg += ch;
	}
	if(m_fLFSeen)
		Flush(itf);
}


void CBmpSwo::PumpData(ISwoTarget &itf, const BYTE c)
{
	switch (m_PState)
	{
		// -----------------------------------------------------
	case ITM_IDLE:
		if (c == 0b01110000)
		{
			/* This is an overflow packet */
			Debug(_T("Overflow!\n"));
			break;
		}
		// **********
		if (c == 0)
		{
			/* This is a sync packet - expect to see 4 more 0's followed by 0x80 */
			m_TargetCount = 4;
			m_CurCount = 0;
			m_PState = ITM_SYNCING;
			break;
		}
		// **********
		if (!(c & 0x0F))
		{
			m_CurCount = 1;
			/* This is a timestamp packet */
			m_RxPacket[0] = c;

			if (!(c & 0x80))
			{
				/* A one byte output */
				HandleTS(itf);
			}
			else
			{
				m_PState = ITM_TS;
			}
			break;
		}
		// **********
		if ((c & 0x0F) == 0x04)
		{
			/* This is a reserved packet */
			break;
		}
		// **********
		if (!(c & 0x04))
		{
			/* This is a SWIT packet */
			if ((m_TargetCount = c & 0x03) == 3)
				m_TargetCount = 4;
			m_Addr = (c & 0xF8) >> 3;
			m_CurCount = 0;
			m_PState = ITM_SWIT;
			break;
		}
		// **********
		Debug(_T("Illegal packet start in IDLE state\n"));
		break;
		// -----------------------------------------------------
	case ITM_SWIT:
		m_RxPacket[m_CurCount] = c;
		m_CurCount++;

		if (m_CurCount >= m_TargetCount)
		{
			m_PState = ITM_IDLE;
			HandleSWIT(itf);
		}
		break;
		// -----------------------------------------------------
	case ITM_TS:
		m_RxPacket[m_CurCount++] = c;
		if (!(c & 0x80))
		{
			/* We are done */
			HandleTS(itf);
		}
		else
		{
			if (m_CurCount > 4)
			{
				/* Something went badly wrong */
				m_PState = ITM_IDLE;
			}
			break;
		}

		// -----------------------------------------------------
	case ITM_SYNCING:
		if ((c == 0) && (m_CurCount < m_TargetCount))
		{
			m_CurCount++;
		}
		else
		{
#if 0
			if (c == 0x80)
			{
				m_PState = ITM_IDLE;
			}
			else
			{
				/* This should really be an UNKNOWN state */
				m_PState = ITM_IDLE;
			}
#else
			m_PState = ITM_IDLE;
#endif
		}
		break;
		// -----------------------------------------------------
	}
#ifdef PRINT_TRANSITIONS
	Debug("%s\n", _protoNames[p]);
#endif
}


void CBmpSwo::DoTrace(ISwoTarget &itf)
{
	BYTE buffer[READ_BUFFER_BYTES + 1];
	ULONG xfered;

	while (itf.IsTargetActive())
	{
		if (!m_Device.ReadPipe(m_PipeIn, buffer, READ_BUFFER_BYTES, &xfered, NULL))
		{
			DWORD err = GetLastError();
			if (err != ERROR_SEM_TIMEOUT)
				break;
		}
		if(xfered)
		{
			buffer[xfered] = 0;
			for (size_t i = 0; i < xfered; ++i)
				PumpData(itf, buffer[i]);
		}
	}
}

