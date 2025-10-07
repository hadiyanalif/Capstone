#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <UrlEncode.h>

// Konfigurasi WiFi dan WhatsApp
const char* ssid = "HUAWEI-PLAY";
const char* password = "12341234";
String phoneNumber = "6289696203130";
String apiKey = "5484167";

// Pin assignments
#define WIND_SENSOR_PIN 4  // Anemometer Hall effect sensor
#define DHT_PIN 14          // DHT22 data pin
#define RAIN_SENSOR_PIN 19  // FC-37 analog output pin
#define BUTTON1_PIN 25      // Push button 1
#define BUTTON2_PIN 26      // Push button 2
#define BUTTON3_PIN 27      // Push button 3
#define SIRENE_PIN 1       // Sirine/Buzzer pin

// DHT22 sensor type
#define DHTTYPE DHT22

// Thresholds
#define WIND_SPEED_THRESHOLD 4.0  // Threshold for wind speed in m/s

// Global variables
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD I2C address
DHT dht(DHT_PIN, DHTTYPE);
volatile int mode = 0; // 0: Wind Speed, 1: Temperature/Humidity, 2: Rain Sensor

// Wind speed calculation variables
volatile unsigned long lastTime = 0;
volatile unsigned int counter = 0;

// Function prototypes
void IRAM_ATTR isr();
float getWindSpeed();

// Fungsi untuk mengirimkan pesan ke WhatsApp
void sendMessage(String temperature, String humidity, String windSpeed, String rainStatus) {
  // Buat pesan berdasarkan data sensor
  String message = "Suhu: " + temperature + " C\n";
  message += "Kelembaban: " + humidity + " %\n";
  message += "Kecepatan Angin: " + windSpeed + "\n";
  message += "Indikasi Hujan: " + rainStatus;

  // Data untuk dikirim dengan HTTP POST
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);
  HTTPClient http;
  http.begin(url);

  // Tetapkan header content-type
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Kirim permintaan HTTP POST
  int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200){
    Serial.print("Pesan terkirim dengan sukses");
  }
  else{
    Serial.println("Error mengirim pesan");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  // Bebaskan sumber daya
  http.end();
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());


  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.print("System Init...");

  // Initialize DHT22
  dht.begin();

  // Initialize pins
  pinMode(WIND_SENSOR_PIN, INPUT_PULLUP);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(SIRENE_PIN, OUTPUT);

  // Attach interrupt for wind speed sensor
  attachInterrupt(digitalPinToInterrupt(WIND_SENSOR_PIN), isr, FALLING);

  // Attach interrupts for push buttons
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), []{ mode = 0; }, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), []{ mode = 1; }, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON3_PIN), []{ mode = 2; }, FALLING);
}

void loop() {
  // Read wind speed and control sirene independently of mode
  float windSpeed = getWindSpeed();
  if (windSpeed > WIND_SPEED_THRESHOLD) {
    digitalWrite(SIRENE_PIN, HIGH);
  } else {
    digitalWrite(SIRENE_PIN, LOW);
  }

  // Display the selected mode on LCD
  switch (mode) {
    int sensorValue; // Deklarasi variabel sensorValue di awal fungsi

    case 0: // Wind Speed Mode
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Kecepatan Angin:");
      lcd.setCursor(0, 1);
      lcd.print(windSpeed);
      lcd.print(" m/s");
      break;

    case 1: // Temperature/Humidity Mode
      float temp;
      temp = dht.readTemperature();
      float humidity;
      humidity = dht.readHumidity();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Suhu: ");
      lcd.print(temp);
      lcd.print(" C");
      lcd.setCursor(0, 1);
      lcd.print("Kelembaban: ");
      lcd.print(humidity);
      lcd.print(" %");
      break;

    case 2: // Rain Sensor Mode
      sensorValue = digitalRead(RAIN_SENSOR_PIN); // Inisialisasi nilai sensorValue di dalam case
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Indikasi Hujan:");
      lcd.setCursor(0, 1);
      lcd.print(sensorValue == LOW ? "HUJAN" : "TIDAK HUJAN");
      delay(500);
      break;
  }

  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 10000) {
    String rainStatus = (digitalRead(RAIN_SENSOR_PIN) == LOW) ? "TERINDIKASI HUJAN" : "TIDAK HUJAN";
    sendMessage(String(dht.readTemperature()), String(dht.readHumidity()), String(windSpeed), rainStatus);
    delay(500);
    lastSendTime = millis();
  }


  delay(1000); // Refresh every second
}

void IRAM_ATTR isr() {
  unsigned long currentTime = millis();
  counter++;
  if (currentTime - lastTime >= 1000) {
    counter = 0;
    lastTime = currentTime;
  }
}

float getWindSpeed() {
  return (counter / 2.0) * 2; // Assuming 2 pulses per revolution and 2.4 m/s per revolution
}
