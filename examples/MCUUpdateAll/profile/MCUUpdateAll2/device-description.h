/*
 * Afero Device Profile header file
 * Device Description:		a4b54372-9d2b-4432-a3fe-6f798722ed7a
 * Schema Version:	2
 */

#define AF_BOARD_MODULO_1                                            0
#define AF_BOARD_MODULO_2                                            1
#define AF_BOARD_QUANTA                                              2
#define AF_BOARD_ABELO_2A                                            3
#define AF_BOARD_POTENCO                                             4

#define AF_BOARD                                     AF_BOARD_MODULO_2

#define ATTRIBUTE_TYPE_SINT8                                         2
#define ATTRIBUTE_TYPE_SINT16                                        3
#define ATTRIBUTE_TYPE_SINT32                                        4
#define ATTRIBUTE_TYPE_SINT64                                        5
#define ATTRIBUTE_TYPE_BOOLEAN                                       1
#define ATTRIBUTE_TYPE_UTF8S                                        20
#define ATTRIBUTE_TYPE_BYTES                                        21
#define ATTRIBUTE_TYPE_Q_15_16                                       6

//region Service ID 1
// Attribute Current Temp
#define AF_CURRENT_TEMP                                              1
#define AF_CURRENT_TEMP_SZ                                           2
#define AF_CURRENT_TEMP_TYPE                     ATTRIBUTE_TYPE_SINT16

// Attribute Alarm Temp
#define AF_ALARM_TEMP                                                2
#define AF_ALARM_TEMP_SZ                                             2
#define AF_ALARM_TEMP_TYPE                       ATTRIBUTE_TYPE_SINT16

// Attribute Sample Rate
#define AF_SAMPLE_RATE                                               3
#define AF_SAMPLE_RATE_SZ                                            1
#define AF_SAMPLE_RATE_TYPE                       ATTRIBUTE_TYPE_SINT8

// Attribute Bootloader Version
#define AF_BOOTLOADER_VERSION                                     2001
#define AF_BOOTLOADER_VERSION_SZ                                     8
#define AF_BOOTLOADER_VERSION_TYPE               ATTRIBUTE_TYPE_SINT64

// Attribute Application Version
#define AF_APPLICATION_VERSION                                    2003
#define AF_APPLICATION_VERSION_SZ                                    8
#define AF_APPLICATION_VERSION_TYPE              ATTRIBUTE_TYPE_SINT64

// Attribute Profile Version
#define AF_PROFILE_VERSION                                        2004
#define AF_PROFILE_VERSION_SZ                                        8
#define AF_PROFILE_VERSION_TYPE                  ATTRIBUTE_TYPE_SINT64

// Attribute WiFi Version
#define AF_WIFI_VERSION                                           2006
#define AF_WIFI_VERSION_SZ                                           8
#define AF_WIFI_VERSION_TYPE                     ATTRIBUTE_TYPE_SINT64

// Attribute Offline Schedules Enabled
#define AF_OFFLINE_SCHEDULES_ENABLED                             59001
#define AF_OFFLINE_SCHEDULES_ENABLED_SZ                              2
#define AF_OFFLINE_SCHEDULES_ENABLED_TYPE        ATTRIBUTE_TYPE_SINT16

// Attribute UTC Offset Data
#define AF_SYSTEM_UTC_OFFSET_DATA                                65001
#define AF_SYSTEM_UTC_OFFSET_DATA_SZ                                 8
#define AF_SYSTEM_UTC_OFFSET_DATA_TYPE            ATTRIBUTE_TYPE_BYTES

// Attribute Connected SSID
#define AF_SYSTEM_CONNECTED_SSID                                 65004
#define AF_SYSTEM_CONNECTED_SSID_SZ                                 33
#define AF_SYSTEM_CONNECTED_SSID_TYPE             ATTRIBUTE_TYPE_UTF8S

// Attribute WiFi Bars
#define AF_SYSTEM_WIFI_BARS                                      65005
#define AF_SYSTEM_WIFI_BARS_SZ                                       1
#define AF_SYSTEM_WIFI_BARS_TYPE                  ATTRIBUTE_TYPE_SINT8

// Attribute WiFi Steady State
#define AF_SYSTEM_WIFI_STEADY_STATE                              65006
#define AF_SYSTEM_WIFI_STEADY_STATE_SZ                               1
#define AF_SYSTEM_WIFI_STEADY_STATE_TYPE          ATTRIBUTE_TYPE_SINT8

// Attribute Command
#define AF_SYSTEM_COMMAND                                        65012
#define AF_SYSTEM_COMMAND_SZ                                         4
#define AF_SYSTEM_COMMAND_TYPE                   ATTRIBUTE_TYPE_SINT32

// Attribute ASR State
#define AF_SYSTEM_ASR_STATE                                      65013
#define AF_SYSTEM_ASR_STATE_SZ                                       1
#define AF_SYSTEM_ASR_STATE_TYPE                  ATTRIBUTE_TYPE_SINT8

// Attribute Low Power Warn
#define AF_SYSTEM_LOW_POWER_WARN                                 65014
#define AF_SYSTEM_LOW_POWER_WARN_SZ                                  1
#define AF_SYSTEM_LOW_POWER_WARN_TYPE             ATTRIBUTE_TYPE_SINT8

// Attribute Linked Timestamp
#define AF_SYSTEM_LINKED_TIMESTAMP                               65015
#define AF_SYSTEM_LINKED_TIMESTAMP_SZ                                4
#define AF_SYSTEM_LINKED_TIMESTAMP_TYPE          ATTRIBUTE_TYPE_SINT32

// Attribute Reboot Reason
#define AF_SYSTEM_REBOOT_REASON                                  65019
#define AF_SYSTEM_REBOOT_REASON_SZ                                 100
#define AF_SYSTEM_REBOOT_REASON_TYPE              ATTRIBUTE_TYPE_UTF8S

// Attribute MCU Interface
#define AF_SYSTEM_MCU_INTERFACE                                  65021
#define AF_SYSTEM_MCU_INTERFACE_SZ                                   1
#define AF_SYSTEM_MCU_INTERFACE_TYPE              ATTRIBUTE_TYPE_SINT8
//endregion
