/*
 * ============================================================
 *  STATION VENT — ESP32  →  Firebase
 *  FC-03  (GPIO 34)  +  AS5600  (I²C 0x36)
 * ============================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

/* ─── Config ───────────────────────────────────────────────── */
#define WIFI_SSID      "MEZX"
#define WIFI_PASS      "12312312345"

const char* FIREBASE_URL =
    "https://meteo-f6152-default-rtdb.europe-west1.firebasedatabase.app/weather.json";

/* ─── Broches ──────────────────────────────────────────────── */
#define FC03_PIN       4
#define SDA_PIN        21
#define SCL_PIN        22

/* ─── Anémomètre ───────────────────────────────────────────── */
#define NB_FENTES      30
#define RAYON_M        0.04f
#define DIST_PAR_PULSE (TWO_PI * RAYON_M / NB_FENTES)   // ≈ 0.008378 m
#define MOYS_NB        5

/* ─── AS5600 ───────────────────────────────────────────────── */
#define AS5600_ADDR    0x36
#define REG_ANGLE_MSB  0x0E

/* ─── Timings ──────────────────────────────────────────────── */
#define T_VENT_MS      1000    // calcul vitesse
#define T_FIREBASE_MS  5000    // envoi Firebase

/* ─── Variables ────────────────────────────────────────────── */
volatile uint32_t g_pulses = 0;
float   buf_moy[MOYS_NB]   = {0};
uint8_t idx_moy             = 0;

float   v_ms       = 0.0f;
float   v_kmh      = 0.0f;
uint8_t beaufort   = 0;
float   dir_deg    = 0.0f;
String  cardinal   = "N";

/* ─── Prototypes ───────────────────────────────────────────── */
void    IRAM_ATTR isrFC03();
uint16_t lireAngle();
float   calcVitesse(uint32_t imp, uint32_t dt_ms);
float   moyGlissante(float val);
uint8_t echelleBeaufort(float v);
String  degreVersCardinal(float deg);
void    envoyerFirebase();

/* ═══════════════════════════════════════════════════════════
   SETUP
   ═══════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);

    /* FC-03 */
    pinMode(FC03_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(FC03_PIN), isrFC03, FALLING);

    /* I²C */
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    /* WiFi */
    Serial.print("[WiFi] Connexion");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
    Serial.println("\n[WiFi] OK  IP: " + WiFi.localIP().toString());
    Serial.println("[Station vent] Demarree");
}

/* ═══════════════════════════════════════════════════════════
   LOOP
   ═══════════════════════════════════════════════════════════ */
void loop()
{
    static uint32_t t_vent     = 0;
    static uint32_t t_firebase = 0;
    static uint32_t pulses_prec = 0;

    uint32_t now = millis();

    /* — Calcul vent toutes les 1 s — */
    if (now - t_vent >= T_VENT_MS) {
        uint32_t dt = now - t_vent;
        t_vent = now;

        noInterrupts();
        uint32_t pc = g_pulses;
        interrupts();

        v_ms     = moyGlissante(calcVitesse(pc - pulses_prec, dt));
        v_kmh    = v_ms * 3.6f;
        beaufort = echelleBeaufort(v_ms);
        dir_deg  = (lireAngle() / 4096.0f) * 360.0f;
        cardinal = degreVersCardinal(dir_deg);
        pulses_prec = pc;

        /* Serial */
        Serial.printf("[Vent] %.3f m/s | %.2f km/h | Bf %d | %.1f deg %s\n",
                      v_ms, v_kmh, beaufort, dir_deg, cardinal.c_str());
    }

    /* — Firebase toutes les 5 s — */
    if (now - t_firebase >= T_FIREBASE_MS) {
        t_firebase = now;
        envoyerFirebase();
    }
}

/* ═══════════════════════════════════════════════════════════
   ISR
   ═══════════════════════════════════════════════════════════ */
void IRAM_ATTR isrFC03() { g_pulses++; }

/* ═══════════════════════════════════════════════════════════
   AS5600 — lecture angle brut 12 bits
   ═══════════════════════════════════════════════════════════ */
uint16_t lireAngle()
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(REG_ANGLE_MSB);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_ADDR, (uint8_t)2);
    if (Wire.available() < 2) return 0;
    return (((uint16_t)Wire.read() << 8) | Wire.read()) & 0x0FFF;
}

/* ═══════════════════════════════════════════════════════════
   Calculs
   ═══════════════════════════════════════════════════════════ */
float calcVitesse(uint32_t imp, uint32_t dt_ms)
{
    if (dt_ms == 0) return 0.0f;
    return (imp * DIST_PAR_PULSE) / (dt_ms / 1000.0f);
}

float moyGlissante(float val)
{
    buf_moy[idx_moy++ % MOYS_NB] = val;
    float s = 0;
    for (uint8_t i = 0; i < MOYS_NB; i++) s += buf_moy[i];
    return s / MOYS_NB;
}

uint8_t echelleBeaufort(float v)
{
    static const float S[12] = {0.3,1.5,3.4,5.4,7.9,10.7,13.8,17.1,20.7,24.4,28.4,32.7};
    for (uint8_t i = 0; i < 12; i++) if (v < S[i]) return i;
    return 12;
}

String degreVersCardinal(float deg)
{
    static const char* C[16] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSO","SO","OSO","O","ONO","NO","NNO"
    };
    return String(C[(uint8_t)((deg + 11.25f) / 22.5f) % 16]);
}

/* ═══════════════════════════════════════════════════════════
   Firebase — PUT vent uniquement
   ═══════════════════════════════════════════════════════════ */
void envoyerFirebase()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Firebase] WiFi deconnecte, skip.");
        return;
    }

    HTTPClient http;
    http.begin(FIREBASE_URL);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"vent_ms\":"       + String(v_ms, 3)    + ",";
    json += "\"vent_kmh\":"      + String(v_kmh, 2)   + ",";
    json += "\"beaufort\":"      + String(beaufort)    + ",";
    json += "\"direction_deg\":" + String(dir_deg, 1)  + ",";
    json += "\"cardinal\":\""   + cardinal             + "\"";
    json += "}";

    int code = http.PUT(json);
    Serial.printf("[Firebase] Envoye -> code %d\n", code);
    http.end();
}