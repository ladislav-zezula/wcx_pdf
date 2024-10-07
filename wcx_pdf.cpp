/*****************************************************************************/
/* wcx_pdf.cpp                            Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Main file for Total Commander PDF unpacker plugin                         */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 12.06.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"                    // Our functions
#include "resource.h"                   // Resource symbols

//-----------------------------------------------------------------------------
// Local variables

#define PDF_BLOCK_SIZE 0x10000

PFN_PROCESS_DATAA PfnProcessDataA;      // Process data procedure (ANSI)
PFN_PROCESS_DATAW PfnProcessDataW;      // Process data procedure (UNICODE)
PFN_CHANGE_VOLUMEA PfnChangeVolA;       // Change volume procedure (ANSI)
PFN_CHANGE_VOLUMEW PfnChangeVolW;       // Change volume procedure (UNICODE)

//-----------------------------------------------------------------------------
// CanYouHandleThisFile(W) allows the plugin to handle files with different
// extensions than the one defined in Total Commander
// https://www.ghisler.ch/wiki/index.php?title=CanYouHandleThisFile

BOOL WINAPI CanYouHandleThisFileW(LPCWSTR szFileName)
{
    // Call the parser with bFastCheck = true.
    // If succeeded, it will return a dummy non-zero value which is not a complete object.
    return (TPdfDatabase::Open(szFileName, true) != NULL) ? TRUE : FALSE;
}

BOOL WINAPI CanYouHandleThisFile(LPCSTR szFileName)
{
    return CanYouHandleThisFileW(TAnsiToWide(szFileName));
}

//-----------------------------------------------------------------------------
// OpenArchive(W) should perform all necessary operations when an archive is
// to be opened
// https://www.ghisler.ch/wiki/index.php?title=OpenArchive

static HANDLE OpenArchiveAW(TOpenArchiveData * pArchiveData, LPCWSTR szArchiveName)
{
    // Set the default error code
    pArchiveData->OpenResult = E_UNKNOWN_FORMAT;

    // Check the valid parameters
    if(pArchiveData && szArchiveName && szArchiveName[0])
    {
        // Check the valid archive access
        if(pArchiveData->OpenMode == PK_OM_LIST || pArchiveData->OpenMode == PK_OM_EXTRACT)
        {
            TPdfDatabase * pPdfDb;

            // Set the open result to no memory
            pArchiveData->OpenResult = E_NO_MEMORY;

            // Attempt to open the PDF
            if((pPdfDb = TPdfDatabase::Open(szArchiveName, false)) != NULL)
            {
                pArchiveData->OpenResult = 0;
                return (HANDLE)(pPdfDb);
            }
        }
    }
    return NULL;
}

HANDLE WINAPI OpenArchiveW(TOpenArchiveData * pArchiveData)
{
    return OpenArchiveAW(pArchiveData, pArchiveData->szArchiveNameW);
}

HANDLE WINAPI OpenArchive(TOpenArchiveData * pArchiveData)
{
    return OpenArchiveAW(pArchiveData, TAnsiToWide(pArchiveData->szArchiveNameA));
}

//-----------------------------------------------------------------------------
// CloseArchive should perform all necessary operations when an archive
// is about to be closed.
// https://www.ghisler.ch/wiki/index.php?title=CloseArchive

