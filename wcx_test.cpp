/*****************************************************************************/
/* wcx_test.cpp                           Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Testing suite for Total Commander plugins. Windows platform only.         */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 27.06.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"                    // Our functions
#include "./libs/sha256.h"

//-----------------------------------------------------------------------------
// Local structures

struct THeaderData_Linked : public THeaderDataExW
{
    LIST_ENTRY Entry;
};

//-----------------------------------------------------------------------------
// Local variables

static CANYOUHANDLETHISFILEW PfnCanYouHandleThisFileW = NULL;
static OPENARCHIVEW          PfnOpenArchiveW = NULL;
static READHEADEREXW         PfnReadHeaderExW = NULL;
static PROCESSFILEW          PfnProcessFileW = NULL;
static CLOSEARCHIVE          PfnCloseArchive = NULL;

#ifdef _DEBUG
LPCTSTR szDebugOrRelease = _T("Debug");
#else
LPCTSTR szDebugOrRelease = _T("Release");
#endif

//-----------------------------------------------------------------------------
// Local functions

static DWORD GetPluginFunctions(HMODULE hPlugin)
{
    PfnCanYouHandleThisFileW = (CANYOUHANDLETHISFILEW)GetProcAddress(hPlugin, "CanYouHandleThisFileW");
    if(PfnCanYouHandleThisFileW == NULL)
    {
        _tprintf(_T("[x] Missing CanYouHandleThisFileW function\n"));
        return ERROR_PROC_NOT_FOUND;
    }

    PfnOpenArchiveW = (OPENARCHIVEW)GetProcAddress(hPlugin, "OpenArchiveW");
    if(PfnOpenArchiveW == NULL)
    {
        _tprintf(_T("[x] Missing OpenArchiveW function\n"));
        return ERROR_PROC_NOT_FOUND;
    }

    PfnReadHeaderExW = (READHEADEREXW)GetProcAddress(hPlugin, "ReadHeaderExW");
    if(PfnReadHeaderExW == NULL)
    {
        _tprintf(_T("[x] Missing ReadHeaderExW function\n"));
        return ERROR_PROC_NOT_FOUND;
    }

    PfnProcessFileW = (PROCESSFILEW)GetProcAddress(hPlugin, "ProcessFileW");
    if(PfnProcessFileW == NULL)
    {
        _tprintf(_T("[x] Missing ProcessFileW function\n"));
        return ERROR_PROC_NOT_FOUND;
    }

    PfnCloseArchive = (CLOSEARCHIVE)GetProcAddress(hPlugin, "CloseArchive");
    if(PfnCloseArchive == NULL)
    {
        _tprintf(_T("[x] Missing CloseArchive function\n"));
        return ERROR_PROC_NOT_FOUND;
    }
    return ERROR_SUCCESS;
}

static void CalculateSha256(LPSTR szSha256, size_t ccSha256, LPBYTE pbData, size_t cbData)
{
    SHA256_CTX sha_ctx;
    BYTE sha256_digest[0x20];

    // Calculate SHA256 of the whole file
    sha256_init(&sha_ctx);
    sha256_update(&sha_ctx, pbData, cbData);
    sha256_final(&sha_ctx, sha256_digest);
    BinaryToString(szSha256, ccSha256, sha256_digest, sizeof(sha256_digest));

    // Do not use CryptoAPI for calculating SHA256. It's slow like hell.
/*
    HCRYPTPROV hCryptProv = NULL;
    HCRYPTHASH hCryptHash = NULL;
    BYTE sha256_digest[32];

    // Acquire a cryptographic provider context handle
    if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        // Create a hash object
        if(CryptCreateHash(hCryptProv, CALG_SHA_256, 0, 0, &hCryptHash))
        {
            DWORD dwHashLen = sizeof(sha256_digest);

            CryptHashData(hCryptHash, pbData, (DWORD)cbData, 0);
            CryptGetHashParam(hCryptHash, HP_HASHVAL, sha256_digest, &dwHashLen, 0);
            BinaryToString(szSha256, ccSha256, sha256_digest, sizeof(sha256_digest));

            CryptDestroyHash(hCryptHash);
        }
        CryptReleaseContext(hCryptProv, 0);
    }
*/
}

