const https = require('https');
const util = require('util');

const cmdMapping = {
  '00000000': 'Debug Command',
  '58dd10d0': 'cron IT Doorbell Ringing :mega:',
  '408d15d0': 'cron IT Siedle Initiate Audio Stream Request',
  '460d15d0': 'cron IT Open Door Request',
  '50b115d0': 'cron IT Light ON Request',
  '560115d0': 'cron IT Light ON Request',
};

const cmdToIconMapping = {
  '00000000': ':rocket:',
  '58dd10d0': ':bell:',
  '408d15d0': ':sound:',
  '460d15d0': ':white_check_mark:',
  '50b115d0': ':bulb:',
  '560115d0': ':bulb:',
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

  const cmdDescription = cmdMapping[cmdHex];
  if (cmdDescription) {

    let here = '';
    if (cmdHex === '58dd10d0') {
      here = '@here ';
    }

    // replace the first block
    message.blocks.shift();
    message.blocks.unshift({
      type: "section",
      text: {
        type: "mrkdwn",
        text: `${here}*${cmdDescription}*${icon ? ' ' + icon : ''}`,
      }
    });


    if (cmdHex === '00000000') {
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

    if (cmdHex === '58dd10d0') {
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
            value: "unlock_" + cmdHex,
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