int WINAPI CloseArchive(HANDLE hArchive)
{
    TPdfDatabase * pPdfDb;

    if((pPdfDb = TPdfDatabase::FromHandle(hArchive)) != NULL)
    {
        // Force-close all loaded files
        pPdfDb->RemoveAllFiles();

        // Release the database twice to make it go away 
        pPdfDb->Release();
        pPdfDb->Release();
    }
    return (pPdfDb != NULL) ? ERROR_SUCCESS : E_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// GetPackerCaps tells Totalcmd what features your packer plugin supports.
// https://www.ghisler.ch/wiki/index.php?title=GetPackerCaps

// GetPackerCaps tells Total Commander what features we support
int WINAPI GetPackerCaps()
{
    return(PK_CAPS_MULTIPLE |          // Archive can contain multiple files
           PK_CAPS_BY_CONTENT |        // Detect archive type by content
           PK_CAPS_SEARCHTEXT          // Allow searching for text in archives created with this plugin
          );
}

//-----------------------------------------------------------------------------
// ProcessFile should unpack the specified file or test the integrity of the archive.
// https://www.ghisler.ch/wiki/index.php?title=ProcessFile

static void MergePath(LPWSTR szFullPath, size_t ccFullPath, LPCWSTR szDestPath, LPCWSTR szDestName)
{
    // Always the buffer with zero
    if(ccFullPath != 0)
    {
        szFullPath[0] = 0;

        // Append destination path, if exists
        if(szDestPath && szDestPath[0])
        {
            StringCchCopy(szFullPath, ccFullPath, szDestPath);
        }

        // Append the name, if any
        if(szDestName && szDestName[0])
        {
            // Append backslash
            AddBackslash(szFullPath, ccFullPath);
            StringCchCat(szFullPath, ccFullPath, szDestName);
        }
    }
}

static int CallProcessDataProc(LPCWSTR szFullPath, int nSize)
{
    // Prioritize UNICODE version of the callback, if exists.
    // This leads to nicer progress dialog shown by Total Commander
    if(PfnProcessDataW != NULL)
        return PfnProcessDataW(szFullPath, nSize);

    // Call ANSI version of callback, if needed
    if(PfnProcessDataA != NULL)
        return PfnProcessDataA(TWideToAnsi(szFullPath), nSize);

    // Continue the operation
    return TRUE;
}

static int ExtractFile(HANDLE hArchive, LPCWSTR szDestPath, LPCWSTR szDestName)
{
    TPdfDatabase * pPdfDb;
    TPdfFile * pPdfFile;
    LPCTSTR szPlainName;
    HANDLE hFile;
    TCHAR szFullPath[MAX_PATH];
    int nError = 0;

    // Check the archive handle
    if((pPdfDb = TPdfDatabase::FromHandle(hArchive)) != NULL)
    {
        // Prepare the complete path of the destination file
        MergePath(szFullPath, _countof(szFullPath), szDestPath, szDestName);
        szPlainName = GetPlainName(szFullPath);

        // Attempt to find the file within thew PDF
        // Note that if the user selects "rename file", a completely arbitrary name can be passed here
        // and we are unable to find the file in the PDF.
        if((pPdfFile = pPdfDb->ReferenceFile(szPlainName)) != NULL)
        {
            DWORD dwWritten = 0;
            DWORD dwTotalSize = pPdfFile->FileSize();

            // Write the target file
            hFile = CreateFile(szFullPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
            if(hFile != INVALID_HANDLE_VALUE)
            {
                while(dwWritten < dwTotalSize)
                {
                    LPBYTE pbBuffer = pPdfFile->GetData().pbData + dwWritten;
                    DWORD dwToWrite = ((dwWritten + PDF_BLOCK_SIZE) < dwTotalSize) ? PDF_BLOCK_SIZE : dwTotalSize - dwWritten;
                    DWORD dwBytedTransferred = 0;

                    // Write the data to the file
                    if(!WriteFile(hFile, pbBuffer, dwToWrite, &dwBytedTransferred, NULL))
                    {
                        nError = E_EWRITE;
                        break;
                    }

                    // Increment the number of bytes written to the file
                    dwWritten += dwToWrite;
                    CallProcessDataProc(szFullPath, (int)(dwTotalSize));
                }

                CloseHandle(hFile);
            }
            else
            {
                nError = E_ECREATE;
            }
        }
        else
        {
            nError = E_EOPEN;
        }

        // Dereference the file
        pPdfDb->UnlockAndRelease();
    }
    else
    {
        nError = E_BAD_ARCHIVE;
    }
    return nError;
}

static int ExtractFileToMemory(HANDLE hArchive, TExtractedData ** ppOut, LPCWSTR szPlainName)
{
    TExtractedData * pExtractedData;
    TPdfDatabase * pPdfDb;
    TPdfFile * pPdfFile;
    int nError = 0;

    // Check the archive handle
    if((pPdfDb = TPdfDatabase::FromHandle(hArchive)) != NULL)
    {
        // Attempt to find the file within thew PDF
        // Note that if the user selects "rename file", a completely arbitrary name can be passed here
        // and we are unable to find the file in the PDF.
        if((pPdfFile = pPdfDb->ReferenceFile(szPlainName)) != NULL)
        {
            // Allocate buffer for the extracted data
            pExtractedData = (TExtractedData *)LocalAlloc(LPTR, sizeof(TExtractedData) + pPdfFile->FileSize());
            if(pExtractedData != NULL)
            {
                CallProcessDataProc(szPlainName, 0);
                memcpy(pExtractedData->Data, pPdfFile->GetData().pbData, pPdfFile->FileSize());
                pExtractedData->Length = pPdfFile->FileSize();
                CallProcessDataProc(szPlainName, pPdfFile->FileSize());
                ppOut[0] = pExtractedData;
            }
            else
            {
                nError = E_NO_MEMORY;
            }
        }
        else
        {
            nError = E_EOPEN;
        }

        // Dereference the file
        pPdfDb->UnlockAndRelease();
    }
    else
    {
        nError = E_BAD_ARCHIVE;
    }
    return nError;
}

int WINAPI ProcessFileW(HANDLE hArchive, PROCESS_FILE_OPERATION nOperation, LPCWSTR szDestPath, LPCWSTR szDestName)
{
    switch(nOperation)
    {
        case PK_TEST:           // If verify or skip the file, do nothing
        case PK_SKIP:
            return 0;

        case PK_EXTRACT:        // Do we have to extract the file?
            return ExtractFile(hArchive, szDestPath, szDestName);

        case PK_EXTRACT_TO_MEMORY:
            return ExtractFileToMemory(hArchive, (TExtractedData **)(szDestPath), szDestName);

        default:
            return E_NOT_SUPPORTED;
    }
}

int WINAPI ProcessFile(HANDLE hArchive, PROCESS_FILE_OPERATION nOperation, LPCSTR szDestPath, LPCSTR szDestName)
{
    return ProcessFileW(hArchive, nOperation, TAnsiToWide(szDestPath), TUTF8ToWide(szDestName));
}

//-----------------------------------------------------------------------------
// Totalcmd calls ReadHeader to find out what files are in the archive.
// https://www.ghisler.ch/wiki/index.php?title=ReadHeader

template <typename HDR>
static void StoreFoundFile(struct TPdfFile * pPdfFile, HDR * pHeaderData, const DOS_FTIME & ft)
{
    TCHAR szFileName[MAX_PATH];

    // Store the file name
    pPdfFile->GetName(szFileName, _countof(szFileName));
    StringCchCopyX(pHeaderData->FileName, _countof(pHeaderData->FileName), szFileName);

    // Store the file time
    pHeaderData->FileTime = ft;

    // Store the file sizes
    pHeaderData->PackSize = pPdfFile->PackSize();
    pHeaderData->UnpSize = pPdfFile->FileSize();

    // Store file attributes
    pHeaderData->FileAttr = FILE_ATTRIBUTE_ARCHIVE;
}

template <typename HDR>
int ReadHeaderTemplate(HANDLE hArchive, HDR * pHeaderData)
{
    TPdfDatabase * pPdfDb;
    TPdfFile * pPdfFile;
    DWORD dwErrCode = E_UNKNOWN_FORMAT;

    // Check the proper parameters
    if((pPdfDb = TPdfDatabase::FromHandle(hArchive)) != NULL)
    {
        // Split the action
        if((pPdfFile = pPdfDb->OpenNextFile()) != NULL)
        {
            StoreFoundFile(pPdfFile, pHeaderData, pPdfDb->FileTime());
            dwErrCode = 0;
        }
        else
        {
            // No more files found.
            dwErrCode = E_END_ARCHIVE;
        }

        pPdfDb->UnlockAndRelease();
    }
    return dwErrCode;
}

int WINAPI ReadHeader(HANDLE hArchive, THeaderData * pHeaderData)
{
    // Use the common template function
    return ReadHeaderTemplate(hArchive, pHeaderData);
}

int WINAPI ReadHeaderEx(HANDLE hArchive, THeaderDataEx * pHeaderData)
{
    // Use the common template function
    return ReadHeaderTemplate(hArchive, pHeaderData);
}

int WINAPI ReadHeaderExW(HANDLE hArchive, THeaderDataExW * pHeaderData)
{
    // Use the common template function
    return ReadHeaderTemplate(hArchive, pHeaderData);
}

//-----------------------------------------------------------------------------
// SetChangeVolProc(W) allows you to notify user about changing a volume when packing files
// https://www.ghisler.ch/wiki/index.php?title=SetChangeVolProc

// This function allows you to notify user
// about changing a volume when packing files
void WINAPI SetChangeVolProc(HANDLE /* hArchive */, PFN_CHANGE_VOLUMEA PfnChangeVol)
{
    PfnChangeVolA = PfnChangeVol;
}

void WINAPI SetChangeVolProcW(HANDLE /* hArchive */, PFN_CHANGE_VOLUMEW PfnChangeVol)
{
    PfnChangeVolW = PfnChangeVol;
}

//-----------------------------------------------------------------------------
// SetProcessDataProc(W) allows you to notify user about the progress when you un/pack files
// Note that Total Commander may use INVALID_HANDLE_VALUE for the hArchive parameter
// https://www.ghisler.ch/wiki/index.php?title=SetProcessDataProc

void WINAPI SetProcessDataProc(HANDLE /* hArchive */, PFN_PROCESS_DATAA PfnProcessData)
{
    PfnProcessDataA = PfnProcessData;
}

void WINAPI SetProcessDataProcW(HANDLE /* hArchive */, PFN_PROCESS_DATAW PfnProcessData)
{
    PfnProcessDataW = PfnProcessData;
}

//-----------------------------------------------------------------------------
// PackFiles(W) specifies what should happen when a user creates, or adds files to the archive
// https://www.ghisler.ch/wiki/index.php?title=PackFiles

int WINAPI PackFilesW(LPCWSTR szPackedFile, LPCWSTR szSubPath, LPCWSTR szSrcPath, LPCWSTR szAddList, int nFlags)
{
    UNREFERENCED_PARAMETER(szPackedFile);
    UNREFERENCED_PARAMETER(szSubPath);
    UNREFERENCED_PARAMETER(szSrcPath);
    UNREFERENCED_PARAMETER(szAddList);
    UNREFERENCED_PARAMETER(nFlags);
    return E_NOT_SUPPORTED;
}

// PackFiles adds file(s) to an archive
int WINAPI PackFiles(LPCSTR szPackedFile, LPCSTR szSubPath, LPCSTR szSrcPath, LPCSTR szAddList, int nFlags)
{
    UNREFERENCED_PARAMETER(szPackedFile);
    UNREFERENCED_PARAMETER(szSubPath);
    UNREFERENCED_PARAMETER(szSrcPath);
    UNREFERENCED_PARAMETER(szAddList);
    UNREFERENCED_PARAMETER(nFlags);
    return E_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// DeleteFiles(W) should delete the specified files from the archive
// https://www.ghisler.ch/wiki/index.php?title=DeleteFiles

int WINAPI DeleteFilesW(LPCWSTR szPackedFile, LPCWSTR szDeleteList)
{
    UNREFERENCED_PARAMETER(szPackedFile);
    UNREFERENCED_PARAMETER(szDeleteList);
    return E_NOT_SUPPORTED;
}

int WINAPI DeleteFiles(LPCSTR szPackedFile, LPCSTR szDeleteList)
{
    UNREFERENCED_PARAMETER(szPackedFile);
    UNREFERENCED_PARAMETER(szDeleteList);
    return E_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// ConfigurePacker gets called when the user clicks the Configure button
// from within "Pack files..." dialog box in Totalcmd.
// https://www.ghisler.ch/wiki/index.php?title=ConfigurePacker

void WINAPI ConfigurePacker(HWND hParent, HINSTANCE hDllInstance)
{
    UNREFERENCED_PARAMETER(hDllInstance);
    UNREFERENCED_PARAMETER(hParent);
}

//-----------------------------------------------------------------------------
// PackSetDefaultParams is called immediately after loading the DLL, before
// any other function. This function is new in version 2.1. It requires Total
// Commander >=5.51, but is ignored by older versions.
// https://www.ghisler.ch/wiki/index.php?title=PackSetDefaultParams

void WINAPI PackSetDefaultParams(TPackDefaultParamStruct * dps)
{
    UNREFERENCED_PARAMETER(dps);
}
