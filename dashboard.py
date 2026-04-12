"""
dashboard.py
Flask dashboard untuk memvisualisasikan data sensor dari Supabase.
Pengganti dashboard_V2.py dari repo referensi (SQLite → Supabase).

Install: pip install flask requests python-dotenv
Run    : python dashboard.py
Akses  : http://localhost:5000/dashboard
"""

from dotenv import load_dotenv
import os
import requests
from flask import Flask, jsonify, request

load_dotenv()

app = Flask(__name__)

SUPABASE_URL = os.getenv("SUPABASE_URL")
SUPABASE_KEY = os.getenv("SUPABASE_KEY")

HEADERS = {
    "apikey": SUPABASE_KEY,
    "Authorization": f"Bearer {SUPABASE_KEY}",
    "Content-Type": "application/json",
}

# ─────────────────────────────────────────────────────────────
@app.route("/api/data")
def api_data():
    """Ambil 100 data terbaru dari Supabase, kembalikan sebagai JSON."""
    limit = request.args.get("limit", 100)
    url   = f"{SUPABASE_URL}/rest/v1/sensor_data"
    params = {
        "select": "id,created_at,suhu,kelembapan,cahaya,kondisi",
        "order":  "created_at.desc",
        "limit":  limit,
    }
    resp = requests.get(url, headers=HEADERS, params=params, timeout=10)

    if resp.status_code != 200:
        return jsonify({"status": "error", "detail": resp.text}), resp.status_code

    data = resp.json()
    return jsonify({
        "status":     "success",
        "total_data": len(data),
        "data":       data,
    })

