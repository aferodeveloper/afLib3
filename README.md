# Afero Secure Radio Arduino Library #

**afLib3 Version 1.0**

## Welcome ##

This library implements the SPI and UART protocols used to communicate between an MCU and the Afero Secure Radio Module. It provides a simple API for reading and writing attribute values from an MCU application.

This version of the library, called afLib3, supercedes and replaces previous Afero libraries named afLib and afLib2.

afLib3 differs significantly from previous versions by implementing a new more logical API, adding support for other MCUs and operating systems, and reducing several callback routines into a single callback.

However, afLib3 is also API compatible with afLib2. Your code running on afLib2 should not need to be updated to use this library, though updating your code to use the new APIs is strongly recommended for future compatibility.

*Please Note:* This new version of the Afero MCU Library, written in C, is intended to replace the older C++ version called simply *afLib*. You can have both the afLib and afLib3 packages installed at the same time, they are distinct from one another. Please use *afLib3* for all new development, but *afLib* will remain available for compatibility with existing projects that use it.

AfLib3 is packaged as an Arduino-compatible library for ease of installing in the Arduino IDE. For other MCU platforms see the Afero Developer Github at https://github.com/aferodeveloper/.


### Arduino Installation ###

* If you are familiar with using git, you can use it to clone this directory into your Arduino installation.
  From a command line, change directories to your Aduino "libraries" folder ("Documents/Arduino/libraries" for Mac, "My Documents/Ardino/libraries" for Windows) and clone this project with "git clone https://github.com/aferodeveloper/afLib3.git"

* If you don't use git, you can download this project from the "Clone or Download" button at the top of the page, then select "Download ZIP". Unzip the downloaded file "afLib3-master.zip" and it will create a folder called "afLib3-master". *Rename this folder* from "afLib3-master" to just "afLib3", then copy it to the Arduino/libraries folder. The folder must be named *afLib3* and not *afLib3-master* for the Arduino sotware to properly recognize it.

* After installation, restart the Arduino IDE if it was running.

### Upgrading an existing Arduino Installation ###

* If you've previously installed an older version of afLib3, delete the old folder under Arduino/libaries and then install this new version via the installation instructions above.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

afLib3 1.0.${BUILD_NUMBER} 7/23/18 Release Notes

* New, more logical API.
* Atmel SAMD21 support (UART only).
* Linux support for connectivity to ASR-1 and ASR-2 via UART, when used in conjunction with Firmware v2.0 and later
* Better handling of MCU attributes with default values. These are now differentiated in the callback when they're sent to the MCU.
* More granular event-handling in callback simplifies usage.
* MCU OTA support. You can download new firmware for your connected MCU via the Afero Platform.
Added function af_lib_asr_has_capability() to enable MCU to query the ASR firmware for supported features.
* New system attributes:
  AF_ONLINE_STATUS will tell you if ASR is connected to the Afero Cloud.
  AF_DEVICE_ID enables retrieval of the device ID using af_lib_get_attribute().
  AF_ASSOCIATION_ID enables retrieval of the association ID using af_lib_get_attribute()).
* New Module States:
  AF_MODULE_STATE_INITIALIZED indicates initialization is complete, and it's safe for MCU to call af_lib_set_attribute*().
  AF_MODULE_STATE_RELINKED can be used to differentiate a link state at reboot from a link state after the device may have dropped offline.

