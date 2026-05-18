#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// OLED LIBRARY
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define LEBAR   128
#define TINGGI  32
#define OLED_RESET -1 // Wajib untuk pin reset OLED

// INI SOLUSI ERROR 1: Mendeklarasikan objek oled
Adafruit_SSD1306 oled(LEBAR, TINGGI, &Wire, OLED_RESET);

#define PIN_DHT           5
#define TYPE_DHT          DHT11
#define PIN_LDR_AO        34
#define THRESHOLD_GELAP   2400
#define TICK_INTERVAL_MS  1000UL
#define TICKS_PER_SEND    300
#define WIFI_MAX_RETRY    3
#define WIFI_TIMEOUT_MS   15000UL

#define SUPABASE_TABLE "sensor_data"

DHT dht(PIN_DHT, TYPE_DHT);

int   hitungan        = 0;
float totalSuhu       = 0;
float totalKelembapan = 0;
float totalCahaya     = 0;

unsigned long lastTick = 0;

// MENAMPILKAN PADA OLED
void tampilkanOled(String baris1, String baris2, String baris3, String baris4) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);   oled.println(baris1);
  oled.setCursor(0, 8);   oled.println(baris2);
  oled.setCursor(0, 16);  oled.println(baris3);
  oled.setCursor(0, 24);  oled.println(baris4);
  oled.display();
}

// ── WiFi Scan ────────────────────────────────────────────────────────────────
void scanWiFi() {
  Serial.println("[SCAN] Mencari jaringan...");
  tampilkanOled("WiFi Scan", "Mencari Jaringan...", "", "");
  
  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("[SCAN] Tidak ada jaringan ditemukan!");
    tampilkanOled("WiFi Scan", "Tidak ada jaringan!", "Cek jangkauan", "");
    delay(2000);
    return;
  }

  bool ketemu = false;
  for (int i = 0; i < n; i++) {
    bool cocok = (WiFi.SSID(i) == String(WIFI_SSID));
    Serial.printf("[SCAN] %s | %d dBm | CH%d | %s %s\n",
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      WiFi.channel(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Protected",
      cocok ? "<-- TARGET" : ""
    );
    if (cocok) ketemu = true;
  }

  if (!ketemu) {
    Serial.println("[SCAN] ✗ SSID '" + String(WIFI_SSID) + "' TIDAK DITEMUKAN!");
    tampilkanOled("WiFi Scan", "Target SSID:", String(WIFI_SSID), "TDK DITEMUKAN!");
    delay(3000);
  }
}

// ── WiFi Connect ─────────────────────────────────────────────────────────────
bool hubungkanWiFi() {
  for (int percobaan = 1; percobaan <= WIFI_MAX_RETRY; percobaan++) {
    Serial.printf("[WiFi] Percobaan %d/%d — SSID: %s\n", percobaan, WIFI_MAX_RETRY, WIFI_SSID);
                  
    tampilkanOled("Koneksi WiFi", "SSID: " + String(WIFI_SSID), "Percobaan: " + String(percobaan) + "/" + String(WIFI_MAX_RETRY), "Menghubungkan...");

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - t0 > WIFI_TIMEOUT_MS) break;
      delay(200);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] ✓ Terhubung! IP: " + WiFi.localIP().toString());
      tampilkanOled("WiFi Terhubung!", "IP:", WiFi.localIP().toString(), "RSSI: " + String(WiFi.RSSI()) + " dBm");
      delay(2000);
      return true;
    }

    tampilkanOled("WiFi Gagal!", "Status: " + String(WiFi.status()), "Mencoba lagi...", "");
    delay(2000);
  }
  
  tampilkanOled("WiFi Gagal", "Cek Hotspot/Router", "Modul Standby...", "");
  return false;
}

