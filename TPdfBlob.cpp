/*****************************************************************************/
/* TPdfFile.cpp                           Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Implementation of PDF file                                                */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 18.06.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"

//#define __DIAGNOSE_HEAP_ERRORS

//-----------------------------------------------------------------------------
// Constructor and destructor

TPdfBlob::TPdfBlob(LPBYTE pb0, LPBYTE pb1, bool bAllocate)
{
    // Sanity checks
    assert(pb1 >= pb0);

    // Setup the blob
    pbData = pbPtr = pbEnd = NULL;
    SetData(pb0, pb1, bAllocate);
}

TPdfBlob::TPdfBlob(LPBYTE pb, size_t cb, bool bAllocate)
{
    pbData = pbPtr = pbEnd = NULL;
    SetData(pb, pb+cb, bAllocate);
}

TPdfBlob::TPdfBlob(const TPdfBlob & Source)
{
    pbData = pbPtr = pbEnd = NULL;
    SetData(Source);
}

TPdfBlob::TPdfBlob()
{
    pbData = pbPtr = pbEnd = NULL;
    SetData(NULL, NULL, false);
}

TPdfBlob::~TPdfBlob()
{
    FreeData();
}

//-----------------------------------------------------------------------------
// Member functions

DWORD TPdfBlob::SetData(LPCTSTR szFileName)
{
    LARGE_INTEGER FileSize = {0};
    HANDLE hFile;
    HANDLE hMap;
    LPBYTE pbFile;
    DWORD dwErrCode = ERROR_INVALID_DATA;

    // Open the file for read access
    hFile = CreateFileW(szFileName, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
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
                        dwErrCode = SetData(pbFile, pbFile + FileSize.LowPart, true);
                        UnmapViewOfFile(pbFile);
                    }
                    else
                    {
                        dwErrCode = GetLastError();
                    }
                    CloseHandle(hMap);
                }
                else
                {
                    dwErrCode = GetLastError();
                }
            }
        }
        CloseHandle(hFile);
    }
    else
    {
        dwErrCode = GetLastError();
    }

    return dwErrCode;
}

DWORD TPdfBlob::SetData(LPBYTE pb0, LPBYTE pb1, bool bAllocate)
{
    DWORD dwErrCode = ERROR_SUCCESS;

    // Sanity check
    assert(pb1 >= pb0);

    // Free data, whatever they are
    FreeData();

    // Allocate buffer if needed
    if(bAllocate)
    {
        if((dwErrCode = Resize(pb1 - pb0)) != ERROR_SUCCESS)
            return dwErrCode;
        memcpy(pbData, pb0, pb1 - pb0);
    }
    else
    {
        pbData = pbPtr = pb0;
        pbEnd = pb1;
    }
    return true;
}

DWORD TPdfBlob::SetData(const TPdfBlob & Source)
{
    return SetData(Source.pbData, Source.pbEnd, Source.bAllocated);
}

void TPdfBlob::MoveFrom(TPdfBlob & Source)
{
    // Copy all members from the source
    pbData = Source.pbData;
    pbEnd = Source.pbEnd;
    bAllocated = Source.bAllocated;
    ResetPosition();

    // Reset all variables in the source
    Source.pbData = Source.pbPtr = Source.pbEnd = NULL;
    Source.bAllocated = false;
}

bool TPdfBlob::SetPosition(LPBYTE pb)
{
    // Verify whether the data pointer is within the range
    if(pb < pbData || pb > pbEnd)
        return false;

    // Set the new position
    pbPtr = pb;
    return true;
}

void TPdfBlob::ResetPosition()
{
    pbPtr = pbData;
}

void TPdfBlob::FreeData()
{
    if(pbData && bAllocated)
    {
#ifndef __DIAGNOSE_HEAP_ERRORS
        HeapFree(g_hHeap, 0, pbData);
#else
        free(pbData);
#endif
    }

    pbData = pbPtr = pbEnd = NULL;
    bAllocated = false;
}

LPBYTE TPdfBlob::FindEndOfLine()
{
    // Find one of the EOL characters
    while(pbPtr < pbEnd)
    {
        if(pbPtr[0] == 0x0D || pbPtr[0] == 0x0A)
        {
            // Skip all EOLs
            while((pbPtr < pbEnd) && (pbPtr[0] == 0x0D || pbPtr[0] == 0x0A))
                pbPtr++;
            return pbPtr;
        }

        // Move to the next char
        pbPtr++;
    }
    return NULL;
}

LPBYTE TPdfBlob::SkipEndOfLine()
{
    while((pbPtr < pbEnd) && (pbPtr[0] == 0x0A || pbPtr[0] == 0x0D))
        pbPtr++;
    return pbPtr;
}

LPBYTE TPdfBlob::SkipPdfSpaces()
{
    while((pbPtr < pbEnd) && (pbPtr[0] == 0x20))
        pbPtr++;
    return pbPtr;
}

DWORD TPdfBlob::AppendBytes(LPCVOID pvData, size_t cbData)
{
    // Shall we reallocate the buffer?
    if((pbPtr + cbData) > pbEnd)
    {
        size_t nOldSize = (pbEnd - pbData);
        size_t nNewSize = nOldSize + cbData;
        DWORD dwErrCode;

        // Calc new size
        if((nOldSize * 2) > nNewSize)
            nNewSize = (nOldSize * 2);

        // Resize the buffer. Set min. 0x10 bytes
        if((dwErrCode = Resize(nNewSize)) != ERROR_SUCCESS)
        {
            return dwErrCode;
        }
    }

    // Append the data
    memcpy(pbPtr, pvData, cbData);
    pbPtr += cbData;
    return ERROR_SUCCESS;
}

