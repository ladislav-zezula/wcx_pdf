/*****************************************************************************/
/* decode_ascii85.h                       Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for ASCII85 encoding                                   */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 01.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __DECODE_ASCII85_H__
#define __DECODE_ASCII85_H__

#define ASCII85_OFFSET 33

// Maximum length of the decoded data. Not stripping <~ and ~>
#define ASCII85_DECODED_LENGTH(encoded_length)  (encoded_length * 4 / 5)

DWORD ascii85_decode(TPdfBlob & Output, TPdfBlob & Input);

#endif // __DECODE_ASCII85_H__