// ── HTTP ke Supabase ─────────────────────────────────────────────────────────
void kirimKeSupabase(float suhu, float kelembapan, float cahaya, bool gelap) {
  if (WiFi.status() != WL_CONNECTED) {
    tampilkanOled("WiFi Putus!", "Mencoba reconnect...", "", "");
    if (!hubungkanWiFi()) {
      tampilkanOled("Kirim Gagal", "WiFi tidak ada", "Data dibuang", "");
      delay(2000);
      return;
    }
  }

  tampilkanOled("Supabase...", "Mengirim Data AVG", "Suhu: " + String(suhu, 1) + "C", "Mohon tunggu...");

  JsonDocument doc;
  doc["suhu"]       = round(suhu * 10.0) / 10.0;
  doc["kelembapan"] = round(kelembapan * 10.0) / 10.0;
  doc["cahaya"]     = round(cahaya);
  doc["kondisi"]    = gelap ? "GELAP" : "TERANG";

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + SUPABASE_TABLE;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Prefer", "return=minimal");
  http.setTimeout(10000);

  int code = http.POST(payload);

  if (code == 201) {
    tampilkanOled("Sukses Kirim!", "Kode HTTP: 201", "Database Updated", "");
  } else {
    tampilkanOled("Gagal Kirim!", "Kode HTTP: " + String(code), "Cek Auth/URL", "");
  }

  http.end();
  delay(2500); 
}

// ── Baca & Akumulasi ─────────────────────────────────────────────────────────
void bacaDanAkumulasi() {
  float suhu       = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  int   cahaya     = analogRead(PIN_LDR_AO);

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT] Gagal baca sensor!");
    tampilkanOled("ERROR SENSOR", "Gagal baca DHT11", "Cek Kabel PIN " + String(PIN_DHT), "");
    return;
  }

  String kondisiCahaya = (cahaya > THRESHOLD_GELAP) ? "GELAP" : "TERANG";

  if (hitungan % 10 == 0) {
    Serial.printf("[SENSOR] Suhu: %.1f°C | Kelembapan: %.1f%% | Cahaya: %d | %s\n",
      suhu, kelembapan, cahaya, kondisiCahaya.c_str());
  }

  int sisaWaktu = TICKS_PER_SEND - hitungan;
  tampilkanOled(
    "S: " + String(suhu, 1) + "C | H: " + String(kelembapan, 1) + "%",
    "LDR: " + String(cahaya) + " (" + kondisiCahaya + ")",
    "Akumulasi: " + String(hitungan) + "/" + String(TICKS_PER_SEND),
    "Kirim dlm: " + String(sisaWaktu) + " dtk"
  );

  totalSuhu       += suhu;
  totalKelembapan += kelembapan;
  totalCahaya     += cahaya;
  hitungan++;

  if (hitungan >= TICKS_PER_SEND) {
    float avgSuhu       = totalSuhu       / TICKS_PER_SEND;
    float avgKelembapan = totalKelembapan / TICKS_PER_SEND;
    float avgCahaya     = totalCahaya     / TICKS_PER_SEND;
    bool  avgGelap      = (avgCahaya > THRESHOLD_GELAP);

    kirimKeSupabase(avgSuhu, avgKelembapan, avgCahaya, avgGelap);
    
    // INI SOLUSI ERROR 2: Menambahkan parameter ke dalam tampilkanOled
    tampilkanOled("Data Terkirim!", "Suhu AVG: " + String(avgSuhu, 1), "LDR AVG: " + String(avgCahaya, 0), "Status: " + String(avgGelap ? "GELAP" : "TERANG"));
    delay(2000);

    hitungan        = 0;
    totalSuhu       = 0;
    totalKelembapan = 0;
    totalCahaya     = 0;
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  dht.begin();

  Wire.begin(21, 22);
  
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED tidak ditemukan!");
    while (true); 
  }
  
  tampilkanOled("AtmoMind System", "Booting...", "Polines", "");
  delay(2000);

  Serial.println("\n[BOOT] Memulai...");
  scanWiFi();
  hubungkanWiFi();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  if (!WiFi.status() == WL_CONNECTED) {
    hubungkanWiFi();
  }

  unsigned long now = millis();
  if (now - lastTick >= TICK_INTERVAL_MS) {
    lastTick = now;
    bacaDanAkumulasi();
  }
}