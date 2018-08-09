/**
   Copyright 2015-2018 Afero, Inc.

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

#include <SPI.h>

#include "af_lib.h"
#include "arduino_spi.h"
#include "arduino_uart.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "arduino_transport.h"

// Include the constants required to access attribute ids from your profile.
#include "profile/afBlink/device-description.h"

// Select the MCU interface used (set in the profile, and in hardware)
//#define ARDUINO_USE_UART                  1
#define ARDUINO_USE_SPI                   1

// Automatically detect MCU board type
#if defined(ARDUINO_AVR_UNO)  // using a Plinto shield
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

// Modulo LED is active low
#define LED_OFF                   1
#define LED_ON                    0

// Set a blink interval. 
// We can send requests at a slower rate, of course, but try not to send
// requests faster than the minimum so the ASR doesn't get overwhelmed.
#define BLINK_INTERVAL            2000  // 2 seconds

/**
   afLib Stuff
*/
af_lib_t* af_lib = NULL;
bool initializationPending = false; // If true, we're waiting on AF_MODULE_STATE_INITIALIZED
bool rebootPending = false;         // If true, a reboot is needed, e.g. if we received an OTA firmware update.
bool attr1SyncPending = false;      // // If true (e.g. after reboot), we need to set MCU attr 1

volatile long lastBlink = 0;                   // Time of last blink
volatile bool blinking = false;                // Track whether LED is blinking; represented by attribute AF_BLINK
volatile bool moduloLEDIsOn = false;           // Track whether the Modulo LED is on
uint16_t prevButtonValue = 1;                  // Track the button value...
uint16_t curButtonValue = prevButtonValue;     // ...so we know when it has changed


// Uno boards have insufficient memory to tolerate pretty-print debug methods
// Try a Teensyduino
#if !defined(ARDUINO_AVR_UNO)
#define ATTR_PRINT_HEADER_LEN     60
#define ATTR_PRINT_MAX_VALUE_LEN  512   // Max attribute len is 256 bytes; Each HEX-printed byte is 2 ASCII characters
#define ATTR_PRINT_BUFFER_LEN     (ATTR_PRINT_HEADER_LEN + ATTR_PRINT_MAX_VALUE_LEN)
char attr_print_buffer[ATTR_PRINT_BUFFER_LEN];
# endif

/**
   Forward definition of debug-print method
*/
void printAttribute(const char* label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value);

void toggleModuloLED() {
    setModuloLED(!moduloLEDIsOn);
}

