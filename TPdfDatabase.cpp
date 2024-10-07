/*****************************************************************************/
/* TPdfDatabase.cpp                       Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Global (non-class) PDF functions                                          */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 27.07.23  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"

//-----------------------------------------------------------------------------
// Local (non-class) functions

static LPCSTR SkipSpaces(LPCSTR szString)
{
    while(*szString == ' ' || *szString == '\t')
        szString++;
    return szString;
}

static LPCSTR LoadOneBool(LPCSTR szString, int & nResult)
{
    if(!_strnicmp(szString, "true", 4))
    {
        nResult = 1;
        return szString + 4;
    }
    if(!_strnicmp(szString, "false", 5))
    {
        nResult = 0;
        return szString + 5;
    }
    return NULL;
}

static LPCSTR LoadOneInt(LPCSTR szString, int & nResult)
{
    int nSignValue = 1;
    int nDigits = 0;
    int nValue = 0;

    // Skip spaces
    if(szString[0] == 0)
        return NULL;
    szString = SkipSpaces(szString);

    // Check for the minus sign
    if(*szString == '-')
    {
        nSignValue = -1;
        szString++;
    }

    // Parse the number
    while('0' <= szString[0] && szString[0] <= '9')
    {
        nValue = nValue * 10 + (*szString++ - '0');
        nDigits++;
    }

    // Give the result
    nResult = nValue * nSignValue;
    return nDigits ? szString : NULL;
}

static void FileTimeToDosFTime(DOS_FTIME & DosTime, const FILETIME & ft)
{
    SYSTEMTIME stUTC;                   // Local file time
    SYSTEMTIME st;                      // Local file time

    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &st);

    DosTime.ft_year = st.wYear - 60;
    DosTime.ft_month = st.wMonth;
    DosTime.ft_day = st.wDay;
    DosTime.ft_hour = st.wHour;
    DosTime.ft_min = st.wMinute;
    DosTime.ft_tsec = (st.wSecond / 2);
}

static bool LoadOneString(LPSTR szBuffer, size_t ccBuffer, LPCSTR szPointer)
{
    LPSTR szBufferEnd = szBuffer + ccBuffer - 1;

    // Find the next slash character
    while(szPointer[0] != 0 && szPointer[0] != '/' && szPointer[0] != '[')
        szPointer++;

    // For compound encoding, load all encodings
    if(szPointer[0] == '[')
    {
        // Skip the opening bracket
        szPointer++;

        // Copy the string
        while(szBuffer < szBufferEnd && szPointer[0] != 0 && szPointer[0] != ']')
            *szBuffer++ = *szPointer++;
        szBuffer[0] = 0;
        return true;
    }

    // If there is a slash character, copy the subtype
    if(szPointer[0] == '/')
    {
        if(szBuffer < szBufferEnd)
            *szBuffer++ = *szPointer++;

        while(szBuffer < szBufferEnd && szPointer[0] != '/' && isalnum(szPointer[0]))
            *szBuffer++ = *szPointer++;
        szBuffer[0] = 0;
        return true;
    }
    return false;
}

LPCSTR GetObjectVariablePosition(LPCSTR szObjParams, LPCSTR szVariableName, bool bStringBeginAllowed = false)
{
    LPCSTR szVariablePos = szObjParams;

    // Find the position of the variable
    while((szVariablePos = strstr(szVariablePos, szVariableName)) != NULL)
    {
        // Move to the position of the integer value
        szVariablePos = szVariablePos + strlen(szVariableName);

        // Spaces are always allower
        if(0x09 <= szVariablePos[0] && szVariablePos[0] <= 0x20)
        {
            return SkipSpaces(szVariablePos);
        }

        // For strings, we allow also the slash or '['
        if(bStringBeginAllowed && (szVariablePos[0] == '/' || szVariablePos[0] == '['))
        {
            return szVariablePos;
        }
    }

    // We did not find that variable in the object parameters
    return NULL;
}

bool GetObjectVariableInt(LPCSTR szObjParams, LPCSTR szVariableName, int & RefValue, int nDefaultValue, bool bBoolAllowed)
{
    LPCSTR szVariablePos;
    int nResult = 0;

    // Find the position of the variable
    if((szVariablePos = GetObjectVariablePosition(szObjParams, szVariableName)) != NULL)
    {
        // Convert to integer
        if(LoadOneInt(szVariablePos, nResult) != NULL)
        {
            RefValue = nResult;
            return true;
        }

        // Try to convert from Boolean, if allowed
        if(bBoolAllowed && LoadOneBool(szVariablePos, nResult))
        {
            RefValue = nResult;
            return true;
        }
    }

    // Supply default value
    RefValue = nDefaultValue;
    return false;
}

