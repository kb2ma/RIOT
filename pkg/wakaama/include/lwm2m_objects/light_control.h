/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     lwm2m_objects
 * @defgroup    lwm2m_objects_light_control Light Control LWM2M object
 * @brief       Light Control object implementation for LWM2M client using
 *              Wakaama
 * @{
 *
 * @file
 *
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 */

#ifndef OBJECTS_LIGHT_CONTROL_H
#define OBJECTS_LIGHT_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liblwm2m.h"
#include "periph/gpio.h"
#include "xtimer.h"

/**
 * @brief LWM2M ID for the Light Control object
 */
#define LWM2M_LIGHT_CONTROL_OBJECT_ID   (3311)

/**
 * @brief Maximum length of the application type resource of the Light Control
 *        object
 */
#ifndef LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN
#define LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN (32)
#endif /* LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN */

/**
 * @brief Resource ids for the Light Control object
 */
typedef enum {
    LWM2M_LIGHT_CTRL_RES_SENSOR_UNITS = 5701,
    LWM2M_LIGHT_CTRL_RES_COLOUR = 5706,
    LWM2M_LIGHT_CTRL_RES_APP_TYPE = 5750,
    LWM2M_LIGHT_CTRL_RES_CUM_ACTIVE_PWR = 5805,
    LWM2M_LIGHT_CTRL_RES_PWR_FACTOR = 5820,
    LWM2M_LIGHT_CTRL_RES_ON_OFF = 5850,
    LWM2M_LIGHT_CTRL_RES_DIMMER,
    LWM2M_LIGHT_CTRL_RES_ON_TIME,
} light_ctrl_resource_t;

/**
 * @brief Handler prototype for the state changes on the light
 */
typedef void (light_ctrl_state_handle_t)(void *arg, bool state);

/**
 * @brief Handler prototype for the dimmer value changes on the light
 */
typedef void (light_ctrl_dimmer_handle_t)(void *arg, uint8_t value);

/**
 * @brief Configuration parameters for a Light Control object instance
 */
typedef struct {
    void *arg;                                 /**< argument for handlers */
    const char *type;                          /**< application type string */
    light_ctrl_state_handle_t *state_handle;   /**< light state handler */
    light_ctrl_dimmer_handle_t *dimmer_handle; /**< light dimmer handler */
} light_ctrl_params_t;

/**
 * @brief Callback that is used in the on time update function
 */
typedef struct {
    lwm2m_context_t *ctx;                 /**< LWM2M context */
    struct light_ctrl_instance *instance; /**< instance of light ctrl object */
} light_ctrl_ontime_cb_arg_t;

/**
 * @brief Light Control object instance descriptor
 */
typedef struct light_ctrl_instance {
    struct light_ctrl_instance *next;  /**< matches lwm2m_list_t::next */
    uint16_t shortID;                  /**< matches lwm2m_list_t::id */
    bool light_onoff;                  /**< ON/OFF state of the light */
    uint8_t light_dimmer;              /**< Dimmer value of the light */
    light_ctrl_params_t *params;       /**< configuration parameters */
    uint32_t on_time;                  /**< time when light is turned on */
    xtimer_t xtimer;                   /**< xtimer for on time update */
    light_ctrl_ontime_cb_arg_t cb_arg; /**< argument for on time update */
    char app_type[LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN]; /**< app type */
} light_ctrl_instance_t;

/**
 * @brief Frees a Light Control object
 *
 * @param[in] object pointer to the object
 */
void object_light_control_free(lwm2m_object_t *object);

/**
 * @brief Creates a Light Control object and one instance per element in the
 *        @p params array.
 *
 * @param[in] params array of parameters for instances
 * @param[in] params_numof number of elements in @p params
 *
 * @return Pointer to the created object
 * @return NULL if could not create the object
 */
lwm2m_object_t *object_light_control_get(light_ctrl_params_t *params,
                                         uint16_t params_numof);

/**
 * @brief Toggles the state of the light of an instance of Light Control object
 *
 * @param[in] lwm2m_ctx LWM2M context pointer
 * @param[in] object pointer to the Light Control object
 * @param[in] instance_id ID of the instance to switch the light to
 *
 * @return 0 on success,
 * @return non-zero otherwise
 */
int object_light_control_toggle(lwm2m_context_t *lwm2m_ctx,
                                lwm2m_object_t *object, uint16_t instance_id);

/**
 * @brief Enables the automatic refresh of the on time of the light for a
 *        certain instance of Light Control object
 *
 * @param[in] lwm2m_ctx LWM2M context pointer
 * @param[in] object pointer to the Light Control object
 * @param[in] instance_id ID of the instance to enable the on time refresh
 *
 * @return 0 on success
 * @return -1 otherwise
 */
int object_light_control_enable_ontime(lwm2m_context_t *lwm2m_ctx,
                                       lwm2m_object_t *object,
                                       uint16_t instance_id);

#ifdef __cplusplus
}
#endif

#endif /* OBJECTS_LIGHT_CONTROL_H */
/** @} */
