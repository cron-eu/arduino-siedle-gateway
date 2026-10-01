Per-device configuration (devcfg)
====

Everything that differs between devices lives in its own NVS partition `devcfg` (24 KB at `0x12000`), not in the
firmware image. So every device runs the same firmware, OTA updates never touch the identity, and holding the BOOT
button (which forgets Wi-Fi) doesn't either.

| Key           | Required for | Description                                                                 |
|---------------|--------------|-----------------------------------------------------------------------------|
| `mqtt_uri`    | cloud        | `mqtts://<endpoint>-ats.iot.<region>.amazonaws.com:8883`                    |
| `client_id`   | cloud        | MQTT client id, use the AWS IoT thing name                                  |
| `client_cert` | cloud        | Device certificate (PEM file)                                               |
| `client_key`  | cloud        | Device private key (PEM file)                                               |
| `ap_pass`     | –            | WPA2 password (8+ characters) of the setup hotspot. Open hotspot if unset   |
| `hostname`    | –            | Overrides the hostname (default `siedle`, reachable as `siedle.local`)      |

Without the cloud keys the gateway still works locally (web UI, Wi-Fi setup).

Provisioning
----

Everything in this directory except the README and the example is ignored by git, so keys never end up in the
repository.

### 1. Create the AWS IoT identity

Generate the key pair locally, so the private key never leaves your machine, and let AWS IoT sign it:

```bash
cd firmware-idf/devcfg
THING=SiedleGateway

openssl ecparam -name prime256v1 -genkey -noout -out private.pem.key
openssl req -new -key private.pem.key -subj "/CN=${THING}" -out device.csr

aws iot create-thing --thing-name "$THING"
CERT_ARN=$(aws iot create-certificate-from-csr --set-as-active \
  --certificate-signing-request file://device.csr \
  --certificate-pem-outfile certificate.pem.crt \
  --query certificateArn --output text)
aws iot attach-thing-principal --thing-name "$THING" --principal "$CERT_ARN"
aws iot attach-policy --policy-name SiedleGateway --target "$CERT_ARN"

# the endpoint for mqtt_uri, must be the -ats one (verified against the firmware's CA bundle)
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

The `SiedleGateway` policy (replace `REGION` and `ACCOUNT`). It relies on the client id being the thing name:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:client/${iot:Connection.Thing.ThingName}"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT:topic/siedle/received",
        "arn:aws:iot:REGION:ACCOUNT:topic/siedle/sent",
        "arn:aws:iot:REGION:ACCOUNT:topic/siedle/${iot:Connection.Thing.ThingName}/status"
      ]
    },
    {
      "Effect": "Allow",
      "Action": "iot:RetainPublish",
      "Resource": "arn:aws:iot:REGION:ACCOUNT:topic/siedle/${iot:Connection.Thing.ThingName}/status"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT:topicfilter/siedle/send",
        "arn:aws:iot:REGION:ACCOUNT:topicfilter/siedle/${iot:Connection.Thing.ThingName}/ota"
      ]
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": [
        "arn:aws:iot:REGION:ACCOUNT:topic/siedle/send",
        "arn:aws:iot:REGION:ACCOUNT:topic/siedle/${iot:Connection.Thing.ThingName}/ota"
      ]
    }
  ]
}
```

### 2. Write `devcfg.csv`

```bash
cp devcfg.example.csv devcfg.csv   # then fill in mqtt_uri, client_id and ap_pass
```

`@DEVCFG_DIR@` is replaced with the absolute path of this directory at build time. File entries are embedded as
strings. The format is described in the
[NVS partition generator docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html).

### 3. Flash

The build creates `build/devcfg.bin` whenever `devcfg.csv` exists, and `idf.py flash` writes it together with the
firmware. To update only the configuration:

```bash
idf.py build
parttool.py --port /dev/cu.usbserial-XXXX write_partition --partition-name devcfg --input build/devcfg.bin
```

For several devices, keep one directory per device outside the repository and select it at configure time (the
value is cached in the build directory):

```bash
idf.py -D DEVCFG_DIR=$HOME/siedle-devices/office reconfigure flash
```

Security note
----

Until flash encryption is enabled (planned hardening phase), the private key is stored in plain text. Anyone with
physical access to the board can read it. If a device is lost, revoke its certificate in AWS IoT.
