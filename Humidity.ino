#include "DHT.h"
#include <esp_task_wdt.h> // เรียกใช้ Watchdog สำหรับ ESP32

#define DHTPIN 13       
#define DHTPIN1 14      
#define DHTPIN2 27      
#define DHTPIN3 26
#define DHTTYPE DHT22 
#define LED 2           

// ตั้งค่า Sampling Rate และ Watchdog
const unsigned long interval = 2000;    // อ่านค่าทุก 2 วินาที
unsigned long lastMillis = 0;           // ตัวแปรเก็บเวลาล่าสุด
#define WDT_TIMEOUT 8                   // ถ้าค้างเกิน 8 วินาที ให้ Reset เครื่อง

DHT dht(DHTPIN, DHTTYPE); 
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);

float startSpray = 43.0; 
float stopSpray = 55.0;  
bool isSpraying = false; 

// ประกาศชื่อฟังก์ชันไว้ด้านบน ป้องกัน Error ของ Compiler บางเวอร์ชัน
void handleSensorError(float h1, float h2, float h3, float h4);
void blinkError(int times);

void setup() {
  Serial.begin(9600);
  
  // เริ่มต้น Watchdog Timer (แบบใหม่ สำหรับ ESP32 Core v3.x)
  Serial.println("Configuring Watchdog Timer...");
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000, // แปลง 8 วินาที เป็น 8000 มิลลิวินาที
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // ให้เช็คทุก Core ของซีพียู
    .trigger_panic = true             // ให้ Reset เมื่อค้าง
  };
  esp_task_wdt_init(&wdt_config);     // ส่งโครงสร้างตั้งค่าเข้าไป
  esp_task_wdt_add(NULL);             // เพิ่ม Task ปัจจุบันเข้าไปในระบบ Watchdog

  dht.begin();
  dht1.begin();
  dht2.begin();
  dht3.begin();
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  
  Serial.println("System Ready!");
}

void loop() {
  // --- ส่วนของ Watchdog ---
  esp_task_wdt_reset(); // "Feed the dog" ส่งสัญญาณบอกว่าระบบยังทำงานปกติ

  // --- ส่วนของ Sampling Rate (ทำงานทุกๆ 2 วินาที) ---
  if (millis() - lastMillis >= interval) {
    lastMillis = millis(); // บันทึกเวลาที่ทำงานล่าสุด

    float h1 = dht.readHumidity();      
    float h2 = dht1.readHumidity();      
    float h3 = dht2.readHumidity();   
    float h4 = dht3.readHumidity();   

    // ตรวจสอบ Error
    if (isnan(h1) || isnan(h2) || isnan(h3) || isnan(h4)) {
      Serial.println("Warning: มีเซนเซอร์บางตัวอ่านค่าไม่ได้!");
      handleSensorError(h1, h2, h3, h4); 
    }

    // ==========================================
    // ส่วนคำนวณค่าเฉลี่ยแบบไดนามิก (ตามจำนวนที่ Online)
    // ==========================================
    float totalHumidity = 0;
    int validSensors = 0;

    // เช็คทีละตัว ถ้าใช้งานได้ให้เอามาบวกและนับจำนวน
    if (!isnan(h1)) { totalHumidity += h1; validSensors++; }
    if (!isnan(h2)) { totalHumidity += h2; validSensors++; }
    if (!isnan(h3)) { totalHumidity += h3; validSensors++; }
    if (!isnan(h4)) { totalHumidity += h4; validSensors++; }

    // ถ้าไม่มีเซนเซอร์ตัวไหนอ่านค่าได้เลย ให้หยุดการทำงาน
    if (validSensors == 0) {
      Serial.println("Critical Error: เซนเซอร์หยุดทำงานทุกตัว! ปิดระบบปั๊มน้ำชั่วคราว");
      digitalWrite(LED, LOW);
      isSpraying = false;
      return; 
    }

    // หารหาค่าเฉลี่ยจากจำนวนเซนเซอร์ที่ทำงานได้จริง
    float Avg = totalHumidity / validSensors;

    Serial.print("เซนเซอร์ที่ทำงาน: "); Serial.print(validSensors); Serial.print(" ตัว | ");
    Serial.print("ความชื้นเฉลี่ย: "); Serial.print(Avg); Serial.print(" % | ");

    // ==========================================
    // โลจิกควบคุมการพ่นน้ำต่อเนื่อง
    // ==========================================
    if (Avg <= startSpray) {
      isSpraying = true; 
    } 
    else if (Avg >= stopSpray) {
      isSpraying = false;
    }

    if (isSpraying) {
      digitalWrite(LED, HIGH);
      Serial.println("สถานะ: [ กำลังพ่นน้ำ... ]");
    } else {
      digitalWrite(LED, LOW);
      Serial.println("สถานะ: [ หยุดพ่นน้ำ ]");
    }
  }
}

// ฟังก์ชันแยกสำหรับจัดการไฟกระพริบเมื่อมี Error
void handleSensorError(float h1, float h2, float h3, float h4) {
  esp_task_wdt_reset(); 
  if(isnan(h1)) blinkError(1); // ตัวที่ 1 พัง กระพริบ 1 ครั้ง
  if(isnan(h2)) blinkError(2); // ตัวที่ 2 พัง กระพริบ 2 ครั้ง
  if(isnan(h3)) blinkError(3); // ตัวที่ 3 พัง กระพริบ 3 ครั้ง
  if(isnan(h4)) blinkError(4); // ตัวที่ 4 พัง กระพริบ 4 ครั้ง
}

// ฟังก์ชันสั่งไฟกระพริบ
void blinkError(int times) {
  for(int i=0; i<times; i++) {
    digitalWrite(LED, HIGH); delay(300); 
    digitalWrite(LED, LOW);  delay(300);
    esp_task_wdt_reset(); // เลี้ยง Watchdog ไปด้วยตอนกระพริบไฟ
  }
  delay(1000); 
}