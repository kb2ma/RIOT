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
static void _copy_sock_ep(const sock_udp_ep_t *remote, session_t *session);
static void _copy_tdsec_ep(const session_t *session, sock_udp_ep_t *remote);

dtls_handler_t _td_handlers = {
    .write        = _send_to_remote,
    .read         = _recv_from_dtls,
    .event        = NULL,
    .get_psk_info = NULL
};

static int _recv_from_dtls(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len)
{
    tdsec_ref_t *tdsec = (tdsec_ref_t *)dtls_get_app_data(ctx);

    sock_udp_ep_t sock_remote;
    _copy_tdsec_ep(session, &sock_remote);

    tdsec->recv_handler(tdsec->sock, data, len, &sock_remote);
    return 0;
}

static int _send_to_remote(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len)
{
    /* convert session to remote */
    sock_udp_ep_t sock_remote;
    _copy_tdsec_ep(session, &sock_remote);

    tdsec_ref_t *tdsec = (tdsec_ref_t *)dtls_get_app_data(ctx);

    return sock_udp_send(tdsec->sock, data, len, &sock_remote);
}

static void _copy_sock_ep(const sock_udp_ep_t *remote, session_t *session) {
    session->size = sizeof(remote->addr.ipv6) + sizeof(remote->port);
    memcpy(session->addr.u8, remote->addr.ipv6, sizeof(remote->addr.ipv6));
    session->port = remote->port;
    session->ifindex = remote->netif;
}

static void _copy_tdsec_ep(const session_t *session, sock_udp_ep_t *remote) {
    remote->family = AF_INET6;
    memcpy(remote->addr.ipv6, session->addr.u8, sizeof(session->addr.u8));
    remote->port = session->port;
    remote->netif = SOCK_ADDR_ANY_NETIF;
}

int tdsec_create(tdsec_ref_t *tdsec, sock_udp_t *sock,
                 tdsec_recv_handler_t recv_handler)
{
    tdsec->sock = sock;
    tdsec->td_context = dtls_new_context(tdsec);
    tdsec->recv_handler = recv_handler;

    dtls_set_handler(tdsec->td_context, &_td_handlers);

    return 0;
}

ssize_t tdsec_read_msg(tdsec_ref_t *tdsec, uint8_t *buf, size_t len, 
                      tdsec_endpoint_t *td_ep)
{
    session_t td_session;
    _copy_sock_ep(td_ep->sock_remote, &td_session);

    int res = dtls_handle_message(tdsec->td_context, &td_session, buf, len);
    return res;
}

ssize_t tdsec_send(tdsec_ref_t *tdsec, const void *data, size_t len,
                   const sock_udp_ep_t *remote)
{
    session_t session;
    _copy_sock_ep(remote, &session);

    return dtls_write(tdsec->td_context, &session, (uint8_t *)data, len);
}

void tdsec_init(void)
{
    /* verify cipher suites here */

    dtls_init();

#ifdef TINYDTLS_LOG_LVL
    dtls_set_log_level(TINYDTLS_LOG_LVL);
#endif
}