void setModuloLED(bool on) {
    if (moduloLEDIsOn != on) {
        int16_t attrVal = on ? LED_ON : LED_OFF; // Modulo LED is active low

        int timeout = 0;
        while (af_lib_set_attribute_16(af_lib, AF_MODULO_LED, attrVal) != AF_SUCCESS) {
            delay(10);
            af_lib_loop(af_lib);
            timeout++;
            if (timeout > 500) {
                // If we haven't been successful after 5 sec (500 tries, each after 10 msec delay)
                // we assume we're in some desperate state, and reboot
                pinMode(RESET, OUTPUT);
                digitalWrite(RESET, 0);
                delay(250);
                digitalWrite(RESET, 1);
                return;
            }
        }

        moduloLEDIsOn = on;
    }
}

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
                case AF_MODULO_LED:
                    // Update the state of the LED based on the actual attribute value.
                    moduloLEDIsOn = (*value == 0);
                    break;

                case AF_MODULO_BUTTON:
                    // curButtonValue is checked in loop(). If changed, will toggle the blinking state.
                    curButtonValue = *(uint16_t*) value;
                    break;

                case AF_SYSTEM_ASR_STATE:
                    Serial.print("ASR state: ");
                    switch (value[0]) {
                        case AF_MODULE_STATE_REBOOTED:
                            Serial.println("Rebooted - Always set MCU attributes after reboot");
                            initializationPending = true;   // Rebooted, so we're not yet initialized
                            attr1SyncPending = true;   // Set Attribute 1 (happens in loop())
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
            switch (attributeId) {

                case AF_BLINK:
                    // This MCU attribute controls whether we should be blinking.
                    blinking = (*value == 1);
                    af_lib_send_set_response(AF_BLINK, true, valueLen, value);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

void setup() {

    Serial.begin(115200);

    while (!Serial) { // wait for serial port
        ;
    }

    Serial.println("Starting sketch: afBlink");

    // The Plinto board automatically connects reset on UNO to reset on Modulo
    // If we're using Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
    Serial.println("Using Teensy - Resetting Modulo");
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
    // Give the afLib state machine some time.
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

        // After reboot, need to inform service of value of all MCU attributes (in this case just attr 1)
        if (attr1SyncPending) {
            if (af_lib_set_attribute_bool(af_lib, AF_BLINK, blinking) == AF_SUCCESS) {
                attr1SyncPending = false;
            }
        }

        // Modulo button toggles 'blinking'
        if (prevButtonValue != curButtonValue) {
            if (af_lib_set_attribute_bool(af_lib, AF_BLINK, !blinking) == AF_SUCCESS) {
                blinking = !blinking;
                prevButtonValue = curButtonValue;
            }
        }

        // Flash the LED whenever the 'blinking' value is true
        if (blinking) {
            if (millis() - lastBlink > BLINK_INTERVAL) {
                toggleModuloLED();
                lastBlink = millis();
            }
        }
    }
}

/****************************************************************************************************
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
   NOTE: if your sketch doesn't run due to lack of memory, try commenting-out these methods
   and declaration of attr_print_buffer. These are handy, but they do require significant memory.
 ****************************************************************************************************/
#ifdef ATTR_PRINT_BUFFER_LEN

void getPrintAttrHeader(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId,
                        const uint16_t valueLen) {
    memset(attr_print_buffer, 0, ATTR_PRINT_BUFFER_LEN);
    snprintf(attr_print_buffer, ATTR_PRINT_BUFFER_LEN, "%s id: %s len: %05d value: ", sourceLabel, attrLabel,
             valueLen);
}

void
printAttrBool(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
              const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    if (valueLen > 0) {
        strcat(attr_print_buffer, *value == 1 ? "true" : "false");
    }
    Serial.println(attr_print_buffer);
}

void printAttr8(const char* sourceLabel, const char* attrLabel, const uint8_t attributeId, const uint16_t valueLen,
                const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    if (valueLen > 0) {
        char intStr[6];
        strcat(attr_print_buffer, itoa(*((int8_t*) value), intStr, 10));
    }
    Serial.println(attr_print_buffer);
}

void
printAttr16(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    if (valueLen > 0) {
        char intStr[6];
        strcat(attr_print_buffer, itoa(*((int16_t*) value), intStr, 10));
    }
    Serial.println(attr_print_buffer);
}

void
printAttr32(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    if (valueLen > 0) {
        char intStr[11];
        strcat(attr_print_buffer, itoa(*((int32_t*) value), intStr, 10));
    }
    Serial.println(attr_print_buffer);
}

void
printAttrHex(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    for (int i = 0; i < valueLen; i++) {
        strcat(attr_print_buffer, String(value[i], HEX).c_str());
    }
    Serial.println(attr_print_buffer);
}

void
printAttrStr(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
    getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    int len = strlen(attr_print_buffer);
    for (int i = 0; i < valueLen; i++) {
        attr_print_buffer[len + i] = (char) value[i];
    }
    Serial.println(attr_print_buffer);
}

#endif

void printAttribute(const char* label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value) {
#ifdef ATTR_PRINT_BUFFER_LEN
    switch (attributeId) {
        case AF_BLINK:
            printAttrBool(label, "AF_BLINK", attributeId, valueLen, value);
            break;

        case AF_MODULO_LED:
            printAttr16(label, "AF_MODULO_LED", attributeId, valueLen, value);
            break;

        case AF_GPIO_0_CONFIGURATION:
            printAttrHex(label, "AF_GPIO_0_CONFIGURATION", attributeId, valueLen, value);
            break;

        case AF_MODULO_BUTTON:
            printAttr16(label, "AF_MODULO_BUTTON", attributeId, valueLen, value);
            break;

        case AF_GPIO_3_CONFIGURATION:
            printAttrHex(label, "AF_GPIO_3_CONFIGURATION", attributeId, valueLen, value);
            break;

        case AF_BOOTLOADER_VERSION:
            printAttrHex(label, "AF_BOOTLOADER_VERSION", attributeId, valueLen, value);
            break;

        case AF_SOFTDEVICE_VERSION:
            printAttrHex(label, "AF_SOFTDEVICE_VERSION", attributeId, valueLen, value);
            break;

        case AF_APPLICATION_VERSION:
            printAttrHex(label, "AF_APPLICATION_VERSION", attributeId, valueLen, value);
            break;

        case AF_PROFILE_VERSION:
            printAttrHex(label, "AF_PROFILE_VERSION", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_ASR_STATE:
            printAttr8(label, "AF_SYSTEM_ASR_STATE", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_LOW_POWER_WARN:
            printAttr8(label, "AF_ATTRIBUTE_LOW_POWER_WARN", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_REBOOT_REASON:
            printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_MCU_INTERFACE:
            printAttr8(label, "AF_SYSTEM_MCU_INTERFACE", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_LINKED_TIMESTAMP:
            printAttr32(label, "AF_SYSTEM_LINKED_TIMESTAMP", attributeId, valueLen, value);
            break;
    }
#endif
}
