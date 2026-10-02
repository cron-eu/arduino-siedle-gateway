Connecting a gateway to AWS IoT
====

Each gateway connects to AWS IoT Core with an identity of its own: a *thing*, a certificate for it, and a policy
that says what the gateway may do. This guide sets them up in the AWS IoT console and hands the certificate to the
gateway on its setup page. No developer tools are needed: no openssl, AWS CLI, ESP-IDF or USB cable.

The gateway creates its private key itself (EC P-256) on first boot, and the key never leaves it. AWS IoT only gets
a certificate signing request (CSR) with the public key and signs a certificate for it.

1. [Create the policy](#1-create-the-policy) (once per AWS account)
2. [Open the setup page](#2-open-the-setup-page)
3. [Download the CSR](#3-download-the-csr)
4. [Create the thing and its certificate](#4-create-the-thing-and-its-certificate)
5. [Upload the certificate](#5-upload-the-certificate)

1. Create the policy
----

Once per AWS account, all gateways share it.

1. Open the [AWS IoT console](https://console.aws.amazon.com/iot/home) in the region the gateways should use, e.g.
   *Europe (Frankfurt)*.
2. Choose **Security**, **Policies**, **Create policy**.
3. Policy name: `SiedleGateway`.
4. Switch the policy document to **JSON** and paste the policy below. Replace `REGION` with the region code (e.g.
   `eu-central-1`) and `ACCOUNT` with the 12 digit account ID from the account menu at the top right.
5. Choose **Create**.

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

`${iot:Connection.Thing.ThingName}` is the thing attached to the certificate the gateway connects with, so one
policy fits all gateways. It only works because the gateway uses its thing name as MQTT client ID.

2. Open the setup page
----

The cloud settings are only available on the gateway's setup hotspot **`Siedle-Setup-XXXX`**, and only when someone
opened it on site:

- **New gateway** (no Wi-Fi configured yet): the hotspot is open from the start.
- **Installed gateway:** hold the BOOT button for 3 seconds and release it. The gateway stays on its network, the
  hotspot closes again after 10 minutes without clients.

Join the hotspot and open `http://192.168.4.1/` in a browser. The window phones and macOS pop up for hotspots can't
download files, so a real browser is the better choice for the next step.

The hotspot that opens by itself when the gateway has been offline for 5 minutes only offers the Wi-Fi setup: the
Cloud card shows the settings as locked. Holding the BOOT button for 3 seconds unlocks them.

3. Download the CSR
----

On the **Cloud** card, enter the **Thing name**, e.g. `SiedleGateway`, or one per gateway like
`SiedleGateway-Office`. Letters, digits, `-`, `_` and `:` are allowed. Choose **Download CSR** and keep the file,
e.g. `SiedleGateway.csr`. If the download doesn't start, copy the text that appears below the button into a file of
that name.

4. Create the thing and its certificate
----

The hotspot has no internet access: switch back to your usual network for this step.

1. In the AWS IoT console, choose **All devices**, **Things**, **Create things**, **Create a single thing**,
   **Next**.
2. **Thing name:** exactly the name from step 3. Choose **Next**.
3. **Configure device certificate:** choose **Upload CSR**, select the CSR file, choose **Next**.
4. **Attach policies to certificate:** select `SiedleGateway`, choose **Create thing**.
5. Download the **device certificate** (`…-certificate.pem.crt`). There is no private key to download, the gateway
   has it. The root CA files aren't needed either, the firmware brings them.

Then copy the **Device data endpoint** from **Settings** in the AWS IoT console. It ends in `-ats.iot.<region>.amazonaws.com`.

The same with the AWS CLI:

```bash
THING=SiedleGateway
aws iot create-thing --thing-name "$THING"
CERT_ARN=$(aws iot create-certificate-from-csr --set-as-active \
  --certificate-signing-request "file://$THING.csr" \
  --certificate-pem-outfile "$THING-certificate.pem.crt" \
  --query certificateArn --output text)
aws iot attach-thing-principal --thing-name "$THING" --principal "$CERT_ARN"
aws iot attach-policy --policy-name SiedleGateway --target "$CERT_ARN"
aws iot describe-endpoint --endpoint-type iot:Data-ATS
```

5. Upload the certificate
----

Back on the setup hotspot (hold the BOOT button for 3 seconds again if it closed in the meantime), fill in the
**Cloud** card:

- **Endpoint:** the device data endpoint from step 4.
- **Thing name:** as in step 3.
- **Certificate:** choose the `…-certificate.pem.crt` file, or paste its content.

Choose **Save**. The gateway only accepts a certificate created from its own CSR, and connects right away: once it
is on Wi-Fi, **Connection** changes to *Connected as SiedleGateway* within a few seconds. The **Certificate** row
shows the certificate's SHA-256 fingerprint as ID, AWS IoT uses it as certificate ID as well.

Then leave the hotspot. It closes after 10 minutes without clients.

Troubleshooting
----

While the connection fails, the Cloud card and the status on the regular web UI show why:

| Message                                           | Check                                                              |
|---------------------------------------------------|--------------------------------------------------------------------|
| Endpoint not found                                | the endpoint, compare it with **Settings** in the console          |
| Endpoint not trusted, use the -ats endpoint       | the endpoint must end in `-ats.iot.<region>.amazonaws.com`         |
| AWS IoT rejected the certificate, is it active?   | the certificate is active, and in the same region as the endpoint  |
| AWS IoT closed the connection, check the policy   | the policy and the thing are attached to the certificate, and the thing name matches |
| Connects once the gateway is on Wi-Fi             | the Wi-Fi setup, the gateway connects to AWS IoT after it          |

*This certificate is for another key* when uploading means the certificate was created from the CSR of another
gateway, or from this gateway's CSR before it got a new key. Download the CSR again and create a new certificate.

Replacing the certificate or the key
----

- **New certificate**, e.g. after revoking the old one: create a certificate from the gateway's current CSR (step
  3), attach the policy and the thing to it, upload it (step 5). The key stays.
- **New key:** **Cloud** card, **Device key**, **Create a new key**. The current certificate stops working right
  away. Download the new CSR and continue with step 4, then deactivate the old certificate in AWS IoT.
- **Lost gateway:** deactivate its certificate in the AWS IoT console (**Security**, **Certificates**).

Security notes
----

- The cloud settings decide where the gateway takes its commands from, and `siedle/send` can open the door. So they
  can only be changed by someone on site: on the setup hotspot opened with the BOOT button, or on the one of a
  gateway without Wi-Fi. On the regular network the web UI is read-only.
- Set a password for the setup hotspot on the **Gateway** card, so nobody else nearby can join it while it is open.
  Holding the BOOT button for 10 seconds forgets the Wi-Fi network and this password, the cloud identity stays.
- Until flash encryption is enabled (planned hardening phase), the private key is stored in plain text in flash.
  Anyone with physical access to the board can read it. If a gateway is lost, deactivate its certificate.
- The identity lives in the `nvs` partition. If the firmware ever has to erase it (corrupted or after a format
  change, which it logs), set the gateway up again with this guide.
