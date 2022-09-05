
time_t requestSync() {
  timeClient.update();
  //Serial.write(TIME_REQUEST);
  //return 0; // the time will be sent later in response to serial mesg
  return (timeClient.getEpochTime());
}
