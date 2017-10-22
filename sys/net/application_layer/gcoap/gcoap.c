/*
 * Copyright (c) 2015-2017 Ken Bannister. All rights reserved.
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
 * @brief       GNRC's implementation of CoAP protocol
 *
 * Runs a thread (_pid) to manage request/response messaging.
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 */

#include <errno.h>
#include "net/gcoap.h"
#include "random.h"
#include "thread.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/* Internal functions */
static void *_event_loop(void *arg);
static void _listen(sock_udp_t *sock);
static ssize_t _well_known_core_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len);
static ssize_t _write_options(coap_pkt_t *pdu, uint8_t *buf, size_t len);
static size_t _handle_req(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                         sock_udp_ep_t *remote);
static ssize_t _finish_pdu(coap_pkt_t *pdu, uint8_t *buf, size_t len);
static void _expire_request(gcoap_request_memo_t *memo);
static void _find_req_memo(gcoap_request_memo_t **memo_ptr, coap_pkt_t *pdu,
                           int match_type);
static void _find_resource(coap_pkt_t *pdu, coap_resource_t **resource_ptr,
                                            gcoap_listener_t **listener_ptr);
static int _find_observer(sock_udp_ep_t **observer, const sock_udp_ep_t *remote);
static int _find_obs_memo(gcoap_observe_memo_t **memo, const sock_udp_ep_t *remote,
                          uint8_t *token, int token_len);
static void _find_obs_memo_resource(gcoap_observe_memo_t **memo,
                                   const coap_resource_t *resource);
static void _clear_obs_memo(gcoap_observe_memo_t *memo,
                            const sock_udp_ep_t *remote);

/* Internal variables */
const coap_resource_t _default_resources[] = {
    { "/.well-known/core", COAP_GET, _well_known_core_handler },
};

static gcoap_listener_t _default_listener = {
    (coap_resource_t *)&_default_resources[0],
    sizeof(_default_resources) / sizeof(_default_resources[0]),
    NULL
};

static gcoap_state_t _coap_state = {
    .listeners   = &_default_listener,
};

static kernel_pid_t _pid = KERNEL_PID_UNDEF;
static char _msg_stack[GCOAP_STACK_SIZE];
static sock_udp_t _sock;


