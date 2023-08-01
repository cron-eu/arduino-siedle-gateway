default: build

src/aws_iot_secrets.h:
	aws s3 cp s3://cron.cm/arduino-doorbell/aws_iot_secrets.h src/

setup: src/aws_iot_secrets.h

check-git-clean:
	@if ! [ -z "$(shell git status --untracked-files=no --porcelain)" ]; then exit 1 ; fi

build: setup
	platformio run

upload:
	platformio run --target upload
