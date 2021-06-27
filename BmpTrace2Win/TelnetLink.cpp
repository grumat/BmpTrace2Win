#include "stdafx.h"
#include "TelnetLink.h"
#include "TheLogger.h"
#include "AppConfig.h"


using namespace Logger;



static void cleanup()
{
	WSACleanup();
}


CTelnetLink::CTelnetLink(const ISwoFormatter &fmt)
	: m_Fmt(fmt)
	, m_ListenSock(0)
	, m_AcceptSock(0)
	, m_fEnableXmit(false)
	, m_hReady(NULL)
	, m_hReadThread(0)
{
	static bool init = false;
	if (!init)
	{
		WORD wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(2, 2);
		if (WSAStartup(wVersionRequested, &wsaData))
		{
			DWORD dw = WSAGetLastError();
			Error(_T("Call to socket() failed.\n"));
			AtlThrow(HRESULT_FROM_WIN32(dw));
		}
		atexit(cleanup);

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			/* Tell the user that we could not find a usable */
			/* WinSock DLL.                                  */
			Error(_T("Could not find a usable version of Winsock.dll\n"));
			AtlThrow(HRESULT_FROM_WIN32(WSAESOCKTNOSUPPORT));
		}
		else
			Debug(_T("The Winsock 2.2 dll was found okay\n"));
	}
}


CTelnetLink::~CTelnetLink()
{
}


void CTelnetLink::Close()
{
	CCritSecLock lock(m_Lock);
	if (m_ListenSock)
	{
		closesocket(m_ListenSock);
		m_ListenSock = 0;
	}
	if (m_AcceptSock)
	{
		closesocket(m_AcceptSock);
		m_AcceptSock = 0;
	}
	m_SwoMessages.RemoveAll();
}


void CTelnetLink::PutMessage(const SwoMessage &msg)
{
	if(!msg.IsClear())
	{
		if (IsListening())
		{
			CCritSecLock m_Lock(m_Lock);
			m_SwoMessages.AddTail(msg);
		}
		if (m_hDoSend)
			SetEvent(m_hDoSend);
	}
}


