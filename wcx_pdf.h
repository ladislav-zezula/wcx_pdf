/*****************************************************************************/
/* wcx_pdf.h                              Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Main header file for the wcx_pdf plugin                                   */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 12.06.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __WCX_PDF_H__
#define __WCX_PDF_H__

#define _CRT_SECURE_NO_WARNINGS

#pragma warning (disable: 4091)                 // 4091: 'typedef ': ignored on left of 'tagDTI_ADTIWUI' when no variable is declared
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <strsafe.h>

#include <vector>

#include "Utils.h"                              // Utility functions
#include "TStringConvert.h"                     // String conversions
#include "TPdf.h"                               // PDF handling functions

//-----------------------------------------------------------------------------
// C++ types missing in older compilers

#ifndef _STDINT
typedef unsigned char uint8_t;
#endif

//-----------------------------------------------------------------------------
// Defines

/* Error codes returned to calling application */
#define E_END_ARCHIVE     10                    // No more files in archive
#define E_NO_MEMORY       11                    // Not enough memory
#define E_BAD_DATA        12                    // Data is bad
#define E_BAD_ARCHIVE     13                    // CRC error in archive data
#define E_UNKNOWN_FORMAT  14                    // Archive format unknown
#define E_EOPEN           15                    // Cannot open existing file
#define E_ECREATE         16                    // Cannot create file
#define E_ECLOSE          17                    // Error closing file
#define E_EREAD           18                    // Error reading from file
#define E_EWRITE          19                    // Error writing to file
#define E_SMALL_BUF       20                    // Buffer too small
#define E_EABORTED        21                    // Function aborted by user
#define E_NO_FILES        22                    // No files found
#define E_TOO_MANY_FILES  23                    // Too many files to pack
#define E_NOT_SUPPORTED   24                    // Function not supported

/* flags for ProcessFile */
enum PROCESS_FILE_OPERATION
{
    PK_SKIP = 0,                                // Skip this file
    PK_TEST,
    PK_EXTRACT,
    PK_EXTRACT_TO_MEMORY,                       // For testing purposes. The "szDestPath" contains pointer where to store the data
                                                // in the TExtractedData structure. The caller must free the data using LocalFree
};

/* Flags passed through PFN_CHANGE_VOL */
#define PK_VOL_ASK          0                   // Ask user for location of next volume
#define PK_VOL_NOTIFY       1                   // Notify app that next volume will be unpacked

/* Flags for packing */

/* For PackFiles */
#define PK_PACK_MOVE_FILES  1                   // Delete original after packing
#define PK_PACK_SAVE_PATHS  2                   // Save path names of files

/* Returned by GetPackCaps */
#define PK_CAPS_NEW         1                   // Can create new archives
#define PK_CAPS_MODIFY      2                   // Can modify existing archives
#define PK_CAPS_MULTIPLE    4                   // Archive can contain multiple files
#define PK_CAPS_DELETE      8                   // Can delete files
#define PK_CAPS_OPTIONS    16                   // Has options dialog
#define PK_CAPS_MEMPACK    32                   // Supports packing in memory
#define PK_CAPS_BY_CONTENT 64                   // Detect archive type by content
#define PK_CAPS_SEARCHTEXT 128                  // Allow searching for text in archives created with this plugin
#define PK_CAPS_HIDE       256                  // Show as normal files (hide packer icon), open with Ctrl+PgDn, not Enter

/* Flags for packing in memory */
#define MEM_OPTIONS_WANTHEADERS 1               // Return archive headers with packed data

#define MEMPACK_OK          0                   // Function call finished OK, but there is more data
#define MEMPACK_DONE        1                   // Function call finished OK, there is no more data

//-----------------------------------------------------------------------------
// Definitions of callback functions

// Ask to swap disk for multi-volume archive
typedef int (WINAPI * PFN_CHANGE_VOLUMEA)(LPCSTR szArcName, int nMode);
typedef int (WINAPI * PFN_CHANGE_VOLUMEW)(LPCWSTR szArcName, int nMode);

// Notify that data is processed - used for progress dialog
typedef int (WINAPI * PFN_PROCESS_DATAA)(LPCSTR szFileName, int nSize);
typedef int (WINAPI * PFN_PROCESS_DATAW)(LPCWSTR szFileName, int nSize);

//-----------------------------------------------------------------------------
// Structures

struct THeaderData
{
    char      ArcName[260];
    char      FileName[260];
    DWORD     Flags;
    DWORD     PackSize;
    DWORD     UnpSize;
    DWORD     HostOS;
    DWORD     FileCRC;
    DOS_FTIME FileTime;
    DWORD     UnpVer;
    DWORD     Method;
    DWORD     FileAttr;
    char    * CmtBuf;
    DWORD     CmtBufSize;
    DWORD     CmtSize;
    DWORD     CmtState;
};

