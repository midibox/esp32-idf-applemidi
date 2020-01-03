/*
 * Interface Layer for Apple MIDI Driver
 * LWIP Variant
 *
 * See README.md for usage hints
 *
 * =============================================================================
 *
 * MIT License
 *
 * Copyright (c) 2020 Thorsten Klose (tk@midibox.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * =============================================================================
 */

#ifndef _APPLEMIDI_IF_H
#define _APPLEMIDI_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

#include "applemidi.h"


#ifndef APPLEMIDI_IF_LOG_TAG
#define APPLEMIDI_IF_LOG_TAG "[APPLEMIDI_IF] "
#endif

#ifndef APPLEMIDI_IF_MAX_PACKET_SIZE
#define APPLEMIDI_IF_MAX_PACKET_SIZE 1472 /* based on Ethernet MTU of 1500 */
#endif

#ifndef APPLEMIDI_IF_ENABLE_CONSOLE
#define APPLEMIDI_IF_ENABLE_CONSOLE 1
#endif

/**
 * @brief Initializes the UDP sockets (we assume that the network interface is already configured by the application)
 *
 * @return < 0 on errors
 */
extern int32_t applemidi_if_init(uint16_t port);

/**
 * @brief De-Initializes the UDP sockets
 *
 * @return < 0 on errors
 */
extern int32_t applemidi_if_deinit(void);

/**
 * @brief Sends a UDP Datagram
 *
 * @return < 0 on errors
 */
extern int32_t applemidi_if_send_udp_datagram(uint8_t *ip_addr, uint16_t port, uint8_t *tx_data, size_t tx_len);

/**
 * @brief Handles incoming UDP datagrams, should be periodically called from a task
 *
 * @return < 0 on errors
 */
extern int32_t applemidi_if_tick(void *_parse_udp_datagram);


#if APPLEMIDI_IF_ENABLE_CONSOLE
/**
 * @brief Register Console Commands
 *
 * @return < 0 on errors
 */
extern void applemidi_if_register_console_commands(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _APPLEMIDI_IF_H */
