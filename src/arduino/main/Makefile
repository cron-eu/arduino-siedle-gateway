ARDUINO_LIBRARIES_DIR := ~/Documents/Arduino/libraries
ARDUINO_HARDWARE_DIR := ~/Library/Arduino15/packages/arduino/hardware
ARDUINO_LIBRARIES := Scheduler WiFiNINA
ARDUINO_BOARD := arduino:samd:mkrwifi1010

all: build

setup: arduino-dev-setup

clean:
	rm -rf build

arduino-dev-setup: arduino_samd_platform arduino_libraries

arduino_samd_platform: $(ARDUINO_HARDWARE_DIR)/samd
arduino_libraries: $(addprefix $(ARDUINO_LIBRARIES_DIR)/,$(ARDUINO_LIBRARIES))

$(ARDUINO_HARDWARE_DIR)/%:
	arduino-cli core install arduino:$(notdir ${@})

$(ARDUINO_LIBRARIES_DIR)/%:
	arduino-cli lib install $(notdir ${@})

build: setup
	mkdir -p build
	arduino-cli compile --fqbn  $(ARDUINO_BOARD) -o build/main main.ino
