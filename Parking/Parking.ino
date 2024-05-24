#include <ESP8266WiFi.h>        // Include ESP8266WiFi library for WiFi features
#include <FirebaseESP8266.h>    // Include FirebaseESP8266 library for Firebase integration
#include <PubSubClient.h>       // Include the MQTT library
#include <Servo.h>              // Include the Servo library

#define SERVO_PIN D2            // Define the pin for the servo signal
Servo myservo;                  // Create a servo object to control a servo

#define red_led D3              // Define the pin for the red LED
#define green_led D4            // Define the pin for the green LED

bool door = false;              // Status variable for the parking door
bool s1 = false;                // Status variable for parking slot 1
bool s2 = false;                // Status variable for parking slot 2
bool s3 = false;                // Status variable for parking slot 3
bool previousdoorSensor = false;// Previous status variable for parking door sensor 
bool previousS1 = false;        // Previous status variable for parking slot 1
bool previousS2 = false;        // Previous status variable for parking slot 2
bool previousS3 = false;        // Previous status variable for parking slot 3

int doorsensor = D1;            // Pin assignment for the parking door sensor                
int slot1 = D5;                 // Pin assignment for parking slot 1
int slot2 = D6;                 // Pin assignment for parking slot 2
int slot3 = D7;                 // Pin assignment for parking slot 3

const char* broker = "192.168.137.1";    // MQTT broker address
const int port = 1883;                   // MQTT broker port
const char* topic1 = "parking/place1";   // MQTT topic for place 1
const char* topic2 = "parking/place2";   // MQTT topic for place 2
const char* topic3 = "parking/place3";   // MQTT topic for place 3
const char* carCountTopic = "parking/carcount";  // MQTT topic for car count

const char* WIFI_SSID = "Fatma";         // WiFi network SSID
const char* WIFI_PASS = "5112002015";    // WiFi network password

// Firebase Realtime Database URL and secret
const char* FIREBASE_HOST = "https://smart-parking-6e771-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "JDOZNkXHc3M8eYNVPFDkTOMe0mwRPaPpCz5TuvWG";

FirebaseData fbdo;                        // Firebase Data object
WiFiClient espClient;                     // WiFi client object
PubSubClient client(espClient);           // MQTT client instance

bool previousLedState = LOW;              // Previous state of the LED
int carCount = 0;                         // Variable to store the car count
int indoor = 0;
int carsInGarage = 0;                     // Variable to store the number of cars in the garage

void setup() {
  Serial.begin(115200);                   // Start serial communication
  pinMode(doorsensor, INPUT);             // Set parking door sensor as input
  pinMode(slot1, INPUT);                  // Set parking slot 1 as input
  pinMode(slot2, INPUT);                  // Set parking slot 2 as input
  pinMode(slot3, INPUT);                  // Set parking slot 3 as input 
  myservo.attach(SERVO_PIN);              // Attach the servo on pin D2
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);       // Connect to WiFi
  
  // Wait until WiFi is connected
  while(WiFi.status() != WL_CONNECTED) {  
    delay(1000);                          // Wait for 1 second
    Serial.println("Connecting to WiFi..."); // Print connection status
  }

  Serial.println("Connected to WiFi.");   // WiFi connected
  Serial.print("IP Address: ");           // Print IP Address
  Serial.println(WiFi.localIP());         // Print assigned IP Address
  
  pinMode(red_led, OUTPUT);               // Set red LED pin as output
  pinMode(green_led, OUTPUT);             // Set green LED pin as output
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); // Initialize Firebase connection
  Firebase.reconnectWiFi(true);           // Enable automatic reconnection to WiFi
  
  client.setServer(broker, port);         // Connect to MQTT broker
  
  // Wait until connected to MQTT broker
  while (!client.connected()) {           
    Serial.println("Connecting to MQTT broker..."); // Print connection status
    if (client.connect("Publisher")) {    // Connect to MQTT broker
      Serial.println("Connected to MQTT broker."); // Connection successful
    } else {
      Serial.println("Connection to MQTT broker failed. Retrying..."); // Retry connection
      delay(2000);                        // Wait for 2 seconds before retrying
    }
  }
}

