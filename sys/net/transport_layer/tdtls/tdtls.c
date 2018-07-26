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

static int _recv_from_dtls(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len)
{
    tdsec_ref_t *tdsec = (tdsec_ref_t *)dtls_get_app_data(ctx);

    /*
     Must add 'app' void* to session_t
    sock_udp_ep_t *sock_remote_ptr;
    if (session->app) {
        sock_remote_ptr = ((tdsec_endpoint_t *)session->app)->sock_remote;
    } else {
        sock_udp_ep_t sock_remote;
    }
    */
    sock_udp_ep_t sock_remote;
    /* convert session to sock remote ep*/

    tdsec->recv_handler(tdsec, data, len, &sock_remote);
    return 0;
}

static int _send_to_remote(dtls_context_t *ctx, session_t *session,
                           uint8 *data, size_t len)
{
    /* convert session to remote */
    sock_udp_ep_t remote;

    tdsec_ref_t *tdsec = (tdsec_ref_t *)dtls_get_app_data(ctx);

    return sock_udp_send(tdsec->sock, data, len, &remote);
}

int tdsec_create(tdsec_ref_t *tdsec, sock_udp_ep_t *sock,
                 tdsec_recv_handler_t recv_handler)
{
    tdsec->sock = sock;
    tdsec->td_context = dtls_new_context(tdsec);
    tdsec->recv_handler = recv_handler;

    dtls_set_handler(tdsec->td_context, _td_handlers);

    return 0;
}

ssize_t tdsec_decrypt(tdsec_ref_t *tdsec, uint8_t *buf, size_t len, 
                      tdsec_endpoint_t *td_ep)
{
    session_t *td_session_ptr;
    if (td_ep->td_session) {
        td_session_ptr = td_ep->td_session;
    } else {
        session_t td_session;
        td_session_ptr = &td_session;
        /* create session from sock remote ep */
    }

    res = dtls_handle_message(tdsec->td_context, td_session_ptr, buf, len);
    return res;
}

ssize_t tdsec_encrypt(tdsec_ref_t *tdsec, const void *data, size_t len,
                      const sock_udp_ep_t *remote)
{
    /* convert remote to session */
    session_t session;
    
    return dtls_write(tdsec->td_context, &session, data, len);
}

void tdsec_init(void)
{
    /* verify cipher suites here */

    dtls_init();

#ifdef TINYDTLS_LOG_LVL
    dtls_set_log_level(TINYDTLS_LOG_LVL);
#endif
}