/* Event/Message loop for gcoap _pid thread. */
static void *_event_loop(void *arg)
{
    msg_t msg_rcvd, msg_queue[GCOAP_MSG_QUEUE_SIZE];
    (void)arg;

    msg_init_queue(msg_queue, GCOAP_MSG_QUEUE_SIZE);

    sock_udp_ep_t local;
    memset(&local, 0, sizeof(sock_udp_ep_t));
    local.family = AF_INET6;
    local.netif  = SOCK_ADDR_ANY_NETIF;
    local.port   = GCOAP_PORT;

    int res = sock_udp_create(&_sock, &local, NULL, 0);
    if (res < 0) {
        DEBUG("gcoap: cannot create sock: %d\n", res);
        return 0;
    }

    while(1) {
        res = msg_try_receive(&msg_rcvd);

        if (res > 0) {
            switch (msg_rcvd.type) {
            case GCOAP_MSG_TYPE_TIMEOUT: {
                gcoap_request_memo_t *memo = (gcoap_request_memo_t *)msg_rcvd.content.ptr;

                /* no retries remaining */
                if (memo->send_limit == GCOAP_SEND_LIMIT_NON
                        || memo->send_limit == 0) {
                    _expire_request(memo);
                }
                /* retries remaining; double timeout and resend */
                else {
                    /* decrement send limit, and add 1 to advance the timeout */
                    unsigned i       = COAP_MAX_RETRANSMIT - memo->send_limit-- + 1;
                    uint32_t timeout = ((uint32_t)COAP_ACK_TIMEOUT << i) * US_PER_SEC;
                    timeout = random_uint32_range(timeout, timeout * COAP_RANDOM_FACTOR);

                    size_t res = sock_udp_send(&_sock, memo->msg.data.pdu_buf,
                                               memo->msg.data.pdu_len,
                                               &memo->msg.data.remote_ep);
                    if (res) {
                        xtimer_set_msg(&memo->response_timer, timeout,
                                       &memo->timeout_msg, _pid);
                    }
                    else {
                        DEBUG("gcoap: sock resend failed: %d\n", res);
                        _expire_request(memo);
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        _listen(&_sock);
    }

    return 0;
}

/* Listen for an incoming CoAP message. */
static void _listen(sock_udp_t *sock)
{
    coap_pkt_t pdu;
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    sock_udp_ep_t remote;
    gcoap_request_memo_t *memo = NULL;
    uint8_t open_reqs = gcoap_op_state();

    /* We expect a -EINTR response here when unlimited waiting (SOCK_NO_TIMEOUT)
     * is interrupted when sending a message in gcoap_req_send2(). While a
     * request is outstanding, sock_udp_recv() is called here with limited
     * waiting so the request's timeout can be handled in a timely manner in
     * _event_loop(). */
    ssize_t res = sock_udp_recv(sock, buf, sizeof(buf),
                                open_reqs > 0 ? GCOAP_RECV_TIMEOUT : SOCK_NO_TIMEOUT,
                                &remote);
    if (res <= 0) {
#if ENABLE_DEBUG
        if (res < 0 && res != -ETIMEDOUT) {
            DEBUG("gcoap: udp recv failure: %d\n", res);
        }
#endif
        return;
    }

    res = coap_parse(&pdu, buf, res);
    if (res < 0) {
        DEBUG("gcoap: parse failure: %d\n", res);
        /* If a response, can't clear memo, but it will timeout later. */
        return;
    }

    if (pdu.hdr->code == COAP_CODE_EMPTY) {
        _find_req_memo(&memo, &pdu, GCOAP_FIND_REQ_MSGID);
        if (memo) {
            /* empty ACK for confirmable request */
            if ((coap_get_type(&pdu) == COAP_TYPE_ACK
                    || coap_get_type(&pdu) == COAP_TYPE_RST)
                    && memo->send_limit >= 0) {
                /* For an observe notification ACK from the client, no further
                 * response expected. Clear the request memo. */
                gcoap_observe_memo_t *obs_memo = NULL;
                coap_hdr_t *req_hdr = (coap_hdr_t *)memo->msg.data.pdu_buf;
                unsigned token_len  = req_hdr->ver_t_tkl & 0xf;
                uint8_t *token      = token_len > 0 ? &req_hdr->data[0] : NULL;
                _find_obs_memo(&obs_memo, &remote, token, token_len);
                if (obs_memo) {
                    xtimer_remove(&memo->response_timer);
                    if (coap_get_type(&pdu) == COAP_TYPE_RST) {
                        DEBUG("gcoap: Deregistering observer for: %s\n",
                              obs_memo->resource->path);
                        _clear_obs_memo(obs_memo, &memo->msg.data.remote_ep);
                    }
                    *memo->msg.data.pdu_buf = 0;    /* clear resend PDU buffer */
                    memo->state = GCOAP_MEMO_UNUSED;
                }
                else {
                    /* For an immediate ACK from a server, we expect a separate
                     * response later. Not supported yet. */
                    DEBUG("gcoap: separate response not supported yet\n");
                }
            }
        }
        else {
            DEBUG("gcoap: can't match empty message to request\n");
        }
        return;
    }

    /* validate class and type for incoming */
    switch (coap_get_code_class(&pdu)) {

    /* incoming request */
    case COAP_CLASS_REQ:
        if (coap_get_type(&pdu) == COAP_TYPE_NON
                || coap_get_type(&pdu) == COAP_TYPE_CON) {
            size_t pdu_len = _handle_req(&pdu, buf, sizeof(buf), &remote);
            if (pdu_len > 0) {
                sock_udp_send(sock, buf, pdu_len, &remote);
            }
        }
        else {
            DEBUG("gcoap: illegal request type: %u\n", coap_get_type(&pdu));
        }
        break;

    /* incoming response */
    case COAP_CLASS_SUCCESS:
    case COAP_CLASS_CLIENT_FAILURE:
    case COAP_CLASS_SERVER_FAILURE:
        _find_req_memo(&memo, &pdu, GCOAP_FIND_REQ_TOKEN);
        if (memo) {
            switch (coap_get_type(&pdu)) {
            case COAP_TYPE_NON:
            case COAP_TYPE_ACK:
                xtimer_remove(&memo->response_timer);
                memo->state = GCOAP_MEMO_RESP;
                if (memo->resp_handler) {
                    memo->resp_handler(memo->state, &pdu, &remote);
                }

#if !GCOAP_SEND_WAIT_FOR_RESPONSE
                if (memo->send_limit >= 0) {        /* if confirmable */
                    *memo->msg.data.pdu_buf = 0;    /* clear resend PDU buffer */
                }
                memo->state = GCOAP_MEMO_UNUSED;
#else
                thread_wakeup(memo->waiting_thread);
#endif
                break;
            case COAP_TYPE_CON:
                DEBUG("gcoap: separate CON response not handled yet\n");
                break;
            default:
                DEBUG("gcoap: illegal response type: %u\n", coap_get_type(&pdu));
                break;
            }
        }
        else {
            DEBUG("gcoap: msg not found for ID: %u\n", coap_get_id(&pdu));
        }
        break;
    default:
        DEBUG("gcoap: illegal code class: %u\n", coap_get_code_class(&pdu));
    }
}

/*
 * Main request handler: generates response PDU in the provided buffer.
 *
 * Caller must finish the PDU and send it.
 *
 * return length of response pdu, or < 0 if can't handle
 */
static size_t _handle_req(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                         sock_udp_ep_t *remote)
{
    coap_resource_t *resource;
    gcoap_listener_t *listener;
    sock_udp_ep_t *observer    = NULL;
    gcoap_observe_memo_t *memo = NULL;
    gcoap_observe_memo_t *resource_memo = NULL;

    _find_resource(pdu, &resource, &listener);
    if (resource == NULL) {
        return gcoap_response(pdu, buf, len, COAP_CODE_PATH_NOT_FOUND);
    }
    else {
        /* used below to ensure a memo not already recorded for the resource */
        _find_obs_memo_resource(&resource_memo, resource);
    }

    if (coap_get_observe(pdu) == COAP_OBS_REGISTER) {
        int empty_slot = _find_obs_memo(&memo, remote, pdu->token,
                                        coap_get_token_len(pdu));
        /* record observe memo */
        if (memo == NULL) {
            if (empty_slot >= 0 && resource_memo == NULL) {

                int obs_slot = _find_observer(&observer, remote);
                /* cache new observer */
                if (observer == NULL) {
                    if (obs_slot >= 0) {
                        observer = &_coap_state.observers[obs_slot];
                        memcpy(observer, remote, sizeof(sock_udp_ep_t));
                    } else {
                        DEBUG("gcoap: can't register observer\n");
                    }
                }
                if (observer != NULL) {
                    memo = &_coap_state.observe_memos[empty_slot];
                }
            }
            if (memo == NULL) {
                coap_clear_observe(pdu);
                DEBUG("gcoap: can't register observe memo\n");
            }
        }
        if (memo != NULL) {
            memo->observer  = observer;
            memo->resource  = resource;
            memo->token_len = coap_get_token_len(pdu);
            if (memo->token_len) {
                memcpy(&memo->token[0], pdu->token, memo->token_len);
            }
            DEBUG("gcoap: Registered observer for: %s\n", memo->resource->path);
            /* generate initial notification value */
            uint32_t now       = xtimer_now_usec();
            pdu->observe_value = (now >> GCOAP_OBS_TICK_EXPONENT) & 0xFFFFFF;
        }

    } else if (coap_get_observe(pdu) == COAP_OBS_DEREGISTER) {
        _find_obs_memo(&memo, remote, pdu->token, coap_get_token_len(pdu));
        if (memo != NULL) {
            DEBUG("gcoap: Deregistering observer for: %s\n", memo->resource->path);
            _clear_obs_memo(memo, remote);
        }
        coap_clear_observe(pdu);

    } else if (coap_has_observe(pdu)) {
        /* bogus request; don't respond */
        DEBUG("gcoap: Observe value unexpected: %" PRIu32 "\n", coap_get_observe(pdu));
        return -1;
    }

    ssize_t pdu_len = resource->handler(pdu, buf, len);
    if (pdu_len < 0) {
        pdu_len = gcoap_response(pdu, buf, len,
                                 COAP_CODE_INTERNAL_SERVER_ERROR);
    }
    return pdu_len;
}

/*
 * Searches listener registrations for the resource matching the path in a PDU.
 *
 * param[out] resource_ptr -- found resource
 * param[out] listener_ptr -- listener for found resource
 */
static void _find_resource(coap_pkt_t *pdu, coap_resource_t **resource_ptr,
                                            gcoap_listener_t **listener_ptr)
{
    unsigned method_flag = coap_method2flag(coap_get_code_detail(pdu));

    /* Find path for CoAP msg among listener resources and execute callback. */
    gcoap_listener_t *listener = _coap_state.listeners;
    while (listener) {
        coap_resource_t *resource = listener->resources;
        for (size_t i = 0; i < listener->resources_len; i++) {
            if (i) {
                resource++;
            }
            if (! (resource->methods & method_flag)) {
                continue;
            }

            int res = strcmp((char *)&pdu->url[0], resource->path);
            if (res > 0) {
                continue;
            }
            else if (res < 0) {
                /* resources expected in alphabetical order */
                break;
            }
            else {
                *resource_ptr = resource;
                *listener_ptr = listener;
                return;
            }
        }
        listener = listener->next;
    }
    /* resource not found */
    *resource_ptr = NULL;
    *listener_ptr = NULL;
}

/*
 * Finishes handling a PDU -- write options and reposition payload.
 *
 * Returns the size of the PDU within the buffer, or < 0 on error.
 */
static ssize_t _finish_pdu(coap_pkt_t *pdu, uint8_t *buf, size_t len)
{
    ssize_t hdr_len = _write_options(pdu, buf, len);
    DEBUG("gcoap: header length: %i\n", (int)hdr_len);

    if (hdr_len > 0) {
        /* move payload over unused space after options */
        if (pdu->payload_len) {
            memmove(buf + hdr_len, pdu->payload, pdu->payload_len);
        }

        return hdr_len + pdu->payload_len;
    }
    else {
        return -1;      /* generic failure code */
    }
}

/*
 * Finds the memo for an outstanding request within the _coap_state.open_reqs
 * array. Matches on token or message ID based on match_type parameter.
 *
 * memo_ptr[out] -- Registered request memo, or NULL if not found
 * src_pdu[in] -- PDU for token to match
 * match_type[in] -- GCOAP_FIND_REQ_TOKEN or GCOAP_FIND_REQ_MSGID
 */
static void _find_req_memo(gcoap_request_memo_t **memo_ptr, coap_pkt_t *src_pdu,
                           int match_type)
{
    *memo_ptr = NULL;
    /* no need to initialize struct; we only care about buffer contents below */
    coap_pkt_t memo_pdu_data;
    coap_pkt_t *memo_pdu = &memo_pdu_data;
    unsigned cmplen      = coap_get_token_len(src_pdu);

    for (int i = 0; i < GCOAP_REQ_WAITING_MAX; i++) {
        if (_coap_state.open_reqs[i].state == GCOAP_MEMO_UNUSED)
            continue;

        gcoap_request_memo_t *memo = &_coap_state.open_reqs[i];
        if (memo->send_limit == GCOAP_SEND_LIMIT_NON) {
            memo_pdu->hdr = (coap_hdr_t *) &memo->msg.hdr_buf[0];
        }
        else {
            memo_pdu->hdr = (coap_hdr_t *) memo->msg.data.pdu_buf;
        }

        if (match_type == GCOAP_FIND_REQ_MSGID) {
            if (src_pdu->hdr->id == memo_pdu->hdr->id) {
                *memo_ptr = memo;
            }
        }
        else if (coap_get_token_len(memo_pdu) == cmplen) {
            if (cmplen) {
                memo_pdu->token = &memo_pdu->hdr->data[0];
                if (memcmp(src_pdu->token, memo_pdu->token, cmplen) == 0) {
                    *memo_ptr = memo;
                    break;
                }
            }
            /* if no token, just match the first memo */
            else {
                *memo_ptr = memo;
                break;
            }
        }
    }
}

/*
 * Calls handler callback on receipt of a timeout message. Also clears observe
 * memo, if any, if the request was confirmable.
 */
static void _expire_request(gcoap_request_memo_t *memo)
{
    DEBUG("coap: received timeout message\n");
    if (memo->state == GCOAP_MEMO_WAIT) {
        memo->state = GCOAP_MEMO_TIMEOUT;
        /* Pass response to handler */
        if (memo->resp_handler) {
            coap_pkt_t req;
            if (memo->send_limit == GCOAP_SEND_LIMIT_NON) {
                req.hdr = (coap_hdr_t *)&memo->msg.hdr_buf[0];   /* for reference */
            }
            else {
                req.hdr = (coap_hdr_t *)memo->msg.data.pdu_buf;
            }
            memo->resp_handler(memo->state, &req, NULL);
        }
        if (memo->send_limit != GCOAP_SEND_LIMIT_NON) {
            gcoap_observe_memo_t *obs_memo = NULL;
            coap_hdr_t *req_hdr = (coap_hdr_t *)memo->msg.data.pdu_buf;
            unsigned token_len  = req_hdr->ver_t_tkl & 0xf;
            uint8_t *token      = token_len > 0 ? &req_hdr->data[0] : NULL;

            _find_obs_memo(&obs_memo, &memo->msg.data.remote_ep, token, token_len);
            if (obs_memo != NULL) {
                DEBUG("gcoap: Deregistering observer for: %s\n",
                      obs_memo->resource->path);
                _clear_obs_memo(obs_memo, &memo->msg.data.remote_ep);
            }
#if !GCOAP_SEND_WAIT_FOR_RESPONSE
            *memo->msg.data.pdu_buf = 0;    /* clear resend buffer */
#endif
        }

#if !GCOAP_SEND_WAIT_FOR_RESPONSE
        memo->state = GCOAP_MEMO_UNUSED;
#else
        thread_wakeup(memo->waiting_thread);
#endif
    }
    else {
        /* Response already handled; timeout must have fired while response */
        /* was in queue. */
    }
}

/*
 * Handler for /.well-known/core. Lists registered handlers, except for
 * /.well-known/core itself.
 */
static ssize_t _well_known_core_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len)
{
   /* write header */
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    int plen = gcoap_get_resource_list(pdu->payload, (size_t)pdu->payload_len,
                                      COAP_FORMAT_LINK);
    /* response content */
    return gcoap_finish(pdu, (size_t)plen, COAP_FORMAT_LINK);
}

/*
 * Creates CoAP options and sets payload marker, if any.
 *
 * Returns length of header + options, or -EINVAL on illegal path.
 */
static ssize_t _write_options(coap_pkt_t *pdu, uint8_t *buf, size_t len)
{
    uint8_t last_optnum = 0;
    (void)len;

    uint8_t *bufpos = buf + coap_get_total_hdr_len(pdu);  /* position for write */

    /* Observe for notification or registration response */
    if (coap_get_code_class(pdu) == COAP_CLASS_SUCCESS && coap_has_observe(pdu)) {
        uint32_t nval  = htonl(pdu->observe_value);
        uint8_t *nbyte = (uint8_t *)&nval;
        unsigned i;
        /* find address of non-zero MSB; max 3 bytes */
        for (i = 1; i < 4; i++) {
            if (*(nbyte+i) > 0) {
                break;
            }
        }
        bufpos += coap_put_option(bufpos, last_optnum, COAP_OPT_OBSERVE,
                                                       nbyte+i, 4-i);
        last_optnum = COAP_OPT_OBSERVE;
    }

    /* Uri-Path for request */
    if (coap_get_code_class(pdu) == COAP_CLASS_REQ) {
        size_t url_len = strlen((char *)pdu->url);
        if (url_len) {
            if (pdu->url[0] != '/') {
                DEBUG("gcoap: _write_options: path does not start with '/'\n");
                return -EINVAL;
            }
            bufpos += coap_put_option_uri(bufpos, last_optnum, (char *)pdu->url,
                                          COAP_OPT_URI_PATH);
            last_optnum = COAP_OPT_URI_PATH;
        }
    }

    /* Content-Format */
    if (pdu->content_type != COAP_FORMAT_NONE) {
        bufpos += coap_put_option_ct(bufpos, last_optnum, pdu->content_type);
        last_optnum = COAP_OPT_CONTENT_FORMAT;
    }

    /* Uri-query for requests */
    if (coap_get_code_class(pdu) == COAP_CLASS_REQ) {
        bufpos += coap_put_option_uri(bufpos, last_optnum, (char *)pdu->qs,
                                      COAP_OPT_URI_QUERY);
        /* uncomment when further options are added below ... */
        /* last_optnum = COAP_OPT_URI_QUERY; */
    }

    /* write payload marker */
    if (pdu->payload_len) {
        *bufpos++ = GCOAP_PAYLOAD_MARKER;
    }
    return bufpos - buf;
}

/*
 * Find registered observer for a remote address and port.
 *
 * observer[out] -- Registered observer, or NULL if not found
 * remote[in] -- Endpoint to match
 *
 * return Index of empty slot, suitable for registering new observer; or -1
 *        if no empty slots. Undefined if observer found.
 */
static int _find_observer(sock_udp_ep_t **observer, const sock_udp_ep_t *remote)
{
    int empty_slot = -1;
    *observer      = NULL;
    for (unsigned i = 0; i < GCOAP_OBS_CLIENTS_MAX; i++) {
        unsigned cmplen = 0;

        if (_coap_state.observers[i].family == AF_UNSPEC) {
            cmplen     = 0;
            empty_slot = i;
        }
        else if (_coap_state.observers[i].family == AF_INET6) {
            cmplen = 16;
        }
        else {
            cmplen = 4;
        }
        if (cmplen &&
                memcmp(&_coap_state.observers[i].addr.ipv6[0], &remote->addr.ipv6[0],
                                                               cmplen) == 0
                && _coap_state.observers[i].port == remote->port) {

            *observer = &_coap_state.observers[i];
            break;
        }
    }
    return empty_slot;
}

/*
 * Find registered observe memo for a remote address and token.
 *
 * memo[out] -- Registered observe memo, or NULL if not found
 * remote[in] -- Endpoint for address to match
 * token[in] -- Token to match; NULL if zero-length
 * token_len[in] -- Length of token, or -1 to match only on remote address
 *
 * return Index of empty slot, suitable for registering new memo; or -1 if no
 *        empty slots. Undefined if memo found.
 */
static int _find_obs_memo(gcoap_observe_memo_t **memo, const sock_udp_ep_t *remote,
                          uint8_t *token, int token_len)
{
    int empty_slot = -1;
    *memo          = NULL;
    assert(token_len >= -1);

    sock_udp_ep_t *remote_observer = NULL;
    _find_observer(&remote_observer, remote);

    for (unsigned i = 0; i < GCOAP_OBS_REGISTRATIONS_MAX; i++) {
        if (_coap_state.observe_memos[i].observer == NULL) {
            empty_slot = i;
            continue;
        }

        if (_coap_state.observe_memos[i].observer == remote_observer) {
            if (token_len == -1) {
                *memo = &_coap_state.observe_memos[i];
                break;
            }

            if ((int)_coap_state.observe_memos[i].token_len == token_len) {
                if (!token_len ||
                        memcmp(&_coap_state.observe_memos[i].token[0], token,
                               token_len) == 0) {
                    *memo = &_coap_state.observe_memos[i];
                    break;
                }
            }
        }
    }
    return empty_slot;
}

/*
 * Find registered observe memo for a resource.
 *
 * memo[out] -- Registered observe memo, or NULL if not found
 * resource[in] -- Resource to match
 */
static void _find_obs_memo_resource(gcoap_observe_memo_t **memo,
                                   const coap_resource_t *resource)
{
    *memo = NULL;
    for (int i = 0; i < GCOAP_OBS_REGISTRATIONS_MAX; i++) {
        if (_coap_state.observe_memos[i].observer != NULL
                && _coap_state.observe_memos[i].resource == resource) {
            *memo = &_coap_state.observe_memos[i];
            break;
        }
    }
}

/* Clear memo, and clear observer if no other memos */
static void _clear_obs_memo(gcoap_observe_memo_t *memo,
                            const sock_udp_ep_t *remote)
{
    assert(memo != NULL);
    sock_udp_ep_t *observer = NULL;

    memo->observer = NULL;
    memo           = NULL;
    _find_obs_memo(&memo, remote, NULL, -1);
    if (memo == NULL) {
        _find_observer(&observer, remote);
        if (observer != NULL) {
            observer->family = AF_UNSPEC;
        }
    }
}

/*
 * gcoap interface functions
 */

kernel_pid_t gcoap_init(void)
{
    if (_pid != KERNEL_PID_UNDEF) {
        return -EEXIST;
    }
    _pid = thread_create(_msg_stack, sizeof(_msg_stack), THREAD_PRIORITY_MAIN - 1,
                            THREAD_CREATE_STACKTEST, _event_loop, NULL, "coap");

    mutex_init(&_coap_state.lock);
    /* Blank lists so we know if an entry is available. */
    memset(&_coap_state.open_reqs[0], 0, sizeof(_coap_state.open_reqs));
    memset(&_coap_state.observers[0], 0, sizeof(_coap_state.observers));
    memset(&_coap_state.observe_memos[0], 0, sizeof(_coap_state.observe_memos));
#if GCOAP_RESEND_BUFS_MAX
    memset(&_coap_state.resend_bufs[0], 0, sizeof(_coap_state.resend_bufs));
#endif
    /* randomize initial value */
    atomic_init(&_coap_state.next_message_id, (unsigned)random_uint32());

    return _pid;
}

void gcoap_register_listener(gcoap_listener_t *listener)
{
    /* Add the listener to the end of the linked list. */
    gcoap_listener_t *_last = _coap_state.listeners;
    while (_last->next) {
        _last = _last->next;
    }

    listener->next = NULL;
    _last->next = listener;
}

int gcoap_req_init_opts(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                        const gcoap_send_opts_t *opts) {
    (void)len;

    pdu->hdr = (coap_hdr_t *)buf;
    memset(pdu->url, 0, NANOCOAP_URL_MAX);
    memset(pdu->qs, 0, NANOCOAP_QS_MAX);

    /* generate token */
#if GCOAP_TOKENLEN
    uint8_t token[GCOAP_TOKENLEN];
    for (size_t i = 0; i < GCOAP_TOKENLEN; i += 4) {
        uint32_t rand = random_uint32();
        memcpy(&token[i],
               &rand,
               (GCOAP_TOKENLEN - i >= 4) ? 4 : GCOAP_TOKENLEN - i);
    }
    uint16_t msgid = (uint16_t)atomic_fetch_add(&_coap_state.next_message_id, 1);
    ssize_t hdrlen = coap_build_hdr(pdu->hdr, opts->msg_type, &token[0],
                                    GCOAP_TOKENLEN, opts->msg_code, msgid);
#else
    uint16_t msgid = (uint16_t)atomic_fetch_add(&_coap_state.next_message_id, 1);
    ssize_t hdrlen = coap_build_hdr(pdu->hdr, opts->msg_type, NULL,
                                    GCOAP_TOKENLEN, opts->msg_code, msgid);
#endif

    if (hdrlen > 0) {
        /* Reserve some space between the header and payload to write options later */
        pdu->payload      = buf + coap_get_total_hdr_len(pdu) + strlen(opts->req_path)
                                                              + GCOAP_REQ_OPTIONS_BUF;
        /* Payload length really zero at this point, but we set this to the available
         * length in the buffer. Allows us to reconstruct buffer length later. */
        pdu->payload_len  = len - (pdu->payload - buf);
        pdu->content_type = COAP_FORMAT_NONE;

        memcpy(&pdu->url[0], opts->req_path, strlen(opts->req_path));
        return 0;
    }
    else {
        /* reason for negative hdrlen is not defined, so we also are vague */
        return -1;
    }
}

int gcoap_req_init(coap_pkt_t *pdu, uint8_t *buf, size_t len, unsigned code,
                                                              char *path) {
    gcoap_send_opts_t opts = {
        .msg_code = code,
        .req_path = path,
        .msg_type = GCOAP_REQ_DEFAULT_MSG_TYPE
    };
    return gcoap_req_init_opts(pdu, buf, len, &opts);
}

ssize_t gcoap_finish(coap_pkt_t *pdu, size_t payload_len, unsigned format)
{
    /* reconstruct full PDU buffer length */
    size_t len = pdu->payload_len + (pdu->payload - (uint8_t *)pdu->hdr);

    pdu->content_type = format;
    pdu->payload_len  = payload_len;
    return _finish_pdu(pdu, (uint8_t *)pdu->hdr, len);
}

size_t gcoap_req_send(const uint8_t *buf, size_t len, const ipv6_addr_t *addr,
                      uint16_t port, gcoap_resp_handler_t resp_handler)
{
    sock_udp_ep_t remote;

    remote.family = AF_INET6;
    remote.netif  = SOCK_ADDR_ANY_NETIF;
    remote.port   = port;

    memcpy(&remote.addr.ipv6[0], &addr->u8[0], sizeof(addr->u8));

    return gcoap_req_send2(buf, len, &remote, resp_handler);
}

size_t gcoap_req_send2(const uint8_t *buf, size_t len,
                       const sock_udp_ep_t *remote,
                       gcoap_resp_handler_t resp_handler)
{
    gcoap_request_memo_t *memo = NULL;
    assert(remote != NULL);

    /* Find empty slot in list of open requests. */
    mutex_lock(&_coap_state.lock);
    for (int i = 0; i < GCOAP_REQ_WAITING_MAX; i++) {
        if (_coap_state.open_reqs[i].state == GCOAP_MEMO_UNUSED) {
            memo = &_coap_state.open_reqs[i];
            memo->state = GCOAP_MEMO_WAIT;
            break;
        }
    }
    if (!memo) {
        mutex_unlock(&_coap_state.lock);
        DEBUG("gcoap: dropping request; no space for response tracking\n");
        return 0;
    }

    unsigned msg_type  = (*buf & 0x30) >> 4;
    uint32_t timeout   = 0;
    memo->resp_handler = resp_handler;

#if GCOAP_SEND_WAIT_FOR_RESPONSE
    memo->waiting_thread = thread_getpid();
    if (memo->waiting_thread == _pid) {
        memo->state = GCOAP_MEMO_UNUSED;
        DEBUG("gcoap: can't send; would put gcoap thread to sleep\n");
        return 0;
    }
#endif

    switch (msg_type) {
    case COAP_TYPE_CON:
#if GCOAP_RESEND_BUFS_MAX
        /* copy buf to resend_bufs record */
        memo->msg.data.pdu_buf = NULL;
        for (int i = 0; i < GCOAP_RESEND_BUFS_MAX; i++) {
            if (!_coap_state.resend_bufs[i*GCOAP_PDU_BUF_SIZE]) {
                memo->msg.data.pdu_buf = &_coap_state.resend_bufs[i*GCOAP_PDU_BUF_SIZE];
                memcpy(memo->msg.data.pdu_buf, buf, GCOAP_PDU_BUF_SIZE);
                memo->msg.data.pdu_len = len;
            }
        }
#else
        assert(GCOAP_SEND_WAIT_FOR_RESPONSE);
        memo->msg.data.pdu_buf = (uint8_t *)buf;
        memo->msg.data.pdu_len = len;
#endif
        if (memo->msg.data.pdu_buf) {
            memcpy(&memo->msg.data.remote_ep, remote, sizeof(sock_udp_ep_t));
            memo->send_limit = COAP_MAX_RETRANSMIT;
            timeout = (uint32_t)COAP_ACK_TIMEOUT * US_PER_SEC;
            timeout = random_uint32_range(timeout, timeout * COAP_RANDOM_FACTOR);
        }
        else {
            memo->state = GCOAP_MEMO_UNUSED;
            DEBUG("gcoap: no space for PDU in resend bufs\n");
        }
        break;

    case COAP_TYPE_NON:
        memo->send_limit = GCOAP_SEND_LIMIT_NON;
        memcpy(&memo->msg.hdr_buf[0], buf, GCOAP_HEADER_MAXLEN);
        timeout = GCOAP_NON_TIMEOUT;
        break;
    default:
        memo->state = GCOAP_MEMO_UNUSED;
        DEBUG("gcoap: illegal msg type %u\n", msg_type);
        break;
    }
    mutex_unlock(&_coap_state.lock);
    if (memo->state == GCOAP_MEMO_UNUSED) {
        return 0;
    }

    /* Memos complete; send msg and start timer */
    size_t res = sock_udp_send(&_sock, buf, len, remote);

    if (res && timeout > 0) {     /* timeout may be zero for non-confirmable */
        /* We assume gcoap_req_send2() is called on some thread other than
         * gcoap's. First, put a message in the mbox for the sock udp object,
         * which will interrupt listening on the gcoap thread. (When there are
         * no outstanding requests, gcoap blocks indefinitely in _listen() at
         * sock_udp_recv().) While the message sent here is outstanding, the
         * sock_udp_recv() call will be set to a short timeout so the request
         * timer below, also on the gcoap thread, is processed in a timely
         * manner. */
        msg_t mbox_msg;
        mbox_msg.type          = GCOAP_MSG_TYPE_INTR;
        mbox_msg.content.value = 0;
        if (mbox_try_put(&_sock.reg.mbox, &mbox_msg)) {
            /* start response wait timer on the gcoap thread */
            memo->timeout_msg.type        = GCOAP_MSG_TYPE_TIMEOUT;
            memo->timeout_msg.content.ptr = (char *)memo;
            xtimer_set_msg(&memo->response_timer, timeout, &memo->timeout_msg, _pid);

#if (GCOAP_SEND_WAIT_FOR_RESPONSE)
        thread_sleep();
        res = memo->state;
        memo->state = GCOAP_MEMO_UNUSED;
#endif
        }
        else {
            res = 0;
            DEBUG("gcoap: can't wake up mbox; no timeout for msg\n");
        }
    }
    if (!res) {
        if (msg_type == COAP_TYPE_CON) {
            *memo->msg.data.pdu_buf = 0;    /* clear resend buffer */
        }
        memo->state = GCOAP_MEMO_UNUSED;
        DEBUG("gcoap: sock send failed: %d\n", res);
    }
    return res;
}

int gcoap_resp_init(coap_pkt_t *pdu, uint8_t *buf, size_t len, unsigned code)
{
    if (coap_get_type(pdu) == COAP_TYPE_CON) {
        coap_hdr_set_type(pdu->hdr, COAP_TYPE_ACK);
    }
    coap_hdr_set_code(pdu->hdr, code);

    /* Reserve some space between the header and payload to write options later */
    pdu->payload      = buf + coap_get_total_hdr_len(pdu) + GCOAP_RESP_OPTIONS_BUF;
    /* Payload length really zero at this point, but we set this to the available
     * length in the buffer. Allows us to reconstruct buffer length later. */
    pdu->payload_len  = len - (pdu->payload - buf);
    pdu->content_type = COAP_FORMAT_NONE;

    return 0;
}

int gcoap_obs_init_opts(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                        const gcoap_send_opts_t *opts)
{
    gcoap_observe_memo_t *memo = NULL;

    _find_obs_memo_resource(&memo, opts->obs_resource);
    if (memo == NULL) {
        /* Unique return value to specify there is not an observer */
        return GCOAP_OBS_INIT_UNUSED;
    }

    pdu->hdr       = (coap_hdr_t *)buf;
    uint16_t msgid = (uint16_t)atomic_fetch_add(&_coap_state.next_message_id, 1);
    ssize_t hdrlen = coap_build_hdr(pdu->hdr, opts->msg_type, &memo->token[0],
                                    memo->token_len, COAP_CODE_CONTENT, msgid);

    if (hdrlen > 0) {
        uint32_t now       = xtimer_now_usec();
        pdu->observe_value = (now >> GCOAP_OBS_TICK_EXPONENT) & 0xFFFFFF;

        /* Reserve some space between the header and payload to write options later */
        pdu->payload       = buf + coap_get_total_hdr_len(pdu) + GCOAP_OBS_OPTIONS_BUF;
        /* Payload length really zero at this point, but we set this to the available
         * length in the buffer. Allows us to reconstruct buffer length later. */
        pdu->payload_len   = len - (pdu->payload - buf);
        pdu->content_type  = COAP_FORMAT_NONE;

        return GCOAP_OBS_INIT_OK;
    }
    else {
        /* reason for negative hdrlen is not defined, so we also are vague */
        return GCOAP_OBS_INIT_ERR;
    }
}

int gcoap_obs_init(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                   const coap_resource_t *resource)
{
    gcoap_send_opts_t opts = {
        .obs_resource = resource,
        .msg_type     = GCOAP_OBS_DEFAULT_MSG_TYPE
    };
    return gcoap_obs_init_opts(pdu, buf, len, &opts);
}

size_t gcoap_obs_send(const uint8_t *buf, size_t len,
                      const coap_resource_t *resource)
{
    gcoap_observe_memo_t *memo = NULL;

    _find_obs_memo_resource(&memo, resource);
    if (!memo) {
        return 0;
    }

    coap_hdr_t *hdr   = (coap_hdr_t *)buf;
    unsigned msg_type = (hdr->ver_t_tkl & 0x30) >> 4;

    switch (msg_type) {
    case COAP_TYPE_NON:
        return sock_udp_send(&_sock, buf, len, memo->observer);
    case COAP_TYPE_CON:
        return gcoap_req_send2(buf, len, memo->observer, NULL);
    default:
        DEBUG("gcoap: unexpected obs msg type: %u\n", msg_type);
        return 0;
    }
}

uint8_t gcoap_op_state(void)
{
    uint8_t count = 0;
    for (int i = 0; i < GCOAP_REQ_WAITING_MAX; i++) {
        if (_coap_state.open_reqs[i].state != GCOAP_MEMO_UNUSED) {
            count++;
        }
    }
    return count;
}

int gcoap_get_resource_list(void *buf, size_t maxlen, uint8_t cf)
{
    assert(cf == COAP_CT_LINK_FORMAT);
#ifndef DEVELHELP
    (void)cf;
#endif

    /* skip the first listener, gcoap itself (we skip /.well-known/core) */
    gcoap_listener_t *listener = _coap_state.listeners->next;

    char *out = (char *)buf;
    size_t pos = 0;

    /* write payload */
    while (listener) {
        coap_resource_t *resource = listener->resources;

        for (unsigned i = 0; i < listener->resources_len; i++) {
            size_t path_len = strlen(resource->path);
            if (out) {
                /* only add new resources if there is space in the buffer */
                if ((pos + path_len + 3) > maxlen) {
                    break;
                }
                if (pos) {
                    out[pos++] = ',';
                }
                out[pos++] = '<';
                memcpy(&out[pos], resource->path, path_len);
                pos += path_len;
                out[pos++] = '>';
            }
            else {
                pos += (pos) ? 3 : 2;
                pos += path_len;
            }
            ++resource;
        }

        listener = listener->next;
    }

    return (int)pos;
}

int gcoap_add_qstring(coap_pkt_t *pdu, const char *key, const char *val)
{
    size_t qs_len = strlen((char *)pdu->qs);
    size_t key_len = strlen(key);
    size_t val_len = (val) ? (strlen(val) + 1) : 0;

    /* make sure if url_len + the new query string fit into the url buffer */
    if ((qs_len + key_len + val_len + 2) >= NANOCOAP_QS_MAX) {
        return -1;
    }

    pdu->qs[qs_len++] = '&';
    memcpy(&pdu->qs[qs_len], key, key_len);
    qs_len += key_len;
    if (val) {
        pdu->qs[qs_len++] = '=';
        memcpy(&pdu->qs[qs_len], val, val_len);
        qs_len += val_len;
    }
    pdu->qs[qs_len] = '\0';

    return (int)qs_len;
}

/** @} */
