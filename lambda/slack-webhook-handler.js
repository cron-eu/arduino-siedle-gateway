const https = require('https');
const util = require('util');

const cmdMapping = {
  '58dd10d0': 'cron IT Doorbell Ringing :mega:',
  '408d15d0': 'cron IT Siedle Initiate Audio Stream Request',
  '460d15d0': 'cron IT Open Door Request',
};

const cmdToIconMapping = {
  '58dd10d0': ':bell:',
  '408d15d0': ':sound:',
  '460d15d0': ':white_check_mark:',
};

exports.handler = (event, context) => {

  function decimalToRadix(d, radix, padding) {
    let s = Number(d).toString(radix);

    while (s.length < padding) {
      s = '0' + s;
    }

    return s;
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

  const cmdHex = decimalToRadix(event.cmd, 16,8);
  const icon = cmdToIconMapping[cmdHex];

  // noinspection JSUnresolvedVariable
  const timestamp = new Date(event.ts * 1000);
  const message = {
    channel: slackChannel,
    username: 'siedle-bot',
    icon_emoji: icon || ':question:',
    blocks: [
      {
        type: "section",
        text: {
          type: "mrkdwn",
          text: `Received command \`${cmdHex}\` @ ${timestamp.toLocaleString()}`,
        }
      },
      {
        type: "section",
        text: {
          type: "mrkdwn",
          text: `> ${decimalToRadix(event.cmd, 2,32)}`,
        }
      },
    ],
  };

  const cmdDescription = cmdMapping[cmdHex];
  if (cmdDescription) {
    message.blocks.unshift({
      type: "section",
      text: {
        type: "mrkdwn",
        text: `${icon ? icon + ' ' : ''}*${cmdDescription}*`,
      }
    });
  }


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
