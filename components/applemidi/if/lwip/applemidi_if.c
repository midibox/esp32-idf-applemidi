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

#include "if/lwip/applemidi_if.h"

#include "freertos/FreeRTOS.h"

#include "esp_log.h"

#if APPLEMIDI_IF_ENABLE_CONSOLE
# include "applemidi.h"
# include "esp_console.h"
# include "argtable3/argtable3.h"
#endif

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


//! We need 2 sockets: 1 for control, 1 for data packets
typedef struct {
  int handle;
  struct sockaddr_in socket_addr;
} applemidi_if_socket_t;

typedef enum {
  APPLEMIDI_IF_SOCKET_CONTROL = 0,
  APPLEMIDI_IF_SOCKET_DATA,
  APPLEMIDI_IF_NUM_SOCKETS
} applemidi_if_socket_e;

static applemidi_if_socket_t applemidi_if_socket[APPLEMIDI_IF_NUM_SOCKETS];


////////////////////////////////////////////////////////////////////////////////////////////////////
// Initializes the UDP sockets
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t applemidi_if_init(uint16_t port)
{
  int i;
  applemidi_if_socket_t *s = &applemidi_if_socket[0];
  for(i=0; i<APPLEMIDI_IF_NUM_SOCKETS; ++i, ++s) {
    uint16_t rx_port = port + i;
    memset(s, 0, sizeof(applemidi_if_socket_t));
#if 1
    s->socket_addr.sin_family = AF_INET;
    s->socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s->socket_addr.sin_port = htons(rx_port);
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    //inet_ntoa_r(s->socket_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
    applemidi_socket_addr.sin6_family = AF_INET6;
    applemidi_socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    applemidi_socket_addr.sin6_port = htons(rx_port);
    int addr_family = AF_INET6;
    int ip_protocol = IPPROTO_IPV6;
    //inet6_ntoa_r(s->socket_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

    s->handle = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if( s->handle < 0 ) {
      if( applemidi_get_debug_level() >= 1 ) {
        printf(APPLEMIDI_IF_LOG_TAG "Unable to create socket #%d: errno %d\n", i, errno);
      }
      return -1;
    } else {
      lwip_fcntl(s->handle, F_SETFL, lwip_fcntl(s->handle, F_GETFL, 0) | O_NONBLOCK);

      if( bind(s->handle, (struct sockaddr *)&s->socket_addr, sizeof(s->socket_addr)) < 0 ) {
        close(s->handle);
        s->handle = -1;
        if( applemidi_get_debug_level() >= 1 ) {
          printf(APPLEMIDI_IF_LOG_TAG "Unable to bind socket #%d: errno %d\n", i, errno);
        }
        return -2;
      }
    }
  }

  return 0; // no error
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// De-Initializes the UDP sockets
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t applemidi_if_deinit(void)
{
  int i;
  applemidi_if_socket_t *s = &applemidi_if_socket[0];
  for(i=0; i<APPLEMIDI_IF_NUM_SOCKETS; ++i, ++s) {
    if( s->handle >= 0 ) {
      shutdown(s->handle, 0);
      close(s->handle);
      s->handle = -1;
    }
  }

  return 0; // no error
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sends an UTP datagram
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t applemidi_if_send_udp_datagram(uint8_t *ip_addr, uint16_t port, uint8_t *tx_data, size_t tx_len)
{
  applemidi_if_socket_t *s = (port == htons(applemidi_if_socket[APPLEMIDI_IF_SOCKET_CONTROL].socket_addr.sin_port))
    ? &applemidi_if_socket[APPLEMIDI_IF_SOCKET_CONTROL]
    : &applemidi_if_socket[APPLEMIDI_IF_SOCKET_DATA];

  if( s->handle < 0 ) {
    return -1; // socket not open
  } else {
    struct sockaddr_in tx_socket_addr = s->socket_addr;
    memcpy(&tx_socket_addr.sin_addr.s_addr, ip_addr, 4); // TODO: consider IPv6
    tx_socket_addr.sin_port = htons(port);

    if( applemidi_get_debug_level() >= 2 ) {
      printf(APPLEMIDI_IF_LOG_TAG "sending %d bytes to %d.%d.%d.%d:%d\n",
        tx_len,
        (tx_socket_addr.sin_addr.s_addr & 0x000000ff) >> 0,
        (tx_socket_addr.sin_addr.s_addr & 0x0000ff00) >> 8,
        (tx_socket_addr.sin_addr.s_addr & 0x00ff0000) >> 16,
        (tx_socket_addr.sin_addr.s_addr & 0xff000000) >> 24,
        htons(tx_socket_addr.sin_port));
    }
    if( applemidi_get_debug_level() >= 3 ) {
      esp_log_buffer_hex(APPLEMIDI_IF_LOG_TAG, tx_data, tx_len);
    }

    int err = sendto(s->handle, tx_data, tx_len, 0, (struct sockaddr *)&tx_socket_addr, sizeof(tx_socket_addr));
    if( err < 0 ) {
      if( applemidi_get_debug_level() >= 1 ) {
        printf(APPLEMIDI_IF_LOG_TAG "Failed to send datagram to %d.%d.%d.%d:%d - errno %d\n",
          (tx_socket_addr.sin_addr.s_addr & 0x000000ff) >> 0,
          (tx_socket_addr.sin_addr.s_addr & 0x0000ff00) >> 8,
          (tx_socket_addr.sin_addr.s_addr & 0x00ff0000) >> 16,
          (tx_socket_addr.sin_addr.s_addr & 0xff000000) >> 24,
          htons(tx_socket_addr.sin_port),
          errno);
      }

      return -2; // no packet sent
    }
  }

  return 0; // no error
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles incoming UDP datagrams
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t applemidi_if_tick(void *_parse_udp_datagram)
{
  int32_t (*parse_udp_datagram)(uint8_t *ip_addr, uint16_t port, uint8_t *rx_data, size_t rx_len, uint8_t is_dataport) = _parse_udp_datagram;
  uint8_t rx_data[APPLEMIDI_IF_MAX_PACKET_SIZE];

  int i;
  applemidi_if_socket_t *s = &applemidi_if_socket[0];
  for(i=0; i<APPLEMIDI_IF_NUM_SOCKETS; ++i, ++s) {
    socklen_t socklen = sizeof(s->socket_addr);
    int rx_len = recvfrom(s->handle, rx_data, sizeof(rx_data), 0, (struct sockaddr *)&s->socket_addr, &socklen);

    if( rx_len < 0 ) {
      if( errno != EWOULDBLOCK ) {
        if( applemidi_get_debug_level() >= 1 ) {
          printf(APPLEMIDI_IF_LOG_TAG "recvfrom of %s socket failed: errno %d\n",
            i == APPLEMIDI_IF_SOCKET_CONTROL ? "Control" : "Data",
            errno);
        }
        break;
      }
    } else { // Data received
      if( applemidi_get_debug_level() >= 2 ) {
        printf(APPLEMIDI_IF_LOG_TAG "%s socket received %d bytes from %d.%d.%d.%d:%d\n",
          i == APPLEMIDI_IF_SOCKET_CONTROL ? "Control" : "Data",
          rx_len,
          (s->socket_addr.sin_addr.s_addr & 0x000000ff) >> 0,
          (s->socket_addr.sin_addr.s_addr & 0x0000ff00) >> 8,
          (s->socket_addr.sin_addr.s_addr & 0x00ff0000) >> 16,
          (s->socket_addr.sin_addr.s_addr & 0xff000000) >> 24,
          htons(s->socket_addr.sin_port));
      }
      if( applemidi_get_debug_level() >= 3 ) {
        esp_log_buffer_hex(APPLEMIDI_IF_LOG_TAG, rx_data, rx_len);
      }

      uint8_t is_dataport = i == APPLEMIDI_IF_SOCKET_DATA;
      parse_udp_datagram((uint8_t *)&s->socket_addr.sin_addr.s_addr, htons(s->socket_addr.sin_port), rx_data, rx_len, is_dataport);
    }
  }

  return 0; // no error
}


#if APPLEMIDI_IF_ENABLE_CONSOLE
////////////////////////////////////////////////////////////////////////////////////////////////////
// Optional Console Commands
////////////////////////////////////////////////////////////////////////////////////////////////////
static int cmd_info(int argc, char **argv)
{
  int i;

  printf("Control UDP Socket: %s\n", (applemidi_if_socket[APPLEMIDI_IF_SOCKET_CONTROL].handle >= 0) ? "up" : "down");
  printf("Data UDP Socket: %s\n", (applemidi_if_socket[APPLEMIDI_IF_SOCKET_CONTROL].handle >= 0) ? "up" : "down");
  printf("\n");

  for(i=0; i<APPLEMIDI_MAX_PEERS; ++i) {
    applemidi_peer_t *peer = applemidi_peer_get_info(i);

    printf("Peer #%d (%s)\n", i, (i == 0) ? "local" : "remote");

    printf("  - Connection State: ");
    switch( peer->connection_state ) {
    case APPLEMIDI_CONNECTION_STATE_SLAVE: printf("Slave\n"); break;
    case APPLEMIDI_CONNECTION_STATE_MASTER_CONNECT_CTRL: printf("Master sent Invite over Control Port\n"); break;
    case APPLEMIDI_CONNECTION_STATE_MASTER_CONNECT_DATA: printf("Master sent Invite over Data Port\n"); break;
    case APPLEMIDI_CONNECTION_STATE_MASTER_CONNECTED: printf("Master is connected\n"); break;
    default: printf("Unknown!\n");
    }

    printf("  - SSRC: 0x%08x\n", peer->ssrc);
    printf("  - Name: '%s'\n", peer->name);
    printf("  - IP: %d.%d.%d.%d\n", peer->ip_addr[0], peer->ip_addr[1], peer->ip_addr[2], peer->ip_addr[3]); // TODO: IPv6 support
    printf("  - Control Port: %d\n", peer->control_port);
    printf("  - Data Port: %d\n", peer->data_port);
    printf("  - Last Sequence Number: %d\n", peer->seq_nr);
    printf("  - Packets Sent: %d\n", peer->packets_sent);
    printf("  - Packets Received: %d\n", peer->packets_received);
    printf("  - Packets Loss: %d\n", peer->packets_loss);
    printf("\n");
  }

  printf("Current Debug Level: %d\n", applemidi_get_debug_level());

  return 0;
}


static struct {
  struct arg_str *on_off;
  struct arg_int *verbosity;
  struct arg_end *end;
} applemidi_if_debug_args;

static int cmd_debug(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **)&applemidi_if_debug_args);
  if( nerrors != 0 ) {
      arg_print_errors(stderr, applemidi_if_debug_args.end, argv[0]);
      return 1;
  }

  if( strcasecmp(applemidi_if_debug_args.on_off->sval[0], "on") == 0 ) {
    uint8_t verbosity = 2;
    if( applemidi_if_debug_args.verbosity->count > 0) {
      verbosity = applemidi_if_debug_args.verbosity->ival[0];
    }
    printf("Enabled debug messages with verbosity=%d\n", verbosity);
    applemidi_set_debug_level(verbosity);
  } else {
    printf("Disabled debug messages - they can be re-enabled with 'applemidi_debug on'\n");
    applemidi_set_debug_level(1);
  }

  return 0; // no error
}


static struct {
  struct arg_str *ip;
  struct arg_int *control_port;
  struct arg_int *peer_port;
  struct arg_end *end;
} applemidi_if_start_session_args;

static int cmd_start_session(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **)&applemidi_if_start_session_args);
  if( nerrors != 0 ) {
      arg_print_errors(stderr, applemidi_if_start_session_args.end, argv[0]);
      return 1;
  }

  int control_port;
  if( applemidi_if_start_session_args.control_port->count == 0) {
    control_port = 5004;
  } else {
    control_port = applemidi_if_start_session_args.control_port->ival[0];

    if( control_port < 0 || control_port >= 65536 ) {
      ESP_LOGE(__func__, "Invalid port number, should be within 0..65535!");
      return 1;
    }
  }

  int applemidi_port;
  if( applemidi_if_start_session_args.peer_port->count == 0) {
    applemidi_port = applemidi_search_free_port();
    if( applemidi_port < 0 ) {
      ESP_LOGE(__func__, "No free peer port available!");
      return 1;
    }
  } else {
    applemidi_port = applemidi_if_start_session_args.peer_port->ival[0];

    if( applemidi_port < 1 || applemidi_port >= APPLEMIDI_MAX_PEERS ) {
      ESP_LOGE(__func__, "Invalid peer port number, should be within 1..%d!", APPLEMIDI_MAX_PEERS-1);
      return 1;
    }
  }

  struct sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = inet_addr(applemidi_if_start_session_args.ip->sval[0]);
  uint8_t *ip_addr = (uint8_t *)&dest_addr.sin_addr.s_addr;

  int status = applemidi_start_session(applemidi_port, ip_addr, control_port);
  if( status < 0 ) {
    ESP_LOGE(__func__, "Command failed!");
  }

  return 0; // no error
}