DWORD TPdfBlob::ReadByte(BYTE & RefValue)
{
    if(pbPtr >= pbEnd)
        return ERROR_NO_MORE_ITEMS;

    RefValue = *pbPtr++;
    return ERROR_SUCCESS;
}

DWORD TPdfBlob::GetHexValue(BYTE & RefValue)
{
    // Skip EOLs
    pbPtr = SkipEndOfLine();

    // Are there at least two chars?
    if((pbPtr + 2) <= pbEnd)
    {
        // Convert both to unsigned char to get rid of negative indexes produced by szString[x]
        BYTE StringByte0 = pbPtr[0];
        BYTE StringByte1 = pbPtr[1];
        pbPtr += 2;

        // Each character must be within the range of 0x80
        if(StringByte0 > 0x80 || StringByte1 > 0x80)
            return ERROR_INVALID_PARAMETER;
        if(CharToByte[StringByte0] == 0xFF || CharToByte[StringByte1] == 0xFF)
            return ERROR_INVALID_PARAMETER;

        RefValue = (CharToByte[StringByte0] << 0x04) | CharToByte[StringByte1];
        return ERROR_SUCCESS;
    }
    return ERROR_NO_DATA;
}

LPBYTE TPdfBlob::LoadOneLine(LPSTR szBuffer, size_t ccBuffer)
{
    LPBYTE pbSavePtr = pbPtr;
    LPBYTE pbNextPtr = NULL;

    // Copy the line from the PDF
    while(pbPtr < pbEnd)
    {
        if(pbPtr + 2 <= pbEnd)
        {
            if(pbPtr[0] == '<' && pbPtr[1] == '<')
            {
                pbNextPtr = pbPtr + 2;
                break;
            }
            if(pbPtr[0] == 0x0D && pbPtr[1] == 0x0A)
            {
                pbNextPtr = pbPtr + 2;
                break;
            }
        }
        if(pbPtr < pbEnd && (pbPtr[0] == 0x0A || pbPtr[0] == 0x0D))
        {
            pbNextPtr = pbPtr + 1;
            break;
        }
        pbPtr++;
    }

    // Did we found a properly terminated EOL?
    if(pbNextPtr != NULL)
    {
        // Do we have buffer for storing the line?
        if(szBuffer && ccBuffer)
        {
            size_t cbBytesToCopy = (pbPtr - pbSavePtr);

            if(cbBytesToCopy + 1 <= ccBuffer)
            {
                memcpy(szBuffer, pbSavePtr, cbBytesToCopy);
                szBuffer[cbBytesToCopy] = 0;
            }
            else
            {
                pbNextPtr = NULL;
            }
        }

        // Set the new position to the EOL
        pbPtr = pbNextPtr;
    }
    return pbNextPtr;
}

LPBYTE TPdfBlob::FindEndOfPdf()
{
    LPBYTE pbPdfEnd = pbEnd - 5;
    size_t nCharCount = 0;

    // Try to find the EOF marker. Don't go too far.
    while(pbPdfEnd > pbData && nCharCount < 0x20)
    {
        if(pbPdfEnd[0] == '%' && !memcmp(pbPdfEnd, "%%EOF", 5))
            return pbPdfEnd;
        nCharCount++;
        pbPdfEnd--;
    }

    // If we haven't found the EOF marker, just take the end of the file
    return pbEnd;
}

LPBYTE TPdfBlob::ReallocateBuffer(LPBYTE ptr, SIZE_T cbNewSize)
{
    LPBYTE pbNewData = NULL;

#ifndef __DIAGNOSE_HEAP_ERRORS
    if(pbData == NULL)
    {
        pbNewData = (LPBYTE)HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, cbNewSize);
    }
    else
    {
        pbNewData = (LPBYTE)HeapReAlloc(g_hHeap, HEAP_ZERO_MEMORY, ptr, cbNewSize);
        if(pbNewData == NULL)
        {
            HeapFree(g_hHeap, 0, pbData);
            pbData = NULL;
        }
}
#else
    if(pbPtr == NULL)
    {
        pbNewData = (LPBYTE)malloc(cbNewSize);
    }
    else
    {
        pbNewData = (LPBYTE)realloc(ptr, cbNewSize);
    }
#endif
    return pbNewData;
}

DWORD TPdfBlob::Resize(size_t cbNewSize)
{
    LPBYTE pbNewData;

    if(pbData == NULL || bAllocated)
    {
        // We need to remember offset to the data
        size_t cbOffset = (pbPtr - pbData);

        // Make sure there's at least one byte allocated
        cbNewSize = max(cbNewSize, 1);

        // Reallocate the buffer
        if((pbNewData = ReallocateBuffer(pbData, cbNewSize)) == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Reconfigure the data
        pbData = pbNewData;
        pbPtr = pbData + cbOffset;
        pbEnd = pbData + cbNewSize;
        bAllocated = true;
        return ERROR_SUCCESS;
    }

    // Should never happen
    assert(false);
    return ERROR_INVALID_PARAMETER;
}

bool TPdfBlob::CheckData(LPCVOID pv, size_t cb)
{
    // Beware of buffer overflow
    return (pbPtr + cb <= pbEnd && memcmp(pbPtr, pv, cb) == 0);
}

bool TPdfBlob::GetOneByte(BYTE & RefValue)
{
    if(!IsEof())
    {
        RefValue = *pbPtr++;
        return true;
    }
    return false;
}

bool TPdfBlob::IsEof()
{
    return (pbPtr != NULL && pbPtr >= pbEnd);
}
