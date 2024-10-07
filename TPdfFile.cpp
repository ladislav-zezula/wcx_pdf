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
#include "decode_ascii85.h"                     // Decoding ASCII85 data
#include "decode_ccitt.h"                       // Decoding CCITTFax data
#include "decode_lzw.h"                         // Decoding LZW data
#include "decode_runlength.h"                   // Decoding run-length data
#include "./zlib/zlib.h"                        // Decoding FlateDecode data

//-----------------------------------------------------------------------------
// Constructor and destructor

TPdfFile::TPdfFile(LPBYTE pbData, LPBYTE pbEnd, DWORD dwObjectId) : TPdfBlob(pbData, pbEnd)
{
    m_Entry.Flink = m_Entry.Blink = NULL;
    m_pPdfDb = NULL;
    m_dwRefs = 1;

    // Setup the filters
    memset(m_Filters, 0, sizeof(m_Filters));
    m_dwFilters = 0;

    // Setup the object data
    m_szExtension = NULL;
    m_szFileType = NULL;
    m_dwObjectId = dwObjectId;
    m_dwRawSize = 0;
}

TPdfFile::~TPdfFile()
{
    if(m_pPdfDb != NULL)
    {
        RemoveEntryList(&m_Entry);
        m_pPdfDb->Release();
    }
}

//-----------------------------------------------------------------------------
// Member functions

DWORD TPdfFile::AddRef()
{
    return ++m_dwRefs;
}

DWORD TPdfFile::Release()
{
    if(m_dwRefs == 1)
    {
        delete this;
        return 0;
    }

    return --m_dwRefs;
}

void TPdfFile::SetOwner(TPdfDatabase * pPdfDb)
{
    assert(m_pPdfDb == NULL);
    pPdfDb->AddRef();
    m_pPdfDb = pPdfDb;
}

LPCSTR TPdfFile::GetStreamFilter(LPCSTR szPtr, size_t & RefLength)
{
    size_t nLength = 0;

    // Skip spaces until we find the "/" character 
    while(szPtr[0] != 0 && szPtr[0] != '/')
        szPtr++;

    // Did we find a filter name?
    if(szPtr[0] == '/')
    {
        // Skip the opening slash
        szPtr++;

        // Copy the filter name
        while('0' <= szPtr[nLength] && szPtr[nLength] <= 'z')
            nLength++;

        // Give the length of the filter name
        RefLength = nLength;
        return szPtr;
    }
    return NULL;
}

DWORD TPdfFile::GetStreamFilters(LPCSTR szObjParams, PDFFL * Filters, DWORD & RefFilterCount)
{
    DWORD dwFilters = 0;
    char szFilters[64];

    // Parse filters
    if(GetObjectVariableString(szObjParams, "/Filter", szFilters, _countof(szFilters)))
    {
        LPCSTR szPtr = szFilters;
        size_t nLength = 0;

        while((szPtr = GetStreamFilter(szPtr, nLength)) != NULL)
        {
            //DWORD dwSaveFilters = dwFilters;

            // Perform filter checking
            switch(nLength)
            {
                case sizeof("Fl") - 1:
                {
                    if(!memcmp(szPtr, "Fl", nLength))
                        Filters[dwFilters++] = PDFF_Flate;
                    if(!memcmp(szPtr, "RL", nLength))
                        Filters[dwFilters++] = PDFF_RunLength;
                    break;
                }
                case sizeof("A85") - 1:
                {
                    if(!memcmp(szPtr, "A85", nLength))
                        Filters[dwFilters++] = PDFF_Ascii85;
                    if(!memcmp(szPtr, "AHx", nLength))
                        Filters[dwFilters++] = PDFF_AsciiHex;
                    if(!memcmp(szPtr, "CCF", nLength))
                        Filters[dwFilters++] = PDFF_CCITTFaxDecode;
                    if(!memcmp(szPtr, "DCT", nLength))
                        Filters[dwFilters++] = PDFF_DCT;
                    break;
                }
                case sizeof("DCTDecode") - 1:
                {
                    if(!memcmp(szPtr, "DCTDecode", nLength))
                        Filters[dwFilters++] = PDFF_DCT;
                    if(!memcmp(szPtr, "LZWDecode", nLength))
                        Filters[dwFilters++] = PDFF_LZW;
                    break;
                }
                case sizeof("FlateDecode") - 1:
                {
                    if(!memcmp(szPtr, "FlateDecode", nLength))
                        Filters[dwFilters++] = PDFF_Flate;
                    break;
                }
                case sizeof("ASCII85Decode") - 1:
                {
                    if(!memcmp(szPtr, "ASCII85Decode", nLength))
                        Filters[dwFilters++] = PDFF_Ascii85;
                    break;
                }
                case sizeof("ASCIIHexDecode") - 1:
                {
                    if(!memcmp(szPtr, "ASCIIHexDecode", nLength))
                        Filters[dwFilters++] = PDFF_AsciiHex;
                    if(!memcmp(szPtr, "CCITTFaxDecode", nLength))
                        Filters[dwFilters++] = PDFF_CCITTFaxDecode;
                    break;
                }
                case sizeof("RunLengthDecode") - 1:
                {
                    if(!memcmp(szPtr, "RunLengthDecode", nLength))
                        Filters[dwFilters++] = PDFF_RunLength;
                    break;
                }
            }

            // An unknown filter
            //if(dwFilters == dwSaveFilters)
            //{
            //    assert(false);
            //    return ERROR_BAD_FORMAT;
            //}

            // Move past the filter
            szPtr += nLength;
        }
    }

    if(GetObjectVariableString(szObjParams, "/Subtype", szFilters, _countof(szFilters)))
    {
        if(strstr(szFilters, "/XML"))
        {
            Filters[0] = PDFF_PlainXml;
            dwFilters = 1;
        }
    }

    // Give the number of filters
    RefFilterCount = dwFilters;
    return ERROR_SUCCESS;
}

