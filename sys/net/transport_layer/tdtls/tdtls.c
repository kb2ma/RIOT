/*
 * Copyright (C) 2018 Ken Bannister <kb2ma@runbox.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_sock_tdtls
 * @{
 *
 * @file
 * @brief       tinydtls sock wrapper implementation
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 * @}
 */

#include "net/sock/tdtls.h"
#include "dtls.h"

/*
 * From tinydtls:
 * TLS_PSK_WITH_AES_128_CCM_8           0xC0A8  (RFC 6655)
 * TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8   0xC0AE  (RFC 7251)
 */
#define SECURE_CIPHER_PSK_IDS (0xC0A8)
#define SECURE_CIPHER_RPK_IDS (0xC0AE)
#define SECURE_CIPHER_LIST { SECURE_CIPHER_PSK_IDS, SECURE_CIPHER_RPK_IDS }

static int _recv_from_dtls(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len);
static int _send_to_remote(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len);

dtls_handler_t _td_handlers = {
    .write       = _send_to_remote,
    .read        = _recv_from_dtls,
    .event       = NULL,
    .get_psk_key = NULL
};

/**
 * @brief   Handler function for a server response, including the state for the
 *          originating request
 *
 * If request timed out, the packet header is for the request.
 */
typedef void (*tdsock_recv_handler_t)(unsigned req_state, coap_pkt_t* pdu,
                                     sock_udp_ep_t *remote);

static int _recv_from_dtls(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len)
{
    /* calls tdsock read callback */
    tdsock_t * tdsock = (tdsock_t *)dtls_get_app_data(ctx);
    /* convert session to remote */

    tdsock->recv_handler(tdsock, data, len, remote);
    return 0;
}

static int _send_to_remote(dtls_context_t *ctx, session_t *session,
                         uint8 *data, size_t len)
{
    /* convert session to remote */
    sock_udp_ep_t remote;

    tdsock_t * tdsock = (tdsock_t *)dtls_get_app_data(ctx);

    return sock_udp_send(tdsock->sock, data, len, &remote);
}

int tdsock_create(tdsock_t *tdsock, const sock_udp_ep_t *local,
                  const sock_udp_ep_t *remote, uint16_t flags)
{
    int res = sock_udp_create(tdsock->sock, local, remote, flags);

    tdsock->td_context = dtls_new_context(tdsock);
    dtls_set_handler(tdsock->td_context, _td_handlers);

    return res;
}

ssize_t tdsock_recv(tdsock_t *tdsock, size_t buf_len, uint32_t timeout,
                    const sock_udp_ep_t *remote)
{
    /* Add to buffer length for DTLS overhead */
    uint8_t buf[buf_len];

    ssize_t res = sock_udp_recv(tdsock->sock, buf, buf_len, timeout, remote);

    if (res) {
        /* convert remote to session */
        session_t remote_addr;

        res = dtls_handle_message(tdsock->td_context, &remote_addr, buf, buf_len);
    }

    return res;
}

ssize_t tdsock_send(tdsock_t *tdsock, const void *data, size_t len,
                    const sock_udp_ep_t *remote)
{
    /* convert remote to session */
    session_t remote_addr;
    
    return dtls_write(tdsock->td_context, session, data, len);
}

void tdsock_init(void)
{
    /* verify cipher suites here */

    dtls_init();

#ifdef TINYDTLS_LOG_LVL
    dtls_set_log_level(TINYDTLS_LOG_LVL);
#endif
}
