/**
   Copyright 2017 Afero, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/*
  Get a reasonably accurate time-of-day from the Afero Cloud, two different ways

  When the ASR links with the Afero Cloud, it returns two attributes to the MCU:
  65015: LINKED_TIMESTAMP     (signed 32-bit long)
         This is a UNIX Epoch (number of seconds since 00:00:00 1/1/1970) UTC timestamp
         of when the ASR successfully last linked to the Afero Cloud. When returned
         shortly after the ASR reboots, it should be reasonably close to the actual
         current time. The latency from the Cloud back to the MCU should typically be less
         than 1-2 seconds, but it's not safe to rely on this timestamp to be more accurate
         than maybe +/- 1 minute or so. But if your TOD requirements are not too critical, it's
         a usable timestamp that's given to you automatically.
  65001: UTC_OFFSET_DATA      (byte[8])
         This byte array contains the current localtime offset from UTC, the next
         UTC offset, and a UNIX Epoch timestamp of when the "next" UTC offset is valid.
         The byte array consists of:
           [0-1]: little-endian signed int containing the local timezone offzet from UTC in minutes
           [2-5]: little-endian unsigned long containing an Epoch timestamp (UTC) for "next" offset validity
           [6-7]: little-endian signed int containing the "next" local timezone offset from UTC in minutes

         NOTE: UTC Offset is determined by the Location attributes set (by you) for the ASR and
               ARE NOT DYNAMIC in any way. The UTC offset can and will be wrong if the Location in the
               device configuration is incorrect. You can set this location for your Afero device
               with the Afero mobile app or in the Inspector tool at https://inspector.afero.io

  For example, you may receive a attributes that look like this:
  LINKED_TIMESTAMP=0x5a2b06c3
  UTC_OFFSET_DATA=0xD4FEF0D3A45A10FF

  This translates to:
  Link Time: 0x5a2b06c3 (Fri 2017-12-08 21:40:19 GMT)
  Current UTC offset: -300 minutes (0xFED4)    (Eastern Standard Time, in this example)
  Next UTC offset: -240 minutes (0xFF10)       (Eastern Daylight Time, in this example)
  UTC Offset Change: Sun 2018-03-11 07:00:00 GMT (0x5AA4D3F0) (Spring 2018 USA DST Time Change, 2am local time)

  Care should be taken that LINKED_TIMESTAMP only be assumed to be current when the ASR state transitions
  to a connected state. It is possible to use getAttribute to return the linked timestamp at any point,
  and the result returned will be the time the ASR was first connected to the Cloud, which might be
  significantly in the past if the device has been connected for a long time. This example code uses a flag called
  timestamp_read as a very paranoid way to prevent these timestamps from being updated - if the risk of a spurious
  linked timestamp update is not that much of a concern to you all of the logic that around that flag can be
  removed for simplification.

  This example app will provide a simple method to listen for these attributes in attrNotifyHandler and set a
  C time struct that can be manipulated within your application. For better results, you should use a real
  RTC chip and use these attributes to set the RTC so you have a reasonably accurate clock when not connected
  to the Afero Cloud.

  To work properly, this application needs a Modulo-1 or 2 running firmware 1.2.1 or higher, with a
  profile created with Afero Profile Editor 1.2.1 or higher.

  When enabled in your profile, the ASR will return this attribute every minute after it's linked:
   1201: ASR_UTC_TIME     (signed 32-bit long)
         This is a UNIX Epoch (number of seconds since 00:00:00 1/1/1970) UTC timestamp
         sent from the ASR every minute "on the minute" (:00 seconds) after the ASR connects to the Afero Cloud.
         This attribute is not synced to any Network Time protocol, so it's not guaranteed to be accurate to more
         than +/- 1-2s or so, but it is accurate enough to use this timestamp to provide a time of day if your
         hardware doesn't have RTC support.

*/


#include <SPI.h>

#include "af_lib.h"
#include "arduino_spi.h"
#include "arduino_uart.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "arduino_transport.h"

#include <avr/pgmspace.h>
#include <stdio.h>
#include <time.h>

// We don't actually need any specific profile config on the Modulo since these
// system attributes exist in all profiles created with Profile Editor > 1.2.1
// as long as the Modulo is flashed with a profile that has MCU support enabled.
// We have a demo profile here that you can flash to our Modulo if you need one
//#include "profile/afero_clock1/device-description.h"
#include "profile/afero_clock2/device-description.h"


// By default, this application uses the SPI interface to the Modulo.
// This code works just fine using the UART interface to the Modulo as well,
// see the afBlink example for suggested UART setup - to keep this code simple
// we're just going to use the SPI interface accessible through the Uno+Plinto or
// through the Modulo's Teensyduino compatible pinout.
//  See the afBlink example app for communicating with Modulo over its UART interface.

#define ARDUINO_USE_SPI 1
//#define ARDUINO_USE_UART 1

// Serial monitor baud rate
#define BAUD_RATE                 115200

// Automatically detect MCU board type
#if defined(ARDUINO_AVR_UNO) // using a Plinto shield
#define INT_PIN                   2
#define CS_PIN                    10

