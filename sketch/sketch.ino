#include "secrets.h" 

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <DHT.h>
#include <WiFi.h> 
#include <HTTPClient.h>
#include <ArduinoJson.h>


// ==========================================
// KONFIGURASI PIN TERBAIK (TFT KANAN, SENSOR KIRI)
// ==========================================

// --- PIN TFT & TOUCH (Blok Kanan / Deretan 3V3) ---
#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22
#define TOUCH_CS  15
// Catatan: MISO=19, MOSI=23, SCK=18 otomatis di deretan 3V3

// --- PIN SENSOR DHT11 (Blok Kiri / Deretan VIN) ---
#define DHTPIN    27     
#define DHTTYPE   DHT11  

// --- PIN SENSOR LDR (Blok Kiri / Deretan VIN) ---
#define PIN_LDR_AO       34  // Tetap di D34 (Analog)
#define THRESHOLD_GELAP   2400

// ==========================================
// KONFIGURASI SISTEM
// ==========================================
#define TICK_INTERVAL_MS  2000UL 
#define TICKS_PER_SEND    150    
#define DURASI_MODE2_MS   10000UL  // Auto-kembali ke Mode 1 setelah 10 detik

#ifndef SUPABASE_TABLE
#define SUPABASE_TABLE "sensor_data"
#endif

const char* ssid = "POCO M5";
const char* password = "miko1234";

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);
DHT dht(DHTPIN, DHTTYPE);

// --- VARIABEL KONTROL UI ---
bool modeUtama = true;
bool modeBerubah = true; 
bool paksaUpdateAngka = true; 

unsigned long lastTick = 0;
float suhuLama = -999.0;
float kelemLama = -999.0;
int statusWifiLama = -1; 

uint16_t warnaSuhu = tft.color565(244, 67, 54); 
uint16_t warnaKelem = tft.color565(0, 150, 136);

// --- VARIABEL AKUMULASI DATA ---
int   hitungan        = 0;
float totalSuhu       = 0;
float totalKelembapan = 0;
float totalCahaya     = 0;

// --- VARIABEL MEMORI STATUS (Agar tidak hilang saat ganti mode UI) ---
String teksWifi       = "Menghubungkan WiFi...";
uint16_t warnaTeksWifi= ILI9341_YELLOW;
String teksDB         = "Menunggu Jaringan...";
uint16_t warnaTeksDB  = ILI9341_ORANGE;
String teksLDR        = "LDR: - | Sisa Waktu: -";
uint16_t warnaTeksLDR = ILI9341_WHITE;


// ==========================================
// DATA BITMAP / HEX ARRAY 
// ==========================================
const unsigned char thermo_bmp[] PROGMEM = {
  0x03, 0x80, 0x00, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x0c, 0x73, 0xff, 0xf8,
  0x18, 0x37, 0xff, 0xfc, 0x18, 0x37, 0xff, 0xfc, 0x18, 0x30, 0x00, 0x00,
  0x18, 0x30, 0x00, 0x00, 0x18, 0x33, 0xfc, 0x00, 0x18, 0x37, 0xfe, 0x00,
  0x19, 0x37, 0xfe, 0x00, 0x19, 0xb1, 0xfc, 0x00, 0x19, 0xb0, 0x00, 0x00,
  0x19, 0xb0, 0x00, 0x00, 0x19, 0xb7, 0xff, 0xf0, 0x19, 0xb7, 0xff, 0xf0,
  0x19, 0xb3, 0xff, 0xe0, 0x19, 0xb0, 0x00, 0x00, 0x39, 0xb8, 0x00, 0x00,
  0x71, 0x9c, 0x00, 0x00, 0x61, 0x8c, 0x00, 0x00, 0xe7, 0xce, 0x00, 0x00,
  0xc7, 0xc6, 0x00, 0x00, 0xc7, 0xe6, 0x00, 0x00, 0xc7, 0xc6, 0x00, 0x00,
  0xe7, 0xc6, 0x00, 0x00, 0x61, 0x0c, 0x00, 0x00, 0x70, 0x1c, 0x00, 0x00,
  0x38, 0x38, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x00
};
const unsigned char drop_bmp[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x7e, 0x00,
  0x00, 0xff, 0x00, 0x01, 0xff, 0x80, 0x03, 0xff, 0xc0, 0x07, 0xff, 0xe0,
  0x0f, 0xff, 0xf0, 0x1f, 0xff, 0xf8, 0x1f, 0xff, 0xf8, 0x3f, 0xff, 0xfc,
  0x3f, 0x9f, 0xfc, 0x3f, 0x8f, 0xfc, 0x3f, 0xc7, 0xfc, 0x1f, 0xff, 0xf8,
  0x1f, 0xff, 0xf8, 0x0f, 0xff, 0xf0, 0x07, 0xff, 0xe0, 0x03, 0xff, 0xc0,
  0x00, 0xff, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char image_Wifi_icon_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x7f,0xfe,0x00,0x01,0xff,0xff,0x80,0x07,0xff,0xff,0xe0,0x1f,0xff,0xff,0xf8,
  0x3f,0xff,0xff,0xfc,0x7f,0xe0,0x07,0xfe,0xff,0x00,0x00,0xff,0xfe,0x00,0x00,0x7f,
  0xf8,0x00,0x00,0x1f,0xf0,0x0f,0xf0,0x0f,0x00,0x3f,0xfc,0x00,0x00,0xff,0xff,0x00,
  0x01,0xff,0xff,0x80,0x03,0xff,0xff,0x80,0x03,0xf8,0x1f,0xc0,0x01,0xf0,0x0f,0x80,
  0x00,0xc0,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xc0,0x00,0x00,0x07,0xe0,0x00,
  0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x03,0xc0,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
const unsigned char image_Files_icon_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x1f,0xf8,0x00,0x00,0x3f,0xfe,0x00,0x00,0x7f,0xff,0x80,0x00,
  0xfe,0x3f,0xff,0xf8,0xf8,0x0f,0xff,0xfe,0xf0,0x03,0xff,0xfe,0xe0,0x00,0x00,0x3f,
  0xe0,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,
  0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,
  0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,0xe0,0x00,0x00,0x07,
  0xf0,0x00,0x00,0x0f,0xf8,0x00,0x00,0x1f,0xfe,0x00,0x00,0x7f,0x7f,0xff,0xff,0xfe,
  0x3f,0xff,0xff,0xfc,0x1f,0xff,0xff,0xf8,0x00,0x00,0x00,0x00
};

