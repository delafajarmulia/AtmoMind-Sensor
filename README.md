# Suhu Detector V2

Sistem monitoring suhu, kelembapan, dan intensitas cahaya berbasis ESP32 dengan dashboard web. Data sensor dikirim ke Supabase setiap 5 menit (rata-rata), dan dapat dipantau secara remote melalui dashboard Flask.

Project ini dibuat untuk memenuhi tugas mata kuliah **Artificial Intelligence (AI)** dengan pendekatan pengumpulan data lingkungan.

## Stack

- **Hardware**: ESP32 + DHT11 + LDR module
- **Database**: Supabase (PostgreSQL)
- **Dashboard**: Python Flask
- **Protokol**: HTTP REST API

## Struktur Project

```
suhu-detector-v2/
├── sketch/
│   ├── sketch.ino          # Kode ESP32
│   └── secrets.h.example   # Template secrets
├── dashboard.py            # Dashboard Flask
├── requirements.txt
├── .env.example                    
├── .gitignore
└── README.md
```

## Hardware

### Komponen

- ESP32 Dev Board
- Sensor DHT11 (suhu & kelembapan)
- Modul LDR (intensitas cahaya)

### Wiring

| Komponen | Pin Komponen | Pin ESP32 |
|----------|-------------|-----------|
| DHT11    | VCC         | 3V3       |
| DHT11    | DATA        | D5 (GPIO 5) |
| DHT11    | GND         | GND       |
| LDR      | VCC         | 3V3       |
| LDR      | AO          | D34 (GPIO 34) |
| LDR      | GND         | GND       |

> Pin DO pada modul LDR tidak digunakan. Kondisi gelap/terang dihitung dari nilai AO dengan threshold `2400`.

## Setup

### 1. Supabase

Buat tabel di Supabase dengan SQL berikut:

```sql
create table sensor_data (
  id bigint generated always as identity primary key,
  created_at timestamptz default now(),
  suhu float,
  kelembapan float,
  cahaya float,
  kondisi text
);
```

Aktifkan Row Level Security dan tambahkan policy insert untuk `anon` role.

### 2. ESP32

Install library berikut di Arduino IDE:
- DHT sensor library (Adafruit)
- ArduinoJson
- Board: ESP32 Dev Module

Buat file `sketch/secrets.h` dari template:

```cpp
#define WIFI_SSID    "nama_wifi"
#define WIFI_PASS    "password_wifi"
#define SUPABASE_URL "https://xxxxx.supabase.co/rest/v1/sensor_data"
#define SUPABASE_KEY "eyJhbGci..."
```

Upload `sketch.ino` ke ESP32.

### 3. Dashboard

Install dependencies:

```bash
py -m pip install flask requests python-dotenv
```

Buat file `.env` di root project:

```
SUPABASE_URL=https://xxxxx.supabase.co
SUPABASE_KEY=eyJhbGci...
```

Jalankan dashboard:

```bash
py dashboard.py
```

Buka browser: `http://localhost:5000/dashboard`

## Cara Kerja

ESP32 membaca sensor setiap 1 detik dan mengakumulasi nilainya. Setiap 300 detik (5 menit), rata-rata dihitung dan dikirim ke Supabase via HTTP POST. Dashboard Flask membaca data dari Supabase dan menampilkan grafik suhu, kelembapan, dan intensitas cahaya secara otomatis refresh tiap 10 detik.

```
ESP32 → (setiap 5 menit) → Supabase → (dibaca oleh) → Dashboard Flask
```

## Catatan

- ESP32 hanya support WiFi 2.4GHz
- GPIO 34 adalah input-only, cocok untuk baca analog LDR