# ─────────────────────────────────────────────────────────────
@app.route("/dashboard")
def dashboard():
    html = """<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Dashboard Sensor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: system-ui, sans-serif; background: #0f172a; color: #e2e8f0; padding: 24px; }
    h1   { color: #38bdf8; text-align: center; margin-bottom: 24px; font-size: 1.5rem; }

    .cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 16px; margin-bottom: 28px; }
    .card  { background: #1e293b; border-radius: 12px; padding: 20px; text-align: center; border: 1px solid #334155; }
    .card .val  { font-size: 2rem; font-weight: 700; }
    .card .lbl  { font-size: 0.8rem; color: #94a3b8; margin-top: 4px; }
    .card.suhu  .val { color: #f97316; }
    .card.hum   .val { color: #38bdf8; }
    .card.ldr   .val { color: #a78bfa; }
    .card.cond  .val { font-size: 1.2rem; color: #4ade80; }

    .charts { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 24px; }
    .chart-box { background: #1e293b; border-radius: 12px; padding: 20px; border: 1px solid #334155; }
    .chart-box h3 { font-size: 0.9rem; color: #94a3b8; margin-bottom: 12px; }
    @media (max-width: 600px) { .charts { grid-template-columns: 1fr; } }

    .tbl-wrap { background: #1e293b; border-radius: 12px; padding: 20px; border: 1px solid #334155; overflow-x: auto; }
    .tbl-wrap h3 { font-size: 0.9rem; color: #94a3b8; margin-bottom: 12px; }
    table { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
    th { text-align: left; padding: 8px 12px; color: #64748b; border-bottom: 1px solid #334155; }
    td { padding: 8px 12px; border-bottom: 1px solid #1e293b; }
    tr:hover td { background: #273549; }
    .badge { padding: 2px 8px; border-radius: 999px; font-size: 0.75rem; font-weight: 600; }
    .badge.gelap  { background: #1e1b4b; color: #a5b4fc; }
    .badge.terang { background: #fef9c3; color: #78350f; }

    .status { text-align: center; font-size: 0.8rem; color: #475569; margin-top: 16px; }
  </style>
</head>
<body>
  <h1>📡 Dashboard Sensor</h1>

  <!-- Kartu ringkasan -->
  <div class="cards">
    <div class="card suhu">
      <div class="val" id="v-suhu">--</div>
      <div class="lbl">Suhu (°C) terkini</div>
    </div>
    <div class="card hum">
      <div class="val" id="v-hum">--</div>
      <div class="lbl">Kelembapan (%) terkini</div>
    </div>
    <div class="card ldr">
      <div class="val" id="v-ldr">--</div>
      <div class="lbl">Cahaya (raw ADC) terkini</div>
    </div>
    <div class="card cond">
      <div class="val" id="v-cond">--</div>
      <div class="lbl">Kondisi cahaya</div>
    </div>
  </div>

  <!-- Grafik -->
  <div class="charts">
    <div class="chart-box">
      <h3>Suhu (°C)</h3>
      <canvas id="chartSuhu"></canvas>
    </div>
    <div class="chart-box">
      <h3>Kelembapan (%)</h3>
      <canvas id="chartHum"></canvas>
    </div>
  </div>
  <div class="charts">
    <div class="chart-box" style="grid-column: 1 / -1;">
      <h3>Intensitas cahaya (ADC 0-4095)</h3>
      <canvas id="chartLdr"></canvas>
    </div>
  </div>

  <!-- Tabel data -->
  <div class="tbl-wrap">
    <h3>Data terbaru (100 entri)</h3>
    <table>
      <thead>
        <tr><th>Waktu</th><th>Suhu</th><th>Kelembapan</th><th>Cahaya</th><th>Kondisi</th></tr>
      </thead>
      <tbody id="tabel-data"></tbody>
    </table>
  </div>

  <p class="status" id="status-lbl">Memuat data...</p>

<script>
const CFG_LINE = (color) => ({
  borderColor: color,
  backgroundColor: color + '22',
  borderWidth: 1.5,
  pointRadius: 2,
  fill: true,
  tension: 0.4,
});

const chartSuhu = new Chart(document.getElementById('chartSuhu').getContext('2d'), {
  type: 'line',
  data: { labels: [], datasets: [{ label: 'Suhu °C', data: [], ...CFG_LINE('#f97316') }] },
  options: { animation: false, scales: { y: { suggestedMin: 20, suggestedMax: 40 } } },
});
const chartHum = new Chart(document.getElementById('chartHum').getContext('2d'), {
  type: 'line',
  data: { labels: [], datasets: [{ label: 'Kelembapan %', data: [], ...CFG_LINE('#38bdf8') }] },
  options: { animation: false, scales: { y: { suggestedMin: 30, suggestedMax: 100 } } },
});
const chartLdr = new Chart(document.getElementById('chartLdr').getContext('2d'), {
  type: 'line',
  data: { labels: [], datasets: [{ label: 'Cahaya (ADC)', data: [], ...CFG_LINE('#a78bfa') }] },
  options: { animation: false, scales: { y: { suggestedMin: 0, suggestedMax: 4095 } } },
});

function fmtTime(ts) {
  const d = new Date(ts);
  return d.toLocaleDateString('id-ID') + ' ' + d.toLocaleTimeString('id-ID');
}

async function refresh() {
  try {
    const res  = await fetch('/api/data?limit=100');
    const json = await res.json();
    if (!json.data || json.data.length === 0) {
      document.getElementById('status-lbl').textContent = 'Belum ada data.';
      return;
    }

    const rows = [...json.data].reverse();   // tampilkan cronologis di grafik
    const times = rows.map(r => fmtTime(r.created_at));
    const suhu  = rows.map(r => parseFloat(r.suhu));
    const hum   = rows.map(r => parseFloat(r.kelembapan));
    const ldr   = rows.map(r => parseFloat(r.cahaya));

    // Update grafik
    chartSuhu.data.labels = times; chartSuhu.data.datasets[0].data = suhu; chartSuhu.update();
    chartHum.data.labels  = times; chartHum.data.datasets[0].data  = hum;  chartHum.update();
    chartLdr.data.labels  = times; chartLdr.data.datasets[0].data  = ldr;  chartLdr.update();

    // Kartu nilai terbaru
    const latest = json.data[0];
    document.getElementById('v-suhu').textContent  = parseFloat(latest.suhu).toFixed(1);
    document.getElementById('v-hum').textContent   = parseFloat(latest.kelembapan).toFixed(1);
    document.getElementById('v-ldr').textContent   = Math.round(latest.cahaya);
    document.getElementById('v-cond').textContent  = latest.kondisi;

    // Tabel
    const tbody = document.getElementById('tabel-data');
    tbody.innerHTML = json.data.map(r => `
      <tr>
        <td>${fmtTime(r.created_at)}</td>
        <td>${parseFloat(r.suhu).toFixed(1)} °C</td>
        <td>${parseFloat(r.kelembapan).toFixed(1)} %</td>
        <td>${Math.round(r.cahaya)}</td>
        <td><span class="badge ${r.kondisi === 'GELAP' ? 'gelap' : 'terang'}">${r.kondisi}</span></td>
      </tr>
    `).join('');

    document.getElementById('status-lbl').textContent =
      `Terakhir diperbarui: ${new Date().toLocaleTimeString('id-ID')} · ${json.total_data} data`;

  } catch (err) {
    document.getElementById('status-lbl').textContent = 'Error: ' + err.message;
  }
}

refresh();
setInterval(refresh, 10000);   // auto-refresh tiap 10 detik
</script>
</body>
</html>"""
    return html


# ─────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("Dashboard berjalan di http://localhost:5000/dashboard")
    app.run(debug=True, host="0.0.0.0", port=5000)