#include <SoftwareSerial.h>
#include <String.h>
#include <DHT.h>

// GPRS communication pins
SoftwareSerial gprsSerial(7, 8);

// DHT sensor definitions
#define DHTPIN 2
DHT dht(DHTPIN, DHT11);

// Moisture sensor pin
int moistureSensorPin = A1;  // Analog pin for moisture sensor

// pH sensor definitions and calibration
#define SensorPin A0           // pH sensor connected to analog pin A0
unsigned long int avgValue;    // Store the average value of the sensor feedback
float slope, intercept;
int buf[10], temp;

void setup() {
  gprsSerial.begin(9600);     // the GPRS baud rate   
  Serial.begin(9600);         // serial communication baud rate 
  dht.begin();                // initialize DHT sensor
  
  pinMode(13, OUTPUT);        // Optional LED for pH sensor feedback

  // Calibration points for PH-4502C sensor
  float calibrationVoltageAt7 = 2.50;  // Voltage at pH 7.0 (replace with your measured value)
  float calibrationVoltageAt4 = 2.95;  // Voltage at pH 4.0 (replace with your measured value)
  
  // Calculate slope and intercept based on calibration points
  slope = (7.0 - 4.0) / (calibrationVoltageAt7 - calibrationVoltageAt4);
  intercept = 7.0 - slope * calibrationVoltageAt7;

  delay(1000);
}

void loop() {
  // Read DHT sensor values
  float Humidity = dht.readHumidity();
  float Temperature = dht.readTemperature();

  // Read Moisture sensor value and convert to percentage
  int moistureValue = analogRead(moistureSensorPin);
  int Moisture = map(moistureValue, 1023, 0, 0, 100);

  // pH sensor reading and calculation
  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(SensorPin);
    delay(10);
  }

  // Sort the analog readings from smallest to largest
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }

  avgValue = 0;
  for (int i = 2; i < 8; i++) {   // Average the middle 6 values to reduce noise
    avgValue += buf[i];
  }

  float voltage = (float)avgValue * 5.0 / 1024 / 6;   // Convert to voltage (0-5V)

  // Calculate the pH value using the calibrated slope and intercept
  float phValue = slope * voltage + intercept;

  // Print sensor data to the Serial Monitor
  Serial.print("Temperature = ");
  Serial.print(Temperature);
  Serial.println(" °C");
  
  Serial.print("Humidity = ");
  Serial.print(Humidity);
  Serial.println(" %");
  
  Serial.print("Moisture Percentage: ");
  Serial.print(Moisture);
  Serial.println(" %");

  Serial.print("pH: ");
  Serial.print(phValue, 2);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.println(" V");

  digitalWrite(13, HIGH);   // Toggle an LED (optional visual indicator)
  delay(800);
  digitalWrite(13, LOW);

  // GPRS communication
  if (gprsSerial.available()) {
    Serial.write(gprsSerial.read());
  }

  gprsSerial.println("AT");
  delay(1000);
  
  gprsSerial.println("AT+CPIN?");
  delay(1000);
  
  gprsSerial.println("AT+CREG?");
  delay(1000);
  
  gprsSerial.println("AT+CGATT?");
  delay(1000);
  
  gprsSerial.println("AT+CIPSHUT");
  delay(1000);
  
  gprsSerial.println("AT+CIPSTATUS");
  delay(2000);
  
  gprsSerial.println("AT+CIPMUX=0");
  delay(2000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CSTT=\"airtelgprs.com\""); // start task and setting the APN
  delay(1000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIICR"); // bring up wireless connection
  delay(3000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIFSR"); // get local IP address
  delay(2000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIPSPRT=0");
  delay(3000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\""); // start up the connection
  delay(6000);
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIPSEND"); // begin send data to remote server
  delay(4000);
  ShowSerialData();
  
  // Send pH and moisture data to ThingSpeak
  String str = "GET https://api.thingspeak.com/update?api_key=OXTED0516ECSHNOZ&field1=" + String(Temperature) + "&field2=" + String(Humidity) + "&field3=" + String(Moisture) + "&field4=" + String(phValue);
  Serial.println(str);
  gprsSerial.println(str); // send data to remote server
  
  delay(4000);
  ShowSerialData();
  
  gprsSerial.println((char)26); // sending
  delay(5000); // waiting for reply, important! the time depends on the condition of internet 
  gprsSerial.println();
  
  ShowSerialData();
  
  gprsSerial.println("AT+CIPSHUT"); // close the connection
  delay(100);
  ShowSerialData();

  // Send SMS if moisture is low
  if (Moisture < 10) {
    sendSMS("Alert! Moisture low: " + String(Moisture) + "%. Water pump ON. Temperature: " + String(Temperature) + "°C, Humidity: " + String(Humidity) + "%, pH: " + String(phValue));
  }
}

void sendSMS(String message) {
  gprsSerial.println("AT+CMGF=1"); // Set SMS mode
  delay(1000);
  
  gprsSerial.println("AT+CMGS=\"+91xxxxxxxxxx\""); // Specify the phone number
  delay(1000);
  
  gprsSerial.print(message); // Message content
  delay(1000);
  
  gprsSerial.println((char)26); // Send message
  delay(1000);
  
  ShowSerialData(); // Display response from the GSM module
}

void ShowSerialData() {
  while (gprsSerial.available() != 0) {
    Serial.write(gprsSerial.read());
  }
  delay(5000); 
}
