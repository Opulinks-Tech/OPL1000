/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef __AT_CMD_DATA_PROCESS_PATCH_H__
#define __AT_CMD_DATA_PROCESS_PATCH_H__

#include "at_cmd_data_process.h"

#define AT_TCP_TRANS_LOCK_ID 0xFFFFFFFF

int data_process_handler_impl(char *pbuf, int len);
void data_process_lock_patch(int module, int data_len);

#endif //__AT_CMD_DATA_PROCESS_PATCH_H__