bool GetObjectVariableString(LPCSTR szObjParams, LPCSTR szVariableName, LPSTR szBuffer, size_t ccBuffer, LPCSTR szDefaultValue)
{
    LPCSTR szVariablePos;

    // Find the position of the variable
    if((szVariablePos = GetObjectVariablePosition(szObjParams, szVariableName, true)) != NULL)
    {
        // Convert to integer
        if(LoadOneString(szBuffer, ccBuffer, szVariablePos) != NULL)
        {
            return true;
        }
    }

    // Supply default value
    if(szDefaultValue != NULL)
        StringCchCopyA(szBuffer, ccBuffer, szDefaultValue);
    return false;
}

//-----------------------------------------------------------------------------
// Static function that opens a file and converts it to PDF database

TPdfDatabase * TPdfDatabase::Open(LPCWSTR szFileName, bool bFastCheck)
{
    LARGE_INTEGER FileSize = {0};
    TPdfDatabase * pPdfDb = NULL;
    FILETIME ft = {0};
    HANDLE hFile;
    HANDLE hMap;
    LPBYTE pbFile;

    // Open the file for read access
    hFile = CreateFileW(szFileName, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        // Retrieve the file time
        GetFileTime(hFile, NULL, NULL, &ft);

        // Retrieve the file size
        if(GetFileSizeEx(hFile, &FileSize))
        {
            if(0x300 <= FileSize.QuadPart)
            {
                hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if(hMap != NULL)
                {
                    pbFile = (LPBYTE)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
                    if(pbFile != NULL)
                    {
                        // Load the PDF database
                        pPdfDb = Load(pbFile, (size_t)(FileSize.QuadPart), ft, bFastCheck);
                        UnmapViewOfFile(pbFile);
                    }
                    CloseHandle(hMap);
                }
            }
        }
        CloseHandle(hFile);
    }
    return pPdfDb;
}

TPdfDatabase * TPdfDatabase::Load(LPBYTE pbFileData, size_t cbFileData, FILETIME & ft, bool bFastCheck)
{
    TPdfDatabase * pPdfDb = NULL;
    TPdfBlob PdfFile(pbFileData, cbFileData);
    LPBYTE pbPdfBegin = NULL;
    LPBYTE pbPdfEnd = NULL;

    // Check the begin of the PDF
    if(PdfFile.CheckData("%PDF-1.", 7))
    {
        // Skip the line and remember the data as the end of the PDF
        pbPdfBegin = PdfFile.LoadOneLine(NULL, 0);
        if(pbPdfBegin != NULL)
        {
            // Find the end of the PDF. There may be EOLs after the %%EOF marker
            if((pbPdfEnd = PdfFile.FindEndOfPdf()) != NULL && pbPdfEnd > pbPdfBegin)
            {
                // If we need a fast check, return a dummy value
                if(bFastCheck)
                    return PDF_PVOID_TRUE;

                // Construct the PDF Database object
                pPdfDb = new TPdfDatabase(pbPdfBegin, pbPdfEnd, ft);
            }
        }
    }

    // Return the created PDF database or NULL
    return pPdfDb;
}

//-----------------------------------------------------------------------------
// Member functions

TPdfDatabase::TPdfDatabase(LPBYTE pbPdfBegin, LPBYTE pbPdfEnd, FILETIME & ft) : TPdfBlob(pbPdfBegin, pbPdfEnd, true)
{
    // Initialize the object
    InitializeCriticalSection(&m_Lock);
    InitializeListHead(&m_Files);
    m_MagicSignature = PDF_MAGIC_SIGNATURE;
    m_dwFiles = 0;
    m_dwRefs = 1;

    // Fill-in the PDF information
    FileTimeToDosFTime(m_FileTime, ft);
}

TPdfDatabase::~TPdfDatabase()
{
    // Close all files that were open in the meantime
    assert(m_dwRefs == 0);
    RemoveAllFiles();

    // Free the rest of the object
    DeleteCriticalSection(&m_Lock);
}