LPCTSTR TPdfFile::FileExtension()
{
    // If the subtype is XML, then its a XML :-)
    if(m_Filters[0] == PDFF_PlainXml)
        return _T(".xml");

    if(CheckData("\x49\x49\x2a\x00", 0x04))
        return _T(".tif");

    if(CheckData("P4\n", 0x03))
        return _T(".pbm");

    if(CheckData("\xFF\xD8\xFF", 0x03))
        return _T(".jpg");

    if(CheckData("%PDF-1.", 0x07))
        return _T(".pdf");

    return _T(".dat");
}

void TPdfFile::GetName(LPTSTR szBuffer, size_t cchBuffer)
{
    StringCchPrintf(szBuffer, cchBuffer, _T("object-%s-%08u%s"), m_szFileType, m_dwObjectId, m_szExtension);
}

DWORD TPdfFile::Load(LPCSTR szObjParams)
{
    DWORD dwErrCode = ERROR_SUCCESS;

    // Load the filters
    GetStreamFilters(szObjParams, m_Filters, m_dwFilters);

    // Apply the filters
    for(DWORD i = 0; i < m_dwFilters && dwErrCode == ERROR_SUCCESS; i++)
    {
        TPdfBlob TempBlob;

        // Move the content from data to the temporary blob
        TempBlob.MoveFrom(*this);

        // Apply the decoding
        switch(m_Filters[i])
        {
            case PDFF_Plain:
            case PDFF_PlainXml:
            case PDFF_DCT:
                MoveFrom(TempBlob);
                break;

            case PDFF_Ascii85:
                dwErrCode = ascii85_decode(*this, TempBlob);
                break;

            case PDFF_AsciiHex:
                dwErrCode = DecodeObject_AsciiHex(TempBlob);
                break;

            case PDFF_Flate:
                dwErrCode = DecodeObject_Flate(TempBlob);
                break;

            case PDFF_LZW:
                dwErrCode = DecodeObject_LZW(TempBlob, szObjParams);
                break;

            case PDFF_CCITTFaxDecode:
                dwErrCode = DecodeObject_CCITT(TempBlob, szObjParams);
                goto __finalize;

            case PDFF_RunLength:
                dwErrCode = DecodeObject_RunLength(TempBlob);
                break;
        }

        // Reset the position to the begin of the stream
        ResetPosition();
    }

__finalize:

    // Reset the position to the begin of the stream
    ResetPosition();

    // On success, generate the file name
    if(dwErrCode == ERROR_SUCCESS)
    {
        // Supply the default extension
        m_szExtension = FileExtension();
        m_szFileType = _T("stream");
    }
    return dwErrCode;
}

DWORD TPdfFile::DecodeObject_AsciiHex(TPdfBlob & Input)
{
    BYTE HexValue;

    // Make sure that there is enough size in the output blob
    if(Resize(Input.Size() / 2) != ERROR_SUCCESS)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Sanity checks
    assert(Input.pbPtr == Input.pbData);
    assert(pbPtr == pbData);

    // Decode the data
    while(Input.GetHexValue(HexValue) == ERROR_SUCCESS)
        *pbPtr++ = HexValue;
    return ERROR_SUCCESS;
}

