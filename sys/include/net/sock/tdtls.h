/*
 * Copyright (C) 2018 Ken Bannister <kb2ma@runbox.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_sock_tdtls   tinydtls sock security
 * @ingroup     net_sock
 *
 * @brief       tinydtls sock security
 *
 * @{
 *
 * @file
 * @brief   tinydtls sock security
 *
 * @author  Ken Bannister <kb2ma@runbox.com>
 */

#ifndef NET_SOCK_TDTLS_H
#define NET_SOCK_TDTLS_H

#include "net/sock/udp.h"
#include "dtls.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Application handler function for decrypted incoming message
 */
typedef void (*tdsec_recv_handler_t)(tdsec_ref_t *tdsec, uint8 *data,
                                      size_t len, sock_udp_ep_t *remote);

/**
 * @brief   Root reference object for tdsec sock security
 */
typedef struct {
    sock_udp_t *sock;                   /**< UDP sock reference */
    dtls_context_t *td_context;         /**< tinydtls context object */
    tdsec_recv_handler_t recv_handler;  /**< Application callback for decrypted message */
} tdsec_ref_t;

/**
 * @brief   Encryption session with a remote endpoint
 */
typedef struct {
    sock_udp_ep_t *sock_remote;
    session_t *td_session;
    dtls_peer_type peer_type;
} tdsec_endpoint_t;

/**
 * @brief   Creates a tinydtls sock security reference object.
 *
 * @return  0 on success.
 */
int tdsec_create(tdsec_ref_t *tdsec, sock_udp_t *sock,
                 tdsec_recv_handler_t recv_handler);

ssize_t tdsec_decrypt(tdsec_ref_t *tdsec, uint8_t buf, size_t len,
                      tdsec_endpoint_t *td_ep);

ssize_t tdsock_send(tdsock_t *tdsock, const void *data, size_t len,
                    const sock_udp_ep_t *remote);


/**
 * @brief One-time initialization
 *
 * Must be called before any other use.
 */
void tdsec_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_SOCK_TDTLS_H */
/** @} */