#pragma pack(push, 4)
struct THeaderDataEx
{
    char      ArcName[1024];
    char      FileName[1024];
    DWORD     Flags;
    ULONGLONG PackSize;
    ULONGLONG UnpSize;
    DWORD     HostOS;
    DWORD     FileCRC;
    DOS_FTIME FileTime;
    DWORD     UnpVer;
    DWORD     Method;
    DWORD     FileAttr;
    char    * CmtBuf;
    DWORD     CmtBufSize;
    DWORD     CmtSize;
    DWORD     CmtState;
    char Reserved[1024];
};

struct THeaderDataExW
{
    WCHAR ArcName[1024];
    WCHAR FileName[1024];
    DWORD Flags;
    ULONGLONG PackSize;
    ULONGLONG UnpSize;
    DWORD HostOS;
    DWORD FileCRC;
    DOS_FTIME FileTime;
    DWORD UnpVer;
    DWORD Method;
    DWORD FileAttr;
    char* CmtBuf;
    DWORD CmtBufSize;
    DWORD CmtSize;
    DWORD CmtState;
    char Reserved[1024];
};
#pragma pack(pop)

struct TExtractedData
{
    ULONGLONG Length;                       // Length of the extracted data, in bytes
    BYTE Data[1];                           // Extracted data (variable length)
};

//-----------------------------------------------------------------------------
// Open archive information

#define PK_OM_LIST          0
#define PK_OM_EXTRACT       1

struct TOpenArchiveData
{
    union
    {
        LPCSTR szArchiveNameA;              // Archive name to open
        LPCWSTR szArchiveNameW;             // Archive name to open
    };

    int    OpenMode;                        // Open reason (See PK_OM_XXXX)
    int    OpenResult;
    char * CmtBuf;
    int    CmtBufSize;
    int    CmtSize;
    int    CmtState;
};

typedef struct
{
	int   size;
	DWORD PluginInterfaceVersionLow;
	DWORD PluginInterfaceVersionHi;
	char  DefaultIniName[MAX_PATH];
} TPackDefaultParamStruct;

//-----------------------------------------------------------------------------
// Plugin function prototypes

typedef BOOL   (WINAPI * CANYOUHANDLETHISFILEW)(LPCWSTR szFileName);
typedef BOOL   (WINAPI * CANYOUHANDLETHISFILE)(LPCSTR szFileName);
typedef HANDLE (WINAPI * OPENARCHIVEW)(TOpenArchiveData * pArchiveData);
typedef HANDLE (WINAPI * OPENARCHIVE)(TOpenArchiveData * pArchiveData);
typedef int    (WINAPI * CLOSEARCHIVE)(HANDLE hArchive);
typedef int    (WINAPI * GETPACKERCAPS)();
typedef int    (WINAPI * PROCESSFILEW)(HANDLE hArchive, PROCESS_FILE_OPERATION nOperation, LPCWSTR szDestPath, LPCWSTR szDestName);
typedef int    (WINAPI * PROCESSFILE)(HANDLE hArchive, PROCESS_FILE_OPERATION nOperation, LPCSTR szDestPath, LPCSTR szDestName);
typedef int    (WINAPI * READHEADER)(HANDLE hArchive, THeaderData * pHeaderData);
typedef int    (WINAPI * READHEADEREX)(HANDLE hArchive, THeaderDataEx * pHeaderData);
typedef int    (WINAPI * READHEADEREXW)(HANDLE hArchive, THeaderDataExW * pHeaderData);
typedef void   (WINAPI * SETCHANGEVOLPROC)(HANDLE hArchive, PFN_CHANGE_VOLUMEA PfnChangeVol);
typedef void   (WINAPI * SETCHANGEVOLPROCW)(HANDLE hArchive, PFN_CHANGE_VOLUMEW PfnChangeVol);
typedef void   (WINAPI * SETPROCESSDATAPROC)(HANDLE hArchive, PFN_PROCESS_DATAA PfnProcessData);
typedef void   (WINAPI * SETPROCESSDATAPROCW)(HANDLE hArchive, PFN_PROCESS_DATAW PfnProcessData);
typedef int    (WINAPI * PACKFILES)(LPCSTR szPackedFile, LPCSTR szSubPath, LPCSTR szSrcPath, LPCSTR szAddList, int nFlags);
typedef int    (WINAPI * PACKFILESW)(LPCWSTR szPackedFile, LPCWSTR szSubPath, LPCWSTR szSrcPath, LPCWSTR szAddList, int nFlags);
typedef int    (WINAPI * DELETEFILESW)(LPCWSTR szPackedFile, LPCWSTR szDeleteList);
typedef int    (WINAPI * DELETEFILES)(LPCSTR szPackedFile, LPCSTR szDeleteList);
typedef void   (WINAPI * CONFIGUREPACKER)(HWND hParent, HINSTANCE hDllInstance);
typedef void   (WINAPI * PACKSETDEFAULTPARAMS)(TPackDefaultParamStruct * dps);

#endif // __WCX_PDF_H__
