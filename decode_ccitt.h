/*****************************************************************************/
/* decode_ccit.h                          Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for ccitt encoding                                     */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 16.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __DECODE_CCITT_H__
#define __DECODE_CCITT_H__

DWORD CCITT_Decode(
    TPdfBlob & Output,
    TPdfBlob & Input,
    int nK,
    int bEndOfLine,
    int bEncodedByteAlign,
    int bEndOfBlock,
    int bBlackIs1,
    int bImageMask,
    int nColumns,
    int nRows
    );

#endif // __DECODE_CCITT_H__
