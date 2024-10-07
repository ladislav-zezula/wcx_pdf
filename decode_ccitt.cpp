/*****************************************************************************/
/* decode_ccit.cpp                        Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for ccitt encoding                                     */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 16.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#include "wcx_pdf.h"
#include "decode_ccitt.h"

//-----------------------------------------------------------------------------
// Local defines

//#define __USE_CCITT_DECODING

#define PDF_MAX_STREAM_UNP_LEN ( 2048u * 1024u )

enum CODING_MODE
{
    MODE_INVALID,
    MODE_PASS,
    MODE_HORIZONTAL,
    MODE_VERTICAL_0,
    MODE_VERTICAL_R,
    MODE_VERTICAL_L,
    MODE_EXTENSION,
};

struct mode_entry
{
    uint8_t bits : 3;
    CODING_MODE mode : 3;
    uint8_t offset : 2;
};

struct code_entry
{
    unsigned short bits : 4;
    unsigned short run_length : 12;
};

typedef struct _TIFF_TAG
{
    unsigned short Tag;                     // Tag value
    unsigned short Type;                    // Field type
    unsigned int Count;
    unsigned int Value;
} TIFF_TAG;

#pragma pack(push, 2)
typedef struct _TIFF_HEADER
{
    unsigned short ByteOrder;               // 'II' as little endian
    unsigned short Version;                 // Always 42
    unsigned int   FirstIfdOffset;          // Offset to the first Image File Directory
    unsigned short NumberOfTags;

    TIFF_TAG Tags[8];                       // List of TIFF tags

    unsigned short EndOfHeader;             // Ending tag

} TIFF_HEADER, * PTIFF_HEADER;
#pragma pack(pop)

//-----------------------------------------------------------------------------
// Local variables

#ifdef __USE_CCITT_DECODING
// Removed
#endif // __USE_CCITT_DECODING

//-----------------------------------------------------------------------------
// CCITT_Decode - Decoding the CCITT image
//
// bEndOfLine
// 
//      A flag indicating whether end-of-line bit patterns are required to be
//      present in the encoding.The CCITTFaxDecode filter always accepts end-of-line
//      bit patterns, but requires them only if EndOfLine is true.
//      Default value: false
// 
// nEncodedByteAlign
// 
//      A flag indicating whether the filter expects extra 0 bits before each encoded
//      line so that the line begins on a byte boundary. If true, the filter skips over
//      encoded bits to begin decoding each line at a byte boundary. If false, the filter
//      does not expect extra bits in the encoded representation.
//      Default value: false
//
// bEndOfBlock
// 
//      A flag indicating whether the filter expects the encoded data to be terminated by
//      an end-of-block pattern, overriding the Rows parameter. If false, the filter stops
//      when it has decoded the number of lines indicated by Rows or when its data has been
//      exhausted, whichever occurs first. The end-of-block pattern is the CCITT end-of-facsimile
//      block(EOFB) or return-to-control (RTC) appropriate for the K parameter.
//      Default value: true
// 
// bBlackIs1
// 
//      A flag indicating whether 1 bits are to be interpreted as black pixels and 0 bits
//      as white pixels, the reverse of the normal PDF convention for image data.
//      Default value: false
//
// nColumns
//
//      The width of the image in pixels. If the value is not a multiple of 8, the filter adjusts
//      the width of the unencoded image to the next multiple of 8 so that each line starts on
//      a byte boundary.
//      Default value: 1728
//
// nRows
//
//      The height of the image in scan lines. If the value is 0 or absent, the image’s height
//      is not predetermined, and the encoded data must be terminated by an end-of-block
//      bit pattern or by the end of the filter’s data.
//      Default value : 0
//

DWORD CCITT_Decode(
    TPdfBlob & Output,
    TPdfBlob & Input,
    int nK,
    int /* bEndOfLine */,
    int bEncodedByteAlign,
    int /* bEndOfBlock */,
    int bBlackIs1,
    int bImageMask,
    int nColumns,
    int nRows)
{
    DWORD dwErrCode = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(bEncodedByteAlign);
    UNREFERENCED_PARAMETER(bImageMask);

#ifdef USE_CCITT_DECODING
// Removed
#endif  // USE_CCITT_DECODING

    {
        // Don't convert masked images to TIFF - it won't work
        if(bImageMask == FALSE)
        {
            TIFF_HEADER header = {0};

            // Fill the TIFF header
            header.ByteOrder = 'II';
            header.Version = 42;
            header.FirstIfdOffset = 8;
            header.NumberOfTags = 8;

            // Fill the TIFF tags
            header.Tags[0].Tag   = 256;                         // Number of columns
            header.Tags[0].Type  = 4;
            header.Tags[0].Count = 1;
            header.Tags[0].Value = nColumns;

            header.Tags[1].Tag   = 257;                         // Number of rows
            header.Tags[1].Type  = 4;
            header.Tags[1].Count = 1;
            header.Tags[1].Value = nRows;

            header.Tags[2].Tag   = 258;                         // BitsPerSample
            header.Tags[2].Type  = 3;
            header.Tags[2].Count = 1;
            header.Tags[2].Value = 1;

            header.Tags[3].Tag   = 259;                         // Compression
            header.Tags[3].Type  = 3;                           // K < 0 --- Pure two-dimensional encoding (Group 4)
            header.Tags[3].Count = 1;                           // K = 0 --- Pure one-dimensional encoding (Group 3, 1-D)
            header.Tags[3].Value = (nK < 0) ? 4 : 3;            // K > 0 --- Mixed one- and two-dimensional encoding (Group 3, 2-D)

            header.Tags[4].Tag   = 262;                         // WhiteIsZero
            header.Tags[4].Type  = 3;
            header.Tags[4].Count = 1;
            header.Tags[4].Value = bBlackIs1;                   // 0: 0 is imaged as white; 1: 0 is imaged as black

            header.Tags[5].Tag   = 273;                         // LengthOfHeader
            header.Tags[5].Type  = 4;
            header.Tags[5].Count = 1;
            header.Tags[5].Value = sizeof(TIFF_HEADER);

            header.Tags[6].Tag   = 278;                         // RowsPerStrip
            header.Tags[6].Type  = 4;
            header.Tags[6].Count = 1;
            header.Tags[6].Value = nRows;

            header.Tags[7].Tag   = 279;                         // SizeOfImage
            header.Tags[7].Type  = 4;
            header.Tags[7].Count = 1;
            header.Tags[7].Value = (unsigned int)Input.Size();

            // Add the TIFF header
            dwErrCode = Output.AppendBytes(&header, sizeof(TIFF_HEADER));
        }

        // Append the raw image data
        if(dwErrCode == ERROR_SUCCESS)
        {
            if((dwErrCode = Output.AppendBytes(Input.pbData, Input.Size())) == ERROR_SUCCESS)
            {
                Output.pbEnd = Output.pbPtr;
            }
        }
    }

    return dwErrCode;
}
