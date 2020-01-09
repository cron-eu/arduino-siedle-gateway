ARDUINO_LIBRARIES_DIR := ~/Documents/Arduino/libraries
ARDUINO_HARDWARE_DIR := ~/Library/Arduino15/packages/arduino/hardware
ARDUINO_LIBRARIES := Scheduler WiFiNINA

all: setup

setup: arduino-dev-setup

arduino-dev-setup: arduino_samd_platform arduino_libraries

arduino_samd_platform: $(ARDUINO_HARDWARE_DIR)/samd
arduino_libraries: $(addprefix $(ARDUINO_LIBRARIES_DIR)/,$(ARDUINO_LIBRARIES))

$(ARDUINO_HARDWARE_DIR)/%:
	arduino-cli core install arduino:$(notdir ${@})

$(ARDUINO_LIBRARIES_DIR)/%:
	arduino-cli lib install $(notdir ${@})
