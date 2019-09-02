/*
 * Copyright (c) 2019 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gcoap
 * @{
 *
 * @file
 * @brief       Listener based gcoap application
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 */

#include "net/gcoap.h"
#include "net/gcoap_app.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ssize_t _well_known_core_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                                        void *ctx);

/* Internal variables */
const coap_resource_t _default_resources[] = {
    { "/.well-known/core", COAP_GET, _well_known_core_handler, NULL },
};

static gcoap_listener_t _listeners = {
    &_default_resources[0],
    ARRAY_SIZE(_default_resources),
    NULL,
    NULL
};

int gcoap_find_resource(const coap_pkt_t *pdu, const sock_udp_ep_t *remote,
                        coap_resrc_handle_t *resrc_handle)
{
    (void)remote;
    int ret = GCOAP_RESOURCE_NO_PATH;
    unsigned method_flag = coap_method2flag(coap_get_code_detail(pdu));

    /* Find path for CoAP msg among listener resources and execute callback. */
    gcoap_listener_t *listener = &_listeners;

    uint8_t uri[NANOCOAP_URI_MAX];
    if (coap_get_uri_path(pdu, uri) <= 0) {
        return GCOAP_RESOURCE_NO_PATH;
    }

    while (listener) {
        const coap_resource_t *lresource = listener->resources;
        for (size_t i = 0; i < listener->resources_len; i++) {
            if (i) {
                lresource++;
            }

            int res = coap_match_path(lresource, uri);
            if (res > 0) {
                continue;
            }
            else if (res < 0) {
                /* resources expected in alphabetical order */
                break;
            }
            else {
                if (! (lresource->methods & method_flag)) {
                    ret = GCOAP_RESOURCE_WRONG_METHOD;
                    continue;
                }

                resrc_handle->resource = lresource;
                return GCOAP_RESOURCE_FOUND;
            }
        }
        listener = listener->next;
    }

    return ret;
}

int gcoap_get_resource_list(void *buf, size_t maxlen, uint8_t cf)
{
    assert(cf == COAP_FORMAT_LINK);

    /* skip the first listener, gcoap itself (we skip /.well-known/core) */
    gcoap_listener_t *listener = _listeners.next;

    char *out = (char *)buf;
    size_t pos = 0;

    coap_link_encoder_ctx_t ctx;
    ctx.content_format = cf;
    /* indicate initial link for the list */
    ctx.flags = COAP_LINK_FLAG_INIT_RESLIST;

    /* write payload */
    while (listener) {
        if (!listener->link_encoder) {
            continue;
        }
        ctx.link_pos = 0;

        for (; ctx.link_pos < listener->resources_len; ctx.link_pos++) {
            ssize_t res;
            if (out) {
                res = listener->link_encoder(&listener->resources[ctx.link_pos],
                                             &out[pos], maxlen - pos, &ctx);
            }
            else {
                res = listener->link_encoder(&listener->resources[ctx.link_pos],
                                             NULL, 0, &ctx);
            }

            if (res > 0) {
                pos += res;
                ctx.flags &= ~COAP_LINK_FLAG_INIT_RESLIST;
            }
            else {
                break;
            }
        }

        listener = listener->next;
    }

    return (int)pos;
}

void gcoap_register_listener(gcoap_listener_t *listener)
{
    /* Add the listener to the end of the linked list. */
    gcoap_listener_t *_last = &_listeners;
    while (_last->next) {
        _last = _last->next;
    }

    listener->next = NULL;
    if (!listener->link_encoder) {
        listener->link_encoder = gcoap_encode_link;
    }
    _last->next = listener;
}

/*
 * Handler for /.well-known/core. Lists registered handlers, except for
 * /.well-known/core itself.
 */
static ssize_t _well_known_core_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                                        void *ctx)
{
    (void)ctx;

    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    coap_opt_add_format(pdu, COAP_FORMAT_LINK);
    ssize_t plen = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

    plen += gcoap_get_resource_list(pdu->payload, (size_t)pdu->payload_len,
                                    COAP_FORMAT_LINK);
    return plen;
}

/** @} */
