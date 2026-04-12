#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define PIN_DHT    5    
#define TYPE_DHT   DHT11
#define PIN_LDR_AO 34   
#define THRESHOLD_GELAP 2400   

DHT dht(PIN_DHT, TYPE_DHT);

int   hitungan      = 0;
float totalSuhu     = 0;
float totalKelembapan = 0;
float totalCahaya   = 0;

// Timer non-blocking (pengganti delay)
unsigned long lastTick = 0;
const unsigned long INTERVAL_MS = 1000;  

void setup() {
  Serial.begin(115200);
  dht.begin();

  Serial.println(WIFI_SSID);
  Serial.println(WIFI_PASS);

  Serial.println("\n[BOOT] Menghubungkan ke WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int coba = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    coba++;
    if (coba >= 20) {
      Serial.println("\n[ERROR] Gagal konek! Status: " + String(WiFi.status()));
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Terhubung! IP: " + WiFi.localIP().toString());
  }
}

void kirimKeSupabase(float suhu, float kelembapan, float cahaya, bool gelap) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi putus, skip kirim.");
    return;
  }

  JsonDocument doc;
  doc["suhu"]       = round(suhu * 10.0) / 10.0;       
  doc["kelembapan"] = round(kelembapan * 10.0) / 10.0;
  doc["cahaya"]     = round(cahaya);
  doc["kondisi"]    = gelap ? "GELAP" : "TERANG";

  String payload;
  serializeJson(doc, payload);

  Serial.println("[HTTP] Kirim ke Supabase: " + payload);

  HTTPClient http;
  http.begin(SUPABASE_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Prefer", "return=minimal");   // hemat bandwidth

  int code = http.POST(payload);

  if (code == 201) {
    Serial.println("[HTTP] ✓ Berhasil dikirim (201 Created)");
  } else {
    Serial.println("[HTTP] ✗ Gagal. Code: " + String(code));
    Serial.println("[HTTP] Response: " + http.getString());
  }

  http.end();
}

void bacaDanAkumulasi() {
  float suhu       = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  int   cahaya     = analogRead(PIN_LDR_AO);

  // Validasi sensor DHT
  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT] Gagal baca sensor! Cek kabel.");
    return;
  }

  // Log ke Serial setiap 10 detik
  if (hitungan % 10 == 0) {
    Serial.printf("[SENSOR] Suhu: %.1f°C | Kelembapan: %.1f%% | Cahaya: %d | %s\n",
      suhu, kelembapan, cahaya, (cahaya > THRESHOLD_GELAP) ? "GELAP" : "TERANG");
  }

  // Akumulasi
  totalSuhu       += suhu;
  totalKelembapan += kelembapan;
  totalCahaya     += cahaya;
  hitungan++;

  // Setiap 300 tick (5 menit) → hitung rata-rata dan kirim
  if (hitungan >= 300) {
    float avgSuhu       = totalSuhu       / 300.0;
    float avgKelembapan = totalKelembapan / 300.0;
    float avgCahaya     = totalCahaya     / 300.0;
    bool  avgGelap      = (avgCahaya > THRESHOLD_GELAP);

    Serial.println("\n[AVG] ====== Rata-rata 5 menit ======");
    Serial.printf("[AVG] Suhu: %.2f°C | Kelembapan: %.2f%% | Cahaya: %.0f | %s\n",
      avgSuhu, avgKelembapan, avgCahaya, avgGelap ? "GELAP" : "TERANG");

    kirimKeSupabase(avgSuhu, avgKelembapan, avgCahaya, avgGelap);

    // Reset akumulasi
    hitungan        = 0;
    totalSuhu       = 0;
    totalKelembapan = 0;
    totalCahaya     = 0;
  }
}

void loop() {
  unsigned long now = millis();
  if (now - lastTick >= INTERVAL_MS) {
    lastTick = now;
    bacaDanAkumulasi();
  }
}
