const crypto = require('crypto');
const AWS = require('aws-sdk')
const querystring = require('querystring');
// const timeout = ms => new Promise(res => setTimeout(res, ms));

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

  if (payload) {
    return JSON.parse(payload);
  } else {
    return null;
  }
};

const getParamsFromRequest = (event) => {
  const body = new Buffer.from(event.body, 'base64').toString();
  return querystring.parse(body);
}

exports.handler = async (event) => {

  if (!verifySlackSignature(event)) {
    return {
      statusCode: 400,
      body: JSON.stringify({ error: "Invalid signature." }),
    }
  }

  const payload = getSlackPayloadFromRequest(event);

  if (payload) {
    const cmd = payload.actions[0].value;

    console.log(`Siedle cmd ${cmd} from user ${payload.user.name}.`);
    await sendSiedleCommand(cmd);

    return {
      statusCode: 200,
      body: JSON.stringify('OK'),
    };
  } else {
    const params = getParamsFromRequest(event);

    const text = params.text.trim().toUpperCase();

    if (params.command === '/siedle') {
      switch (text) {
        case "UNLOCK":
          await sendSiedleCommand(1177621968);
          break;
        case "EG":
          await sendSiedleCommand(1177621968);
          break;
        case "OG":
          await sendSiedleCommand(1175262672);
          break;
        case "TG":
          await sendSiedleCommand(1178637776);
          break;
        default:
          console.log(`Unknown parameter ${text}`);
          return {
            statusCode: 200,
            body: `Parameter "${text}" invalid. Use \`/siedle EG | OG | TG\``,
          }
      }
    }

    return {
      statusCode: 200,
      body: `Unlock ${text} :thumbsup:`,
    };

  }

};
