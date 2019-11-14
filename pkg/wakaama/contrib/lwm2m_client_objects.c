/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     lwm2m_client
 *
 * @file
 * @brief       Helper functions to interact with the basic objects provided by
 *              Wakaama from a LwM2M client.
 *
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 * @}
 */

#include "kernel_defines.h"
#include "net/lwm2m.h"

#include "lwm2m_client.h"
#include "lwm2m_client_objects.h"

/* These functions are defined by the objects (object_security.c,
 * object_server.c, object_device.c and object_access_control.c) */
lwm2m_object_t *get_security_object(int server_id, const char *server_uri,
                                    char *bs_psk_id, char *psk,
                                    uint16_t psk_len, bool is_bootstrap);
lwm2m_object_t *get_server_object(int server_id, const char *binding,
                                  int lifetime, bool storing);
lwm2m_object_t *lwm2m_get_object_device(void);
lwm2m_object_t *acc_ctrl_create_object(void);
void acl_ctrl_free_object(lwm2m_object_t * objectP);
bool acc_ctrl_obj_add_inst(lwm2m_object_t* accCtrlObjP, uint16_t instId,
                           uint16_t acObjectId, uint16_t acObjInstId,
                           uint16_t acOwner);
bool acc_ctrl_oi_add_ac_val(lwm2m_object_t* accCtrlObjP, uint16_t instId,
                            uint16_t acResId, uint16_t acValue);

lwm2m_object_t *lwm2m_client_get_security_object(
                        lwm2m_client_data_t *client_data)
{
    lwm2m_object_t *ret;
    char *server_uri = LWM2M_SERVER_URI;
    int server_id = LWM2M_SERVER_ID;
    uint16_t psk_len = -1;
    char *psk_buffer = NULL;
    char *psk_id = NULL;

    ret = get_security_object(server_id, server_uri, psk_id, psk_buffer,
                              psk_len, IS_ACTIVE(LWM2M_BOOTSTRAP));

    client_data->obj_security = ret;
    return ret;
}

lwm2m_object_t *lwm2m_client_get_server_object(
                        lwm2m_client_data_t *client_data)
{
    (void)client_data;
    lwm2m_object_t *ret;
    int server_id = LWM2M_SERVER_ID;
    int lifetime = LWM2M_DEVICE_TTL;

    ret = get_server_object(server_id, LWM2M_DEVICE_BINDINGS, lifetime, false);
    return ret;
}

lwm2m_object_t *lwm2m_client_get_device_object(
                        lwm2m_client_data_t *client_data)
{
    (void)client_data;
    return lwm2m_get_object_device();
}

lwm2m_object_t *lwm2m_client_get_acc_ctrl_object(
                        lwm2m_client_data_t *client_data)
{
    (void)client_data;
    lwm2m_object_t *ret;
    uint16_t instance_id = 0;

    ret = acc_ctrl_create_object();

    if (!ret) {
        goto out;
    }

    if (!acc_ctrl_obj_add_inst(ret, instance_id, 0, 1, LWM2M_SERVER_ID)) {
        goto free_out;
    }


    if (!acc_ctrl_oi_add_ac_val(ret, instance_id, LWM2M_SERVER_ID, 0x1F)) {
        goto free_out;
    }

    goto out;

free_out:
    acl_ctrl_free_object(ret);
out:
    return ret;
}
