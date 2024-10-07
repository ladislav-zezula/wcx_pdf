/*****************************************************************************/
/* DllMain.cpp                            Copyright (c) Ladislav Zezula 2023 */
/*---------------------------------------------------------------------------*/
/* Implementation of DLL Entry point for the Total Commander plugin.         */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 22.07.23  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"
#include "resource.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID /* lpvReserved */)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            InitInstance(hInstDll);
            break;

        case DLL_PROCESS_DETACH:
#ifdef _DEBUG
            _CrtDumpMemoryLeaks();
#endif  // _MSC_VER
            g_hInst = NULL;
            break;
    }
    return TRUE;
}