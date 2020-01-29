const crypto = require('crypto');
const AWS = require('aws-sdk')
const querystring = require('querystring');
// const timeout = ms => new Promise(res => setTimeout(res, ms));
const siedle = require('./siedle');

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

  await publish$;
}

// This will decode the payload JSON encoded data from the www-urlencoded body..
// @see https://blog.summercat.com/using-aws-lambda-and-api-gateway-as-html-form-endpoint.html
const getSlackPayloadFromRequest = (event) => {
  const body = new Buffer.from(event.body, 'base64').toString();
  const params = querystring.parse(body);
  /**
   * @type {string}
   */
  const payload = params['payload'];
  return JSON.parse(payload);
};

exports.handler = async (event) => {

  if (!verifySlackSignature(event)) {
    return {
      statusCode: 400,
      body: JSON.stringify({ error: "Invalid signature." }),
    }
  }

  const payload = getSlackPayloadFromRequest(event);

  const action = payload.actions[0].value;

  const cmd = siedle.commands[action];
  if (cmd) {
    console.log(`Siedle cmd ${action} from user ${payload.user.name}.`);
    await sendSiedleCommand(siedle.commands.unlock_og);
  } else {
    return {
      statusCode: 400,
      body: JSON.stringify({error: `Could not find the associated command for action "${action}".`}),
    }
  }

  return {
    statusCode: 200,
    body: JSON.stringify('OK'),
  };
};
