/*****************************************************************************/
/* decode_lzw.h                           Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for run-length encoding                                */
/*                                                                           */
/* LZW code sourced from pdfminer                                            */
/* Copyright (c) 2004-2009 Yusuke Shinyama <yusuke at cs dot nyu dot edu>    */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 03.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __DECODE_LZW_H__
#define __DECODE_LZW_H__

DWORD LZWDecode(TPdfBlob & Output, TPdfBlob & Input, int dwEarlyChange);

#endif // __DECODE_LZW_H__