#define RX_PIN                    7
#define TX_PIN                    8

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

#define RX_PIN                    7
#define TX_PIN                    8

#else
#error "Sorry, afLib does not support this board"
#endif


bool    timestamp_read         = false;
int32_t linked_timestamp       = 0;
int16_t utc_offset_now         = 0;
int16_t utc_offset_next        = 0;
int32_t utc_offset_change_time = 0;
int32_t asr_utc_time           = 0;

// Uno boards have insufficient memory to tolerate pretty debug print methods
// Try a Teensyduino
#if !defined(ARDUINO_AVR_UNO)
#define ATTR_PRINT_HEADER_LEN     60
#define ATTR_PRINT_MAX_VALUE_LEN  512   // Max attribute len is 256 bytes; Each HEX-printed byte is 2 ASCII characters
#define ATTR_PRINT_BUFFER_LEN     (ATTR_PRINT_HEADER_LEN + ATTR_PRINT_MAX_VALUE_LEN)
char attr_print_buffer[ATTR_PRINT_BUFFER_LEN];
# endif

af_lib_t* af_lib = NULL;
bool initializationPending = false; // If true, we're waiting on AF_MODULE_STATE_INITIALIZED
bool rebootPending = false;         // If true, a reboot is needed, e.g. if we received an OTA firmware update.


//   Forward definitions
void printAttribute(const char* label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value);
void printTimeString(time_t t);

// Callback executed any time ASR has information for the MCU
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) {
  printAttribute("attrEventCallback", attributeId, valueLen, value);

  switch (eventType) {
    case AF_LIB_EVENT_UNKNOWN:
      break;

    case AF_LIB_EVENT_ASR_SET_RESPONSE:
      // Response to af_lib_set_attribute() for an ASR attribute
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_SENT:
      // Request from af_lib_set_attribute() for an MCU attribute has been sent to ASR
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_REJECTION:
      // Request from af_lib_set_attribute() for an MCU attribute was rejected by ASR
      break;

    case AF_LIB_EVENT_ASR_GET_RESPONSE:
      // Response to af_lib_get_attribute()
      break;

    case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION:
      // Unsolicited default notification for an MCU attribute
      break;

    case AF_LIB_EVENT_ASR_NOTIFICATION:
      // Unsolicited notification of non-MCU attribute change
      switch (attributeId) {
        case AF_SYSTEM_LINKED_TIMESTAMP:
          if (!timestamp_read) {
            linked_timestamp = *((int32_t *)value);
            Serial.print(F("ASR linked at: "));
            printTimeString((time_t)linked_timestamp);
            // TODO: If you have an RTC this would be a good place to set the clock time
          }
          if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
          break;
        case AF_SYSTEM_UTC_OFFSET_DATA:
          utc_offset_now         = value[1] << 8 | value[0];
          utc_offset_next        = value[7] << 8 | value[6];
          // The line below extracts the int32 timestamp from bytes 2-5 of the attribute
          utc_offset_change_time = *((int32_t *)(value + 2));
          if (utc_offset_change_time > 0) {
            Serial.print(F("Current TZ offset from UTC: "));
            Serial.print((float)utc_offset_now / 60);
            Serial.print(F("h, isDST: "));
            Serial.println(utc_offset_now > utc_offset_next);
            Serial.print(F("   Next TZ offset from UTC: "));
            Serial.print((float)utc_offset_next / 60);
            Serial.println(F("h"));
            Serial.print(F("Time Zone Change at: "));
            printTimeString((time_t)utc_offset_change_time);
            // TODO: If you have an RTC this would be a good place to set the clock time and/or timezone
          }
          if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
          break;

        case AF_UTC_TIME:
          asr_utc_time = *((int32_t *)value);
          Serial.print(F("ASR UTC Time: "));
          printTimeString((time_t)asr_utc_time);
          // TODO: If you have an RTC this would be a good place to set the clock time
          break;

        case AF_SYSTEM_ASR_STATE:
          Serial.print("ASR state: ");
          switch (value[0]) {
            case AF_MODULE_STATE_REBOOTED:
              Serial.println("Rebooted - Always set MCU attributes after reboot");
              initializationPending = true;   // Rebooted, so we're not yet initialized
              timestamp_read = false;
              break;

            case AF_MODULE_STATE_LINKED:
              Serial.println("Linked");
              break;

            case AF_MODULE_STATE_UPDATING:
              Serial.println("Updating");
              break;

            case AF_MODULE_STATE_UPDATE_READY:
              Serial.println("Update ready - reboot needed");
              rebootPending = true;
              break;

            case AF_MODULE_STATE_INITIALIZED:
              Serial.println("Initialized");
              initializationPending = false;
              break;

            case AF_MODULE_STATE_RELINKED:
              Serial.println("Re-linked");
              break;

            default:
              Serial.print("Unexpected state - "); Serial.println(value[0]);
              break;
          }
          break;

        default:
          break;
      }
      break;

    case AF_LIB_EVENT_MCU_SET_REQUEST:
      // Request from ASR to MCU to set an MCU attribute, requires a call to af_lib_send_set_response()
      break;

    default:
      break;
  }
}


