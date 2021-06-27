#pragma once


//! Stores configuration for the App
class CAppConfig
{
public:
	CAppConfig(const CPath &ini);
	virtual ~CAppConfig();

	//! Reads the ini file or loads default values
	void LoadIni();

	//! Retrieves the title for the SWO channel
	const CString &GetTitle(int key) const;
	//! Retrieves the escape sequence to set the foreground background color
	const CString &GetForeground(int key) const;
	//! Retrieves the escape sequence to set the current background color
	const CString &GetBackground(int key) const;
	//! Retrieves the escape sequence to reset terminal colors
	const CString &ResetColors() const;

	void SetMonoChromeMode(bool mode) { m_fMonoChromeMode = mode; }

protected:
	//! Reads a value from the INI file
	CString GetValue(LPCTSTR section, LPCTSTR key, LPCTSTR szDef) const;
	//! Load attributes for one color
	CString GetOneColor(LPCTSTR section, LPCTSTR key, LPCTSTR szDef) const
	{
		return UnEscape(GetValue(section, key, szDef));
	}
	//! Load attributes for one SWO channel
	void LoadColor(int channel, LPCTSTR szFore, LPCTSTR szBack, LPCTSTR szTitle=NULL);
	//! Replaces '^' by the ASCII ESC char
	static CString UnEscape(LPCTSTR s);

protected:
	//! Path to the ini file
	const CPath m_IniPath;
	//! Escape sequence to reset terminal color attributes
	CString m_strResetClr;
	//! Title strings for each SWO channel
	CString m_strTitles[32];
	//! Text color for each SWO channel 
	CString m_arrColor[32];
	//! Background color for each SWO channel 
	CString m_arrBack[32];
	//! Sets monochrome mode
	bool m_fMonoChromeMode;
	//! Just an empty string used as reference placeholder in monochrome mode
	CString m_strMonoColor;

	// Constants used in the class
	enum 
	{
		//! Size of the buffer that stores an VT100 color escape sequence
		kColorBufferSize = 256
	};
};

