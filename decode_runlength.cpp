/*****************************************************************************/
/* decode_runlength.cpp                   Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for run-length encoding                                */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 02.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"
#include "decode_runlength.h"

DWORD runlength_decode(TPdfBlob & Output, TPdfBlob & Input)
{
    DWORD dwErrCode = ERROR_SUCCESS;

    while(Input.pbPtr < Input.pbEnd)
    {
        unsigned char one_byte = *Input.pbPtr++;

        // Ending char
        if(one_byte == 0x80)
            break;

        // Sequence copied
        if(one_byte < 0x80)
        {
            size_t chunk_length = one_byte + 1;

            // Do we have enough bytes in the input buffer?
            if((Input.pbPtr + chunk_length) > Input.pbEnd)
                chunk_length = (Input.pbEnd - Input.pbPtr);
            
            // Copy the data directly
            if((dwErrCode = Output.AppendBytes(Input.pbPtr, chunk_length)) != ERROR_SUCCESS)
                return dwErrCode;
            Input.pbPtr += chunk_length;
            continue;
        }

        // Character multiplied
        if(one_byte > 0x80)
        {
            size_t chunk_length = 0x101 - one_byte;

            // Do we have at least 1 more byte there?
            if(Input.pbPtr >= Input.pbEnd)
                return ERROR_INVALID_DATA;
            one_byte = *Input.pbPtr++;

            // Check how many bytes do we have there
            if((Output.pbPtr + chunk_length) > Output.pbEnd)
            {
                // Resize the output buffer
                if((dwErrCode = Output.Resize((Output.pbPtr - Output.pbData) + chunk_length)) != ERROR_SUCCESS)
                    return dwErrCode;
            }

            // Fill the buffer
            memset(Output.pbPtr, one_byte, chunk_length);
            Output.pbPtr += chunk_length;
            continue;
        }

        break;
    }

    // Reset the length
    Output.pbEnd = Output.pbPtr;
    return dwErrCode;
}


