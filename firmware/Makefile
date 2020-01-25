default: build

setup:
	pip install -U --user platformio
	platformio init --ide clion --board mkrwifi1010
	git checkout CMakeLists.txt

build:
	platformio run

upload:
	platformio run --target upload