TPdfDatabase * TPdfDatabase::FromHandle(HANDLE hHandle)
{
    TPdfDatabase * pPdfDb;

    if(hHandle != NULL && hHandle != INVALID_HANDLE_VALUE)
    {
        pPdfDb = static_cast<TPdfDatabase *>(hHandle);
        if(pPdfDb->m_MagicSignature == PDF_MAGIC_SIGNATURE)
        {
            EnterCriticalSection(&pPdfDb->m_Lock);
            pPdfDb->AddRef();
            return pPdfDb;
        }
    }
    return NULL;

}

DWORD TPdfDatabase::AddRef()
{
    return ++m_dwRefs;
}

DWORD TPdfDatabase::Release()
{
    if(m_dwRefs-- == 1)
    {
        delete this;
        return 0;
    }
    return m_dwRefs;
}

void TPdfDatabase::InsertFile(TPdfFile * pPdfFile)
{
    // Insert the file to the list of files
    assert(pPdfFile->m_Entry.Flink == NULL);
    assert(pPdfFile->m_Entry.Blink == NULL);
    InsertTailList(&m_Files, &pPdfFile->m_Entry);
    pPdfFile->AddRef();

    // Set as the owner of the file
    pPdfFile->SetOwner(this);
}

void TPdfDatabase::RemoveAllFiles()
{
    TPdfFile * pPdfFile;
    PLIST_ENTRY pHeadEntry = &m_Files;
    PLIST_ENTRY pListEntry;

    for(pListEntry = m_Files.Flink; pListEntry != pHeadEntry; )
    {
        // Get the reference to the file
        pPdfFile = CONTAINING_RECORD(pListEntry, TPdfFile, m_Entry);
        pListEntry = pListEntry->Flink;

        // Dereference the file. If this is the last reference,
        // the file will remove from the list
        pPdfFile->Release();
    }
}

TPdfFile * TPdfDatabase::OpenNextFile()
{
    TPdfFile * pPdfFile = NULL;

    try
    {
        if((pPdfFile = OpenNextFile_SEQ()) != NULL)
        {
            InsertFile(pPdfFile);
            pPdfFile->Release();
        }
    }
    catch(std::bad_alloc)
    {
        pPdfFile = NULL;
    }
    return pPdfFile;
}

TPdfFile * TPdfDatabase::ReferenceFile(LPCTSTR szPlainName)
{
    PLIST_ENTRY pHeadEntry = &m_Files;
    PLIST_ENTRY pListEntry = &m_Files;
    LPCTSTR szExtension = GetFileExtension(szPlainName);
    LPCTSTR szFileIndex;
    LPTSTR szEndPtr = NULL;
    DWORD dwObjectId;
    TCHAR szBuffer[32];

    // Check the digits after the last dash
    szFileIndex = _tcsrchr(szPlainName, _T('-'));
    if(szFileIndex > szPlainName && (szFileIndex + 1) < szExtension && isdigit(szFileIndex[1]))
    {
        // Skip the dash
        if(szExtension - (szFileIndex = szFileIndex + 1) <= 10)
        {
            // Convert the string to integer
            StringCchCopyN(szBuffer, _countof(szBuffer), szFileIndex, (szExtension - szFileIndex));
            dwObjectId = _tcstol(szBuffer, &szEndPtr, 10);

            // Check the end of the integer
            if(szEndPtr[0] == 0)
            {
                // Find that file in the PDF
                for(pListEntry = pHeadEntry->Flink; pListEntry != pHeadEntry; pListEntry = pListEntry->Flink)
                {
                    TPdfFile * pPdfFile = CONTAINING_RECORD(pListEntry, TPdfFile, m_Entry);

                    if(pPdfFile->m_dwObjectId == dwObjectId)
                    {
                        return pPdfFile;
                    }
                }
            }
        }
    }
    return NULL;
}

void TPdfDatabase::UnlockAndRelease()
{
    Release();
    LeaveCriticalSection(&m_Lock);
}

TPdfFile * TPdfDatabase::OpenNextFile_SEQ()
{
    TPdfFile * pPdfFile;

    // Try to locate the next object
    while(!IsEof())
    {
        // Check if this may be an object header
        if(CheckBeginOfObject() != NULL)
        {
            if((pPdfFile = LoadPdfObject()) != NULL)
            {
                return pPdfFile;
            }
        }
        else
        {
            // Search the next 0x0A character
            if(FindEndOfLine() == NULL)
                break;
        }
    }
    return NULL;
}

