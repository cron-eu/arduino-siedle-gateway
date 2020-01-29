const https = require('https');
const util = require('util');
const siedle = require('./siedle');

const codeToTitle = {
  debug: 'Debug Command',
  bell_eg: 'Doorbell Ringing (EG) :mega:',
  bell_og: 'Doorbell Ringing (OG) :mega:',
  bell_whohoo: 'Doorbell whohoo (OG)',
  on_hook: 'Handheld on hook :sound:',
  off_hook: 'Handheld off hook :sound:',
  unlock_og: 'cron IT Unlock Door (OG) :unlock:',
  unlock_eg: 'cron IT Unlock Door (EG) :unlock:',
  light_on: 'Light ON :bulb:',
  light_on_2: 'Light ON :bulb:',
};

const cmdToCode = {};

for (let code of Object.keys(siedle.commands)) {
  cmdToCode[siedle.commands[code]] = code;
}

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

  const cmd = +event.cmd;
  const cmdHex = decimalToRadix(cmd, 16,8);
  // noinspection JSUnresolvedVariable
  const timestamp = new Date(event.ts * 1000);

  const message = {
    channel: slackChannel,
    username: 'siedle-bot',
    blocks: [
      {
        type: "section",
        text: {
          type: "mrkdwn",
          text: `Received command \`${cmdHex}\``,
        }
      },
      {
        type: 'context',
        elements: [
          {
            type: 'mrkdwn',
            text: `*Received:* ${timestamp.toLocaleString()}`,
          }
        ]
      },
      // {
      //   type: "section",
      //   text: {
      //     type: "mrkdwn",
      //     text: `> ${decimalToRadix(event.cmd, 2,32)}`,
      //   }
      // },
    ],
  };

  const cmdCode = cmdToCode[cmd];
  const cmdTitle = cmdCode ? codeToTitle[cmdCode] : undefined;

  if (cmdTitle) {

    let here = '';
    if (cmdCode === 'bell_eg' || cmdCode === "bell_og") {
      here = '@here ';
    }

    // replace the first block
    message.blocks.shift();
    message.blocks.unshift({
      type: "section",
      text: {
        type: "mrkdwn",
        text: `${here}*${cmdTitle}* (\`${cmdHex}\`)`,
      }
    });


    if (cmdCode === 'debug') {
      message.blocks.push({
        type: 'actions',
        elements: [
          {
            type: 'button',
            text: {
              type: 'plain_text',
              text: 'Debug Action',
              emoji: true,
            },
            value: "debug_" + cmdHex,
          }
        ]
      });
    }

    if (cmdCode === 'bell_eg' || cmdCode === 'bell_og') {
      message.blocks.push({
        type: 'actions',
        elements: [
          {
            type: 'button',
            text: {
              type: 'plain_text',
              text: 'Unlock the door!',
              emoji: true,
            },
            value: cmdCode === 'bell_eg' ? 'unlock_eg' : 'unlock_og',
          }
        ]
      });
    }
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
