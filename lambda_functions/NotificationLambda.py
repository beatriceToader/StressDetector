import json
import boto3

def lambda_handler(event, context):

    #create the link to the AWS IoT Core and declare the topic where the messages are sent
    client = boto3.client('iot-data', region_name='us-east-1')
    topic = 'esp32/notify'

    #declare and initialize the message that is going to be send
    message = {
        "message": "Don't forget about your quiz!"
    }

    #publish the message on the MQTT topic
    response = client.publish(
        topic=topic,
        qos=1,
        payload=json.dumps(message)
    )

    return {
        'statusCode': 200,
        'body': json.dumps('Message sent!')
    }
