#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "main.h"
#include "FocusNinjaControl.h"
#include <Update.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#define ACCESSPOINT_MODE
#define PREF_REPORT_LENGTH 32
#define MAX_PAYLOAD_SIZE 255
#define PREFERENCES_NAME "focusninja"

FocusNinjaControl focusNinja;
int state = 0;
uint64_t chipid = ESP.getEfuseMac();

//Test parameters
float startPosition = 20;
float stepSizemm = 0.5;
int numberOfSteps = 50;
int stepCount = 0;

Preferences preferences;

#ifdef ACCESSPOINT_MODE
char ssid[20] = {0};
//const char *ssid = "FocusNinja0001";
const char *password = "focusninja";
#else
const char *ssid = "GF_Guest";
const char *password = "gevoelsfotografie";
#endif

AsyncWebServer server = AsyncWebServer(80);
WebSocketsServer webSocket = WebSocketsServer(1337);

uint8_t active_sockets[256] = {0};

void report(const char *buf)
{
#ifndef ACCESSPOINT_MODE
  // if we are not in access point mode,
  // we should try to reconnect if we lost the wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWifi();
  }
#endif
  for (int i = 0; i < 256; i++)
  {
    if (active_sockets[i])
    {
      webSocket.sendTXT(i, buf);
    }
  }
}

void reportIntPreference(const char *name)
{
  char buf[PREF_REPORT_LENGTH + 1]; // extra for terminating 0

  preferences.begin(PREFERENCES_NAME, true);

  uint32_t value = preferences.getUInt(name);
  preferences.end();

  Serial.printf("Reporting preference %s: %d\r\n", name, value);

  snprintf(buf, PREF_REPORT_LENGTH, "pref i %s %d", name, value);
  report(buf);
}

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t *payload,
                      size_t length)
{
  float beginPosition, endPosition, jogSize;
  int steps;

  // since there is no guarantee that payload is terminated by 0,
  // we copy it into a buffer that we terminate with 0 ourselves
  char buffer[MAX_PAYLOAD_SIZE + 1];

  memcpy(buffer, payload, length);

  if (length < MAX_PAYLOAD_SIZE)
  {
    buffer[length] = 0;
  }
  else
  {
    buffer[MAX_PAYLOAD_SIZE] = 0;
  }

  // now the buffer is 0-terminated, so this will be fine.
  String command = String(buffer);

  switch (type)
  {
  // Client has disconnected
  case WStype_DISCONNECTED:
    active_sockets[client_num] = 0;
    Serial.printf("[%u] Disconnected!\n", client_num);
    break;

  // New client has connected
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(client_num);
    Serial.printf("[%u] Connection from ", client_num);
    Serial.println(ip.toString());
    active_sockets[client_num] = 1;
    focusNinja.reportPosition();
  }
  break;

  // Handle text messages from client
  case WStype_TEXT:

    // Print out raw message
    Serial.printf("[%u] Received text: %s\n", client_num, command.c_str());

    if (command.startsWith("stop"))
    {
      focusNinja.stop();
    }
    else if (command.startsWith("home"))
    {
      focusNinja.homeCarriage();
    }
    else if (command.startsWith("go"))
    {
      if (sscanf(command.c_str(), "go %f %f %d", &beginPosition, &endPosition, &steps) == 3)
      {
        // refresh all settings
        preferences.begin(PREFERENCES_NAME, true);
        focusNinja.shutterDelay = preferences.getUInt("sd", 2000);
        focusNinja.shutterAfterDelay = preferences.getUInt("ad", 1000);
        focusNinja.triggerTime = preferences.getUInt("tt", 80);
        preferences.end();

        focusNinja.takePhotos(beginPosition, endPosition, steps);
        Serial.printf("Go from %f to %f in %d steps.\r\n", beginPosition, endPosition, steps);
      }
    }
    else if (command.startsWith("jog "))
    {
      if (sscanf(command.c_str(), "jog %f", &jogSize) == 1)
      {
        if (jogSize < 0)
        {
          focusNinja.moveCarriage(-jogSize, BACKWARDS);
        }
        else
        {
          focusNinja.moveCarriage(jogSize, FORWARDS);
        }
      }
    }
    else if (command.startsWith("pref i "))
    {
      uint32_t pref_value;
      char name[3];

      if (9 == command.length()) // length of "pref i XX"
      {
        // no value, just return it
        reportIntPreference(command.substring(7).c_str());
      }
      else if (sscanf(command.c_str(), "pref i %2s %u", name, &pref_value) == 2)
      {
        preferences.begin(PREFERENCES_NAME, false);

        preferences.putUInt(name, pref_value);

        preferences.end();
      }
      else
      {
        Serial.printf("Unknown preferences: %s\r\n", command.c_str());
      }
    }
    break;

  // For everything else: do nothing
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    Serial.printf("Got some other websocket value: %d\r\n", type);
  default:
    break;
  }
}

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest *request)
{
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");
}

// Callback: send homepage
void onStyleRequest(AsyncWebServerRequest *request)
{
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}

void onUpdateRequest(AsyncWebServerRequest *request)
{
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/update.html", "text/html");
}

void handleDoUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    Serial.println("Update");
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
    {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }

  if (final)
  {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true))
    {
      Update.printError(Serial);
    }
    else
    {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void connectWifi()
{
#ifdef ACCESSPOINT_MODE
  WiFi.softAP(ssid, password);
  //WiFi.softAP("FocusNinjaSSiD", "focusninja");
#else
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" started WiFi.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
#endif
}

void setup()
{
  Serial.begin(115200);

  focusNinja.setLogger(&report);

  preferences.begin(PREFERENCES_NAME, false);

  focusNinja.shutterDelay = preferences.getUInt("sd", 2000000);
  focusNinja.shutterAfterDelay = preferences.getUInt("ad", 1000000);
  focusNinja.triggerTime = preferences.getUInt("tt", 80000);

  preferences.putUInt("sd", focusNinja.shutterDelay);
  preferences.putUInt("ad", focusNinja.shutterAfterDelay);
  preferences.putUInt("tt", focusNinja.triggerTime);

  preferences.end();

  sprintf(ssid, "FocusNinja-%4x", (uint32_t)chipid);
  //sprintf(ssid, "FocusNinja");

  if (!SPIFFS.begin(true))
  {
    Serial.println("Error mounting SPIFFS");
  }
  connectWifi();

  webSocket.onEvent(onWebSocketEvent);
  server.on("/", onIndexRequest);
  server.on("/style.css", onStyleRequest);
  server.on("/update.html", onUpdateRequest);
  server.on(
      "/doUpdate", HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
         size_t len, bool final) { handleDoUpdate(request, filename, index, data, len, final); });
  server.begin();
  webSocket.begin();
  MDNS.begin("focusninja");

  focusNinja.homeCarriage();
}

void loop()
{
  webSocket.loop();
  focusNinja.stateMachine();
}
