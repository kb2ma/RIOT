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
 *
 * @see http://www.openmobilealliance.org/tech/profiles/LWM2M_Device-v1_0_3.xml
 */
enum lwm2m_device_resources {
    LWM2M_RES_MANUFACTURER = 0, /**< Human readable manufacturer name */
    LWM2M_RES_MODEL_NO,         /**< Model identifier (manufacturer specified string) */
    LWM2M_RES_SERIAL,           /**< Serial number */
    LWM2M_RES_FW_VER,           /**< Current firmware version of the device */
    LWM2M_RES_REBOOT,           /**< Reboot the device */
    LWM2M_RES_FRESET,           /**< Perform a factory reset of the device */
    LWM2M_RES_POWER_SRC,        /**< Available power sources */
    LWM2M_RES_POWER_VOL,        /**< Present voltage for each power source */
    LWM2M_RES_POWER_AMP,        /**< Present current for each power source */
    LWM2M_RES_BATTERY_LEVEL,    /**< Current battery level as a percentage */
    LWM2M_RES_MEM_FREE,         /**< Estimated current available storage (kB) */
    LWM2M_RES_ERROR_CODE,       /**< Last error code */
    LWM2M_RES_ERROR_CODE_RESET, /**< Delete all error code instances */
    LWM2M_RES_TIME,             /**< Current UNIX time of the client */
    LWM2M_RES_TIME_OFFSET,      /**< Indicated the UTC offset for the device */
    LWM2M_RES_TIME_ZONE,        /**< Indicates the time zone of the device */
    LWM2M_RES_BINDINGS,         /**< Indicates supported bindings and modes on the client */
    LWM2M_RES_TYPE,             /**< Type of device */
    LWM2M_RES_HW_VERSION,       /**< Current hardware version of the device */
    LWM2M_RES_SW_VERSION,       /**< Current software version on the device */
    LWM2M_RES_BATTERY_STATUS,   /**< Battery status when internal battery is present */
    LWM2M_RES_MEM_TOTAL,        /**< Total amount of storage space in the device (kB*/
    LWM2M_RES_EXT_DEV_INFO,     /**< External device object instance */
    LWM2M_DEVICE_RESOURCES      /**< Number of resources */
};

/**
 * @brief Error codes for the
 * @ref lwm2m_device_resources::LWM2M_RES_ERROR_CODE "Error" resource in the
 * device object of LWM2M
 */
enum lwm2m_device_error_codes {
    LWM2M_DEVICE_ERR_NO_ERR     = 0, /**< No error */
    LWM2M_DEVICE_ERR_LOW_BATT   = 1, /**< Low battery power */
    LWM2M_DEVICE_ERR_EXT_OFF    = 2, /**< External power supply off */
    LWM2M_DEVICE_ERR_GPS_ERR    = 3, /**< GPS module failure */
    LWM2M_DEVICE_ERR_LOW_SIGNAL = 4, /**< Low received signal strength */
    LWM2M_DEVICE_ERR_NO_MEM     = 5, /**< Out of memory */
    LWM2M_DEVICE_ERR_SMS_ERR    = 6, /**< SMS failure */
    LWM2M_DEVICE_ERR_IP_ERR     = 7, /**< IP connectivity failure */
    LWM2M_DEVICE_ERR_PERIPH_ERR = 8  /**< Peripheral malfunction */
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
