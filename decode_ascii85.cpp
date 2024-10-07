/*****************************************************************************/
/* decode_ascii85.cpp                     Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for ASCII85 encoding                                   */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 01.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"
#include "decode_ascii85.h"

#define ASCII85_CHUNK_SIZE 5
#define BINARY_CHUNK_SIZE  4
#define DECODED_BLOCK_SIZE 4

FORCEINLINE DWORD BSWAP32(DWORD x)
{
    return(((x >> 24) & 0x000000FF) | ((x << 24) & 0xFF000000) |
           ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000));
}

DWORD ascii85_decode(TPdfBlob & Output, TPdfBlob & Input)
{
    size_t tuple_length = 0;
    DWORD tuple_value = 0;
    DWORD dwErrCode = ERROR_SUCCESS;
    BYTE ascii85_char = 0;

    // Reset the pointer to the begin of the data
    if((dwErrCode = Output.Resize(Input.Size())) != ERROR_SUCCESS)
        return dwErrCode;
    assert(Input.pbPtr == Input.pbData);

    // Perform the decoding loop
    while(Input.GetOneByte(ascii85_char))
    {
        // Check for valid Ascii85 char
        if('!' <= ascii85_char && ascii85_char <= 'u')
        {
            // Keep loading characters up to ASCII85_CHUNK_SIZE
            tuple_value = tuple_value * 85 + (ascii85_char - ASCII85_OFFSET);
            tuple_length++;

            // Flush if needed
            if(tuple_length == ASCII85_CHUNK_SIZE)
            {
                tuple_value = BSWAP32(tuple_value);
                if((dwErrCode = Output.AppendBytes(&tuple_value, sizeof(tuple_value))) != ERROR_SUCCESS)
                    return dwErrCode;

                // Reset the tuple size
                tuple_length = 0;
                tuple_value = 0;
            }
        }

        else if(ascii85_char == 'z')
        {
            // Reset the tuple
            tuple_length = 0;
            tuple_value = 0;

            // Append four zeros to the output buffer
            if((dwErrCode = Output.AppendBytes(&tuple_value, sizeof(tuple_value))) != ERROR_SUCCESS)
                return dwErrCode;
        }

        // Termination char: Flush the remaining bytes
        // However, we also accept if there is no termination char
        else if(ascii85_char == '~')
        {
            break;
        }

        //
        // All other characters are ignored
        //
    }

    // If there is something left in the input buffer, flush it
    if(tuple_length != 0)
    {
        // Calculate the tuple value
        for(size_t i = tuple_length; i < ASCII85_CHUNK_SIZE; i++)
            tuple_value = tuple_value * 85 + 84;
        tuple_value = BSWAP32(tuple_value);

        // Append the tuple to the output
        dwErrCode = Output.AppendBytes(&tuple_value, tuple_length - 1);
    }

    // Resize the output buffer to the actual size
    if(dwErrCode == ERROR_SUCCESS)
    {
        assert((size_t)(Output.pbPtr - Output.pbData) <= Output.Size());
        Output.pbEnd = Output.pbPtr;
    }

    return dwErrCode;
}
