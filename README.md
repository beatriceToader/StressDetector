# StressDetector

Code for a smart device that is built using three sensors: EDA, body tempeature and heart rate. The sensors send data to a cloud platform (in this case AWS) and get the stress level of the person that uses the device. The stress level is predicted using a Random Forest model. In this code are integrated:
- snippet that is used on the board to send data and receive data from the cloud platform
- snippets that are used as lambda functions for calling the model to predict the stress level, for sending notifications to the user and to email the user
- snippet that is used for training the Random Forest model used for prediction
