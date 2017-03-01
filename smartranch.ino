#include "TimeAlarms.h"
#include "MQTT.h"
// #undef now()

//#define BLYNK_DEBUG       // Uncomment this to see debug prints
#define BLYNK_PRINT Serial
#include "blynk.h"

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "123";

// Attach a Button widget (mode: Switch) to the Digital pin 7 - and control the built-in blue led.
// Attach a Graph widget to Analog pin 1
// Attach a Gauge widget to Analog pin 2

WidgetTable table;
BLYNK_ATTACH_WIDGET(table, V1);

// Static IP connection
//Global variables
boolean useStaticIP = true;


uint8_t retry_count = 0;
unsigned long old_time = millis();

// better wifi
const uint32_t msRetryDelay = 1 * 60000; // retry every min
const uint32_t msRetryTime  =   30000; // stop trying after 30sec
const uint32_t dslStartupTime  =   60000; // let dsl start up before connecting
int connectRetry = 0; // reintentos de conectar antes de apagar router

bool   retryRunning = false;

Timer retryTimer(msRetryDelay, retryConnect);  // timer to retry connecting
Timer stopTimer(msRetryTime, stopConnect);     // timer to stop a long running try


// Riego

const uint32_t frecuenciaRiego = 30 * 60;
const uint32_t tiempoRiegoCorto = 1 * 60;
const uint32_t tiempoRiegoLargo = 6 * 60;
const uint32_t mstiempoRiegoCorto = 1 * 20 * 1000;
const uint32_t mstiempoRiegoLargo = 5 * 60 * 1000;
Timer riegoCortoTimer(mstiempoRiegoCorto, cerrarRiego);
Timer riegoLargoTimer(mstiempoRiegoLargo, cerrarRiego);

AlarmID_t alarm1;

#define led_pin D7
#define valvula_plantines_pin D1
#define luces_general_pin D2
#define dsl_pin D3
#define valvula_pin D4

void callback(char* topic, byte* payload, unsigned int length);

/**
 * if want to use IP address,
 * byte server[] = { XXX,XXX,XXX,XXX };
 * MQTT client(server, 1883, callback);
 * want to use domain name,
 * MQTT client("www.sample.com", 1883, callback);
 **/
// byte server[] = { 192,168,1,100 };
// MQTT client(server, 1883, callback);

MQTT client("example.com", 1883, callback);

String PROJECT_TOPIC  =				"smartranch/";
String STATUS	=  			"smartranch/status";

// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    String t = String(topic);

    // Serial.print("topic:");
    // Serial.print(t);
    // Serial.println();
    // Serial.print("message:");
    // Serial.print(message);
    // Serial.println();
    
    if(t.equalsIgnoreCase(PROJECT_TOPIC+"in/valvula1"))
          riego(message);

    // delay(1000);
}

void setup() {
    // #if (PLATFORM_ID==6)  // for Photon Time issue
    // while(Time.year() < 2000) {
    //     Spark.process(); // wait for Time to sync
    //     sendEvent("Procesando...");
    // }
    // #endif
    delay(5000);
    Serial.begin(57600);
    delay(2000);
    Serial.println("Iniciado...");
    
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, LOW);
    
    pinMode(valvula_pin, OUTPUT);
    digitalWrite(valvula_pin, HIGH);
    
    pinMode(luces_general_pin, OUTPUT);
    digitalWrite(luces_general_pin, HIGH);
    
    pinMode(dsl_pin, OUTPUT);
    digitalWrite(dsl_pin, HIGH);
    
    Time.zone(-3);
    
    
    // máximo 6 alarmasss!!! #define dtNBR_ALARMS 6   // max is 255
    // Alarm.alarmRepeat(6,00,0, riegoLargo);
    // Alarm.alarmRepeat(10,00,0, riegoCorto);
    // Alarm.alarmRepeat(12,00,0, riegoLargo);
    // Alarm.alarmRepeat(14,00,0, riegoCorto);
    // Alarm.alarmRepeat(16,00,0, riegoCorto);
    Alarm.alarmRepeat(18,00,0, riegoLargo);

    // read wifi .INI and connect to network
    networkConnect();
    
    //RGB.control(true);
    
    // Better wifi connection
    // Particle.connect();
    if (!waitFor(Particle.connected, msRetryTime)){
        WiFi.off();                // no luck, no need for WiFi
    }
    Blynk.begin(auth);
    
    // connect to the server
    client.connect("photon");

    // publish/subscribe
    if (client.isConnected()) {
    		sendEvent("Iniciado :)");
    		client.subscribe("smartranch/#");
    }
}

