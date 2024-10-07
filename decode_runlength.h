/*****************************************************************************/
/* decode_runlength.h                     Copyright (c) Ladislav Zezula 2024 */
/*---------------------------------------------------------------------------*/
/* Decoding functions for run-length encoding                                */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 02.07.24  1.00  Lad  Created                                              */
/*****************************************************************************/

#ifndef __DECODE_RUNLENGTH_H__
#define __DECODE_RUNLENGTH_H__

DWORD runlength_decode(TPdfBlob & Output, TPdfBlob & Input);

#endif // __DECODE_RUNLENGTH_H__
