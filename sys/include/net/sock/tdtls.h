/*
 * Copyright (C) 2018 Ken Bannister <kb2ma@runbox.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_sock_tdtls   tinydtls sock wrapper
 * @ingroup     net_sock
 *
 * @brief       tinydtls sock wrapper
 *
 * @{
 *
 * @file
 * @brief   tinydtls sock wrapper
 *
 * @author  Ken Bannister <kb2ma@runbox.com>
 */

#ifndef NET_SOCK_TDTLS_H
#define NET_SOCK_TDTLS_H

#include "net/sock/udp.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief  Wrapper for UDP sock
 */
typedef struct {
    sock_udp_t *sock;             /**< Wrapped sock */
    dtls_context_t *td_context;   /**< tinydtls context object */
} tdsock_t;


/**
 * @brief   Creates a new tinydtls UDP sock object
 *
 * Follows sock_udp_create(). See that function for details.
 *
 * @pre `(tdsock != NULL)`
 * @pre `(remote == NULL) || (remote->port != 0)`
 *
 * @param[out] tdsock   The resulting tdsock object.
 * @param[in] local     Local end point for the sock object.
 * @param[in] flags     Flags for the sock object. See also
 *                      [sock flags](@ref net_sock_flags).
 *                      May be 0.
 *
 * @return  0 on success.
 * @return <- on failure, per sock_udp_create() documentation
 */
int tdsock_create(tdsock_t *tdsock, const sock_udp_ep_t *local,
                  const sock_udp_ep_t *remote, uint16_t flags);

ssize_t tdsock_recv(tdsock_t *tdsock, size_t buf_len, uint32_t timeout,
                    sock_udp_ep_t *remote);

ssize_t tdsock_send(tdsock_t *tdsock, const void *data, size_t len,
                    const sock_udp_ep_t *remote);


/**
 * @brief One-time initialization.
 *
 * Must be called before any other use.
 */
void tdsock_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_SOCK_TDTLS_H */
/** @} */
