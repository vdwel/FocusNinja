#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include "main.h"
#include "FocusNinjaControl.h"
#include <Update.h>
#include <ESPmDNS.h>

#define ACCESSPOINT_MODE

FocusNinjaControl focusNinja;
int state = 0;
uint64_t chipid=ESP.getEfuseMac();

//Test parameters
float startPosition = 20;
float stepSizemm = 0.5;
int numberOfSteps = 50;
int stepCount = 0;

#ifdef ACCESSPOINT_MODE
  char ssid[19];
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
  for (int i = 0; i < 256; i++)
  {
    if (active_sockets[i])
    {
      webSocket.sendTXT(i, buf);
    }
  }
}

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t *payload,
                      size_t length)
{
  String command = String((char *)payload);
  float beginPosition, endPosition, jogSize;
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
    } 
    else if (command.startsWith("go"))
    {
      if (sscanf((char*)payload, "go %f %f %d", &beginPosition, &endPosition, &steps) == 3)
      {
        focusNinja.takePhotos(beginPosition, endPosition, steps);
        Serial.printf("Go from %f to %f in %d steps.\r\n", beginPosition, endPosition, steps);
      }    
    }
    else if (command.startsWith("jogsize"))
    {
      if (sscanf((char*)payload, "jogsize %f", &jogSize) == 1)
      {
        focusNinja.jogSize = jogSize;
        Serial.printf("Setting jogSize to %f\r\n", jogSize);
      }  
    }
    else if (command.startsWith("jogfw"))
    {
        focusNinja.moveCarriage(focusNinja.jogSize, FORWARDS);
    }
    else if (command.startsWith("jogbw"))
    {
        focusNinja.moveCarriage(focusNinja.jogSize, BACKWARDS);
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

void onUpdateRequest(AsyncWebServerRequest *request)
{
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/update.html", "text/html");
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    Serial.println("Update");
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
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
#else
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
#endif
  Serial.println(" started WiFi.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

//This loop runs on the second core
void loop2(){
  //report position every 0.5 seconds
  delayMicroseconds(100000);
  focusNinja.reportPosition();
}

void coreTask0(void * pvParameters){
  while(true){
    vTaskDelay(10);
    loop2();
  }
}

void setup()
{
  Serial.begin(115200);
  focusNinja.setLogger(&report);
  sprintf(ssid,"FocusNinja-%4x",(uint32_t)chipid);

  if (!SPIFFS.begin(true))
  {
    Serial.println("Error mounting SPIFFS");
  }
  connectWifi();
  webSocket.onEvent(onWebSocketEvent);
  server.on("/", onIndexRequest);
  server.on("/style.css", onStyleRequest);
  server.on("/update.html", onUpdateRequest);
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.begin();
  webSocket.begin();
  MDNS.begin("focusninja");
  
  xTaskCreatePinnedToCore(
                    coreTask0,   // Function to implement the task 
                    "coreTask0", // Name of the task
                    10000,      // Stack size in words
                    NULL,       // Task input parameter
                    0,          // Priority of the task
                    NULL,       // Task handle.
                    0);  // Core where the task should run

  focusNinja.homeCarriage();
}

void loop()
{
  webSocket.loop();
  focusNinja.motorControl();
}
