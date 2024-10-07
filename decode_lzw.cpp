/*****************************************************************************/
/* decode_lzw.cpp                         Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for run-length encoding                                */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 03.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"
#include "decode_lzw.h"

// Decoding table
typedef struct
{
    unsigned int length;
    unsigned int head;
    unsigned char tail;
} LZWTable;

DWORD LZWDecode(TPdfBlob & Output, TPdfBlob & Input, int nEarlyChange)
{
    LZWTable * pTable;
    DWORD dwErrCode = ERROR_SUCCESS;
    DWORD dwInBuffer = 0;                           // Input bit buffer
    DWORD dwNextCode = 258;                         // Code for next dictionary entry
    DWORD dwBitCount = 9;                           // Number of bits in a code
    DWORD dwInpBits = 0;                            // Number of bits in input buffer
    DWORD dwPrevCode = 0;                           // Previous code used in stream
    DWORD dwCode;
    DWORD dwSeqLength = 0;                          // Length of current sequence
    unsigned char SequenceBuffer[4097];             // Buffer for current sequence
    unsigned char uchNewChar = 0;                   // Next char to be added to table
    bool bFirst = true;                             // First code after a table clear
    int i, j;

    // Pre-allocate the output buffer to be of the same size like the input data
    // This prevents the need to do many reallocations during decompression
    Output.Resize(Input.Size());

    // Create the decoding table
    if((pTable = new LZWTable[4097]) != NULL)
    {
        // Decompressing cycle
        while(Input.pbPtr < Input.pbEnd)
        {
            // Read bytes from compressed data
            while(dwInpBits < dwBitCount)
            {
                // Make sure that there is enough bytes in the input buffer
                if(Input.pbPtr >= Input.pbEnd)
                {
                    dwErrCode = ERROR_INVALID_DATA;
                    goto __LZW_Error;
                }

                // Add those 8 bits to the accumulator
                dwInBuffer = (dwInBuffer << 8) | *Input.pbPtr++;
                dwInpBits += 8;
            }

            // Get the decompression code
            dwCode = (dwInBuffer >> (dwInpBits - dwBitCount)) & ((1 << dwBitCount) - 1);
            //Dbg(_T("Code: %u\n"), dwCode);
            dwInpBits -= dwBitCount;

            // Check for the end of decompression
            if(dwCode == 257)
                break;

            // Check for the end of block
            if(dwCode == 256)
            {
                // Reset the decompression table
                dwNextCode = 258;
                dwBitCount = 9;
                dwSeqLength = 0;
                bFirst = true;
                continue;
            }

            // Check for too big table
            if(dwNextCode >= 4097)
            {
                dwErrCode = ERROR_INVALID_DATA;
                goto __LZW_Error;
            }

            DWORD dwNextLength = dwSeqLength + 1;

            if(dwCode < 256)
            {
                SequenceBuffer[0] = (uint8_t)dwCode;
                dwSeqLength = 1;
            }
            else if(dwCode < dwNextCode)
            {
                dwSeqLength = pTable[dwCode].length;

                for(i = dwSeqLength - 1, j = dwCode; i > 0; --i)
                {
                    SequenceBuffer[i] = pTable[j].tail;
                    j = pTable[j].head;
                }
                SequenceBuffer[0] = (uint8_t)j;
            }
            else if(dwCode == dwNextCode)
            {
                SequenceBuffer[dwSeqLength] = uchNewChar;
                ++dwSeqLength;
            }
            else
            {
                dwErrCode = ERROR_INVALID_DATA;
                goto __LZW_Error;
            }

            uchNewChar = SequenceBuffer[0];

            if(bFirst)
                bFirst = false;
            else
            {
                pTable[dwNextCode].length = dwNextLength;
                pTable[dwNextCode].head = dwPrevCode;
                pTable[dwNextCode++].tail = uchNewChar;

                switch(dwNextCode + nEarlyChange)
                {
                    case 512:
                        dwBitCount = 10;
                        break;
                    case 1024:
                        dwBitCount = 11;
                        break;
                    case 2048:
                        dwBitCount = 12;
                        break;
                }
            }
            dwPrevCode = dwCode;

            // Append the decompressed sequence to the output buffer
            if((dwErrCode = Output.AppendBytes(SequenceBuffer, dwSeqLength)) != ERROR_SUCCESS)
            {
                break;
            }
        }

        __LZW_Error:
        delete [] pTable;
    }
    else
    {
        dwErrCode = ERROR_NOT_ENOUGH_MEMORY;
    }
    
    // Fix the length of the decoded data
    if(dwErrCode == ERROR_SUCCESS)
        Output.pbEnd = Output.pbPtr;
    return dwErrCode;
}