void loop() {
  
    if (!retryRunning && !Particle.connected())
    { // if we have not already scheduled a retry and are not connected
        sendEvent("Agendado retry");
        // Alarm.timerOnce(msRetryTime, stopConnect);
        stopTimer.start();         // set timeout for auto-retry by system
        retryRunning = true;
        // Alarm.timerOnce(msRetryDelay, retryConnect);
        retryTimer.start();        // schedula a retry
        connectRetry++;
    }
    
    if (Particle.connected())
    {      
      Blynk.run();
    }
    
    if (client.isConnected()){
        // MQTT client loop processing
        client.loop();
    }else{
        Serial.println("conectando...");
        // clientID, username, MD5 encoded password
        // client.connect("arduino-mqtt", "john@m2m.io", "00000000000000000000000000000");
        
        // connect to the server
        client.connect("photon");
        client.publish(STATUS,"Re-conectando cliente mqtt.");
        client.subscribe(PROJECT_TOPIC);
    }
    // sendEvent("Ping");
    Alarm.delay(1000);
}


void retryConnect()
{
  if (!Particle.connected())   // if not connected to cloud
  {
    sendEvent("Reconectando");
    // Alarm.timerOnce(msRetryTime, stopConnect);
    stopTimer.start();         // set of the timout time
    networkConnect();
    // WiFi.on();
    // Particle.connect();        // start a reconnectino attempt
  }
  else                         // if already connected
  {
    sendEvent("Conectado!");
    retryTimer.stop();         // no further attempts required
    // id = Alarm.getTriggeredAlarmId();
    // Alarm.disable(id); 
    // cont char alarmsCount = 0;
    // alarmsCount = Alarm.count();
    // sendEvent(alarmsCount);
    retryRunning = false;
    connectRetry = 0;
  }
}

void stopConnect()
{
    sendEvent("Detenido intento de conexión");

    if (!Particle.connected()){ // if after retryTime no connection
        WiFi.off();              // stop trying and swith off WiFi
        // if (connectRetry == 1){
        //     switchDsl();
        //     sendEvent("Reintentos: " + String(connectRetry));
        // }
        // if (connectRetry == 2){
        //     sendEvent("Reintentos: " + String(connectRetry));
        //     sendEvent("Reset!!!");
        //     System.reset();
        //     switchDsl();
        //     connectRetry = 0;
        // }
    }
    
    stopTimer.stop();
}

void switchDsl(){
    digitalWrite(dsl_pin, LOW);
    delay(10000);
    digitalWrite(dsl_pin, HIGH);
    sendEvent("Reiniciando DSL...");
}

void networkConnect()
{
  char networkSSID[33] = "user";
  char networkPass[65] = "pass";

  // WLAN_SEC_UNSEC = 0
  // WLAN_SEC_WEP = 1
  // WLAN_SEC_WPA = 2
  // WLAN_SEC_WPA2 = 3
  int authValue = 3;

  // WLAN_CIPHER_NOT_SET = 0
  // WLAN_CIPHER_AES = 1
  // WLAN_CIPHER_TKIP = 2
  // WLAN_CIPHER_AES_TKIP = 3
  int cipherValue = 1;

  uint8_t serverIP[] = {192, 168, 1, 100};
  IPAddress myAddress = serverIP;
  uint8_t serverNM[] = {255, 255, 255, 0};
  IPAddress netmask = serverNM;
  uint8_t serverGW[] = {192, 168, 1, 1};
  IPAddress gateway = serverGW;
  uint8_t serverDNS[] = {192, 168, 1, 1};
  IPAddress dns = serverDNS;

  // Static IP or DHCP?
  if (useStaticIP)
  {
    Serial.print("Using Static IP Address: ");
    Serial.print(serverIP[0]);
    Serial.print(".");
    Serial.print(serverIP[1]);
    Serial.print(".");
    Serial.print(serverIP[2]);
    Serial.print(".");
    Serial.println(serverIP[3]);
    WiFi.setStaticIP(myAddress, netmask, gateway, dns);
    WiFi.useStaticIP();
  }
  else Serial.println("Using DHCP");

  if (Particle.connected() == false) {
    Serial.println("WiFi On...");
    WiFi.on();
    // Serial.println("Clearing Credentials...");
    //WiFi.clearCredentials();

    Serial.println("Setting Credentials...");
    // set Credentials
    if (authValue > -1 && cipherValue > -1)
    {
      WiFi.setCredentials(networkSSID, networkPass, authValue, cipherValue);
    }
    else if (authValue > -1)
    {
      WiFi.setCredentials(networkSSID, networkPass, authValue);
    }
    else WiFi.setCredentials(networkSSID, networkPass);
    WiFi.connect(WIFI_CONNECT_SKIP_LISTEN);
    if (WiFi.hasCredentials())
    {
      Serial.println("We have Credentials! Connecting to Cloud...");
      Particle.connect();
      while(!Particle.connected())
      {
        Particle.process();
      }
      Serial.println("Connected to:");
      Serial.println(WiFi.localIP());
      Serial.println(WiFi.subnetMask());
      Serial.println(WiFi.gatewayIP());
      Serial.println(WiFi.SSID());
    }
    else Serial.println("We have NO Credentials.");
  }
  Serial.println("---");
  Serial.print("deviceID: ");
  String myID = System.deviceID();
  Serial.println(myID);
  Serial.printlnf("System version: %s", System.version().c_str());
}


