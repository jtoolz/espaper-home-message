
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include "HomeMessage.h"

const String HomeMessage::_APIRequestURL = "http://twilio-home-msg-api.appspot.com/_ah/api/home_message/v1/messages";

/*
 GET /_ah/api/home_message/v1/messages HTTP/1.1
Host: twilio-home-msg-api.appspot.com
Cache-Control: no-cache
Postman-Token: 76ffdd4a-c1f3-4c10-a485-848b304517c2
 */

HomeMessage::HomeMessage() {
}

void HomeMessage::updateMessage(HomeMsgFields *fields) {
  doUpdate(fields);
}

void HomeMessage::doUpdate(HomeMsgFields *fields) {
  this->fields = fields;
  JsonStreamingParser parser;
  parser.setListener(this);

  HTTPClient http;

  Serial.println("Update(): creating http connection with " + _APIRequestURL);
  http.begin(_APIRequestURL);
  bool isBody = false;
  char c;
  int size;
  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  if(httpCode > 0) {

    WiFiClient * client = http.getStreamPtr();
    //Serial.printf("Reading response...\n");
    while(client->connected()) {
      while((size = client->available()) > 0) {
        c = client->read();
        //Serial.printf("%c", c);
        if (c == '{' || c == '[') {

          isBody = true;
        }
        if (isBody) {
          parser.parse(c);
        }
      }
    }
    //Serial.printf("\n");
  }
  this->fields = nullptr;
}

void HomeMessage::whitespace(char c) {
  Serial.println("whitespace");
}

void HomeMessage::startDocument() {
  Serial.println("start document");
}

void HomeMessage::key(String key) {
  currentKey = String(key);
}

void HomeMessage::value(String value) {

  if (currentKey == "content") {
    fields->content = value;
  }

  if (currentKey == "sender_name") {
    fields->senderName = value;
  }

   if (currentKey == "sender_phone") {
    fields->senderPhone = value;
  }

   if (currentKey == "created") {
    fields->created = value;
  }
}

void HomeMessage::endArray() {

}


void HomeMessage::startObject() {
  currentParent = currentKey;
}

void HomeMessage::endObject() {
  currentParent = "";
}

void HomeMessage::endDocument() {

}

void HomeMessage::startArray() {

}