static void ProcessArchiveWithPlugin(LPCWSTR szFullPath, LPCTSTR szPlainName)
{
    THeaderData_Linked * pHdrData;
    HANDLE hArchive;
    int nFilesExtracted = 0;
    int nFilesFound = 0;

    if(PfnCanYouHandleThisFileW(szFullPath))
    {
        TOpenArchiveData ArchData = {NULL};

        ArchData.szArchiveNameW = szFullPath;
        _tprintf(_T("[*] === %s ===============\n"), szPlainName);

        if((hArchive = PfnOpenArchiveW(&ArchData)) != NULL)
        {
            PLIST_ENTRY pHeadEntry;
            PLIST_ENTRY pListEntry;
            LIST_ENTRY FileList;

            // Initialize the file list
            InitializeListHead(&FileList);
            pHeadEntry = &FileList;

            // Load the whole file list to memory
            for(;;)
            {
                // Allocate a new header
                pHdrData = (THeaderData_Linked *)HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY, sizeof(THeaderData_Linked));
                if(pHdrData == NULL)
                {
                    _tprintf(_T("\r[x] Failed to allocate memory for header data\n"));
                    break;
                }

                // Read the header
                if(PfnReadHeaderExW(hArchive, pHdrData) != 0)
                {
                    HeapFree(g_hHeap, 0, pHdrData);
                    break;
                }

                // Insert the file to the list
                InsertTailList(&FileList, &pHdrData->Entry);
                nFilesFound++;
            }

            // Process the whole file list
            for(pListEntry = pHeadEntry->Flink; pListEntry != pHeadEntry; pListEntry = pListEntry->Flink)
            {
                TExtractedData * pExtractedData = NULL;
                int nError;

                pHdrData = CONTAINING_RECORD(pListEntry, THeaderData_Linked, Entry);

                nError = PfnProcessFileW(hArchive, PK_EXTRACT_TO_MEMORY, (LPCWSTR)(&pExtractedData), pHdrData->FileName);
                if(nError == ERROR_SUCCESS)
                {
                    char szSha256[0x80];

                    // Calculate the SHA256 and print it
                    CalculateSha256(szSha256, _countof(szSha256), pExtractedData->Data, (size_t)pExtractedData->Length);
                    _tprintf(_T("    %s = %hs\n"), pHdrData->FileName, szSha256);

                    // Free the allocated buffer
                    LocalFree(pExtractedData);
                    nFilesExtracted++;
                }
                else
                {
                    // Bail out on low memory
                    if(nError == E_NO_MEMORY)
                    {
                        _tprintf(_T("\r[x] Out of memory when extracting file \"%s\"\n"), pHdrData->FileName);
                        break;
                    }

                    // Show the error
                    _tprintf(_T("\r[x] Failed to extract file \"%s\"\n"), pHdrData->FileName);
                }
            }

            // Free the file list
            for(pListEntry = pHeadEntry->Flink; pListEntry != pHeadEntry; )
            {
                // Get the file entry
                pHdrData = CONTAINING_RECORD(pListEntry, THeaderData_Linked, Entry);
                pListEntry = pListEntry->Flink;
                HeapFree(g_hHeap, 0, pHdrData);
            }

            // Close the archive
            _tprintf(_T("    Extracted files: %u of %u\n"), nFilesExtracted, nFilesFound);
            PfnCloseArchive(hArchive);
        }
        else
        {
            _tprintf(_T("[x] Failed to open archive \"%s\" (error code: %u)"), szPlainName, ArchData.OpenResult);
        }
    }
}

//-----------------------------------------------------------------------------
// The 'main' function

int _tmain(int argc, TCHAR * argv[])
{
    WIN32_FIND_DATA wf;
    ULONGLONG dwStartTickCount = GetTickCount64();
    HMODULE hPlugin = NULL;
    HANDLE hFind = NULL;
    LPTSTR szFolderName = NULL;
    LPTSTR szDllName = NULL;
    TCHAR szPathBuffer[MAX_PATH];
    DWORD dwMilliseconds;
    BOOL bFound = TRUE;

    // Initialize the instance
    InitInstance();

    // Retrieve both parameters
    if(argc > 1)
        szDllName = argv[1];
    if(argc > 2)
        szFolderName = argv[2];

    // Measure the time of run
    _tprintf(_T("[*] WCX_TEST v 1.0 (%s version)\n"), szDebugOrRelease);

    // Load the Total Commander plugin
    if(szDllName && szDllName[0])
    {
        _tprintf(_T("[*] Loading plugin \"%s\" ...\n"), szDllName);
        if((hPlugin = LoadLibrary(szDllName)) != NULL)
        {
            _tprintf(_T("[*] Resolving entry points ...\n"));
            if(GetPluginFunctions(hPlugin) == ERROR_SUCCESS)
            {
                if(szFolderName && szFolderName[0])
                {
                    // Folder: Search it. File: Load it.
                    if(GetFileAttributes(szFolderName) & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        // Create the search mask for the folder
                        StringCchCopy(szPathBuffer, _countof(szPathBuffer), szFolderName);
                        AddBackslash(szPathBuffer, _countof(szPathBuffer));
                        StringCchCat(szPathBuffer, _countof(szPathBuffer), _T("*"));

                        // Perform the folder search
                        _tprintf(_T("[*] Searching folder \"%s\" ...\n"), szFolderName);
                        if((hFind = FindFirstFile(szPathBuffer, &wf)) != INVALID_HANDLE_VALUE)
                        {
                            while(bFound)
                            {
                                // Skip the "." and ".." entries
                                if((wf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                                {
                                    // Create the search mask for the folder
                                    StringCchCopy(szPathBuffer, _countof(szPathBuffer), szFolderName);
                                    AddBackslash(szPathBuffer, _countof(szPathBuffer));
                                    StringCchCat(szPathBuffer, _countof(szPathBuffer), wf.cFileName);

                                    // Call the plugin function
                                    ProcessArchiveWithPlugin(szPathBuffer, wf.cFileName);
                                }

                                // Continue search
                                bFound = FindNextFile(hFind, &wf);
                            }
                            FindClose(hFind);
                        }
                        else
                        {
                            _tprintf(_T("[x] Failed to search the folder (error code: %u)\n"), GetLastError());
                        }
                    }
                    else
                    {
                        ProcessArchiveWithPlugin(szFolderName, GetPlainName(szFolderName));
                    }
                }
                else
                {
                    _tprintf(_T("[x] Missing the folder name. Syntax: wcx_test.exe PluginName FolderName\n"));
                }
            }
            FreeLibrary(hPlugin);
        }
        else
        {
            _tprintf(_T("[x] Failed to load the plugin (error code: %u)\n"), GetLastError());
        }
    }
    else
    {
        _tprintf(_T("[x] Missing the plugin name. Syntax: wcx_test.exe PluginName FolderName\n"));
    }


    // Show the time of the execution
    dwMilliseconds = (DWORD)(GetTickCount64() - dwStartTickCount);
    _tprintf(_T("[*] Execution time: %u.%03u seconds.\n"), dwMilliseconds / 1000, dwMilliseconds % 1000);

    // Unload the plugin
    _tprintf(_T("[*] Press any key to exit ...\n"));
}
