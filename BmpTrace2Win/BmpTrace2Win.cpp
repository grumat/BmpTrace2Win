// BmpTrace2Win.cpp : Define o ponto de entrada para a aplicação de console.
//

#include "stdafx.h"
#include "TheLogger.h"
#include "BmpSwo.h"
#include "TelnetLink.h"
#include "AppConfig.h"
#include "TerminalLink.h"


using namespace Logger;


static int Usage(bool full)
{
	puts(
		"BmpTrace2Win 0.1\n"
		"================\n"
		"A proxy interface to dump the BMP Trace port to Windows Console.\n"
		"You can optionally specify a TCP port to bind to a terminal, like Putty.\n"
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
		"   -c <cfg> : Specify a custom .ini file\n"
		"   -h       : This help message\n"
		"   -m       : Monochrome mode\n"
		"   -p <nnn> : Enable TCP mode; raw output directed to the given port number\n"
		"   -v       : Increase verbose level (TCP mode only)\n"
	);
	return EXIT_FAILURE;
}


bool EnableVTMode()
{
	// Set output mode to handle virtual terminal sequences
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
	{
		return false;
	}

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
	{
		return false;
	}
	return true;
}


class SwoFormatter : public ISwoFormatter
{
public:
	SwoFormatter(const CAppConfig &cfg) : m_Config(cfg) {}

	virtual CStringA Format(const SwoMessage &swo) const override
	{
		CStringA res;
		int chan = swo.chan;
		if (chan < 0)
			chan = 0;
		res = m_Config.GetForeground(chan);
		res += m_Config.GetBackground(chan);
		res += m_Config.GetTitle(chan);
		res.AppendFormat(" %5d.%03d:", swo.time / 1000, swo.time % 1000);
		res += m_Config.ResetColors();
		res += ' ';
		res += swo.msg;
		return res;
	}

protected:
	const CAppConfig &m_Config;
};


int main(int argc, char *argv[])
{
#ifdef _DEBUG
	int log_level = DEBUG_LEVEL;
#else
	int log_level = WARN_LEVEL;
#endif
	int port = 0;
	enum CmdState
	{
		kNormal,
		kGetConfig,
		kGetPort,
	} state = kNormal;
	bool fMonoChrome = false;

	CPath ini_path;
	GetModuleFileName(NULL, CStrBuf(ini_path.m_strPath, MAX_PATH), MAX_PATH);
	ini_path.RemoveExtension();
	ini_path.AddExtension(_T(".ini"));

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
			else if (sw == 'c')
			{
				// Config file
				state = kGetConfig;
			}
			else if (sw == 'm')
			{
				// Momochrome mode
				fMonoChrome = true;
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
		else if (state == kGetConfig)
		{
			// Get config file name
			state = kNormal;
			ini_path.m_strPath = arg;
			if(!ini_path.FileExists())
			{
				fprintf(stderr, "Specified .ini file '%s' does not exists!", arg);
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
	if (port == 0)	// terminal mode
		log_level = ERROR_LEVEL;

	ISwoTarget *link = NULL;
	try
	{
		// Setup logger
		TheLogger().SetLevel((Level_e)log_level);
		if(IsInfoLevel())
			puts("Starting BmpTrace2Win...\n");

		// Load configuration from ini file
		CAppConfig cfg(ini_path);
		cfg.LoadIni();
		cfg.SetMonoChromeMode(fMonoChrome);

		// Create object to format the SWO messages using VT100 colors
		SwoFormatter fmt(cfg);
		if (port == 0)
		{
			if (fMonoChrome == false)
				EnableVTMode();	// Enables VT100 on Windows 10 Anniversary update and later
			// Terminal mode
			link = new CTerminalLink(fmt);
		}
		else
		{
			// TCP mode
			CTelnetLink *raw_ftp = new CTelnetLink(fmt);
			// Wait for Putty (RAW)
			raw_ftp->Serve(port);
			link = raw_ftp;
		}
		// Connect to hardware
		CBmpSwo swo;
		swo.Open();
		// Welcome message
		CTime tm = CTime::GetCurrentTime();
		SwoMessage payload;
		payload.Clear();
		payload.chan = 0;
		payload.msg.Format("Logging started in %S\n", tm.Format(_T("%X")). GetString());
		link->Send(payload);
		// Enter main loop
		swo.DoTrace(*link);
		// Stop USB pipe
		swo.Close();
		if (link)
		{
			link->Close();
			delete link;
		}
	}
	catch (CAtlException &e)
	{
		_com_error err(e.m_hr);
		_ftprintf(stderr, _T("%s\n"), err.ErrorMessage());
		ATLTRACE(_T("%s\n"), err.ErrorMessage());
		if (link)
		{
			link->Close();
			delete link;
		}
	}
	return EXIT_SUCCESS;
}

