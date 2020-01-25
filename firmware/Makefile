default: build

setup: check-git-clean
	pip install -U --user platformio

	platformio init --ide clion --board mkrwifi1010
	git checkout CMakeLists.txt .idea/clion.iml .idea/misc.xml .idea/platformio.iml

check-git-clean:
	@if ! [ -z "$(shell git status --untracked-files=no --porcelain)" ]; then exit 1 ; fi

build:
	platformio run

upload:
	platformio run --target upload
