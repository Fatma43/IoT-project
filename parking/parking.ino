#include <ESP8266WiFi.h>                        // Include ESP8266WiFi library for WiFi features
#include <FirebaseESP8266.h>                    // Include FirebaseESP8266 library for Firebase integration
#include <PubSubClient.h>           // Include the MQTT library

#define LED_PIN D0                      // Define LED_PIN D1 (GPIO5)
int photoresistor = 0;                  // Define a that holds a the photoresistor reading
int threshold = 900;                    // Define a threshold variable
bool s1 = false;
bool s2 = false;
                
int slot1 = D1;
int slot2  = D2;

const char* broker = "192.168.137.1";  // MQTT broker address
const int port = 1883;               // MQTT broker port
const char* topic1 = "parking/place1"; // MQTT topic for place1
const char* topic2 = "parking/place2"; // MQTT topic for place1
const char* carCountTopic = "parking/carcount"; // MQTT topic for car count

const char* WIFI_SSID = "Fatma";               // Define the WiFi network SSID
const char* WIFI_PASS = "5112002015";           // Define the WiFi network password

// Firebase Realtime Database URL and secret
const char* FIREBASE_HOST = "https://smart-parking-6e771-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "JDOZNkXHc3M8eYNVPFDkTOMe0mwRPaPpCz5TuvWG";

FirebaseData fbdo;                              // Define Firebase Data object

WiFiClient espClient;                 // Create an object of the WiFiClient class
PubSubClient client(espClient);       // Create an MQTT client instance

bool previousLedState = LOW;                    // Variable to store the previous state of the LED
int carCount = 0;                               // Variable to store the car count


void setup() {
  Serial.begin(115200);                         // Start serial communication at 115200 baudrate

  pinMode(slot1, INPUT);
  pinMode(slot2, INPUT);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);             // Begin WiFi connection using SSID and password

  while(WiFi.status() != WL_CONNECTED){         // Check if the WiFi status is not connected
    delay(1000);                                // Wait 1 second between WiFi connection checks
    Serial.println("Connecting to WiFi...");    // Print message indicating an attempt to connect to WiFi
  }

  Serial.println("Connected to WiFi.");         // Print message when WiFi connection is successful
  Serial.print("IP Address: ");                 // Print the label for the IP address
  Serial.println(WiFi.localIP());               // Print the assigned IP address
  
  pinMode(LED_PIN, OUTPUT);             // Set LED_PIN as an output pin                                  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); // Initialize Firebase connection
  Firebase.reconnectWiFi(true);                 // Automatic reconnection to WiFi if connection is lost

  client.setServer(broker, port);                     // Connect to the MQTT broker
  while (!client.connected()) {                       // Wait until connected to the MQTT broker
    Serial.println("Connecting to MQTT broker...");   // A message indicating an attempt to connect to MQTT broker
    if (client.connect("Publisher")) {        // Connect to MQTT broker with the name "NodeMCU_Publisher"
      Serial.println("Connected to MQTT broker.");   // Successful connection to MQTT broker
    } else {
      Serial.println("Connection to MQTT broker failed. Retrying..."); // Failed connection, retrying
      delay(2000);                                    // Wait 2 seconds before retrying
    }
  }
}

void loop() {
  delay(2000);                                  // Wait 2 seconds between measurements
  photoresistor = analogRead(A0);       // Read the brightness of the light
  
  if (photoresistor < threshold)        // If the photoresistor value is below the threshold
    {digitalWrite(LED_PIN, HIGH);        // Turn on the LED
    }
  else                                  // Else
    {digitalWrite(LED_PIN, LOW);         // Turn off the LED
    }

  if(digitalRead(slot1))
    s1 = 0;
  else    
    s1 = 1;
    
  if(digitalRead(slot2))
    s2 = 0;
  else
    s2 = 1;

  bool currentLedState = digitalRead(LED_PIN);  // Read the current state of the LED
  
  // Increment car count if the LED changes from LOW to HIGH
  if (currentLedState == HIGH && previousLedState == LOW) {
    carCount++;
    // Store car count and timestamp in Firebase
    Firebase.setInt(fbdo, "parking/door", "carCount", carCount);
    Firebase.setInt(fbdo, "parking/door", "time", millis() / 1000);
  }

  // Update the previous LED state
  previousLedState = currentLedState;
   
  if(Firebase.setBool(fbdo, "parking/place1", s1)){   // Set slot1 status in Firebase
    Serial.println(s1);        // Print the value of slot1 status on serial monitor
  }
  else                                          // If Firebase operation fails,
    Serial.println(fbdo.errorReason());         // Print the error reason

  if(Firebase.setBool(fbdo, "parking/place2", s2)){   // Set slot2 status in Firebase
    Serial.println(s2);        // Print the value of slot2 status on serial monitor
  }
  else                                          // If Firebase operation fails,
    Serial.println(fbdo.errorReason());         // Print the error reason
    
  // Publish slot statuses to MQTT topics
  client.publish(topic1, String(s1).c_str());
  Serial.print("Published status1: ");
  Serial.println(s1);
  client.publish(topic2, String(s2).c_str());
  Serial.print("Published status2: ");
  Serial.println(s2);
  
  // Publish car count to MQTT
  client.publish(carCountTopic, String(carCount).c_str());
  Serial.print("Published car count: ");
  Serial.println(carCount);
}
