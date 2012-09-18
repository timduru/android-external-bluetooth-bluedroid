/************************************************************************************
 *
 *  Copyright (C) 2009-2011 Broadcom Corporation
 *
 *  This program is the proprietary software of Broadcom Corporation and/or its
 *  licensors, and may only be used, duplicated, modified or distributed
 *  pursuant to the terms and conditions of a separate, written license
 *  agreement executed between you and Broadcom (an "Authorized License").
 *  Except as set forth in an Authorized License, Broadcom grants no license
 *  (express or implied), right to use, or waiver of any kind with respect to
 *  the Software, and Broadcom expressly reserves all rights in and to the
 *  Software and all intellectual property rights therein.
 *  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS
 *  SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE
 *  ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization,
 *         constitutes the valuable trade secrets of Broadcom, and you shall
 *         use all reasonable efforts to protect the confidentiality thereof,
 *         and to use this information only in connection with your use of
 *         Broadcom integrated circuit products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *         "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *         REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY,
 *         OR OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *         DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *         NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *         ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *         CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT
 *         OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
 *         ITS LICENSORS BE LIABLE FOR
 *         (i)   CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR EXEMPLARY
 *               DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
 *               YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
 *               HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR
 *         (ii)  ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
 *               SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 *               LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
 *               ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 ************************************************************************************/

/*******************************************************************************
 *
 *  Filename:      btif_config.h
 *
 *  Description:   Bluetooth configuration Interface
 *
 *******************************************************************************/

#ifndef BTIF_CONFIG_H
#define BTIF_CONFIG_H

#ifdef __cplusplus
#include <stdint.h>
extern "C" {
#endif

/*******************************************************************************
** Constants & Macros
********************************************************************************/

#define BTIF_CFG_TYPE_INVALID   0
#define BTIF_CFG_TYPE_STR       1
#define BTIF_CFG_TYPE_INT      (1 << 1)
#define BTIF_CFG_TYPE_BIN      (1 << 2)
#define BTIF_CFG_TYPE_VOLATILE (1 << 15)


/*******************************************************************************
**  Functions
********************************************************************************/

int btif_config_init();

int btif_config_exist(const char* section, const char* key, const char* name);
int btif_config_get_int(const char* section, const char* key, const char* name, int* value);
int btif_config_set_int(const char* section, const char* key, const char* name, int value);
int btif_config_get_str(const char* section, const char* key, const char* name, char* value, int* bytes);
int btif_config_set_str(const char* section, const char* key, const char* name, const char* value);

int btif_config_get(const char* section, const char* key, const char* name, char* value, int* bytes, int* type);
int btif_config_set(const char* section, const char* key, const char* name, const char*  value, int bytes, int type);

int btif_config_remove(const char* section, const char* key, const char* name);

short btif_config_next_key(short current_key_pos, const char* section, char * key_name, int* key_name_bytes);
short btif_config_next_value(short pos, const char* section, const char* key, char* value_name, int* value_name_bytes);

typedef void (*btif_config_enum_callback)(void* user_data, const char* section, const char* key, const char* name,
                                          const char*  value, int bytes, int type);
int btif_config_enum(btif_config_enum_callback cb, void* user_data);

int btif_config_save();
void btif_config_flush();

#ifdef __cplusplus
}
#endif

#endif
