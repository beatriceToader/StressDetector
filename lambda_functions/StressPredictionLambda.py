import json
import boto3
import numpy as np
from joblib import load
import logging
from datetime import datetime, timedelta, timezone

logger = logging.getLogger()
logger.setLevel(logging.INFO)

#access the model, download it locally and load it
s3_client = boto3.client('s3')
bucket_name = 'random-forest-bucket'
file_name = 'stress_model.pkl'

s3_client.download_file(bucket_name, file_name ,'/tmp/stress_model.pkl')
model = load('/tmp/stress_model.pkl')

def lambda_handler(event, context):
    try:
        #extract the data and change its format and predict the stress level
        features = np.array([event['eda'], event['heart_rate'], event['temperature']]).reshape(1,-1)
        stressLevel = model.predict(features)[0]

        #obtain the current time in the Bucharest timezone
        bucharest_offset = timedelta(hours=3)
        bucharest_tz = timezone(bucharest_offset)
        utc_now = datetime.utcnow()
        bucharest_now = utc_now.replace(tzinfo=timezone.utc).astimezone(bucharest_tz)
        current_time_bucharest = bucharest_now.strftime('%Y-%m-%d %H:%M:%S')

        #prepare the item that is going to be inserted in the database
        item = {
            'id':{'S':context.aws_request_id},
            'eda':{'N': str(event['eda'])},
            'heart_rate':{'N': str(event['heart_rate'])},
            'temperature':{'N': str(event['temperature'])},
            'stressLevel':{'N':str(stressLevel)},
            'time':{'S':current_time_bucharest}
        }

        #store the item in the database
        dynamodb = boto3.client('dynamodb')
        dynamodb.put_item(TableName='StressPredictionScores', Item=item)

        #publish the data in the topic that is used to send messages to the ESP32
        iot_client = boto3.client('iot-data')
        iot_client.publish(
            topic='esp32/sub',
            qos=1,
            payload=json.dumps({'stressLevel':stressLevel})
        )

        return {
            'statusCode': 200,
            'body': json.dumps({'stressLevel':stressLevel})
        }

    except Exception as e:
        return {
            'statusCode': 500,
            'body': json.dumps({'error':str(e)})
        }