static struct {
  struct arg_int *peer_port;
  struct arg_end *end;
} applemidi_if_end_session_args;

static int cmd_end_session(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **)&applemidi_if_end_session_args);
  if( nerrors != 0 ) {
      arg_print_errors(stderr, applemidi_if_end_session_args.end, argv[0]);
      return 1;
  }

  int applemidi_port;
  if( applemidi_if_end_session_args.peer_port->count == 0) {
    ESP_LOGE(__func__, "Please specify the --peer_port!");
    return 1;
  } else {
    applemidi_port = applemidi_if_end_session_args.peer_port->ival[0];

    if( applemidi_port < 1 || applemidi_port >= APPLEMIDI_MAX_PEERS ) {
      ESP_LOGE(__func__, "Invalid peer port number, should be within 1..%d!", APPLEMIDI_MAX_PEERS-1);
      return 1;
    }
  }

  int status = applemidi_terminate_session(applemidi_port);
  if( status < 0 ) {
    ESP_LOGE(__func__, "Command failed!");
  }

  return 0; // no error
}


void applemidi_if_register_console_commands(void)
{
  {
    const esp_console_cmd_t info_cmd = {
      .command = "applemidi_info",
      .help = "Information about the AppleMIDI Interface",
      .hint = NULL,
      .func = &cmd_info,
      .argtable = NULL
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&info_cmd) );
  }

  {
    applemidi_if_debug_args.on_off = arg_str1(NULL, NULL, "<on/off>", "Enables/Disables debug messages");
    applemidi_if_debug_args.verbosity = arg_int0(NULL, "verbosity", "<level>", "Verbosity Level (0..3)");
    applemidi_if_debug_args.end = arg_end(20);

    const esp_console_cmd_t debug_cmd = {
      .command = "applemidi_debug",
      .help = "Enables/Disables Debug Messages",
      .hint = NULL,
      .func = &cmd_debug,
      .argtable = &applemidi_if_debug_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&debug_cmd) );
  }

  {
    applemidi_if_start_session_args.ip = arg_str1(NULL, NULL, "<ip>", "IP of remote peer");
    applemidi_if_start_session_args.control_port = arg_int0(NULL, "port", "<port-number>", "Port number of remote peer (default: 5004)");
    applemidi_if_start_session_args.peer_port = arg_int0(NULL, "peer_port", "<session-number>", "Session number (1..4)"); // TODO: insert APPLEMIDI_MAX_SESSIONS
    applemidi_if_start_session_args.end = arg_end(20);

    const esp_console_cmd_t start_session_cmd = {
      .command = "applemidi_start_session",
      .help = "Initiates a new session with given peer",
      .hint = NULL,
      .func = &cmd_start_session,
      .argtable = &applemidi_if_start_session_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&start_session_cmd) );
  }

  {
    applemidi_if_end_session_args.peer_port = arg_int1(NULL, "peer_port", "<session-number>", "Session number (1..4)"); // TODO: insert APPLEMIDI_MAX_SESSIONS
    applemidi_if_end_session_args.end = arg_end(20);

    const esp_console_cmd_t end_session_cmd = {
      .command = "applemidi_end_session",
      .help = "Terminates a session with given peer",
      .hint = NULL,
      .func = &cmd_end_session,
      .argtable = &applemidi_if_end_session_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&end_session_cmd) );
  }

}
#endif
