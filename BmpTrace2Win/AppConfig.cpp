#include "stdafx.h"
#include "AppConfig.h"


CAppConfig::CAppConfig(const CPath &ini)
	: m_IniPath(ini)
	, m_fMonoChromeMode(false)
{
}


CAppConfig::~CAppConfig()
{
}


CString CAppConfig::UnEscape(LPCTSTR s)
{
	TCHAR last_ch = 0;
	CString out;
	for (LPCTSTR p = s; *p; ++p)
	{
		if (last_ch)
		{
			if (last_ch == *p)		// handle double "^^"
			{
				out += last_ch;
				last_ch = 0;
				continue;
			}
			else
			{
				out += _T('\x1b');	// single "^" converts to ESC
				last_ch = 0;
			}
		}
		if (*p == _T('^'))
			last_ch = *p;			// process on next iteration
		else
			out += *p;				// copy chars
	}
	if(last_ch)
		out += _T('\x1b');			// string ends with escape
	return out;
}


CString CAppConfig::GetValue(LPCTSTR section, LPCTSTR key, LPCTSTR szDef) const
{
	CString s;
	DWORD err = ::GetPrivateProfileString(
		section
		, key
		, szDef
		, CStrBuf(s, kColorBufferSize)
		, kColorBufferSize
		, m_IniPath
	);

#if 0
	// Uncomment the code to generate the ini file
	if (err)
	{
		::WritePrivateProfileString(
			section
			, key
			, s
			, m_IniPath
		);
	}
#endif
	return s;
}


void CAppConfig::LoadColor(int channel, LPCTSTR szFore, LPCTSTR szBack, LPCTSTR szTitle)
{
	CString section;
	CString s;
	section.Format(_T("Channel%d"), channel);
	if (szTitle == NULL)
		s.Format(_T("C%02d"), channel);
	else
		s = szTitle;
	m_strTitles[channel] = GetValue(section, _T("Title"), s);
	m_arrColor[channel] = GetOneColor(section, _T("Fore"), szFore);
	m_arrBack[channel] = GetOneColor(section, _T("Back"), szBack);
}


void CAppConfig::LoadIni()
{
	m_strResetClr = GetOneColor(_T("General"), _T("Reset"), _T("^[0m"));
	LoadColor( 0, _T("^[97m"), _T("^[42m"), _T("Msg"));
	LoadColor( 1, _T("^[97m"), _T("^[41m"), _T("Err"));
	LoadColor( 2, _T("^[97m"), _T("^[45m"), _T("Dbg"));
	LoadColor( 3, _T("^[33m"), _T("^[40m"));
	LoadColor( 4, _T("^[34m"), _T("^[40m"));
	LoadColor( 5, _T("^[35m"), _T("^[40m"));
	LoadColor( 6, _T("^[36m"), _T("^[40m"));
	LoadColor( 7, _T("^[37m"), _T("^[40m"));
	LoadColor( 8, _T("^[91m"), _T("^[40m"));
	LoadColor( 9, _T("^[92m"), _T("^[40m"));
	LoadColor(10, _T("^[93m"), _T("^[40m"));
	LoadColor(11, _T("^[94m"), _T("^[40m"));
	LoadColor(12, _T("^[95m"), _T("^[40m"));
	LoadColor(13, _T("^[96m"), _T("^[40m"));
	LoadColor(14, _T("^[97m"), _T("^[40m"));
	LoadColor(15, _T("^[37m"), _T("^[90m"));
	LoadColor(16, _T("^[31m"), _T("^[90m"));
	LoadColor(17, _T("^[32m"), _T("^[90m"));
	LoadColor(18, _T("^[33m"), _T("^[90m"));
	LoadColor(19, _T("^[34m"), _T("^[90m"));
	LoadColor(20, _T("^[35m"), _T("^[90m"));
	LoadColor(21, _T("^[36m"), _T("^[90m"));
	LoadColor(22, _T("^[37m"), _T("^[90m"));
	LoadColor(23, _T("^[91m"), _T("^[90m"));
	LoadColor(24, _T("^[92m"), _T("^[90m"));
	LoadColor(25, _T("^[93m"), _T("^[90m"));
	LoadColor(26, _T("^[94m"), _T("^[90m"));
	LoadColor(27, _T("^[95m"), _T("^[90m"));
	LoadColor(28, _T("^[96m"), _T("^[90m"));
	LoadColor(29, _T("^[97m"), _T("^[90m"));
	LoadColor(30, _T("^[37m"), _T("^[41m"));
	LoadColor(31, _T("^[31m"), _T("^[41m"), _T("OS "));
}


const CString &CAppConfig::GetTitle(int key) const
{
	if (key < 0 || key >= _countof(m_strTitles))
		key = 0;
	return m_strTitles[key];
}


const CString &CAppConfig::GetForeground(int key) const
{
	if (m_fMonoChromeMode)
		return m_strMonoColor;
	if (key < 0 || key >= _countof(m_arrColor))
		key = 0;
	return m_arrColor[key];
}


const CString &CAppConfig::GetBackground(int key) const
{
	if (m_fMonoChromeMode)
		return m_strMonoColor;
	if (key < 0 || key >= _countof(m_arrBack))
		key = 0;
	return m_arrBack[key];
}


const CString &CAppConfig::ResetColors() const
{
	if (m_fMonoChromeMode)
		return m_strMonoColor;
	return m_strResetClr;
}