//
// Wait for a connection on iPort and return the 
// 
void CTelnetLink::Listen(unsigned int iPort)
{
	CCritSecLock lock(m_Lock);
	struct   sockaddr_in sin;
	int so_reuseaddr = 1;

	//
	// Open an internet socket 
	//
	if ((m_ListenSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to socket() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Attempt to set SO_REUSEADDR to avoid "address already in use" errors
	setsockopt(m_ListenSock,
			   SOL_SOCKET,
			   SO_REUSEADDR,
			   (const char *)&so_reuseaddr,
			   sizeof(so_reuseaddr));

	//
	// Set up to listen on all interfaces/addresses and port iPort
	//
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(iPort);

	//
	// Bind out file descriptor to the port/address
	//
	Detail(_T("Bind to port %d\n"), iPort);
	if (bind(m_ListenSock, (struct sockaddr *)&sin, sizeof(sin)) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to bind() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	//
	// Put the file descriptor in a state where it can listen for an 
	// incoming connection
	//
	Info(_T("Listen\n"));
	if (listen(m_ListenSock, 1) == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to listen() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	DWORD dwFlags = 0;
	DWORD dwBytes;
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;

	// Load the AcceptEx function into memory using WSAIoctl.
	// The WSAIoctl function is an extension of the ioctlsocket()
	// function that can use overlapped I/O. The function's 3rd
	// through 6th parameters are input and output buffers where
	// we pass the pointer to our AcceptEx function. This is used
	// so that we can call the AcceptEx function directly, rather
	// than refer to the Mswsock.lib library.
	int iResult = WSAIoctl(m_ListenSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
						   &GuidAcceptEx, sizeof(GuidAcceptEx),
						   &lpfnAcceptEx, sizeof(lpfnAcceptEx),
						   &dwBytes, NULL, NULL);
	if (iResult == SOCKET_ERROR)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to WSAIoctl() failed.\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Create an accepting socket
	m_AcceptSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_AcceptSock == INVALID_SOCKET)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Create accept socket failed!\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	// Empty our overlapped structure and accept connections.
	WSAOVERLAPPED op;
	memset(&op, 0, sizeof(op));
	op.hEvent = WSACreateEvent();
	if (op.hEvent == WSA_INVALID_EVENT)
	{
		DWORD dw = WSAGetLastError();
		Error(_T("Call to WSACreateEvent() failed!\n"));
		AtlThrow(HRESULT_FROM_WIN32(dw));
	}

	char lpOutputBuf[2 * (sizeof(SOCKADDR_IN) + 16)];

	BOOL bRetVal = lpfnAcceptEx(m_ListenSock, m_AcceptSock, lpOutputBuf,
								0,
								sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
								&dwBytes, &op);
	DWORD err = 0;
	if (bRetVal == FALSE)
	{
		err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
			Error(_T("Call to AcceptEx() failed!\n"));
		else
		{
			err = 0;
			// Wait until a connection comes in
			while (err == 0)
			{
				DWORD dw = WaitForSingleObject(op.hEvent, 100);
				// Continue waiting until connection arrives
				if (dw == WAIT_TIMEOUT)
					continue;
				WSAResetEvent(op.hEvent);
				// Connection arrived: break wait loop
				if (dw == WAIT_OBJECT_0)
				{
					if (WSAGetOverlappedResult(m_ListenSock, &op, &dwBytes, FALSE, &dwFlags) == FALSE)
					{
						err = WSAGetLastError();
						Error(_T("Call to WSAGetOverlappedResult() failed!\n"));
					}
					break;
				}
				// Copy error to be later handled
				err = dw;
				Error(_T("Error waiting for incomming connection!\n"));
			}
		}
	}
	// Free local stuff
	WSACloseEvent(op.hEvent);

	//
	// Close the file descriptor that we used for listening
	//
	closesocket(m_ListenSock);
	m_ListenSock = 0;

	// Handle pending errors
	if (err)
	{
		// Cancel overlapped I/O
		closesocket(m_AcceptSock);
		m_AcceptSock = 0;
		AtlThrow(HRESULT_FROM_WIN32(err));
	}
}


DWORD CTelnetLink::StartReadThread()
{
	m_hReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hReadThread = _beginthread(ReadThread, 128 * 1024, (LPVOID)this);
	DWORD err = WaitForSingleObject(m_hReady, 5000);
	CloseHandle(m_hReady);
	m_hReady = NULL;
	if (err != WAIT_OBJECT_0)
		return err;
	return 0;
}


void __cdecl CTelnetLink::ReadThread(LPVOID pThis)
{
	((CTelnetLink *)pThis)->ReadThread();
	((CTelnetLink *)pThis)->m_hReadThread = 0;
	_endthread();
}


void CTelnetLink::ReadThread()
{
	DWORD err = 0;

	if (m_hReady)
		SetEvent(m_hReady);

	WSAOVERLAPPED op;
	memset(&op, 0, sizeof(op));
	op.hEvent = WSACreateEvent();
	if (op.hEvent == WSA_INVALID_EVENT)
	{
		err = WSAGetLastError();
		_com_error cerr(err);
		Error(_T("Call to WSACreateEvent() failed: %s\n"), cerr.ErrorMessage());
		return;
	}

	std::string message;

	while (IsListening())
	{
		DWORD dwBytes;
		DWORD dwFlags = MSG_PARTIAL;
		dwBytes = 0;
		WSABUF bufs;
		// Allocate buffer
		message.resize(READ_BUF_SIZE);
		bufs.buf = (char *)message.data();
		bufs.len = READ_BUF_SIZE;
		// Start data Receive
		if (WSARecv(m_AcceptSock, &bufs, 1, &dwBytes, &dwFlags, &op, NULL) == SOCKET_ERROR)
		{
			err = WSAGetLastError();
			if (err == WSA_IO_PENDING)
			{
				bool do_string = false;
				SwoMessage payload;
				err = 0;
				while (err == 0 && IsListening())
				{
					DWORD dw = WaitForSingleObject(op.hEvent, 100);
					// Continue waiting until connection arrives
					if (dw == WAIT_TIMEOUT)
						continue;
					WSAResetEvent(op.hEvent);
					// Connection arrived: break wait loop
					if (dw == WAIT_OBJECT_0)
					{
						if (WSAGetOverlappedResult(m_AcceptSock, &op, &dwBytes, FALSE, &dwFlags) == FALSE)
						{
							err = WSAGetLastError();
							Error(_T("Call to WSAGetOverlappedResult() failed with error %d!\n"), err);
						}
						else
						{
							payload.msg = CStringA(message.c_str(), dwBytes);
							do_string = true;
						}
						break;
					}
					// Copy error to be later handled
					err = dw;
					Error(_T("Call to WaitForSingleObject() failed with error %d!\n"), err);
				}
				// Send string
				if (do_string)
				{
					// Echo any read information
					payload.chan = -1;
					payload.msg = message.c_str();
					PutMessage(payload);
				}
			}
			else
			{
				Close();
			}
		}
		else
		{
			Close();
		}
	}
}


DWORD CTelnetLink::StartWriteThread()
{
	m_hReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hWriteThread = _beginthread(WriteThread, 128 * 1024, (LPVOID)this);
	DWORD err = WaitForSingleObject(m_hReady, 5000);
	CloseHandle(m_hReady);
	m_hReady = NULL;
	if (err != WAIT_OBJECT_0)
		return err;
	return 0;
}


void __cdecl CTelnetLink::WriteThread(LPVOID pThis)
{
	((CTelnetLink *)pThis)->WriteThread();
	_endthread();
}


void CTelnetLink::WriteThread()
{
	DWORD err = 0;

	m_hDoSend = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_hReady)
		SetEvent(m_hReady);
	if(m_hDoSend)
	{

		while (IsListening())
		{
			DWORD dw = WaitForSingleObject(m_hDoSend, 100);
			SwoMessage swo;
			while(m_SwoMessages.GetCount())
			{
				// BLOCK
				{
					CCritSecLock lock(m_Lock);
					swo = m_SwoMessages.RemoveHead();
				}
				CStringA out = m_Fmt.Format(swo);
				send(m_AcceptSock, out.GetString(), (int)out.GetLength(), 0);
			}
		}
		m_hWriteThread = 0;
		CloseHandle(m_hDoSend);
	}
}


void CTelnetLink::Serve(unsigned int iPort)
{
	m_fEnableXmit = false;
	m_SwoMessages.RemoveAll();
	Listen(iPort);
	if (IsListening())
	{
		m_fEnableXmit = true;
		DWORD err = StartWriteThread();
		if (err != 0)
		{
			Close();
			Error(_T("Write thread failed to be ready with error code %d\n"), err);
			AtlThrow(HRESULT_FROM_WIN32(err));
		}
		err = StartReadThread();
		if (err != 0)
		{
			Close();
			Error(_T("Read thread failed to be ready with error code %d\n"), err);
			AtlThrow(HRESULT_FROM_WIN32(err));
		}
	}
}

