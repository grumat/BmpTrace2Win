// BmpTrace2Win.cpp : Define o ponto de entrada para a aplicação de console.
//

#include "stdafx.h"
#include "TheLogger.h"
#include "BmpSwo.h"
#include "TelnetLink.h"


using namespace Logger;


static int Usage(bool full)
{
	puts(
		"BmpTrace2Win 0.1\n"
		"================\n"
		"A proxy interface to link the BMP Trace port to a raw TCP.\n"
		"This is tuned to connect to Putty and obtain a handy trace output.\n"
		"The default listen port is 2332.\n"
	);
	if (full == false)
	{
		puts("Use '-h' switch for help.\n");
		return EXIT_FAILURE;
	}
	puts(
		"\n"
		"USAGE: BmpTrace2Win [-v] [-p nnn]\n"
		"WHERE\n"
		"   -h     : This help message\n"
		"   -p nnn : Listen in the given port number\n"
		"   -v     : Increase verbose level\n"
	);
	return EXIT_FAILURE;
}


int main(int argc, char *argv[])
{
#ifdef _DEBUG
	int log_level = DEBUG_LEVEL;
#else
	int log_level = WARN_LEVEL;
#endif
	int port = 2332;
	enum CmdState
	{
		kNormal,
		kGetPort,
	} state = kNormal;
	for (int i = 1; i < argc; ++i)
	{
		const char *arg = argv[i];
		if (*arg == '-' || *arg == '/')
		{
			// Get switch
			char sw = *++arg;
			// No support for compound switches
			if (*++arg != 0)
				goto unknown_switch;
			if (sw == 'h')
			{
				// Help
				return Usage(true);
			}
			else if (sw == 'p')
			{
				// Port
				state = kGetPort;
			}
			else if (sw == 'v')
			{
				// Log level
				++log_level;
			}
			else
			{
unknown_switch:
				// Unknown switch found
				fprintf(stderr, "Unknown switch '%s'! Use '-h' for Help!", argv[i]);
				return EXIT_FAILURE;
			}
		}
		else if (state == kGetPort)
		{
			// Get port number
			state = kNormal;
			char *end;
			port = strtoul(arg, &end, 0);
			// Validate
			if (*end || port == 0)
			{
				fprintf(stderr, "Unknown value '%s' for port number! It should be an integer value!", arg);
				return EXIT_FAILURE;
			}
		}
		else
		{
			// Unknown token on command line
			Usage(false);
			fprintf(stderr, "Unknown parameter '%s'!", argv[i]);
			return EXIT_FAILURE;
		}
	}
	// Ensure log level is in a valid range
	if (log_level > DEBUG_LEVEL)
		log_level = DEBUG_LEVEL;

	try
	{
		TheLogger().SetLevel((Level_e)log_level);
		if(IsInfoLevel())
			puts("Starting BmpTrace2Win...\n");

		CTelnetLink lnk;
		// Wait for Putty (RAW)
		lnk.Serve(2332);
		// Connect to hardware
		CBmpSwo swo;
		swo.Open();
		CTime tm = CTime::GetCurrentTime();
		SwoMessage payload;
		payload.ctx = 0;
		payload.msg.Format("Logging started in %S\n", tm.Format(_T("%X")). GetString());
		lnk.PutMessage(payload);
		swo.DoTrace(lnk);
		swo.Close();
		lnk.Close();
#if 0
		CTivaIcdi tiva;
		s_pTivaObj = &tiva;
		CGdbLink gdb_link(tiva);

		tiva.Open(gdb_link);
		SetConsoleCtrlHandler(CloseTiva, TRUE);
		if (IsInfoLevel())
			printf("Listening port %d\n", port);
		gdb_link.Serve(port);
		s_pTivaObj = NULL;	// object is going out of scope
		tiva.Close();
#endif
	}
	catch (CAtlException &e)
	{
		_com_error err(e.m_hr);
		_ftprintf(stderr, _T("%s\n"), err.ErrorMessage());
		ATLTRACE(_T("%s\n"), err.ErrorMessage());
	}
	return EXIT_SUCCESS;
}

