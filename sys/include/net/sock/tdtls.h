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
 * @brief       Simple tinydtls based DTLS adapter for sock
 *
 * @{
 *
 * @file
 * @brief   tdtls definition
 *
 * @author  Ken Bannister <kb2ma@runbox.com>
 */

#ifndef NET_SOCK_TDTLS_H
#define NET_SOCK_TDTLS_H

#include "net/sock/udp.h"
#include "dtls.h"
#include "dtls_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Application handler function for decrypted incoming message
 */
typedef void (*tdsec_recv_handler_t)(sock_udp_t *sock, uint8_t *data,
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
 * @brief PSK parameters
 */
typedef struct {
    const char *client_id;   /**< client identity */
    size_t id_len;           /**< length of client_id */ 
    const char *key;         /**< key itself */
    size_t key_len;          /**< length of key */ 
} tdsec_psk_params_t;


/**
 * @brief tinydtls initialization
 *
 * Must be called before any other use.
 */
void tdsec_init(void);

/**
 * @brief   Creates a tinydtls sock security object.
 *
 * @return  0 on success.
 */
int tdsec_create(tdsec_ref_t *tdsec, sock_udp_t *sock,
                 tdsec_recv_handler_t recv_handler);

/**
 * @brief   Establish a DTLS connection/session with a remote endpoint.
 *
 * Waits 5 seconds for connection.
 *
 * @return >= 0 on success
 * @return <= 0 on failure
 */
ssize_t tdsec_connect(tdsec_ref_t *tdsec, const sock_udp_ep_t *remote);

/**
 * @brief   Decrypts and reads a message from a remote peer.
 */
ssize_t tdsec_read(tdsec_ref_t *tdsec, uint8_t *buf, size_t len,
                   const sock_udp_ep_t *remote);

/**
 * @brief   Encrypts and sends a message to a remote peer.
 */
ssize_t tdsec_send(tdsec_ref_t *tdsec, const void *data, size_t len,
                   const sock_udp_ep_t *remote);

#ifdef __cplusplus
}
#endif

#endif /* NET_SOCK_TDTLS_H */
/** @} */
