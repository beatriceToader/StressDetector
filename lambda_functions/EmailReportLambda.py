import boto3
import requests
import json
from datetime import datetime, timedelta
import os

#AWS AppSync GraphQL endpoint definition
APPSYNC_API_URL = 'https://api_url/graphql'
APPSYNC_API_KEY = 'api_key'

#declare the admin's and the sender's emails
ADMIN_EMAIL = 'admin@gmail.com'
SENDER_EMAIL = 'sender@gmail.com'

#define a SES client to send emails using AWS SES
SES_CLIENT = boto3.client('ses', region_name='us-east-1')


def lambda_handler(event, context):

    #get the current date
    today = datetime.utcnow().strftime('%Y-%m-%d')

    #declare the subject and the body of the email
    email_subject = f"Daily Stress Levels Report for {today}"
    email_body = "Stress Levels Report:\n\n"


    #define the query to fetch the stress levels from the current date
    query = '''
    query ListStressLevels($date: String!) {
        listScores(filter: {createdAt: {beginsWith: $date}}) {
            items {
                email
                time
                score
            }
        }
    }
    '''

    #send the query to the AppSync endpoint and get the data
    response = requests.post(
        APPSYNC_API_URL,
        headers={
            'x-api-key': APPSYNC_API_KEY,
            'Content-Type': 'application/json'
        },
        json={
            'query': query,
            'variables': {'date': today}
        }
    )
    data = response.json()['data']['listScores']['items']

    #format the data retrieved from the database and add it to the email's body
    for item in data:
        time_db = datetime.strptime(item['time'], '%Y-%m-%dT%H:%M:%S.%fZ')
        time_formatted = time_db.strftime('%H:%M')
        if item['score'] == 0 :
            stress_string = "No Stress"
        elif item['score'] == 1:
            stress_string = "Low Stress"
        else:
            stress_string = "High Stress"
        email_body += f"Email: {item['email']}, Time: {time_formatted}, Stress Level: {stress_string}\n"

    #send the email to the admin
    SES_CLIENT.send_email(
        Source=SENDER_EMAIL,
        Destination={'ToAddresses': [ADMIN_EMAIL]},
        Message={
            'Subject': {'Data': email_subject},
            'Body': {'Text': {'Data': email_body}}
        }
    )

    return {
        'statusCode': 200,
        'body': json.dumps('Email sent successfully!')
    }