TPdfFile * TPdfDatabase::LoadPdfObject()
{
    TPdfFile * pPdfFile = NULL;
    LPBYTE pbObjectEnd;
    LPSTR szObjParams;
    int nObjectId = 0;

    // Parse the header of the object
    if(ParseBeginOfObject(nObjectId) == NULL)
        return NULL;

    // Load the object parameters
    if((szObjParams = LoadObjectParameters()) != NULL)
    {
        // We only accept objects that are streams
        // Either there is "stream" or "endobj". Note that not all PDFs have "stream\x0d\x0a".
        // Example: 001f7e150598548000e088d61a8bc2806271d215b776c88fd9aa7793aaee2b01
        if(IsBeginOfStream())
        {
            // We need to save the pointer to the begin of the object
            LPBYTE pbObjectPtr = pbPtr;

            // Because the object length is pretty unreliable, we need to estimate the object length manually.
            if((pbObjectEnd = FindEndOfStream(szObjParams)) != NULL)
            {
                // Calculate the length of the object
                if((pPdfFile = new TPdfFile(pbObjectPtr, pbObjectEnd, nObjectId)) != NULL)
                {
                    if(pPdfFile->Load(szObjParams) != ERROR_SUCCESS)
                    {
                        pPdfFile->Release();
                        pPdfFile = NULL;
                    }
                }
            }

            // Skip the "endstream", if any
            SkipEndOfStream();
        }

        // Free the object parameters
        HeapFree(g_hHeap, 0, szObjParams);
    }

    // Skip the "endobj", if any
    SkipEndOfObject();
    return pPdfFile;
}

LPBYTE TPdfDatabase::CheckBeginOfObject()
{
    TPdfBlob TempBlob;
    LPBYTE pbSavePtr;
    LPBYTE pbResult = NULL;
    int nObjectId = 0;

    // Skip all EOLs
    pbSavePtr = SkipEndOfLine();

    // Check for "# # obj"
    if(ParseBeginOfObject(nObjectId))
        pbResult = pbSavePtr;
    SetPosition(pbSavePtr);

    // If failed, restore the position of the current pointer
    return pbResult;
}

LPBYTE TPdfDatabase::ParseBeginOfObject(int & nObjectId)
{
    LPCSTR szCharPtr;
    char szOneLine[256];
    int nDummy = 0;

    // Load the whole line
    if((pbPtr = LoadOneLine(szOneLine, _countof(szOneLine))) == NULL)
        return NULL;
    szCharPtr = szOneLine;

    // Load the object ID
    if((szCharPtr = LoadOneInt(szCharPtr, nObjectId)) == NULL)
        return NULL;

    // Load the second integer
    if((szCharPtr = LoadOneInt(szCharPtr, nDummy)) == NULL)
        return NULL;

    // Check for the "obj"
    return (strcmp(SkipSpaces(szCharPtr), "obj") == 0) ? pbPtr : NULL;
}

LPSTR TPdfDatabase::LoadObjectParameters()
{
    LPSTR szObjectParameters = NULL;

    // Check if there is an object specification
    if(CheckData("<<", 2))
    {
        LPBYTE pbParamsBegin = pbPtr;
        DWORD dwNestCount = 1;

        // Skip the opening sequence
        pbPtr += 2;

        // Keep looking for end of the object
        while(pbPtr < (pbEnd - 2))
        {
            // Objects may be nested
            if(pbPtr[0] == '<' && pbPtr[1] == '<')
            {
                pbPtr += 2;
                dwNestCount++;
                continue;
            }

            if(pbPtr[0] == '>' && pbPtr[1] == '>')
            {
                // Can't go to negative nest level
                if(dwNestCount == 0)
                    return NULL;
                pbPtr += 2;

                // If we are at the closing sign, allocate the buffer and return the string
                if(dwNestCount-- == 1)
                {
                    size_t ccObjectParameters = (pbPtr - pbParamsBegin);

                    // Allocate buffer with the object parameters
                    szObjectParameters = (LPSTR)HeapAlloc(g_hHeap, 0, ccObjectParameters + 1);
                    if(szObjectParameters != NULL)
                    {
                        memcpy(szObjectParameters, pbParamsBegin, ccObjectParameters);
                        szObjectParameters[ccObjectParameters] = 0;
                    }

                    // Skip the end of the line and return what we got
                    SkipPdfSpaces();
                    SkipEndOfLine();
                    break;
                }
                continue;
            }
            pbPtr++;
        }
    }
    return szObjectParameters;
}

