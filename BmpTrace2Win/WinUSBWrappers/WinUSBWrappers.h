/*
Module : WinUSBWrappers.h
Purpose: Defines the interface for a set of C++ class which encapsulate WinUSB.
History: PJN / 27-11-2015 1. Initial release
         PJN / 18-12-2015 1. Removed unnecessary inclusion of <functional> from module
         PJN / 30-01-2017 1. Updated copyright details.
                          2. Added support for WinUsb_StartTrackingForTimeSync, 
                          WinUsb_GetCurrentFrameNumberAndQpc & WinUsb_StopTrackingForTimeSync APIs.
                          3. Replaced CString::operator LPC*STR() calls with CString::GetString calls

Copyright (c) 2015 - 2017 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


/////////////////////////// Macros / Defines //////////////////////////////////

#ifndef __WINUSBWRAPPERS_H__
#define __WINUSBWRAPPERS_H__

#ifndef CWINUSBWRAPPERS_EXT_CLASS
#define CWINUSBWRAPPERS_EXT_CLASS
#endif //#ifndef CWINUSBWRAPPERS_EXT_CLASS


#pragma comment(lib, "Winusb.lib")
#pragma comment(lib, "Setupapi.lib")


/////////////////////////// Includes //////////////////////////////////////////

#ifndef __WUSB_H__
#pragma message("To avoid this message, please put WinUSB.h in your pre compiled header (normally stdafx.h)")
#include <WinUSB.h>
#endif //#ifndef __WUSB_H__

#ifndef _INC_SETUPAPI
#pragma message("To avoid this message, please put setupapi.h in your pre compiled header (normally stdafx.h)")
#include <setupapi.h>
#endif //#ifndef _INC_SETUPAPI

#ifndef __ATLBASE_H__
#pragma message("To avoid this message, please put atlbase.h in your pre compiled header (normally stdafx.h)")
#include <atlbase.h>
#endif //#ifndef __ATLBASE_H__

#ifndef WINUSBWRAPPERS_MFC_EXTENSIONS
#ifndef _STRING_
#pragma message("To avoid this message, please put string in your pre compiled header (normally stdafx.h)")
#include <string>
#endif //#ifndef _STRING_

#ifndef _VECTOR_
#pragma message("To avoid this message, please put vector in your pre compiled header (normally stdafx.h)")
#include <vector>
#endif //#ifndef _VECTOR_
#endif //#ifndef WINUSBWRAPPERS_MFC_EXTENSIONS


/////////////////////////// Classes ///////////////////////////////////////////

namespace WinUSB
{

//Typedefs
#ifdef WINUSBWRAPPERS_MFC_EXTENSIONS
  typedef CString String; 
  typedef CStringArray StringArray;
#else
#ifdef _UNICODE
  typedef std::wstring String;
#else
  typedef std::string String;
#endif
  typedef std::vector<String> StringArray;
#endif //#ifdef WINUSBWRAPPERS_MFC_EXTENSIONS

//Wrapper for a WinUSB WINUSB_INTERFACE_HANDLE handle
class CWINUSBWRAPPERS_EXT_CLASS CDevice
{
public:
//Typedefs
  typedef bool (INCLUDE_DEVICE_CALLBACK)(_In_ HDEVINFO hDeviceInfoSet, _In_ PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData, _In_ PSP_DEVINFO_DATA pDeviceInfoData, _In_ LPCTSTR pszDevicePath, _In_ DWORD_PTR dwItemData);
  typedef INCLUDE_DEVICE_CALLBACK* LPINCLUDE_DEVICE_CALLBACK;

//Constructors / Destructors
  CDevice() : m_h(nullptr),
              m_hDeviceHandle(INVALID_HANDLE_VALUE)
  {
  }

  CDevice(_In_ const CDevice& device) = delete;

  CDevice(_In_ CDevice&& device) = delete;

  ~CDevice()
  {
    Free();
  }

//Methods
  CDevice& operator=(_In_ const CDevice& device) = delete;

  CDevice& operator=(_In_ CDevice&& device) = delete;

  operator WINUSB_INTERFACE_HANDLE() const
  {
    return m_h;
  }

  HANDLE GetDeviceHandle()
  {
    return m_hDeviceHandle;
  }

  BOOL Initialize(_In_ HANDLE hDeviceHandle)
  {
    //Validate our parameters
    ATLASSERT(m_h == nullptr);

    return WinUsb_Initialize(hDeviceHandle, &m_h);
  }

  BOOL Initialize(_In_ LPCTSTR pszDeviceName)
  {
    //Validate our parameters
    ATLASSERT(m_h == nullptr);

    //Open the device handle
    if (!OpenDeviceHandle(pszDeviceName))
      return FALSE;

    //Open using WinUSB
    BOOL bSuccess = Initialize(m_hDeviceHandle);
    if (!bSuccess)
    {
      DWORD dwError = GetLastError();
      Free();
      SetLastError(dwError);
    }

    return bSuccess;
  }

  BOOL OpenDeviceHandle(_In_ LPCTSTR pszDeviceName)
  {
    //Validate our parameters
    ATLASSERT(m_hDeviceHandle == INVALID_HANDLE_VALUE);

    m_hDeviceHandle = CreateFile(pszDeviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    return (m_hDeviceHandle != INVALID_HANDLE_VALUE);
  }

  void Free()
  {
    if (m_h != NULL)
    {
      WinUsb_Free(m_h);
      m_h = NULL;
    }
    if (m_hDeviceHandle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(m_hDeviceHandle);
      m_hDeviceHandle = INVALID_HANDLE_VALUE;
    }
  }

  //Enumerate devices given a device interface class guid and a Vendor ID (VID)
  static BOOL EnumerateDevices(_In_ const GUID* pInterfaceGUID, _In_ USHORT idVendor, _Out_ StringArray& deviceNames, _In_ DWORD dwFlags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE, _In_opt_ LPCTSTR pszEnumerator = nullptr)
  {
    CCallbackInfo cbi;
    cbi.pIdVendor = &idVendor;
    cbi.pIdProduct = nullptr;
    return EnumerateDevices(pInterfaceGUID, deviceNames, dwFlags, pszEnumerator, IncludeVIDandPIDDevices, reinterpret_cast<DWORD_PTR>(&cbi));
  }

  //Enumerate devices given an device interface class guid, Vendor ID (VID) and Product ID (PID)
  static BOOL EnumerateDevices(_In_ const GUID* pInterfaceGUID, _In_ USHORT idVendor, _In_ USHORT idProduct, _Out_ StringArray& deviceNames, _In_ DWORD dwFlags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE, _In_opt_ LPCTSTR pszEnumerator = nullptr)
  {
     CCallbackInfo cbi;
     cbi.pIdVendor = &idVendor;
     cbi.pIdProduct = &idProduct;
     return EnumerateDevices(pInterfaceGUID, deviceNames, dwFlags, pszEnumerator, IncludeVIDandPIDDevices, reinterpret_cast<DWORD_PTR>(&cbi));
  }

  //Enumerate devices given a device interface class guid and a generic function pointer callback to decide if the device should be included
  static BOOL EnumerateDevices(_In_ const GUID* pInterfaceGUID, _Out_ StringArray& deviceNames, _In_ DWORD dwFlags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE, _In_opt_ LPCTSTR pszEnumerator = nullptr, _In_opt_ LPINCLUDE_DEVICE_CALLBACK pIncludeFunc = nullptr, DWORD_PTR dwItemData = 0)
  {
  #ifdef WINUSBWRAPPERS_MFC_EXTENSIONS
    deviceNames.RemoveAll();
  #else
    deviceNames.clear();
  #endif //#ifdef WINUSBWRAPPERS_MFC_EXTENSIONS

    //Enumerate all devices which supports the specified interface GUID
    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(pInterfaceGUID, pszEnumerator, nullptr, dwFlags);
    if (hDeviceInfo == INVALID_HANDLE_VALUE)
      return FALSE;

    bool bMoreDevices = true;
    DWORD dwDeviceIndex = 0;
    while (bMoreDevices)
    {
      //Get the next device which supports the specified interface GUID
      SP_DEVICE_INTERFACE_DATA did;
      did.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
      if (!SetupDiEnumDeviceInterfaces(hDeviceInfo, nullptr, pInterfaceGUID, dwDeviceIndex, &did))
      {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_NO_MORE_ITEMS)
        {
          SetupDiDestroyDeviceInfoList(hDeviceInfo);
          SetLastError(dwError);
          return FALSE;
        }
        else
          bMoreDevices = false;
      }
      else
      {
        //Get information about the found device
        DWORD dwLength = 0;
        if (!SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &did, NULL, 0, &dwLength, nullptr))
        {
          DWORD dwError = GetLastError();
          if (dwError != ERROR_INSUFFICIENT_BUFFER)
          {
            SetupDiDestroyDeviceInfoList(hDeviceInfo);
            SetLastError(dwError);
            return FALSE;
          }
        }
        #pragma warning(suppress: 6102)
        HANDLE hHeap = GetProcessHeap();
        PSP_DEVICE_INTERFACE_DETAIL_DATA pDIDD = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(HeapAlloc(hHeap, 0, dwLength));
        if (pDIDD == nullptr) 
        {
          DWORD dwError = GetLastError();
          SetupDiDestroyDeviceInfoList(hDeviceInfo);
          SetLastError(dwError);
          return FALSE;
        }
        pDIDD->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        SP_DEVINFO_DATA devData;
        devData.cbSize = sizeof(devData);
        if (!SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &did, pDIDD, dwLength, &dwLength, &devData))
        {
          DWORD dwError = GetLastError();
          HeapFree(hHeap, 0, pDIDD);
          SetupDiDestroyDeviceInfoList(hDeviceInfo);
          SetLastError(dwError);
          return FALSE;
        }

        //Add the device path to the output array if required
        bool bIncludeDevice = true;
        if (pIncludeFunc != nullptr)
          bIncludeDevice = pIncludeFunc(hDeviceInfo, pDIDD, &devData, pDIDD->DevicePath, dwItemData);
        if (bIncludeDevice)
        {
        #ifdef WINUSBWRAPPERS_MFC_EXTENSIONS
          deviceNames.Add(pDIDD->DevicePath);
        #else
          deviceNames.push_back(pDIDD->DevicePath);
        #endif //#ifdef WINUSBWRAPPERS_MFC_EXTENSIONS
        }

        //Free up the heap memory we have used for this loop
        HeapFree(hHeap, 0, pDIDD);

        //Prepare for the next loop
        ++dwDeviceIndex;
      }
    }
    SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return TRUE;
  }

  BOOL GetAssociatedInterface(_In_  UCHAR AssociatedInterfaceIndex, _Inout_ CDevice& associatedInterface)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);
    ATLASSERT(associatedInterface.m_h == nullptr);

    return WinUsb_GetAssociatedInterface(m_h, AssociatedInterfaceIndex, &associatedInterface.m_h);
  }

  BOOL GetDescriptor(_In_ UCHAR DescriptorType, _In_ UCHAR Index, _In_ USHORT LanguageID, 
                     _Out_writes_bytes_to_opt_(BufferLength, *LengthTransferred) PUCHAR Buffer,
                     _In_  ULONG BufferLength, _Out_ PULONG LengthTransferred)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_GetDescriptor(m_h, DescriptorType, Index, LanguageID, Buffer, BufferLength, LengthTransferred);
  }

  BOOL QueryInterfaceSettings(_In_ UCHAR AlternateInterfaceNumber, _Out_ PUSB_INTERFACE_DESCRIPTOR UsbAltInterfaceDescriptor)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_QueryInterfaceSettings(m_h, AlternateInterfaceNumber, UsbAltInterfaceDescriptor);
  }

  BOOL QueryDeviceInformation(_In_ ULONG InformationType, _Inout_ PULONG BufferLength, _Out_writes_bytes_(*BufferLength) PVOID Buffer)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_QueryDeviceInformation(m_h, InformationType, BufferLength, Buffer);
  }

  BOOL SetCurrentAlternateSetting(_In_ UCHAR SettingNumber)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_SetCurrentAlternateSetting(m_h, SettingNumber);
  }

  BOOL GetCurrentAlternateSetting(_Out_ PUCHAR SettingNumber)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_GetCurrentAlternateSetting(m_h, SettingNumber);
  }

  BOOL QueryPipe(_In_ UCHAR AlternateInterfaceNumber, _In_ UCHAR PipeIndex, 
                 _Out_ PWINUSB_PIPE_INFORMATION PipeInformation)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_QueryPipe(m_h, AlternateInterfaceNumber, PipeIndex,PipeInformation);
  }

  BOOL QueryPipEx(_In_ UCHAR AlternateSettingNumber, _In_ UCHAR PipeIndex,
                  _Out_ PWINUSB_PIPE_INFORMATION_EX PipeInformationEx)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_QueryPipeEx(m_h, AlternateSettingNumber, PipeIndex, PipeInformationEx);
  }

  BOOL SetPipePolicy(_In_ UCHAR PipeID, _In_ ULONG PolicyType, _In_ ULONG ValueLength, 
                     _In_reads_bytes_(ValueLength) PVOID Value)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_SetPipePolicy(m_h, PipeID, PolicyType, ValueLength, Value);
  }

  BOOL GetPipePolicy(_In_ UCHAR PipeID, _In_ ULONG PolicyType, _Inout_ PULONG ValueLength,
                     _Out_writes_bytes_(*ValueLength) PVOID Value)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_GetPipePolicy(m_h, PipeID, PolicyType, ValueLength, Value);
  }

  BOOL ReadPipe(_In_ UCHAR PipeID, _Out_writes_bytes_to_opt_(BufferLength,*LengthTransferred) PUCHAR Buffer,
                _In_ ULONG BufferLength, _Out_opt_ PULONG LengthTransferred, _In_opt_ LPOVERLAPPED Overlapped)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return  WinUsb_ReadPipe(m_h, PipeID, Buffer, BufferLength, LengthTransferred, Overlapped);
  }

  BOOL WritePipe(_In_ UCHAR PipeID, _In_reads_bytes_(BufferLength) PUCHAR Buffer, _In_ ULONG BufferLength,
                 _Out_opt_ PULONG LengthTransferred, _In_opt_ LPOVERLAPPED Overlapped)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_WritePipe(m_h, PipeID, Buffer, BufferLength, LengthTransferred, Overlapped);
  }

  BOOL ControlTransfer(_In_ WINUSB_SETUP_PACKET SetupPacket, _Out_writes_bytes_to_opt_(BufferLength, *LengthTransferred) PUCHAR Buffer,
                       _In_ ULONG BufferLength, _Out_opt_ PULONG LengthTransferred, _In_opt_ LPOVERLAPPED Overlapped)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_ControlTransfer(m_h, SetupPacket, Buffer, BufferLength, LengthTransferred, Overlapped);
  }

  BOOL ResetPipe(_In_ UCHAR PipeID)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_ResetPipe(m_h, PipeID);
  }

  BOOL AbortPipe(_In_ UCHAR PipeID)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_AbortPipe(m_h, PipeID);
  }

  BOOL FlushPipe(_In_ UCHAR PipeID)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_FlushPipe(m_h, PipeID);
  }

  BOOL SetPowerPolicy(_In_ ULONG PolicyType, _In_ ULONG ValueLength, _In_reads_bytes_(ValueLength) PVOID Value)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_SetPowerPolicy(m_h, PolicyType, ValueLength, Value);
  }

  BOOL GetPowerPolicy(_In_ ULONG PolicyType, _Inout_ PULONG ValueLength, _Out_writes_bytes_(*ValueLength) PVOID Value)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_GetPowerPolicy(m_h, PolicyType, ValueLength, Value);
  }

  BOOL GetOverlappedResult(_In_ LPOVERLAPPED lpOverlapped, _Out_ LPDWORD lpNumberOfBytesTransferred, _In_ BOOL bWait)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return  WinUsb_GetOverlappedResult(m_h, lpOverlapped, lpNumberOfBytesTransferred, bWait);
  }

  BOOL GetCurrentFrameNumber(_Out_ PULONG CurrentFrameNumber, _Out_ LARGE_INTEGER* TimeStamp)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    //The Windows SDK documentation say that the first parameter to WinUsb_GetCurrentFrameNumber should
    //be the handle returned from CreateFile rather than the value returned from WinUsb_Initialize but
    //testing and the documentation at https://msdn.microsoft.com/en-us/library/windows/hardware/dn376866%28v=vs.85%29.aspx 
    //clearly indicates that the handle returned from WinUsb_Initialize should be used.
    return WinUsb_GetCurrentFrameNumber(m_h, CurrentFrameNumber,TimeStamp);
  }

  BOOL RegisterIsochBuffer(_In_ UCHAR PipeID, _Inout_updates_bytes_(BufferLength) PUCHAR Buffer,
                           _In_ ULONG BufferLength, _Out_ PWINUSB_ISOCH_BUFFER_HANDLE IsochBufferHandle)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_RegisterIsochBuffer(m_h, PipeID, Buffer, BufferLength, IsochBufferHandle);
  }

__if_exists(WinUsb_StartTrackingForTimeSync)
{
  BOOL StartTrackingForTimeSync(_In_  PUSB_START_TRACKING_FOR_TIME_SYNC_INFORMATION StartTrackingInfo)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_StartTrackingForTimeSync(m_h, StartTrackingInfo);
  }
} //__if_exists(WinUsb_StartTrackingForTimeSync)

__if_exists(WinUsb_GetCurrentFrameNumberAndQpc)
{
  BOOL GetCurrentFrameNumberAndQpc(_In_  PUSB_FRAME_NUMBER_AND_QPC_FOR_TIME_SYNC_INFORMATION FrameQpcInfo)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_GetCurrentFrameNumberAndQpc(m_h, FrameQpcInfo);
  }
} //__if_exists(WinUsb_GetCurrentFrameNumberAndQpc)

__if_exists(WinUsb_StopTrackingForTimeSync)
{
  BOOL StopTrackingForTimeSync(_In_  PUSB_STOP_TRACKING_FOR_TIME_SYNC_INFORMATION StopTrackingInfo)
  {
    //Validate our parameters
    ATLASSERT(m_h != nullptr);

    return WinUsb_StopTrackingForTimeSync(m_h, StopTrackingInfo);
  }
} //__if_exists(WinUsb_StopTrackingForTimeSync)

protected:
//Structs
  struct CCallbackInfo
  {
    USHORT* pIdVendor;
    USHORT* pIdProduct;
  };

//Static methods
  static bool IncludeVIDandPIDDevices(_In_ HDEVINFO /*hDeviceInfoSet*/, _In_ PSP_DEVICE_INTERFACE_DETAIL_DATA /*pDeviceInterfaceDetailData*/, _In_ PSP_DEVINFO_DATA /*pDeviceInfoData*/, _In_ LPCTSTR pszDevicePath, _In_ DWORD_PTR dwItemData)
  {
    //Validate our parameters
    ATLASSERT(dwItemData != 0);
    CCallbackInfo* pCallbackInfo = reinterpret_cast<CCallbackInfo*>(dwItemData);
    ATLASSERT(pCallbackInfo->pIdVendor != nullptr);

    //Allocate a non-const copy of "pszDevicePath" suitable for token parsing
    LPTSTR pszLocalDevicePath = _tcsdup(pszDevicePath);
    if (pszLocalDevicePath == nullptr)
      return false;

    LPTSTR pszNextToken = nullptr;
    LPTSTR pszToken = _tcstok_s(pszLocalDevicePath, _T("\\#&"), &pszNextToken);
    bool bFoundVendor = false;
    bool bFoundProduct = false;
    if (pCallbackInfo->pIdProduct == nullptr)
      bFoundProduct = true;
    while (pszToken != NULL) 
    {
      if (!bFoundVendor && _tcsnicmp(pszToken, _T("VID_"), 4) == 0) //Match on the vendor ID
      {
        LPCTSTR pszVendorID = pszToken + 4;
        long nFoundVendorID = _tcstol(pszVendorID, nullptr, 16);
        bFoundVendor = true;
        if (nFoundVendorID != *pCallbackInfo->pIdVendor)
        {
          //Free up the heap memory we have used
          free(pszLocalDevicePath);

          return false;
        }
      }
      else if (!bFoundProduct && _tcsnicmp(pszToken, _T("PID_"), 4) == 0) //Match on the product ID
      {
        LPCTSTR pszProductID = pszToken + 4;
        long nFoundProductID = _tcstol(pszProductID, nullptr, 16);
        bFoundProduct = true;
        ATLASSERT(pCallbackInfo->pIdProduct != nullptr);
        if (nFoundProductID != *pCallbackInfo->pIdProduct)
        {
          //Free up the heap memory we have used
          free(pszLocalDevicePath);

          return false;
        }
      }

      //Prepare for the next loop
      pszToken = _tcstok_s(nullptr, _T("\\#&"), &pszNextToken);
    }

    //Free up the heap memory we have used
    free(pszLocalDevicePath);

    return (bFoundVendor && bFoundProduct); 
  }

//Member variables
  WINUSB_INTERFACE_HANDLE m_h;
  HANDLE                  m_hDeviceHandle;
};

}; //namespace WinUSB

#endif //#ifndef __WINUSBWRAPPERS_H__
