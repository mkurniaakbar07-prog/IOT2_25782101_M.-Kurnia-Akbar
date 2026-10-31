/*
 * Sistem Telemetri & Kendali Closed-Loop Industri (ESP8266 + DHT + MQTT)
 * NIM: 25782101
 *
 * - Telemetri JSON + klasifikasi kondisi_suhu (NORMAL / PANAS)
 * - Edge safety engine: alert OVERHEAT + LED otomatis + state latch
 * - Last Will and Testament (QoS 1, retained) + pesan ONLINE
 * - Manual override LED via topik kendali saat kondisi aman
 * - Tes LED saat boot + log debug pada callback
 * - Tanpa delay(): semua berbasis millis()
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ===================== KONFIGURASI JARINGAN =====================
const char* ssid     = "AADI JAYA";
const char* password = "ahmd201076";

const char* mqttServer = "broker.hivemq.com";
const int   mqttPort   = 1883;
const char* clientId   = "ESP8266_LabIoT_25782101_Full";   // Wajib unik

// ===================== DEFINISI TOPIK =====================
const char* topicTelemetri = "kampus/labiot/25782101/telemetri";
const char* topicKendali   = "kampus/labiot/25782101/kendali";
const char* topicAlert     = "kampus/labiot/25782101/alert";
const char* topicStatus    = "kampus/labiot/25782101/status";

// Pesan LWT dan pesan ONLINE
const char* msgOffline = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";
const char* msgOnline  = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";

// ===================== PERANGKAT KERAS =====================
const byte dhtPin = 2;   // D4 (GPIO 2)
const byte ledPin = 5;   // D1 (GPIO 5)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// ===================== PARAMETER SISTEM =====================
const float THRESHOLD_BAHAYA = 31.0;   // Ambang suhu kritis (derajat C)
const float HISTERESIS       = 0.5;    // Latch dilepas jika suhu <= 31.0 - 0.5

const unsigned long sensorInterval    = 2000;   // Baca sensor + cek safety
const unsigned long telemetryInterval = 4000;   // Kirim telemetri
const unsigned long mqttRetryInterval = 4000;   // Jeda reconnect MQTT
const unsigned long wifiLogInterval   = 1000;   // Jeda cetak titik saat menunggu Wi-Fi

// ===================== VARIABEL STATE =====================
unsigned long lastSensorTime    = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastMqttAttempt   = 0;
unsigned long lastWifiLog       = 0;

float suhuTerakhir   = NAN;
float lembapTerakhir = NAN;

bool bahayaLatch  = false;   // true = bahaya sudah dilaporkan
bool wifiTercetak = false;   // IP dicetak sekali per koneksi

// ===================== TES LED SAAT BOOT =====================
// LED berkedip 3 kali (6 kali toggle) tanpa delay()
void tesLED() {
  Serial.println("[TES LED] LED akan berkedip 3 kali...");
  for (byte i = 0; i < 6; i++) {
    digitalWrite(ledPin, !digitalRead(ledPin));
    unsigned long mulai = millis();
    while (millis() - mulai < 250) yield();
  }
  digitalWrite(ledPin, LOW);
  Serial.println("[TES LED] Selesai.");
}

// ===================== CALLBACK KENDALI MANUAL =====================
void callback(char* topic, byte* payload, unsigned int length) {
  char incoming[128];
  if (length >= sizeof(incoming)) {
    Serial.println("[KENDALI] Payload terlalu panjang, diabaikan.");
    return;
  }
  memcpy(incoming, payload, length);
  incoming[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incoming);

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, incoming);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  if (doc.containsKey("led")) {
    int val = doc["led"];
    Serial.printf("--> [DEBUG] Nilai led=%d | bahayaLatch=%s\n",
                  val, bahayaLatch ? "AKTIF" : "tidak aktif");

    if (bahayaLatch) {
      // Saat bahaya, LED dikunci ON oleh safety engine
      Serial.println("--> [DITOLAK] Kondisi BAHAYA aktif, LED dikunci menyala.");
    } else {
      digitalWrite(ledPin, val == 1 ? HIGH : LOW);
      Serial.printf("--> [INDIKATOR] Pin D1 (GPIO%d) sekarang: %s\n",
                    ledPin, digitalRead(ledPin) ? "HIGH (MENYALA)" : "LOW (PADAM)");
    }
  } else {
    Serial.println("--> [DEBUG] Key \"led\" tidak ditemukan di payload!");
  }
  Serial.println("----------------------------------------");
}

// ===================== RECONNECT MQTT (NON-BLOCKING) =====================
void reconnectMQTT() {
  unsigned long now = millis();
  if (now - lastMqttAttempt < mqttRetryInterval) return;
  lastMqttAttempt = now;

  Serial.print("Menghubungkan ke MQTT Broker...");

  // connect(id, user, pass, willTopic, willQoS, willRetain, willMessage)
  if (client.connect(clientId, NULL, NULL, topicStatus, 1, true, msgOffline)) {
    Serial.println(" Sukses Terhubung!");

    // Pesan ONLINE (retained) menimpa pesan OFFLINE lama di broker
    client.publish(topicStatus, msgOnline, true);

    // Subscribe ulang karena clean session menghapus langganan lama
    client.subscribe(topicKendali);
    Serial.printf("Subskripsi Topik: %s\n", topicKendali);
  } else {
    Serial.printf(" Gagal (rc=%d). Coba lagi dalam %lu detik...\n",
                  client.state(), mqttRetryInterval / 1000);
  }
}

// ===================== EDGE SAFETY ENGINE =====================
void evaluasiKeselamatan(float t) {
  if (t > THRESHOLD_BAHAYA) {
    if (!bahayaLatch) {
      // Transisi NORMAL -> BAHAYA (pertama kali)
      bahayaLatch = true;
      digitalWrite(ledPin, HIGH);

      StaticJsonDocument<128> docAlert;
      docAlert["status"]     = "BAHAYA";
      docAlert["parameter"]  = "OVERHEAT";
      docAlert["nilai_suhu"] = serialized(String(t, 1));

      char alertBuffer[128];
      serializeJson(docAlert, alertBuffer);

      if (client.publish(topicAlert, alertBuffer)) {
        Serial.printf("[ALERT TERKIRIM] %s\n", alertBuffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan alert!");
      }
    }
  } else if (bahayaLatch && t <= (THRESHOLD_BAHAYA - HISTERESIS)) {
    // Suhu sudah aman: lepas latch, kendali manual aktif lagi
    bahayaLatch = false;
    digitalWrite(ledPin, LOW);
    Serial.println("[SAFETY] Suhu kembali aman. Latch dilepas, kendali manual aktif.");
  }
}

// ===================== PUBLIKASI TELEMETRI =====================
void publishTelemetri() {
  if (isnan(suhuTerakhir) || isnan(lembapTerakhir)) return;

  StaticJsonDocument<256> docOut;
  docOut["node_id"]      = clientId;
  docOut["suhu"]         = serialized(String(suhuTerakhir, 2));
  docOut["kelembapan"]   = serialized(String(lembapTerakhir, 2));
  docOut["status_led"]   = digitalRead(ledPin);
  docOut["kondisi_suhu"] = (suhuTerakhir > THRESHOLD_BAHAYA) ? "PANAS" : "NORMAL";

  char buffer[256];
  serializeJson(docOut, buffer);

  if (client.publish(topicTelemetri, buffer)) {
    Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
  } else {
    Serial.println("[ERROR] Gagal memublikasikan telemetri!");
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  tesLED();                      // Tes wiring LED saat boot

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Menghubungkan ke Wi-Fi: %s\n", ssid);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(512);     // Buffer lebih longgar untuk JSON
  client.setKeepAlive(10);       // LWT terpicu lebih cepat
}

// ===================== LOOP UTAMA =====================
void loop() {
  unsigned long now = millis();

  // 1. Pastikan Wi-Fi terhubung (tanpa delay)
  if (WiFi.status() != WL_CONNECTED) {
    wifiTercetak = false;
    if (now - lastWifiLog >= wifiLogInterval) {
      lastWifiLog = now;
      Serial.print(".");
    }
    return;
  }
  if (!wifiTercetak) {
    wifiTercetak = true;
    Serial.println("\nWi-Fi Terkoneksi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // 2. Pastikan MQTT terhubung (non-blocking)
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // 3. Akuisisi sensor + evaluasi keselamatan
  if (now - lastSensorTime >= sensorInterval) {
    lastSensorTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Peringatan: Gagal membaca data DHT!");
    } else {
      suhuTerakhir   = t;
      lembapTerakhir = h;
      if (client.connected()) {
        evaluasiKeselamatan(t);
      } else if (t > THRESHOLD_BAHAYA) {
        // Proteksi lokal tetap jalan saat MQTT putus
        digitalWrite(ledPin, HIGH);
      }
    }
  }

  // 4. Publikasi telemetri rutin
  if (client.connected() && (now - lastTelemetryTime >= telemetryInterval)) {
    lastTelemetryTime = now;
    publishTelemetri();
  }
}