// ==========================================
// FUNGSI HELPER UNTUK UPDATE STATUS INLINE
// ==========================================
void updateStatusWifi(String pesan, uint16_t warna) {
  teksWifi = pesan;
  warnaTeksWifi = warna;
  if (!modeUtama && !modeBerubah) {
    tft.fillRect(55, 68, 260, 15, ILI9341_BLACK);
    tft.setCursor(55, 68); tft.setTextColor(warnaTeksWifi); tft.setTextSize(1);
    tft.print(teksWifi);
  }
}

void updateStatusDB(String pesan, uint16_t warna) {
  teksDB = pesan;
  warnaTeksDB = warna;
  if (!modeUtama && !modeBerubah) {
    tft.fillRect(55, 109, 260, 15, ILI9341_BLACK);
    tft.setCursor(55, 109); tft.setTextColor(warnaTeksDB); tft.setTextSize(1);
    tft.print(teksDB);
  }
}

void updateStatusLDR(String pesan, uint16_t warna) {
  teksLDR = pesan;
  warnaTeksLDR = warna;
  if (!modeUtama && !modeBerubah) {
    tft.fillRect(15, 145, 300, 15, ILI9341_BLACK);
    tft.setCursor(15, 145); tft.setTextColor(warnaTeksLDR); tft.setTextSize(1);
    tft.print(teksLDR);
  }
}

// ==========================================
// FUNGSI HTTP KE SUPABASE
// ==========================================
void kirimKeSupabase(float suhu, float kelembapan, float cahaya, bool gelap) {
  if (WiFi.status() != WL_CONNECTED) {
    updateStatusDB("Gagal Kirim: WiFi Tidak Terhubung!", ILI9341_RED);
    return;
  }

  updateStatusDB("Mengirim Data HTTP POST...", ILI9341_YELLOW);

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
  
  int code = http.POST(payload);
  if (code == 201) {
    updateStatusDB("Sukses HTTP 201 (DB Updated!)", ILI9341_GREEN);
  } else {
    updateStatusDB("Gagal HTTP " + String(code), ILI9341_RED);
  }
  http.end();
}

