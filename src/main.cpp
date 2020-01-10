#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "main.h"
#include "FocusNinjaControl.h"

FocusNinjaControl focusNinja;
int state = 0;

//Test parameters
float startPosition = 20;
float stepSizemm = 0.5;
int numberOfSteps = 50;
int stepCount = 0;

const char *ssid = "FocusNinja0001";
const char *password = "FocusNinja";
//const char *ssid = "GF_Guest";
//const char *password = "gevoelsfotografie";

AsyncWebServer server = AsyncWebServer(80);
WebSocketsServer webSocket = WebSocketsServer(1337);

uint8_t active_sockets[256] = {0};

void report(const char *buf)
{
  //Commented out, because it slows the system down
/*   for (int i = 0; i < 256; i++)
  {
    if (active_sockets[i])
    {
      webSocket.sendTXT(i, buf);
    }
  }*/
}

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t *payload,
                      size_t length)
{
  String command = String((char *)payload);
  float beginPosition, endPosition;
  int steps;

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
    Serial.printf("[%u] Received text: %s\n", client_num, payload);

    if (command.startsWith("stop"))
    {
      focusNinja.stop();
    }
    else if (command.startsWith("home"))
    {
      focusNinja.homeCarriage();
    } else if (command.startsWith("go"))
    {
      if (sscanf((char*)payload, "go %f %f %d", &beginPosition, &endPosition, &steps) == 3)
      {
        focusNinja.takePhotos(beginPosition, endPosition, steps);
        Serial.printf("Go from %f to %f in %d steps.\r\n", beginPosition, endPosition, steps);
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

void connectWifi()
{
  WiFi.softAP(ssid, password);
  //WiFi.begin(ssid, password);
  /* while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
   }
   */
  Serial.println(" started WiFi.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  focusNinja.setLogger(&report);
  if (!SPIFFS.begin(true))
  {
    Serial.println("Error mounting SPIFFS");
  }
  connectWifi();
  webSocket.onEvent(onWebSocketEvent);
  server.on("/", onIndexRequest);
  server.on("/style.css", onStyleRequest);
  server.begin();
  webSocket.begin();
}

void loop()
{
  webSocket.loop();
  focusNinja.motorControl();
}
