const https = require('https');
const util = require('util');
const siedle = require('./siedle-lib');


function decimalToRadix(d, radix, padding) {
  let s = Number(d).toString(radix);

  while (s.length < padding) {
    s = '0' + s;
  }

  return s;
}


const sendToSlack = (event, context, debug) => {
  const slackWebHook = debug ? process.env.SLACK_WEB_HOOK_DEBUG : process.env.SLACK_WEB_HOOK;

  if (!slackWebHook) {
    context.fail("SLACK_WEB_HOOK environment var not defined");
    return;
  }

  const cmd = +event.cmd;
  const cmdHex = decimalToRadix(cmd, 16,8);
  // noinspection JSUnresolvedVariable
  const timestamp = new Date(event.ts * 1000);
  const decoded = siedle.parseCmd(cmd);

  const message = {
    username: 'siedle-bot',
    blocks: [
      {
        type: "section",
        text: {
          type: "mrkdwn",
          text: debug ?
            `*${decoded.signalTitle}* [${decoded.srcTitle} => ${decoded.dstTitle}]` :
            `*${decoded.signalTitle}* [${decoded.srcTitle}]`,
        }
      },
    ],
  };


  if (debug) {
    message.blocks.push({
        type: 'context',
        elements: [
          {
            type: 'mrkdwn',
            text: `\`${cmdHex}\` @${timestamp.toLocaleString()}`,
          }
        ]
      },
    );
  }

  if (!debug && decoded.dst !== 372) {
    // "372" being "cron IT"
    console.log(`Not sending cmd ${cmdHex} to Slack because dst=${decoded.dst} not 23`);
    return;
  }

  // If the signal is "ringing", then render the button to open the door
  if (decoded.signal === 2 || decoded.signal === 12) {

    message.blocks.push({
      type: 'actions',
      elements: [
        {
          type: 'button',
          text: {
            type: 'plain_text',
            text: 'Unlock :unlock:',
            emoji: true,
          },
          value: siedle.getCmd(3, decoded.dst, decoded.src),
        }
      ]
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

exports.handler = (event, context) => {
  sendToSlack(event, context,true);
  sendToSlack(event, context,false);
};
