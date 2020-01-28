const crypto = require('crypto');
const AWS = require('aws-sdk')

const verifySlackSignature = (event) => {
  const timestamp = event.headers['X-Slack-Request-Timestamp'];
  const now = new Date().getTime() / 1000;

  const elapsed = now - timestamp;
  if (elapsed > 60 * 5) {
    // console.log(`elapsed: ${elapsed} sec.`);
    return false;
  }

  const body = new Buffer.from(event.body, 'base64').toString();

  const rawString = `v0:${timestamp}:${body}`;
  const hmac = crypto.createHmac('sha256', process.env['SLACK_SIGNING_SECRET']);
  const signature = event.headers['X-Slack-Signature'];
  const calculatedSignature = hmac.update(rawString).digest('hex');

  console.log(`signature: "${signature}"`);
  console.log(`calculatedSignature: "${calculatedSignature}"`);

  return `v0=${calculatedSignature}` === signature;
};

const sendSiedleCommand = async (command) => {
  const iotdata = new AWS.IotData({endpoint: process.env.MQTT_BROKER})
  const params = {
    topic: 'siedle/send',
    payload: String(command),
    qos: 0
  }

  const publish$ = new Promise( (resolve, reject) => {
    iotdata.publish(params, function(err, data) {
      if (err) { return reject(err) }
      resolve(data);
    });
  });

  const data = await publish$;

  console.log("Message sent.");
  console.log(data);
}

exports.handler = async (event) => {

  console.log(event);

  if (!verifySlackSignature(event)) {
    return {
      statusCode: 400,
      body: JSON.stringify("invalid signature"),
    }
  }

  await sendSiedleCommand(1175262672);

  return {
    statusCode: 200,
    body: JSON.stringify('OK'),
  };
};
