/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_wakaama
 * @defgroup    lwm2m_platform Platform adaption for Wakaama package
 * @brief Adaption of Wakaama LWM2M package to RIOT
 * @{
 * @file
 * @brief Definitions and public API for Wakaama adaption layer
 *
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 */

/**
 * @defgroup lwm2m_platform_conf Wakaama LWM2M platform adaption compile configuration
 * @ingroup config
 * @{
 */

#ifndef LWM2M_PLATFORM_H
#define LWM2M_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Size of allocation buffer */
#ifndef LWM2M_TLSF_BUFFER
#define LWM2M_TLSF_BUFFER     (10240 / sizeof(uint32_t))
#endif

/** @} */

/**
 * @brief Initializes the platform adaption for Wakaama LWM2M
 */
void lwm2m_platform_init(void);


#ifdef __cplusplus
}
#endif

#endif /* LWM2M_PLATFORM_H */
/** @} */
