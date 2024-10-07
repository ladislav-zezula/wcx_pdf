/*****************************************************************************/
/* TPdf.h                                 Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Common header file for PDF classes                                        */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 27.07.23  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __TPDF_H__
#define __TPDF_H__

//-----------------------------------------------------------------------------
// Defines

#define PDF_MAGIC_SIGNATURE     0x434947414D464450 // "PDFMAGIC"
#define PDF_PVOID_TRUE          ((TPdfDatabase *)(INT_PTR)(1))
#define PDF_MAX_FILTERS         8

//-----------------------------------------------------------------------------
// Enums

// PDF filters
typedef enum _PDFFL
{
    PDFF_Plain = 0,
    PDFF_PlainXml,                          // Also plain, but used for XML
    PDFF_Ascii85,
    PDFF_AsciiHex,
    PDFF_Flate,
    PDFF_LZW,
    PDFF_RunLength,
    PDFF_DCT,
    PDFF_CCITTFaxDecode,
} PDFFL, *PPDFFL;

struct DOS_FTIME
{
    unsigned ft_tsec : 5;                   // Two second interval
    unsigned ft_min : 6;                    // Minutes
    unsigned ft_hour : 5;                   // Hours
    unsigned ft_day : 5;                    // Days
    unsigned ft_month : 4;                  // Months
    unsigned ft_year : 7;                   // Year
};

//-----------------------------------------------------------------------------
// PDF objects

struct TPdfBlob
{
    TPdfBlob(LPBYTE pb0, LPBYTE pb1, bool bAllocate = false);
    TPdfBlob(LPBYTE pb, size_t cb, bool bAllocate = false);
    TPdfBlob(const TPdfBlob & Source);
    TPdfBlob();
    ~TPdfBlob();

    DWORD SetData(LPCTSTR szFileName);
    DWORD SetData(LPBYTE pb0, LPBYTE pb1, bool bAllocate);
    DWORD SetData(const TPdfBlob & Source);
    void MoveFrom(TPdfBlob & Source);
    bool SetPosition(LPBYTE pb);
    void ResetPosition();
    void FreeData();

    DWORD AppendBytes(LPCVOID pvData, size_t cbData);

    DWORD ReadByte(BYTE & RefValue);
    DWORD GetHexValue(BYTE & RefValue);

    LPBYTE LoadOneLine(LPSTR szBuffer, size_t ccBuffer);
    LPBYTE FindEndOfPdf();
    LPBYTE FindEndOfLine();
    LPBYTE SkipEndOfLine();
    LPBYTE SkipPdfSpaces();

    LPBYTE ReallocateBuffer(LPBYTE pbPtr, SIZE_T cbNewSize);
    DWORD  Resize(size_t cbNewSize);
    size_t Size() const { return (pbEnd - pbData); }
    bool CheckData(LPCVOID pv, size_t cb);
    bool GetOneByte(BYTE & RefValue);
    bool IsEof();

    LPBYTE pbData;
    LPBYTE pbPtr;
    LPBYTE pbEnd;
    bool bAllocated;
};

struct TPdfFile : public TPdfBlob
{
    TPdfFile(LPBYTE pbData, LPBYTE pbEnd, DWORD dwObjectId);
    ~TPdfFile();

    DWORD AddRef();
    DWORD Release();

    void SetOwner(struct TPdfDatabase * pPdfDb);

    DWORD Load(LPCSTR szObjParams);
    DWORD DecodeObject_AsciiHex(TPdfBlob & Source);
    DWORD DecodeObject_Flate(TPdfBlob & Source);
    DWORD DecodeObject_LZW(TPdfBlob & Source, LPCSTR szObjParams);
    DWORD DecodeObject_CCITT(TPdfBlob & Source, LPCSTR szObjParams);
    DWORD DecodeObject_RunLength(TPdfBlob & Source);

    const TPdfBlob & GetData()  { return *this; };
    DWORD PackSize()            { return m_dwRawSize; }
    DWORD FileSize()            { return (DWORD)Size(); }

    LPCSTR GetStreamFilter(LPCSTR szPtr, size_t & RefLength);
    DWORD  GetStreamFilters(LPCSTR szObjParams, PDFFL * Filters, DWORD & RefFilterCount);

    LPCTSTR FileExtension();
    void GetName(LPTSTR szBuffer, size_t cchBuffer);

    struct TPdfDatabase * m_pPdfDb;         // Mother PDF database
    LIST_ENTRY m_Entry;                     // Link to other files
    LPCTSTR m_szExtension;
    LPCTSTR m_szFileType;
    DWORD m_dwObjectId;                     // Object ID
    PDFFL m_Filters[PDF_MAX_FILTERS];       // Array of filters
    DWORD m_dwFilters;
    DWORD m_dwRawSize;
    DWORD m_dwRefs;
};

// Our structure describing open archive
struct TPdfDatabase : public TPdfBlob
{
    static TPdfDatabase * Open(LPCWSTR szFileName, bool bFastCheck);
    static TPdfDatabase * Load(LPBYTE pbFileData, size_t cbFileData, FILETIME & ft, bool bFastCheck);
    static TPdfDatabase * FromHandle(HANDLE hHandle);

    DWORD AddRef();
    DWORD Release();
    
    void  InsertFile(TPdfFile * pPdfFile);
    void  RemoveAllFiles();

    TPdfFile * OpenNextFile();
    TPdfFile * ReferenceFile(LPCTSTR szPlainName);
    void       UnlockAndRelease();

    const DOS_FTIME & FileTime()         { return m_FileTime; }

    protected:

    TPdfDatabase(LPBYTE pbPdfBegin, LPBYTE pbPdfEnd, FILETIME & ft);
    ~TPdfDatabase();

    TPdfFile * OpenNextFile_SEQ();
    TPdfFile * LoadPdfObject();
    LPBYTE CheckBeginOfObject();
    LPBYTE ParseBeginOfObject(int & nObjectId);
    LPSTR  LoadObjectParameters();
    LPBYTE IsBeginOfStream();
    LPBYTE FindEndOfStream(LPCSTR szObjParams);
    LPBYTE SkipEndOfStream();
    LPBYTE SkipEndOfObject();

    CRITICAL_SECTION m_Lock;
    LIST_ENTRY m_Files;                     // List of files
    ULONGLONG m_MagicSignature;             // PDF_MAGIC_SIGNATURE
    DOS_FTIME m_FileTime;                   // File time of the PDF file
    DWORD m_dwFiles;                        // Number of files
    DWORD m_dwRefs;
};

// Non-class functions
bool GetObjectVariableInt(LPCSTR szObjParams, LPCSTR szVariableName, int & RefValue, int nDefaultValue = 0, bool bBoolAllowed = false);
bool GetObjectVariableString(LPCSTR szObjParams, LPCSTR szVariableName, LPSTR szBuffer, size_t ccBuffer, LPCSTR szDefaultValue = "");


#endif // __TPDF_H__
