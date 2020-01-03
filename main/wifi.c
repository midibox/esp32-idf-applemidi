/*
 * WIFI Initialization & Console
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

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "wifi.h"

#define TAG "MIDIbox_WIFI"

#define WIFI_STORAGE_NAMESPACE "MIDIbox_WIFI"


static char *wifi_config_ssid = NULL;
static char *wifi_config_password = NULL;
static int32_t wifi_config_timeout_ms = 10000;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Store/Restore WIFI Configuration from NVS
////////////////////////////////////////////////////////////////////////////////////////////////////
static int32_t wifi_config_store(void)
{
  nvs_handle nvs_handle;
  esp_err_t err;
  int32_t status = 0;

  err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if( err != ESP_OK ) {
    ESP_LOGI(TAG, "WIFI Configuration can't be stored!");
    return -1;
  }

  err = nvs_set_blob(nvs_handle, "ssid", wifi_config_ssid, strlen(wifi_config_ssid));
  if( err != ESP_OK ) {
    ESP_LOGE(TAG, "Failed to store ssid!");
    status = -1;
  }

  err = nvs_set_blob(nvs_handle, "password", wifi_config_password, strlen(wifi_config_password));
  if( err != ESP_OK ) {
    ESP_LOGE(TAG, "Failed to store password!");
    status = -1;
  }

  err = nvs_set_i32(nvs_handle, "timeout_ms", wifi_config_timeout_ms);
  if( err != ESP_OK ) {
    ESP_LOGE(TAG, "Failed to store timeout_ms!");
    status = -1;
  }

  return status;
}

static int32_t wifi_config_restore(void)
{
  nvs_handle nvs_handle;
  esp_err_t err;

  err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if( err != ESP_OK ) {
    ESP_LOGI(TAG, "WIFI Configuration not stored so far...");
    return -1;
  }

  // SSID
  size_t ssid_size = 0;  // value will default to 0, if not set yet in NVS
  nvs_get_blob(nvs_handle, "ssid", NULL, &ssid_size);

  // Read previously saved blob if available
  if( ssid_size > 0 ) {
    if( wifi_config_ssid != NULL ) // discard previous strong
      free(wifi_config_ssid);

    wifi_config_ssid = malloc(ssid_size+1);
    wifi_config_ssid[ssid_size] = 0;
    err = nvs_get_blob(nvs_handle, "ssid", wifi_config_ssid, &ssid_size);

    if( err != ESP_OK ) {
      ESP_LOGI(TAG, "Failed to restore WIFI SSID!");
      free(wifi_config_ssid);
    } else {
      ESP_LOGI(TAG, "Restored WIFI SSID: '%s'", wifi_config_ssid);
    }
  }


  // Password
  size_t password_size = 0;  // value will default to 0, if not set yet in NVS
  nvs_get_blob(nvs_handle, "password", NULL, &password_size);

  // Read previously saved blob if available
  if( password_size > 0 ) {
    if( wifi_config_password != NULL ) // discard previous strong
      free(wifi_config_password);

    wifi_config_password = malloc(password_size+1);
    wifi_config_password[password_size] = 0;
    err = nvs_get_blob(nvs_handle, "password", wifi_config_password, &password_size);

    if( err != ESP_OK ) {
      ESP_LOGI(TAG, "Failed to restore WIFI Password!");
      free(wifi_config_password);
    } else {
      ESP_LOGI(TAG, "Restored WIFI Password: <hidden>");
    }
  }


  // Timeout
  err = nvs_get_i32(nvs_handle, "timeout_ms", &wifi_config_timeout_ms);

  if( err != ESP_OK ) {
    ESP_LOGI(TAG, "Failed to restore WIFI Connection Timeout!");
  } else {
    ESP_LOGI(TAG, "Restored WIFI Connection Timeout: %d mS", wifi_config_timeout_ms);
  }

  return 0; // no error -- note: by intention we don't return error status if a certain value can't be restored, because we might add values in future
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// WIFI Event Handler
////////////////////////////////////////////////////////////////////////////////////////////////////

static EventGroupHandle_t wifi_event_group;
const int IPV4_GOTIP_BIT = (1 << 0);
const int IPV6_GOTIP_BIT = (1 << 1);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch( event->event_id ) {
  case SYSTEM_EVENT_STA_START:
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
    esp_wifi_connect();
    break;

  case SYSTEM_EVENT_STA_CONNECTED:
    /* enable ipv6 */
    tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
    break;

  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
    ESP_LOGI(TAG, "got ip:%s\n",
    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    ESP_ERROR_CHECK(esp_wifi_connect());
    xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
    xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
    break;

  case SYSTEM_EVENT_AP_STA_GOT_IP6:
    xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
    ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP6");

    char *ip6 = ip6addr_ntoa(&event->event_info.got_ip6.ip6_info.ip);
    ESP_LOGI(TAG, "IPv6: %s", ip6);
    break;

  default:
    break;
  }

  return ESP_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Join WIFI
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
{
  wifi_init();

  wifi_config_t wifi_config = { 0 };
  strncpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  if (pass) {
    strncpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
  }

  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK( esp_wifi_connect() );

  int bits = xEventGroupWaitBits(wifi_event_group, IPV4_GOTIP_BIT,
    pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);

  return (bits & IPV4_GOTIP_BIT) != 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Arguments used by 'join' function */
static struct {
    struct arg_int *timeout;
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_join_args;

static int cmd_wifi_join(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &wifi_join_args);
  if( nerrors != 0 ) {
    arg_print_errors(stderr, wifi_join_args.end, argv[0]);
    return 1;
  }
  ESP_LOGI(TAG, "Connecting to '%s'", wifi_join_args.ssid->sval[0]);

  /* set default value*/
  if( wifi_join_args.timeout->count == 0 ) {
    wifi_join_args.timeout->ival[0] = wifi_config_timeout_ms;
  }

  // for storage in NVS via 'wifi_store'
  {
    wifi_config_timeout_ms = wifi_join_args.timeout->ival[0]; // so that we can store it in NVS

    if( wifi_config_ssid != NULL )
      free(wifi_config_ssid);
    wifi_config_ssid = malloc(strlen(wifi_join_args.ssid->sval[0])+1);
    if( wifi_config_ssid != NULL )
      strcpy(wifi_config_ssid, wifi_join_args.ssid->sval[0]);

    if( wifi_config_password != NULL )
      free(wifi_config_password);
    wifi_config_password = malloc(strlen(wifi_join_args.password->sval[0])+1);
    if( wifi_config_password != NULL )
      strcpy(wifi_config_password, wifi_join_args.password->sval[0]);
  }

  bool connected = wifi_join(wifi_config_ssid, wifi_config_password, wifi_config_timeout_ms);
  if( !connected ) {
    ESP_LOGW(TAG, "Connection timed out");
    return 1;
  }

  ESP_LOGI(TAG, "Connected - enter 'wifi_store' to permanently store this configuration.");

  return 0;
}

static int cmd_wifi_store(int argc, char **argv)
{
  if( wifi_config_store() < 0 ) {
    ESP_LOGE(TAG, "Failed to store WIFI Configuration!");
    return 1;
  }

  ESP_LOGI(TAG, "WIFI Configuration successfully stored.");

  return 0;
}

static int cmd_wifi_restore(int argc, char **argv)
{
  if( wifi_config_restore() < 0 ) {
    ESP_LOGE(TAG, "Failed to restore WIFI Configuration!");
    return 1;
  }

  ESP_LOGI(TAG, "WIFI Configuration successfully restored.");

  return 0;
}

void wifi_register_console_commands(void)
{
  {
    wifi_join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    wifi_join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    wifi_join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    wifi_join_args.end = arg_end(2);

    const esp_console_cmd_t wifi_join_cmd = {
      .command = "wifi_join",
      .help = "Join WiFi AP as a station",
      .hint = NULL,
      .func = &cmd_wifi_join,
      .argtable = &wifi_join_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_join_cmd) );
  }

  {
    const esp_console_cmd_t wifi_store_cmd = {
      .command = "wifi_store",
      .help = "Stores the current WIFI credentials (SSID and Password)",
      .hint = NULL,
      .func = &cmd_wifi_store,
      .argtable = NULL
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_store_cmd) );
  }

  {
    const esp_console_cmd_t wifi_restore_cmd = {
      .command = "wifi_restore",
      .help = "Restores the previously stored WIFI credentials (SSID and Password)",
      .hint = NULL,
      .func = &cmd_wifi_restore,
      .argtable = NULL
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&wifi_restore_cmd) );
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Returns current WIFI status
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t wifi_connected(void)
{
  int bits = xEventGroupGetBits(wifi_event_group);
  return (bits & (IPV4_GOTIP_BIT | IPV6_GOTIP_BIT)) != 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Initializes the WIFI interface and initiates a connection of credentials are stored in NVS
////////////////////////////////////////////////////////////////////////////////////////////////////
void wifi_init(void)
{
  esp_log_level_set("wifi", ESP_LOG_WARN);
  static bool initialized = false;
  if( initialized ) {
    return;
  }
  tcpip_adapter_init();
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
  ESP_ERROR_CHECK( esp_wifi_start() );
  esp_wifi_set_ps(WIFI_PS_NONE); // leads to faster ping
  initialized = true;

  wifi_config_restore();

  if( wifi_config_ssid != NULL && strlen(wifi_config_ssid) ) {
    bool connected = wifi_join(wifi_config_ssid, wifi_config_password, wifi_config_timeout_ms);

    if( !connected ) {
      ESP_LOGW(TAG, "Connection timed out");
    }
  } else {
    ESP_LOGW(TAG, "No WIFI Configuration stored in NVM yet!");
    ESP_LOGW(TAG, "Please enter your credentials with 'wifi_join <ssid> <password>', and thereafter store them with 'wifi_store'!");
  }
}

