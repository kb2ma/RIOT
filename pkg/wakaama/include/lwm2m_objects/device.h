/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     lwm2m_objects
 * @defgroup    lwm2m_objects_device Device LWM2M object
 * @brief       Device object implementation for LWM2M client using Wakaama
 * @{
 *
 * @file
 *
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 */

#ifndef LWM2M_OBJECTS_DEVICE_H
#define LWM2M_OBJECTS_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liblwm2m.h"
#include "net/lwm2m.h"

/**
 * @brief Resources of the LWM2M device object instance
 */
enum {
    LWM2M_RES_MANUFACTURER = 0,
    LWM2M_RES_MODEL_NO,
    LWM2M_RES_SERIAL,
    LWM2M_RES_FW_VER,
    LWM2M_RES_REBOOT,           /* exec */
    LWM2M_RES_FRESET,           /* exec */
    LWM2M_RES_POWER_SRC,        /* multiplea */
    LWM2M_RES_POWER_VOL,        /* multiple */
    LWM2M_RES_POWER_AMP,        /* multiple */
    LWM2M_RES_BATTERY_LEVEL,    /* 0-100 (percentage) */
    LWM2M_RES_MEM_FREE,         /* kB */
    LWM2M_RES_ERROR_CODE,       /* 0-8; multiple */
    LWM2M_RES_ERROR_CODE_RESET, /* exec */
    LWM2M_RES_TIME,             /* rw, string */
    LWM2M_RES_TIME_OFFSET,      /* rw, string; from UTC */
    LWM2M_RES_TIME_ZONE,        /* rw, string */
    LWM2M_RES_BINDINGS,
    LWM2M_RES_TYPE,
    LWM2M_RES_HW_VERSION,
    LWM2M_RES_SW_VERSION,
    LWM2M_RES_BATTERY_STATUS,
    LWM2M_RES_MEM_TOTAL,        /* kB */
    LWM2M_RES_EXT_DEV_INFO,     /* objlink; multiple */
    LWM2M_DEVICE_RESOURCES      /* number of resources */
};

/**
 * @brief Descriptor of a LWM2M device object instance
 */
typedef struct {
    uint8_t *power_sources;      /**< types of power sources (0-7) */
    uint16_t *power_voltage;     /**< voltage of power sources in mV */
    uint16_t *power_current;     /**< current of power sources in mA */
    uint8_t battery_status;      /**< battery status (0-6) */
    uint32_t mem_total;          /**< amount of memory on the device in kB */
    uint16_t(*ext_dev_info)[2];  /**< external devices information */
    uint8_t ext_dev_info_len;    /**< amount of external devices information */
    uint8_t error_code[7];       /**< error codes */
    uint8_t error_code_used;     /**< amount of error codes used */
} dev_data_t;

/**
 * @brief Frees the memory of @p obj device object
 *
 * @param[in] obj pointer to the device object
 */
void lwm2m_free_object_device(lwm2m_object_t *obj);

/**
 * @brief Determines if a reboot request has been issued to the device by a
 *        server.
 *
 * @return true reboot has been requested
 * @return false reboot has not been requested
 */
bool lwm2m_device_reboot_requested(void);


#ifdef __cplusplus
}
#endif

#endif /* LWM2M_OBJECTS_DEVICE_H */
/** @} */
