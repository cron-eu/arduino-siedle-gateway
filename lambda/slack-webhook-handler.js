const https = require('https');
const util = require('util');

exports.handler = (event, context) => {

  function decimalToHex(d, padding) {
    let hex = Number(d).toString(16);

    while (hex.length < padding) {
      hex = "0" + hex;
    }

    return hex;
  }

  const slackWebHook = process.env.SLACK_WEB_HOOK;
  const slackChannel = process.env.SLACK_CHANNEL;

  if (!slackWebHook) {
    context.fail("SLACK_WEB_HOOK environment var not defined");
    return;
  }

  if (!slackChannel) {
    context.fail("SLACK_CHANNEL environment var not defined");
    return;
  }

  // noinspection JSUnresolvedVariable
  const timestamp = new Date(event.ts * 1000);
  const message = {
    channel: slackChannel,
    username: 'siedle-bot',
    icon_emoji: ':bell:',
    blocks: [
      {
        type: "section",
        text: {
          type: "mrkdwn",
          text: `Received command \`${decimalToHex(event.cmd, 8)}\` @ ${timestamp.toLocaleString()}`,
        }
      },
    ],
  };

  const r = https.request(slackWebHook, {method: 'POST'}, res => {
    res.setEncoding('utf8');
    res.on('data', function (data) {
      // noinspection JSUnresolvedFunction
      context.succeed("Message Sent: " + data);
    });
  }).on('error', function (e) {
    context.fail("Failed: " + e);
  })

  r.write(util.format('%j', message));
  r.end();
};
