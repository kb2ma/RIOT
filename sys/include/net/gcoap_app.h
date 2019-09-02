/*
 * Copyright (c) 2015-2017 Ken Bannister. All rights reserved.
 *               2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gcoap
 * @brief       Application for gcoap
 *
 * @{
 *
 * @file
 * @brief       gcoap app definition
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 */

#ifndef NET_GCOAP_APP_H
#define NET_GCOAP_APP_H

#include "net/gcoap.h"
#include "net/sock/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    Return values used by the gcoap_find_resource function
 * @{
 */
#define GCOAP_RESOURCE_FOUND         (0)
#define GCOAP_RESOURCE_WRONG_METHOD (-1)
#define GCOAP_RESOURCE_NO_PATH      (-2)
/** @} */

/**
 * @brief   A modular collection of resources for a server
 */
typedef struct gcoap_listener {
    const coap_resource_t *resources;   /**< First element in the array of
                                         *   resources; must order alphabetically */
    size_t resources_len;               /**< Length of array */
    gcoap_link_encoder_t link_encoder;  /**< Writes a link for a resource */
    struct gcoap_listener *next;        /**< Next listener in list */
} gcoap_listener_t;

/**
 * @brief   Finds the resource for the URI path in a request
 *
 * @param[in] pdu           Request metadata
 * @param[in] remote        Request sender
 * @param[out] resrc_handle Found resource
 *
 * @return  GCOAP_RESOURCE_FOUND if the resource was found
 * @return  GCOAP_RESOURCE_WRONG_METHOD if no resource for the request method
 * @return  GCOAP_RESOURCE_NO_PATH if no resource was found for the path
 */
int gcoap_find_resource(const coap_pkt_t *pdu, const sock_udp_ep_t *remote,
                        coap_resrc_handle_t *resrc_handle);

/**
 * @brief   Get the resource list, currently only `CoRE Link Format`
 *          (COAP_FORMAT_LINK) supported
 *
 * If @p buf := NULL, nothing will be written but the size of the resulting
 * resource list is computed and returned.
 *
 * @param[out] buf      output buffer to write resource list into, my be NULL
 * @param[in]  maxlen   length of @p buf, ignored if @p buf is NULL
 * @param[in]  cf       content format to use for the resource list, currently
 *                      only COAP_FORMAT_LINK supported
 *
 * @todo    add support for `JSON CoRE Link Format`
 * @todo    add support for 'CBOR CoRE Link Format`
 *
 * @return  the number of bytes written to @p buf
 * @return  -1 on error
 */
int gcoap_get_resource_list(void *buf, size_t maxlen, uint8_t cf);

/**
 * @brief   Starts listening for resource paths
 *
 * @param[in] listener  Listener containing the resources.
 */
void gcoap_register_listener(gcoap_listener_t *listener);

#ifdef __cplusplus
}
#endif
#endif /* NET_GCOAP_APP_H */
/** @} */