// ==========================================
// SETUP UTAMA
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. Nyalakan Layar Dulu
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(ILI9341_BLACK);
  delay(1000);
  
  // 2. Nyalakan Sensor
  ts.begin();
  dht.begin(); 
  delay(1000);
  
  // 3. Terakhir, nyalakan WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {

  // --- 1. DETEKSI SENTUHAN ---
  static bool memoriSentuhan = false;     
  static unsigned long waktuSentuhTerakhir = 0;
  static unsigned long waktuMasukMode2 = 0;

  bool sedangDisentuh = false;
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z > 300 && p.x > 100 && p.x < 3900 && p.y > 100 && p.y < 3900) {
      sedangDisentuh = true;
    }
  }

  if (sedangDisentuh && !memoriSentuhan && (millis() - waktuSentuhTerakhir > 300)) {
    modeUtama = !modeUtama;
    modeBerubah = true;
    paksaUpdateAngka = true;
    waktuSentuhTerakhir = millis();
    if (!modeUtama) {
      waktuMasukMode2 = millis();
    }
  }
  memoriSentuhan = sedangDisentuh;

  // Auto kembali ke Mode 1 setelah 10 detik berada di Mode 2
  if (!modeUtama && (millis() - waktuMasukMode2 >= DURASI_MODE2_MS)) {
    modeUtama = true;
    modeBerubah = true;
    paksaUpdateAngka = true;
  }

  // --- 2. KERANGKA TAMPILAN (SKELETON UI) ---
  if (modeBerubah) {
    suhuLama = -999.0;
    kelemLama = -999.0;

    if (modeUtama) {
      // ==== MODE 1: TYPOGRAPHY UI ====
      tft.fillRect(0, 0, 320, 120, warnaSuhu);
      tft.setCursor(10, 10); 
      tft.setTextColor(ILI9341_BLACK); tft.setTextSize(4);
      tft.print("T");
      
      tft.setCursor(285, 15); 
      tft.setTextColor(ILI9341_WHITE); tft.setTextSize(3);
      tft.print("C");
      tft.drawCircle(276, 18, 4, ILI9341_WHITE); 
      tft.drawCircle(276, 18, 3, ILI9341_WHITE);
      tft.fillRect(0, 120, 320, 120, warnaKelem);
      tft.setCursor(10, 130); 
      tft.setTextColor(ILI9341_BLACK); tft.setTextSize(4);
      tft.print("H");
      
      tft.setCursor(285, 135); 
      tft.setTextColor(ILI9341_WHITE); tft.setTextSize(3);
      tft.print("%");

      tft.fillRect(0, 117, 320, 4, ILI9341_BLACK);
    } else {
      // ==== MODE 2: DEV DASHBOARD ====
      tft.fillScreen(ILI9341_BLACK);
      uint16_t warnaCyan = tft.color565(0, 180, 255); 
      uint16_t warnaBiru = tft.color565(30, 144, 255);
      uint16_t warnaDB   = tft.color565(255, 193, 7);
      // --- BARIS 1: Sensor Suhu & Kelembapan ---
      tft.drawBitmap(10, 8, thermo_bmp, 32, 30, warnaCyan);
      tft.setCursor(50, 30); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(1);
      tft.print("Temperature");
      
      tft.drawBitmap(170, 10, drop_bmp, 24, 24, warnaBiru);
      tft.drawFastHLine(0, 45, 320, ILI9341_DARKGREY);
      // --- BARIS 2: Network / WiFi ---
      tft.drawBitmap(15, 50, image_Wifi_icon_bits, 32, 32, ILI9341_WHITE);
      tft.setCursor(55, 55); tft.setTextColor(ILI9341_LIGHTGREY); tft.setTextSize(1);
      tft.print("Network Status:");
      
      tft.setCursor(55, 68); tft.setTextColor(warnaTeksWifi);
      tft.print(teksWifi);
      tft.drawFastHLine(0, 86, 320, ILI9341_DARKGREY);

      // --- BARIS 3: Database / Files ---
      tft.drawBitmap(15, 91, image_Files_icon_bits, 32, 32, warnaDB);
      tft.setCursor(55, 96); tft.setTextColor(ILI9341_LIGHTGREY); tft.setTextSize(1);
      tft.print("Supabase DB Status:");
      
      tft.setCursor(55, 109); tft.setTextColor(warnaTeksDB);
      tft.print(teksDB);
      tft.drawFastHLine(0, 127, 320, ILI9341_DARKGREY);
      
      // --- BARIS 4: LDR & Akumulasi ---
      tft.setCursor(15, 133);
      tft.setTextColor(ILI9341_LIGHTGREY); tft.setTextSize(1);
      tft.print("Sensor LDR & Auto-Submit Status:");
      
      tft.setCursor(15, 145); tft.setTextColor(warnaTeksLDR);
      tft.print(teksLDR);
      tft.drawFastHLine(0, 168, 320, ILI9341_DARKGREY);
    }
    modeBerubah = false;
  }

  // --- 3. LOGIKA INTERVAL (Setiap 2 Detik ATAU saat Layar Disentuh) ---
  bool saatnyaTick = (millis() - lastTick >= TICK_INTERVAL_MS);

  if (saatnyaTick || paksaUpdateAngka) {
    
    // 3A. PENANGANAN WIFI (Jalan terus di background)
    static unsigned long waktuReconnect = millis();
    int statusWifiSekarang = WiFi.status();
    
    if (statusWifiSekarang != statusWifiLama) {
      if (statusWifiSekarang == WL_CONNECTED) {
        updateStatusWifi("Connected", ILI9341_GREEN);
        if(teksDB.startsWith("Menunggu") || teksDB.startsWith("Gagal")) {
           updateStatusDB("Siap Melakukan Transmisi Data...", ILI9341_CYAN);
        }
      } else {
        WiFi.disconnect();
        waktuReconnect = millis(); 
        updateStatusDB("Akses Database Tertunda (No WiFi)", ILI9341_ORANGE);
      }
      statusWifiLama = statusWifiSekarang;
    }
    
    if (statusWifiSekarang != WL_CONNECTED) {
      unsigned long waktuBerlalu = millis() - waktuReconnect;
      if (waktuBerlalu < 30000) { 
        int sisaDetik = (30000 - waktuBerlalu) / 1000;
        updateStatusWifi("Terputus! Reconnect dlm: " + String(sisaDetik) + "s", ILI9341_RED);
      } else {
        updateStatusWifi("Mencoba Reconnect...", ILI9341_YELLOW);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        waktuReconnect = millis(); 
      }
    }

    // Variabel statis agar menyimpan hasil pembacaan walau ganti mode UI
    static float nilaiSuhu = 0.0;
    static float nilaiKelem = 0.0;
    static int cahaya = 0;

    // 3B. BACA SENSOR
    if (saatnyaTick) {
      lastTick = millis();

      float bacaSuhu = dht.readTemperature(); 
      float bacaKelem = dht.readHumidity();
      cahaya = analogRead(PIN_LDR_AO);
      
      if (!isnan(bacaSuhu) && !isnan(bacaKelem)) {
          nilaiSuhu = bacaSuhu;
          nilaiKelem = bacaKelem;
      } else {
          updateStatusLDR("ERROR: Sensor DHT11 (Cek Kabel!)", ILI9341_RED);
      }

      // 3E. AKUMULASI & SUPABASE TRIGGER
      totalSuhu       += nilaiSuhu;
      totalKelembapan += nilaiKelem;
      totalCahaya     += cahaya;
      hitungan++;

      if (hitungan >= TICKS_PER_SEND) {
        float avgSuhu       = totalSuhu       / TICKS_PER_SEND;
        float avgKelembapan = totalKelembapan / TICKS_PER_SEND;
        float avgCahaya     = totalCahaya     / TICKS_PER_SEND;
        bool  avgGelap      = (avgCahaya > THRESHOLD_GELAP);

        kirimKeSupabase(avgSuhu, avgKelembapan, avgCahaya, avgGelap);
        hitungan        = 0;
        totalSuhu       = 0;
        totalKelembapan = 0;
        totalCahaya     = 0;
      }
    }

    paksaUpdateAngka = false;

    // 3C. UPDATE STATUS LDR INLINE
    String kondisiCahaya = (cahaya > THRESHOLD_GELAP) ? "GELAP" : "TERANG";
    String ldrString = "Val: " + String(cahaya) + " (" + kondisiCahaya + ") | Load: " + String(hitungan) + "/" + String(TICKS_PER_SEND) + "s";
    updateStatusLDR(ldrString, ILI9341_CYAN);

    // 3D. UPDATE ANGKA SUHU / KELEMBAPAN DI LAYAR
    if (modeUtama) {
      if (nilaiSuhu != suhuLama) {
        tft.fillRect(60, 30, 200, 60, warnaSuhu);
        tft.setCursor(75, 32); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(7); 
        tft.print(nilaiSuhu, 1);
        suhuLama = nilaiSuhu;
      }
      if (nilaiKelem != kelemLama) {
        tft.fillRect(60, 150, 200, 60, warnaKelem);
        tft.setCursor(75, 152); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(7); 
        tft.print(nilaiKelem, 1);
        kelemLama = nilaiKelem;
      }
    } else {
      if (nilaiSuhu != suhuLama) {
        tft.fillRect(45, 12, 110, 16, ILI9341_BLACK);
        tft.setCursor(55, 12); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2); 
        tft.print(nilaiSuhu, 1); tft.print(" C"); 
        tft.drawCircle(117, 14, 2, ILI9341_WHITE); 
        suhuLama = nilaiSuhu;
      }
      if (nilaiKelem != kelemLama) {
        tft.fillRect(205, 12, 110, 16, ILI9341_BLACK);
        tft.setCursor(205, 12); tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2); 
        tft.print(nilaiKelem, 1); tft.print(" %");
        kelemLama = nilaiKelem;
      }
    }
  }
}