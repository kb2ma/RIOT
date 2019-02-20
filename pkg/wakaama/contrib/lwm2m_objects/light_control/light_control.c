/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     lwm2m_objects_light_control
 * @{
 * @brief       Light control object implementation for LWM2M client using
 *              Wakaama
 *
 * @file
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 * @}
 */

#include <string.h>

#include "liblwm2m.h"
#include "lwm2m_objects/light_control.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _update_ontime_cb(void *data)
{
    lwm2m_uri_t uri;
    light_ctrl_instance_t *instance;
    lwm2m_context_t *ctx;
    light_ctrl_ontime_cb_arg_t *arg = (light_ctrl_ontime_cb_arg_t *)data;

    ctx = arg->ctx;
    instance = arg->instance;

    uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
               LWM2M_URI_FLAG_RESOURCE_ID;
    uri.objectId = LWM2M_LIGHT_CONTROL_OBJECT_ID;
    uri.instanceId = instance->shortID;
    uri.resourceId = LWM2M_LIGHT_CTRL_RES_ON_TIME;
    xtimer_set(&instance->xtimer, 1 * US_PER_SEC);
    DEBUG("update ontime\n");
    lwm2m_resource_value_changed(ctx, &uri);
}

static uint8_t _read(uint16_t instanceId, int *numDataP,
                     lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP)
{
    light_ctrl_instance_t *instance;
    DEBUG("[object_light::_read]");
    /* try to find the requested instance */
    instance = (light_ctrl_instance_t *)lwm2m_list_find(objectP->instanceList,
                                                        instanceId);
    if (!instance) {
        DEBUG("[light_ctrl::read] Could not find instance\n");
        return COAP_404_NOT_FOUND;
    }

    /* if the server does not specify the amount of parameters, return all */
    if (*numDataP == 0) {
        /* alloc memory for the three resources */
        *dataArrayP = lwm2m_data_new(4);
        if (!(*dataArrayP)) {
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
        *numDataP = 4;
        (*dataArrayP)[0].id = LWM2M_LIGHT_CTRL_RES_ON_OFF;
        (*dataArrayP)[1].id = LWM2M_LIGHT_CTRL_RES_DIMMER;
        (*dataArrayP)[2].id = LWM2M_LIGHT_CTRL_RES_APP_TYPE;
        (*dataArrayP)[3].id = LWM2M_LIGHT_CTRL_RES_ON_TIME;
    }

    /* check which resource is wanted */
    for (int i = 0; i < *numDataP; i++) {
        switch((*dataArrayP)[i].id) {
            case LWM2M_LIGHT_CTRL_RES_ON_OFF:
                lwm2m_data_encode_bool(instance->light_onoff,
                                       *dataArrayP + i);
                break;
            case LWM2M_LIGHT_CTRL_RES_DIMMER:
                lwm2m_data_encode_int(instance->light_dimmer, *dataArrayP + i);
                break;
            case LWM2M_LIGHT_CTRL_RES_APP_TYPE:
                lwm2m_data_encode_string(instance->app_type, *dataArrayP + i);
                break;
            case LWM2M_LIGHT_CTRL_RES_ON_TIME:
                if (instance->light_onoff) {
                    uint32_t now = div_u64_by_1000000(xtimer_usec_from_ticks64(
                                                        xtimer_now64()));
                    if (instance->on_time > now) {
                        instance->on_time = now;
                    }
                    lwm2m_data_encode_int(now - instance->on_time,
                                          *dataArrayP + i);
                }
                else {
                    lwm2m_data_encode_int(0, *dataArrayP + i);
                }
                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    return COAP_205_CONTENT;
}

static uint8_t _discover(uint16_t instanceId, int *numDataP,
                         lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP)
{
    (void)instanceId;
    (void)objectP;
    DEBUG("[object_light::_discover]");
    if (*numDataP == 0) {
        *dataArrayP = lwm2m_data_new(4);
        if (!(*dataArrayP)) {
            return COAP_500_INTERNAL_SERVER_ERROR;
        }
        *numDataP = 4;
        (*dataArrayP)[0].id = LWM2M_LIGHT_CTRL_RES_ON_OFF;
        (*dataArrayP)[1].id = LWM2M_LIGHT_CTRL_RES_DIMMER;
        (*dataArrayP)[2].id = LWM2M_LIGHT_CTRL_RES_APP_TYPE;
        (*dataArrayP)[3].id = LWM2M_LIGHT_CTRL_RES_ON_TIME;
        return COAP_205_CONTENT;
    }
    else {
        for (int i = 0; i < *numDataP; i++) {
            switch((*dataArrayP)[i].id) {
                case LWM2M_LIGHT_CTRL_RES_ON_OFF:
                case LWM2M_LIGHT_CTRL_RES_DIMMER:
                case LWM2M_LIGHT_CTRL_RES_APP_TYPE:
                case LWM2M_LIGHT_CTRL_RES_ON_TIME:
                    return COAP_205_CONTENT;
                default:
                    return COAP_404_NOT_FOUND;
            }
        }
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t _write(uint16_t instanceId, int numData,
                      lwm2m_data_t *dataArrayP, lwm2m_object_t *objectP)
{
    light_ctrl_instance_t *instance;
    DEBUG("[object_light::_write]");
    /* try to find the requested instance */
    instance = (light_ctrl_instance_t *)lwm2m_list_find(objectP->instanceList,
                                                        instanceId);
    if (!instance) {
        return COAP_404_NOT_FOUND;
    }

    for (int i = 0; i < numData; i++) {
        switch(dataArrayP[i].id) {
            case LWM2M_LIGHT_CTRL_RES_ON_OFF: {
                bool value;
                if ((lwm2m_data_decode_bool(dataArrayP + i, &value) != 1)) {
                    return COAP_400_BAD_REQUEST;
                }
                /* update value only if it is different */
                if (instance->on_time != value) {
                    instance->light_onoff = value;
                    instance->params->state_handle(instance->params->arg,
                                                   value);
                    /* when light is turned on update on time */
                    if (instance->light_onoff) {
                        instance->on_time = div_u64_by_1000000(
                                                xtimer_usec_from_ticks64(
                                                    xtimer_now64()));
                        /* set the timer only if the callback is configured */
                        if (instance->cb_arg.ctx) {
                            xtimer_set(&instance->xtimer, 1 * US_PER_SEC);
                        }
                    }
                    else {
                        xtimer_remove(&instance->xtimer);
                    }
                }
                break;
            }
            case LWM2M_LIGHT_CTRL_RES_DIMMER: {
                int64_t value;
                if ((lwm2m_data_decode_int(dataArrayP + i, &value) != 1) ||
                    value < 0 || value > 100) {
                    return COAP_400_BAD_REQUEST;
                }
                instance->light_dimmer = (uint8_t)value;
                instance->params->dimmer_handle(instance->params->arg,
                                                (uint8_t)value);
                break;
            }
            case LWM2M_LIGHT_CTRL_RES_APP_TYPE:
                if ((dataArrayP[i].type == LWM2M_TYPE_STRING ||
                     dataArrayP[i].type == LWM2M_TYPE_OPAQUE) &&
                    (dataArrayP[i].value.asBuffer.length <=
                     LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN)) {
                    strncpy(instance->app_type,
                            (char *)dataArrayP[i].value.asBuffer.buffer,
                            dataArrayP[i].value.asBuffer.length);
                }
                else {
                    return COAP_400_BAD_REQUEST;
                }
                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    return COAP_204_CHANGED;
}

static uint8_t _delete(uint16_t instanceId, lwm2m_object_t *objectP)
{
    light_ctrl_instance_t *instance;
    DEBUG("[object_light::_delete]");
    /* try to remote the instance from the list */
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId,
                                              (lwm2m_list_t **)&instance);
    if (!instance) {
        return COAP_404_NOT_FOUND;
    }

    /* free the allocated memory for the instance */
    lwm2m_free(instance);
    return COAP_202_DELETED;
}

static uint8_t _create(uint16_t instanceId, int numData,
                       lwm2m_data_t *dataArray, lwm2m_object_t *objectP)
{
    (void)instanceId;
    (void)numData;
    (void)dataArray;
    (void)objectP;
    return COAP_405_METHOD_NOT_ALLOWED;
}

static uint8_t _execute(uint16_t instanceId, uint16_t resourceId,
                        uint8_t *buffer, int length, lwm2m_object_t *objectP)
{
    (void)instanceId;
    (void)resourceId;
    (void)buffer;
    (void)length;
    (void)objectP;
    return COAP_405_METHOD_NOT_ALLOWED;
}

lwm2m_object_t *object_light_control_get(light_ctrl_params_t *params,
                                         uint16_t params_numof)
{
    lwm2m_object_t *light_ctrl_obj;
    light_ctrl_instance_t *instance;

    light_ctrl_obj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (!light_ctrl_obj) {
        goto out;
    }
    memset(light_ctrl_obj, 0, sizeof(lwm2m_object_t));
    light_ctrl_obj->objID = LWM2M_LIGHT_CONTROL_OBJECT_ID;

    for (int i = 0; i < params_numof; i++) {
        instance = (light_ctrl_instance_t *)lwm2m_malloc(
                                                sizeof(light_ctrl_instance_t));
        if (!instance) {
            goto free_out;
        }
        memset(instance, 0, sizeof(light_ctrl_instance_t));
        instance->shortID = i;
        instance->params = &params[i];
        instance->light_onoff = false;
        instance->light_dimmer = 100;
        instance->params->state_handle(instance->params->arg,
                                       instance->light_onoff);
        instance->params->dimmer_handle(instance->params->arg,
                                       instance->light_dimmer);
        size_t len = strlen(params[i].type);
        if (len > LWM2M_LIGHT_CONTROL_OBJECT_MAX_APP_TYPE_LEN) {
            goto free_out;
        }
        strcpy(instance->app_type, params[i].type);
        light_ctrl_obj->instanceList = lwm2m_list_add(
                                            light_ctrl_obj->instanceList,
                                            (lwm2m_list_t *)instance);
        instance->cb_arg.ctx = NULL;
    }

    light_ctrl_obj->readFunc = _read;
    light_ctrl_obj->discoverFunc = _discover;
    light_ctrl_obj->writeFunc = _write;
    light_ctrl_obj->deleteFunc = _delete;
    light_ctrl_obj->executeFunc = _execute;
    light_ctrl_obj->createFunc = _create;
    goto out;

free_out:
    lwm2m_free(light_ctrl_obj);
    light_ctrl_obj = NULL;
out:
    return light_ctrl_obj;
}

void object_light_control_free(lwm2m_object_t *object)
{
    if (object == NULL) {
        return;
    }
    if (object->userData) {
        lwm2m_free(object->userData);
    }
    if (object->instanceList) {
        lwm2m_free(object->instanceList);
    }
    lwm2m_free(object);
}

int object_light_control_toggle(lwm2m_context_t *lwm2m_ctx,
                                lwm2m_object_t *object, uint16_t instance_id) {
    light_ctrl_instance_t *instance;
    lwm2m_uri_t uri;

    instance = (light_ctrl_instance_t *)lwm2m_list_find(object->instanceList,
                                                        instance_id);
    if (!instance) {
        return -1;
    }
    instance->light_onoff = !instance->light_onoff;
    instance->params->state_handle(instance->params->arg,
                                   instance->light_onoff);
    uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID |
                LWM2M_URI_FLAG_RESOURCE_ID;
    uri.objectId = object->objID;
    uri.instanceId = instance_id;
    uri.resourceId = LWM2M_LIGHT_CTRL_RES_ON_OFF;

    lwm2m_resource_value_changed(lwm2m_ctx, &uri);
    return 0;
}

int object_light_control_enable_ontime(lwm2m_context_t *lwm2m_ctx,
                                       lwm2m_object_t *object,
                                       uint16_t instance_id)
{
    light_ctrl_instance_t *instance;

    instance = (light_ctrl_instance_t *)lwm2m_list_find(object->instanceList,
                                                        instance_id);
    if (!instance) {
        return -1;
    }
    instance->cb_arg.ctx = lwm2m_ctx;
    instance->cb_arg.instance = instance;
    instance->xtimer.callback = _update_ontime_cb;
    instance->xtimer.arg = &instance->cb_arg;
    if (instance->light_onoff) {
        xtimer_set(&instance->xtimer, 1 * US_PER_SEC);
    }
    return 0;
}