void setup() {
  Serial.begin(BAUD_RATE);
  // Wait for Serial to come up, or time out after 3 seconds if there's no serial monitor attached
  while (!Serial || millis() < 3000) {
    ;
  }

  Serial.println(F("afLib: Time of Day from Afero Cloud Example"));

  // The Plinto board automatically connects reset on UNO to reset on Modulo
  // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
  Serial.println(F("Using Teensy - Resetting Modulo"));
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, 0);
  delay(250);
  digitalWrite(RESET, 1);
#endif


    // Start the sketch awaiting initialization
    initializationPending = true;

  /**
     Initialize the afLib - this depends on communications protocol used (SPI or UART)

      Configuration involves a few common items:
          af_transport_t - a transport implementation for a specific protocol (SPI or UART)
          attrEventCallback - the function to be called when ASR has data for MCU.
      And a few protocol-specific items:
          for SPI:
              INT_PIN - the pin used for SPI slave interrupt.
              arduinoSPI - class to handle SPI communications.
          for UART:
              RX_PIN, TX_PIN - pins used for receive, transmit.
              arduinoUART - class to handle UART communications.
  */

  Serial.print("Configuring communications...sketch will use ");
#if defined(ARDUINO_USE_UART)
  Serial.println("UART");
  af_transport_t *arduinoUART = arduino_transport_create_uart(RX_PIN, TX_PIN);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoUART);
#elif defined(ARDUINO_USE_SPI)
  Serial.println("SPI");
  af_transport_t* arduinoSPI = arduino_transport_create_spi(CS_PIN);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoSPI);
  arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));
#else
#error "Please define a a communication transport (ie SPI or UART)."
#endif
}


void loop() {
    // We don't actually do anything here in this example, so we just give afLib some time to process
    // The actual clock functionality all happens in attrEventCallback
    af_lib_loop(af_lib);

    if (initializationPending) {
        // If we're awaiting initialization, don't bother checking/setting attributes
    } else {

        // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop().
        // In order to make this fault-tolerant, we'll continue to retry if the command fails.
        if (rebootPending) {
            int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT);
            rebootPending = (retVal != AF_SUCCESS);
            if (!rebootPending) {
                Serial.println("*************************************************************************");
                Serial.println("REBOOT COMMAND SENT; NOW AWAITING AF_MODULE_STATE_INITIALIZED");
                initializationPending = true;
            }
        }
    }
}

void printTimeString(time_t t) {
#if defined(ARDUINO_AVR_UNO)
  // Arduino Uno seems to have trouble with strftime that other larger MCUs don't have.
  // So instead of an easy and simple strftime, let's dump the timestamp here to show it's
  // at least correct. Parsing this into a time value is an exercise left to the user
  // (solution hint: use a better MCU than an Uno)
  Serial.print("timestamp=");
  Serial.println(t);
#else
  char buf[80];

  struct tm  ts;
  ts = *localtime(&t);

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
  Serial.println(buf);
#endif
}



/****************************************************************************************************
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
 ****************************************************************************************************/
#ifdef ATTR_PRINT_BUFFER_LEN

void getPrintAttrHeader(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen) {
  memset(attr_print_buffer, 0, ATTR_PRINT_BUFFER_LEN);
  snprintf(attr_print_buffer, ATTR_PRINT_BUFFER_LEN, "%s id: %s len: %05d value: ", sourceLabel, attrLabel, valueLen);
}

void printAttrBool(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    strcat(attr_print_buffer, *value == 1 ? "true" : "false");
  }
  Serial.println(attr_print_buffer);
}

void printAttr8(const char *sourceLabel, const char *attrLabel, const uint8_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int8_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttr16(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int16_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttr32(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[11];
    strcat(attr_print_buffer, itoa(*((int32_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttrHex(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  for (int i = 0; i < valueLen; i++) {
    strcat(attr_print_buffer, String(value[i], HEX).c_str());
  }
  Serial.println(attr_print_buffer);
}

void printAttrStr(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  int len = strlen(attr_print_buffer);
  for (int i = 0; i < valueLen; i++) {
    attr_print_buffer[len + i] = (char)value[i];
  }
  Serial.println(attr_print_buffer);
}
#endif

void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
#ifdef ATTR_PRINT_BUFFER_LEN
  switch (attributeId) {
    case AF_SYSTEM_LINKED_TIMESTAMP:
      printAttr32(label, "AF_SYSTEM_LINKED_TIMESTAMP", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_UTC_OFFSET_DATA:
      printAttrHex(label, "AF_SYSTEM_UTC_OFFSET_DATA", attributeId, valueLen, value);
      break;

    case AF_APPLICATION_VERSION:
      printAttrHex(label, "AF_APPLICATION_VERSION", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_ASR_STATE:
      printAttr8(label, "AF_SYSTEM_ASR_STATE", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_REBOOT_REASON:
      printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
      break;
  }
#endif
}