void loop() {
  delay(2000);                            // Delay between measurements

  // Read current status of parking slots
  bool doorSensor = !digitalRead(doorsensor);
  bool currentS1 = !digitalRead(slot1);
  bool currentS2 = !digitalRead(slot2);
  bool currentS3 = !digitalRead(slot3);

  // Control the servo and LEDs based on the door sensor and cars in the garage
  if (doorSensor && carsInGarage < 3) {
    myservo.write(180);                   // Open the door (180 degrees)
    digitalWrite(green_led, HIGH);        // Turn on the green LED
    delay(3000);                          // Wait for 5 seconds
    myservo.write(0);                     // Close the door (0 degrees)
    digitalWrite(green_led, LOW);         // Turn off the green LED
  } else if (doorSensor && carsInGarage == 3) {
    digitalWrite(red_led, HIGH);          // Turn on the red LED if the garage is full
  } else {
    myservo.write(0);                     // Keep the door closed
    digitalWrite(green_led, LOW);         // Turn off the green LED
    digitalWrite(red_led, LOW);           // Turn off the red LED
  }

  // Update the number of cars in the garage
  updateCarsInGarage(currentS1, currentS2, currentS3);

  // Increment car count if LED changes from LOW to HIGH
  if (doorSensor && !previousdoorSensor) {
    indoor++;
    previousdoorSensor = doorSensor;
  }
  
  // Get current time
  unsigned long currentTime = millis() / 1000; // Convert milliseconds to seconds
  String formattedTime = formatTime(currentTime); // Format time as HH:MM:SS

  // Push car count and time to Firebase
  pushToFirebase("/parking/indoor/", currentTime, indoor, formattedTime);
  
  // Push cars in garage count and time to Firebase
  pushToFirebase("/parking/ingarage/", currentTime, carsInGarage, formattedTime);
  
  // Update Firebase with the number of cars at each place
  updateFirebasePlaceStatus("parking/place1", currentS1);
  updateFirebasePlaceStatus("parking/place2", currentS2);
  updateFirebasePlaceStatus("parking/place3", currentS3);
  updateFirebaseCarCount("parking/carcount", carsInGarage);
    
  // Publish status and car count to MQTT topics
  publishToMQTT(currentS1, currentS2, currentS3, carsInGarage);
}

// Function to update the number of cars in the garage
void updateCarsInGarage(bool s1, bool s2, bool s3) {
  // Increment if any place changes from false to true
  if (s1 && !previousS1) carsInGarage++;
  if (s2 && !previousS2) carsInGarage++;
  if (s3 && !previousS3) carsInGarage++;

  // Decrement if any place changes from true to false
  if (!s1 && previousS1) carsInGarage--;
  if (!s2 && previousS2) carsInGarage--;
  if (!s3 && previousS3) carsInGarage--;

  // Update previous place states
  previousS1 = s1;
  previousS2 = s2;
  previousS3 = s3;
}

// Function to push data to Firebase
void pushToFirebase(String path, unsigned long currentTime, int data, String formattedTime) {
  String firebasePath = path + String(currentTime); // Create the path for Firebase
  FirebaseJson json;                                // Create a JSON object
  json.add("carCount", data);                       // Add car count to JSON
  json.add("time", formattedTime);                  // Add formatted time to JSON
  
  if (Firebase.set(fbdo, firebasePath.c_str(), json)) // Push data to Firebase
    Serial.println("Pushed data to Firebase: " + firebasePath);
  else {
    Serial.println("Failed to push data to Firebase: " + firebasePath);
    Serial.println(fbdo.errorReason());             // Print error reason
  }
}

// Function to update place status in Firebase
void updateFirebasePlaceStatus(String placePath, bool status) {
  String statusPath = placePath + "/status";        // Create the status path for Firebase
  String carNumPath = placePath + "/carNum";        // Create the car number path for Firebase
  
  if (Firebase.setBool(fbdo, statusPath.c_str(), status)) { // Set status of the place in Firebase
    Serial.println("Updated status of " + placePath + " in Firebase.");
    if (status) { // If the place is occupied, increment car number
      Firebase.getInt(fbdo, carNumPath.c_str());
      int currentCarNum = fbdo.intData() + 1;
      if (Firebase.setInt(fbdo, carNumPath.c_str(), currentCarNum))
        Serial.println("Updated car number of " + placePath + " in Firebase.");
      else {
        Serial.println("Failed to update car number of " + placePath + " in Firebase.");
        Serial.println(fbdo.errorReason());          // Print error reason
      }
    }
  } else {
    Serial.println("Failed to update status of " + placePath + " in Firebase.");
    Serial.println(fbdo.errorReason());             // Print error reason
  }
}

// Function to update car count in Firebase
void updateFirebaseCarCount(String carcountPath, int carcount) {
  if (Firebase.setInt(fbdo, carcountPath.c_str(), carcount)) { // Update car count in Firebase
    Serial.println("Updated car count in Firebase.");
  } else {
    Serial.println("Failed to update car count in Firebase.");
    Serial.println(fbdo.errorReason());             // Print error reason
  }
}

// Function to publish data to MQTT topics
void publishToMQTT(bool s1, bool s2, bool s3, int carcount) {
  client.publish(topic1, String(s1).c_str());       // Publish status1
  Serial.print("Published status1: ");
  Serial.println(s1);
  client.publish(topic2, String(s2).c_str());       // Publish status2
  Serial.print("Published status2: ");
  Serial.println(s2);
  client.publish(topic3, String(s3).c_str());       // Publish status3
  Serial.print("Published status3: ");
  Serial.println(s3);
  client.publish(carCountTopic, String(carCount).c_str()); // Publish car count
  Serial.print("Published car count: ");
  Serial.println(carCount);
}

// Function to format time as HH:MM:SS
String formatTime(unsigned long currentTime) {
  unsigned long seconds = currentTime % 60;         // Extract seconds
  unsigned long minutes = (currentTime / 60) % 60;  // Extract minutes
  unsigned long hours = (currentTime / 3600) % 24;  // Extract hours
  return String(hours) + ":" + String(minutes) + ":" + String(seconds); // Return formatted time
}
