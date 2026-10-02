const { IoTDataPlaneClient, PublishCommand } = require('@aws-sdk/client-iot-data-plane');

const destToCmdMapping = {
    eg: 1177621968,
    og: 1175262672,
    tg: 1178637776,
};

const ringToCmdMapping = {
    eg: 1155339024,
    tg: 1178801616,
    og: 1490882768,
}

const sendSiedleCommand = async (command) => {
  // SDK v3 needs a full URL, v2 also accepted a bare hostname here
  const broker = process.env.MQTT_BROKER;
  const iotdata = new IoTDataPlaneClient({endpoint: broker.includes('://') ? broker : `https://${broker}`})
  const params = {
    topic: 'siedle/send',
    payload: String(command),
    qos: 0
  }

  await iotdata.send(new PublishCommand(params));
}

const corsHeaders = {
    "Access-Control-Allow-Origin": "*" ,
    "Access-Control-Allow-Methods": "OPTIONS, POST",
    "Access-Control-Allow-Headers": "Origin, Content-Type, Accept, Authorization",
};

exports.handler = async (event) => {
    
    if (event.httpMethod === 'OPTIONS') {
        return {
            statusCode: 204,
            headers: corsHeaders,
        };
    }
    
    try {
        const params = event.queryStringParameters;
        
        if (!params) {
            throw Error('Invalid query params');
        }
        
        const dest = params['dest'];
        const action = params['action']
        
        if (!dest) {
            throw Error('Invalid query params (missing dest)');
        }

        if (!action) {
            throw Error('Invalid query params (missing action)');
        }
        
        const userID = event.requestContext.authorizer.claims.sub;
        
        if (!userID) {
            throw Error('invalid requestContext data');
        }
        
        let cmd;
        switch (action) {
            case 'ring':
                cmd = ringToCmdMapping[dest];
                break;
            case 'unlock':
                cmd = destToCmdMapping[dest];
                break;
            default:
                throw new Error('invalid action code');
                
        }

        if (!cmd) {
            throw Error(`Unknown destination: ${dest}`);
        }
        
        console.log(`Siedle cmd ${cmd} (action=${action}, ${dest}) from OAuth2 sub ${userID}.`);
        await sendSiedleCommand(cmd);
        
        return {
            statusCode: 200,
            headers: corsHeaders,
            body: JSON.stringify('OK'),
        };

    } catch (e) {
        return {
            statusCode: 400,
            headers: corsHeaders,
            body: JSON.stringify(e.message || 'unknown error'),
        }
    }
};
