#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

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

// ── WiFi Scan ────────────────────────────────────────────────────────────────

void scanWiFi() {
  Serial.println("[SCAN] Mencari jaringan...");
  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("[SCAN] Tidak ada jaringan ditemukan!");
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
    Serial.println("[SCAN]   → Cek: hotspot aktif? Band 2.4 GHz? SSID typo?");
  }
}

// ── WiFi Connect ─────────────────────────────────────────────────────────────

bool hubungkanWiFi() {
  for (int percobaan = 1; percobaan <= WIFI_MAX_RETRY; percobaan++) {
    Serial.printf("[WiFi] Percobaan %d/%d — SSID: %s\n",
                  percobaan, WIFI_MAX_RETRY, WIFI_SSID);

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
      Serial.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");
      return true;
    }

    Serial.printf("[WiFi] ✗ Gagal. Status: %d", WiFi.status());
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:  Serial.println(" (SSID tidak ditemukan — cek nama & 2.4 GHz)"); break;
      case WL_CONNECT_FAILED: Serial.println(" (Password salah)");                             break;
      case WL_DISCONNECTED:   Serial.println(" (Disconnected — cek jangkauan / daya)");        break;
      default:                Serial.println();
    }
    delay(2000);
  }
  return false;
}

// ── HTTP ke Supabase ─────────────────────────────────────────────────────────

void kirimKeSupabase(float suhu, float kelembapan, float cahaya, bool gelap) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi putus, mencoba reconnect...");
    if (!hubungkanWiFi()) {
      Serial.println("[HTTP] Reconnect gagal, data dibuang.");
      return;
    }
  }

  JsonDocument doc;
  doc["suhu"]       = round(suhu * 10.0) / 10.0;
  doc["kelembapan"] = round(kelembapan * 10.0) / 10.0;
  doc["cahaya"]     = round(cahaya);
  doc["kondisi"]    = gelap ? "GELAP" : "TERANG";

  String payload;
  serializeJson(doc, payload);

  // ── DEBUG: print URL sebelum kirim ──
  // Serial.println("[HTTP] URL    : " + String(SUPABASE_URL));
  Serial.println("[HTTP] Payload: " + payload);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + SUPABASE_TABLE;
  Serial.println("[HTTP] URL    : " + url);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Prefer", "return=minimal");
  http.setTimeout(10000);

  int code = http.POST(payload);

  if (code == 201) {
    Serial.println("[HTTP] ✓ Berhasil dikirim (201 Created)");
  } else {
    Serial.printf("[HTTP] ✗ Gagal. Code: %d\n", code);
    if (code > 0) Serial.println("[HTTP] Response: " + http.getString());
  }

  http.end();
}

// ── Baca & Akumulasi ─────────────────────────────────────────────────────────

void bacaDanAkumulasi() {
  float suhu       = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  int   cahaya     = analogRead(PIN_LDR_AO);

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT] Gagal baca sensor!");
    return;
  }

  if (hitungan % 10 == 0) {
    Serial.printf("[SENSOR] Suhu: %.1f°C | Kelembapan: %.1f%% | Cahaya: %d | %s\n",
      suhu, kelembapan, cahaya,
      (cahaya > THRESHOLD_GELAP) ? "GELAP" : "TERANG");
  }

  totalSuhu       += suhu;
  totalKelembapan += kelembapan;
  totalCahaya     += cahaya;
  hitungan++;

  if (hitungan >= TICKS_PER_SEND) {
    float avgSuhu       = totalSuhu       / TICKS_PER_SEND;
    float avgKelembapan = totalKelembapan / TICKS_PER_SEND;
    float avgCahaya     = totalCahaya     / TICKS_PER_SEND;
    bool  avgGelap      = (avgCahaya > THRESHOLD_GELAP);

    Serial.println("\n[AVG] ===== Rata-rata 1 menit =====");
    Serial.printf("[AVG] Suhu: %.2f°C | Kelembapan: %.2f%% | Cahaya: %.0f | %s\n",
      avgSuhu, avgKelembapan, avgCahaya, avgGelap ? "GELAP" : "TERANG");

    kirimKeSupabase(avgSuhu, avgKelembapan, avgCahaya, avgGelap);

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

  Serial.println("\n[BOOT] Memulai...");
  scanWiFi();
  hubungkanWiFi();
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  unsigned long now = millis();
  if (now - lastTick >= TICK_INTERVAL_MS) {
    lastTick = now;
    bacaDanAkumulasi();
  }
}