DWORD TPdfFile::DecodeObject_Flate(TPdfBlob & Input)
{
    z_stream z = {NULL};
    size_t cbSize = Input.Size() * 2;
    DWORD dwErrCode = ERROR_VERSION_PARSE_ERROR;
    bool bDecompressionComplete = false;

    // Sanity checks
    assert(Input.pbPtr == Input.pbData);
    assert(pbPtr == pbData);

    // Fill the stream structure for zlib
    z.next_in   = (Bytef *)Input.pbData;
    z.avail_in =  (uInt)Input.Size();
    z.total_in  = (uLong)Input.Size();
    z.next_out  = (Bytef *)pbData;
    z.avail_out = (uInt)   Size();
    z.total_out = 0;

    // Perform the decompression
    // Initialize the decompression
    if(inflateInit2(&z, MAX_WBITS) == Z_OK)
    {
        while(bDecompressionComplete == false && (dwErrCode = Resize(cbSize)) == ERROR_SUCCESS)
        {
            // Setup the output buffer
            z.next_out  = (Bytef *)(pbData + z.total_out);
            z.avail_out = (uInt)(Size() - z.total_out);
            bDecompressionComplete = true;

            // Call zlib to decompress the data
            switch(inflate(&z, Z_NO_FLUSH))
            {
                case Z_OK:
                    if(z.avail_in != 0)
                    {
                        bDecompressionComplete = false;
                        cbSize = cbSize * 2;
                    }
                    break;

                case Z_STREAM_END:
                    break;

                default:
                    dwErrCode = ERROR_FILE_CORRUPT;
                    break;
            }
        }

        // If everything went OK, we update the file size
        if(dwErrCode == ERROR_SUCCESS)
            pbEnd = pbData + z.total_out;
        inflateEnd(&z);
    }
    else
    {
        dwErrCode = ERROR_VERSION_PARSE_ERROR;
    }
    return dwErrCode;
}

DWORD TPdfFile::DecodeObject_LZW(TPdfBlob & Input, LPCSTR szObjParams)
{
    int nEarlyChange = 1;

    // Sanity checks
    assert(Input.pbPtr == Input.pbData);
    assert(pbPtr == pbData);

    // Retrieve the early change
    GetObjectVariableInt(szObjParams, "/EarlyChange", nEarlyChange, 1);

    // Perform the LZW Decode
    return LZWDecode(*this, Input, nEarlyChange);
}

DWORD TPdfFile::DecodeObject_CCITT(TPdfBlob & Input, LPCSTR szObjParams)
{
    int nK = 0;
    int bEndOfLine = 0;
    int bEncodedByteAlign = 0;
    int bEndOfBlock = 1;
    int bBlackIs1 = 0;
    int bImageMask = 0;
    int nColumns = 1728;
    int nRows = 0;

    // Retrieve the encoding parameters
    GetObjectVariableInt(szObjParams, "/K", nK, 0);
    GetObjectVariableInt(szObjParams, "/EndOfLine", bEndOfLine, 0);
    GetObjectVariableInt(szObjParams, "/Columns", nColumns, 1728);
    GetObjectVariableInt(szObjParams, "/Rows", nRows, 0);
    GetObjectVariableInt(szObjParams, "/EncodedByteAlign", bEncodedByteAlign, 0, true);
    GetObjectVariableInt(szObjParams, "/EndOfBlock", bEndOfBlock, 1);
    GetObjectVariableInt(szObjParams, "/BlackIs1", bBlackIs1, 0, true);
    GetObjectVariableInt(szObjParams, "/ImageMask", bImageMask, 0, true);

    // Decode the plain image
    return CCITT_Decode(*this, Input, nK, bEndOfLine, bEncodedByteAlign, bEndOfBlock, bBlackIs1, bImageMask, nColumns, nRows);
}

DWORD TPdfFile::DecodeObject_RunLength(TPdfBlob & Input)
{
    DWORD dwErrCode;

    // Sanity checks
    assert(Input.pbPtr == Input.pbData);
    assert(pbPtr == pbData);

    // Let the output data have at least twice the size of the input data
    if((dwErrCode = Resize(Input.Size() * 2)) == ERROR_SUCCESS)
    {
        if((dwErrCode = runlength_decode(*this, Input)) == ERROR_SUCCESS)
        {
            pbEnd = pbPtr;
        }
    }
    return dwErrCode;
}
