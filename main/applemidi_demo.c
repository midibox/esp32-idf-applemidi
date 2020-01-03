/*
 * Apple MIDI Demo
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

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "wifi.h"
#include "console.h"

#include "applemidi.h"
#include "if/lwip/applemidi_if.h"


#define TAG "MIDIbox"


////////////////////////////////////////////////////////////////////////////////////////////////////
// This function is called from the Apple MIDI Driver whenever a new MIDI message has been received
////////////////////////////////////////////////////////////////////////////////////////////////////
void applemidi_callback_midi_message_received(uint8_t applemidi_port, uint32_t timestamp, uint8_t midi_status, uint8_t *remaining_message, size_t len, size_t continued_sysex_pos)
{
  if( applemidi_get_debug_level() >= 3 ) {
    // Note: with these messages enabled, we potentially get packet loss!
    ESP_LOGI(TAG, "receive_packet CALLBACK applemidi_port=%d, timestamp=%d, midi_status=0x%02x, len=%d, continued_sysex_pos=%d, remaining_message:", applemidi_port, timestamp, midi_status, len, continued_sysex_pos);
    esp_log_buffer_hex(TAG, remaining_message, len);
  }

  // loopback received message
  {
    // TODO: more comfortable packet creation via special APIs

    // Note: by intention we create new packets for each incoming message
    // this shows that running status is maintained, and that SysEx streams work as well

    size_t loopback_packet_len = 1 + len; // includes MIDI status and remaining bytes
    uint8_t *loopback_packet = (uint8_t *)malloc(loopback_packet_len * sizeof(uint8_t));
    if( loopback_packet == NULL ) {
      // no memory...
    } else {
      loopback_packet[0] = midi_status;
      memcpy(&loopback_packet[1], remaining_message, len);

      applemidi_send_message(applemidi_port, loopback_packet, loopback_packet_len);

      free(loopback_packet);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Handling the Console in an independent task
////////////////////////////////////////////////////////////////////////////////////////////////////
static void console_task(void *pvParameters)
{
  console_init();

  while( 1 ) {
    console_tick();
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// WIFI Connection + Apple MIDI Handling
////////////////////////////////////////////////////////////////////////////////////////////////////
static void udp_task(void *pvParameters)
{
  wifi_init();

  while( 1 ) {
    if( !wifi_connected() ) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
    } else {
      applemidi_if_init(APPLEMIDI_DEFAULT_PORT);
      applemidi_init(applemidi_callback_midi_message_received, applemidi_if_send_udp_datagram);

      while( wifi_connected() ) {
        applemidi_if_tick(applemidi_parse_udp_datagram);
        applemidi_tick();
      }

      applemidi_if_deinit();
    }
  }

  vTaskDelete(NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// The main function
////////////////////////////////////////////////////////////////////////////////////////////////////
void app_main()
{
  /* Initialize NVS — it is used to store PHY calibration data */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );

  // start with random seed
  srand(esp_random());

  // launch tasks
  xTaskCreate(udp_task, "udp", 4096, NULL, 5, NULL);
  xTaskCreate(console_task, "console", 4096, NULL, 5, NULL);
}