LPBYTE TPdfDatabase::IsBeginOfStream()
{
    // Is there enough space in the buffer?
    if((pbPtr + 8) < pbEnd)
    {
        // Could it be "stream"?
        if(pbPtr[0] == 's')
        {
            if(!memcmp(pbPtr, "stream\r\n", 8))
                return (pbPtr = pbPtr + 8);
            if(!memcmp(pbPtr, "stream\r", 7))
                return (pbPtr = pbPtr + 7);
            if(!memcmp(pbPtr, "stream\n", 7))
                return (pbPtr = pbPtr + 7);
        }
    }
    return NULL;
}

LPBYTE TPdfDatabase::FindEndOfStream(LPCSTR szObjParams)
{
    LPBYTE pbSavePtr = pbPtr;
    LPBYTE pbTestPtr = pbPtr;
    LPBYTE pbEndStream;
    int nLength = 0;

    // Try to get the compressed length
    GetObjectVariableInt(szObjParams, "/Length", nLength);

    // Try of there is "endstream" at the alleged length
    if((pbTestPtr = pbPtr + nLength) < (pbEnd - 9))
    {
        // Remember the end of the file pointer
        pbEndStream = pbTestPtr;

        // Skip an eventual end of the line
        while((pbTestPtr < pbEnd) && (pbTestPtr[0] == 0x0A || pbTestPtr[0] == 0x0D))
            pbTestPtr++;

        // Check for 
        if((pbTestPtr + 9) <= pbEnd && !memcmp(pbTestPtr, "endstream", 9))
        {
            SetPosition(pbTestPtr + 9);
            return pbEndStream;
        }
    }

    // Ok, the length is unreliable. Search for "endstream" manually.
    pbTestPtr = pbPtr;

    // Keep going until we find "endstream".
    // This may find a wrong end-of-stream if the object is e.g. an embedded PDF 
    while(pbTestPtr < (pbEnd - 9))
    {
        // Did we find end of the stream?
        if(pbTestPtr[0] == 'e' && !memcmp(pbTestPtr, "endstream", 9))
        {
            // Check for "\x0D\x0Aendstream" (the most frequent end of the stream)
            if((pbTestPtr - 2) > pbSavePtr && !memcmp(pbTestPtr - 2, "\n\rendstream", 11))
                return pbTestPtr - 2;

            // Check fort "\x0Aendstream" (0106a960deb6409afad94b696a2466c5ff883ad81b71d40663a220ca774b1bc1)
            if((pbTestPtr - 1) > pbSavePtr && !memcmp(pbTestPtr - 1, "\nendstream", 10))
                return pbTestPtr - 1;

            // Check for "\x0Dendstream" (0025ee1f882244e75b68c0c4ff185bfa678f13470c0254039bb46e648fc00e1e)
            if((pbTestPtr - 1) > pbSavePtr && !memcmp(pbTestPtr - 1, "\rendstream", 10))
                return pbTestPtr - 1;
        }

        // Move one char fuhrter
        pbTestPtr++;
    }

    // Not found
    return NULL;
}

LPBYTE TPdfDatabase::SkipEndOfStream()
{
    // Is there enough space in the buffer?
    if((pbPtr + 11) < pbEnd)
    {
        // Could it be "endstream"?
        if(pbPtr[0] == 'e')
        {
            if(!memcmp(pbPtr, "endstream\r\n", 11))
                return (pbPtr = pbPtr + 11);
            if(!memcmp(pbPtr, "endstream\r", 10))
                return (pbPtr = pbPtr + 10);
            if(!memcmp(pbPtr, "endstream\n", 10))
                return (pbPtr = pbPtr + 10);
        }
    }
    return NULL;
}

LPBYTE TPdfDatabase::SkipEndOfObject()
{
    // Is there enough space in the buffer?
    if((pbPtr + 8) < pbEnd)
    {
        // Could it be "endstream"?
        if(pbPtr[0] == 'e')
        {
            if(!memcmp(pbPtr, "endobj\r\n", 8))
                return (pbPtr = pbPtr + 8);
            if(!memcmp(pbPtr, "endobj\r", 7))
                return (pbPtr = pbPtr + 7);
            if(!memcmp(pbPtr, "endobj\n", 7))
                return (pbPtr = pbPtr + 7);
        }
    }
    return NULL;
}