// Riego

int regando = 0;
String path_valvula1 = String(PROJECT_TOPIC+"valvula1");
// String periodoRiego = 'corto'


void riego(String message){
  if(message.equalsIgnoreCase("on"))
    abrirRiego();
  else if(message.equalsIgnoreCase("off"))
    cerrarRiego();
}

void riegoCorto(){
    abrirRiego();
    riegoCortoTimer.start();
    Blynk.virtualWrite(V4, HIGH); 
    sendEvent("Riego Corto");
}
void riegoLargo(){
    abrirRiego();
    riegoLargoTimer.start();
    Blynk.virtualWrite(V5, HIGH); 
    sendEvent("Riego Largo");
}

void abrirRiego(){
    digitalWrite(valvula_pin, LOW);
    Blynk.virtualWrite(V0, HIGH); 
    regando = 1;
    sendEvent("Riego On");
}
void cerrarRiego(){
    digitalWrite(valvula_pin, HIGH);
    Blynk.virtualWrite(V0, LOW); 
    Blynk.virtualWrite(V4, LOW); 
    Blynk.virtualWrite(V5, LOW); 
    regando = 0;
    sendEvent("Riego Off");
}

// controlador Blynk para riego
BLYNK_WRITE(V0)
{
  //BLYNK_LOG("Got a value: %s", param.asStr());
  // You can also use:
  int i = param.asInt();
  if(i==0){
      cerrarRiego();
      riegoCortoTimer.stop();
  }
  else if(i==1){
      riegoCorto();
  }
}

BLYNK_WRITE(V4)
{
  //BLYNK_LOG("Got a value: %s", param.asStr());
  // You can also use:
  int i = param.asInt();
  if(i==0){
      cerrarRiego();
      riegoCortoTimer.stop();
  }
  else if(i==1){
      riegoCorto();
  }
}
// controlador Blynk para riego
BLYNK_WRITE(V5)
{
  //BLYNK_LOG("Got a value: %s", param.asStr());
  // You can also use:
  int i = param.asInt();
  if(i==0){
      cerrarRiego();
      riegoLargoTimer.stop();
  }
  else if(i==1){
      riegoLargo();
  }
}

// Blynk table
int rowIndex = 0;

BLYNK_WRITE(V20) {
  if (param.asInt()) {  
    sendEvent("Hola Bruno!");
  }
}

// Button on V21 clears the table
BLYNK_WRITE(V21) {
  if (param.asInt()) {
    table.clear();
    rowIndex = 0;
  }
}
String event_date;
String event_time;

void myPublish(String topic, String message){
	String serial_msg = topic + ": " + message;
	Serial.println(serial_msg);
	// Serial.println("---------------------------------------------------------");
	blinkLed(100,3);
	// if(remote){
	String mqtt_topic = PROJECT_TOPIC + topic;
	// Particle.publish(mqtt_topic, message);
	client.publish(mqtt_topic,message);
	// 	blinkLed(100,3);
	// }
}

void sendEvent(String evento) {
    event_date = String(Time.day()) + "/" + String(Time.month());
    event_time = String(Time.format("%H:%M:%S"));
    evento = evento + " @ " + event_time;
    table.addRow(rowIndex, evento, event_date);
    table.pickRow(rowIndex);
    rowIndex++;
    Serial.println(evento); 
    client.publish(STATUS, evento);
    // uint32_t freemem = System.freeMemory();
    // Serial.print("free memory: ");
    // Serial.println(freemem);
}

//
void blinkLed(int n, int s){
	int pin = led_pin;
	int counter = 0;
	while (counter < n) {
      digitalWrite(pin, HIGH);
      delay(s);
      digitalWrite(pin, LOW);
      delay(s);
      counter++;
  }
}
