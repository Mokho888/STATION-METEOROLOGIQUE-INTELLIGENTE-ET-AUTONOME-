// ╔══════════════════════════════════════════════════════════════════╗
// ║  STATION MÉTÉO — CODE INTÉGRÉ COMPLET                           ║
// ║  ESP32 DevKit V1                                                 ║
// ║  Parties 1 (BME280+RTC) + 2 (FC-03+AS5600)                     ║
// ║       + 3 (MH-RD+HW-038) + 4 (GPS+TFT)                         ║
// ╠══════════════════════════════════════════════════════════════════╣
// ║  MODIFICATIONS GPIO (conflits résolus) :                         ║
// ║   TFT_RST  : GPIO 4  → GPIO 13                                  ║
// ║   TOUCH_CS : GPIO 21 → GPIO 25                                  ║
// ║   TOUCH_IRQ: GPIO 22 → GPIO 26                                  ║
// ║   FC-03    : GPIO 4  → GPIO 14                                  ║
// ║   MH-RD    : GPIO 4  → GPIO 32                                  ║
// ║   LED pluie: GPIO 5  → GPIO 33                                  ║
// ║   HW-038 VCC: GPIO 15 → 3V3 direct (pas sur GPIO)              ║
// ╚══════════════════════════════════════════════════════════════════╝

// ─── Includes ──────────────────────────────────────────────────────
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <RTClib.h>

// ═══════════════════════════════════════════════════════════════════
//  BROCHES — AFFECTATION FINALE (tous conflits résolus)
// ═══════════════════════════════════════════════════════════════════

// ── Écran TFT ILI9341 (SPI) ──────────────────────────────────────
#define TFT_DC    2
#define TFT_CS    15
#define TFT_RST   13    // ← était 4 → conflit FC-03 + MH-RD
// SPI partagé : SCK=18, MOSI=23, MISO=19

// ── Tactile XPT2046 (SPI partagé) ────────────────────────────────
#define TOUCH_CS  25    // ← était 21 → conflit SDA I2C
#define TOUCH_IRQ 26    // ← était 22 → conflit SCL I2C

// ── I2C partagé : BME280 (0x76/0x77) + DS3231 (0x68) + AS5600 (0x36)
#define SDA_PIN   21
#define SCL_PIN   22

// ── GPS NEO-6M (UART2) ────────────────────────────────────────────
#define GPS_RX    16
#define GPS_TX    17

// ── FC-03 — Capteur vitesse vent ─────────────────────────────────
#define FC03_PIN  14    // ← était 4

// ── MH-RD — Détection pluie ──────────────────────────────────────
#define MH_RD_PIN 32    // ← était 4 (GPIO 32 supporte INPUT_PULLUP)

// ── HW-038 — Niveau d'eau (ADC) ──────────────────────────────────
#define HW038_PIN 35    // ADC input-only, inchangé
// ⚠️ VCC du HW-038 → broche 3V3 physique (pas sur GPIO)

// ── LED indicateur pluie (optionnel) ─────────────────────────────
#define PIN_SIGNAL 33   // ← était 5 (GPIO 5 = strapping pin, à éviter)

// ═══════════════════════════════════════════════════════════════════
//  OBJETS
// ═══════════════════════════════════════════════════════════════════
Adafruit_ILI9341    tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TinyGPSPlus         gps;
HardwareSerial      GPS_Serial(2);
Adafruit_BME280     bme;
RTC_DS3231          rtc;

// ═══════════════════════════════════════════════════════════════════
//  CALIBRATION TACTILE
// ═══════════════════════════════════════════════════════════════════
#define X_MIN 370
#define X_MAX 3800
#define Y_MIN 260
#define Y_MAX 3600

// ═══════════════════════════════════════════════════════════════════
//  VARIABLES PARTAGÉES — DONNÉES CAPTEURS
// ═══════════════════════════════════════════════════════════════════

// ── Partie 1 — BME280 + DS3231 ───────────────────────────────────
float temperature = 0.0;
float humidite    = 0.0;
float pression    = 0.0;
char  dateStr[12] = "--/--/--";
char  heureStr[12] = "--:--:--";

// ── Partie 2 — FC-03 + AS5600 ────────────────────────────────────
float vitesseVent      = 0.0;   // km/h
char  directionVent[8] = "---";

// ── Partie 3 — MH-RD + HW-038 ────────────────────────────────────
float pluviometrie = 0.0;       // mm/heure

// ── Partie 4 — GPS NEO-6M ────────────────────────────────────────
float latGPS     = 0.0;
float lonGPS     = 0.0;
char  altGPS[10] = "---";
int   satGPS     = 0;

// ═══════════════════════════════════════════════════════════════════
//  PALETTE DE COULEURS
// ═══════════════════════════════════════════════════════════════════
#define C_BG      0x0000
#define C_CARD    0x1082
#define C_DKGREY  0x2104
#define C_GREY    0x8410
#define C_WHITE   ILI9341_WHITE
#define C_CYAN    ILI9341_CYAN
#define C_ORANGE  ILI9341_ORANGE
#define C_YELLOW  ILI9341_YELLOW
#define C_GREEN   ILI9341_GREEN
#define C_BLUE    ILI9341_BLUE
#define C_RED     ILI9341_RED
#define C_NAVY    ILI9341_NAVY
#define C_LGREY   ILI9341_LIGHTGREY

#define C_TITLE_P1  0x0010
#define C_TITLE_P2  0x3000
#define C_TITLE_P3  0x0010

// ═══════════════════════════════════════════════════════════════════
//  PARTIE 2 — CONSTANTES ANÉMOMÈTRE / AS5600
// ═══════════════════════════════════════════════════════════════════
#define NB_FENTES      30
#define RAYON_M        0.04f
#define DIST_PAR_PULSE (TWO_PI * RAYON_M / NB_FENTES)  // ≈ 0.008378 m
#define MOYS_NB        5

#define AS5600_ADDR    0x36
#define REG_ANGLE_MSB  0x0E

volatile uint32_t g_pulses = 0;
float   buf_vitesse[MOYS_NB] = {0};
uint8_t idx_moy = 0;

// ═══════════════════════════════════════════════════════════════════
//  PARTIE 3 — CONSTANTES PLUVIOMÈTRE
// ═══════════════════════════════════════════════════════════════════
#define MM_PAR_BASCULE  0.2794f
#define DEBOUNCE_MS     200
#define TIMEOUT_PLUIE   60000UL

// Variables ISR (volatile)
volatile unsigned long compteur_pluie      = 0;
volatile unsigned long dernierTemps_pluie  = 0;
volatile bool          nouvelleBascule     = false;
volatile unsigned long bascules1min        = 0;

// Variables loop
float         mmTotal        = 0.0;
bool          pluieEnCours   = false;
unsigned long t1min          = 0;
unsigned long tDernierePluie = 0;

// ═══════════════════════════════════════════════════════════════════
//  VARIABLES DE TIMING ET NAVIGATION
// ═══════════════════════════════════════════════════════════════════
int           pageActuelle        = 1;
unsigned long dernierAffichageGPS = 0;   // GPS    → toutes les 2s
unsigned long derniereLecture1    = 0;   // BME280 → toutes les 5s
unsigned long derniereVitesse     = 0;   // FC-03  → toutes les 1s
unsigned long dernierRefresh      = 0;   // TFT    → toutes les 5s

// ═══════════════════════════════════════════════════════════════════
//  ISR (Interrupt Service Routines)
// ═══════════════════════════════════════════════════════════════════

// FC-03 — chaque fente = 1 impulsion
void IRAM_ATTR isrFC03() {
  g_pulses++;
}

// MH-RD — CHANGE (montée + descente = 1 bascule complète)
// ✅ Correction bug original : 2 attachInterrupt sur même pin = seul le 2ème actif
// Solution : 1 seule ISR avec CHANGE
void IRAM_ATTR isrPluie() {
  unsigned long maintenant = millis();
  if (maintenant - dernierTemps_pluie > DEBOUNCE_MS) {
    dernierTemps_pluie = maintenant;
    compteur_pluie++;
    bascules1min++;
    nouvelleBascule = true;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  PROTOTYPES
// ═══════════════════════════════════════════════════════════════════
void lireBME280etRTC();
void majVitesseVent(unsigned long dt_ms);
void majDirectionVent();
float moyenneGlissante(float val);
uint16_t lireAngleBrut();
void afficherPage(int num);
void dessinerTitre(const char* titre, uint16_t bg);
void dessinerBoutons();
void dessinerCarte(int x, int y, int w, int h,
                   const char* label, const char* val, const char* unit,
                   uint16_t col, int iconType);
void dessinerCartePleine(int y, uint16_t col, int iconType,
                          const char* label,
                          const char* line1, const char* line2);
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt);
void drawIcon(int type, int cx, int cy, uint16_t c);

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== STATION MÉTÉO — Démarrage ===");

  // ── I2C partagé (BME280 + DS3231 + AS5600) ────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);  // Fast I2C 400kHz (compatible tous les 3)

  // ── BME280 ─────────────────────────────────────────────────────
  // Essai adresse 0x76 puis 0x77 selon la configuration du module
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("[ERREUR] BME280 non trouvé ! Vérifiez SDA=21 SCL=22");
  } else {
    Serial.println("[OK] BME280");
  }

  // ── RTC DS3231 ─────────────────────────────────────────────────
  if (!rtc.begin()) {
    Serial.println("[ERREUR] DS3231 non trouvé ! Vérifiez SDA=21 SCL=22");
  } else {
    Serial.println("[OK] RTC DS3231");
    // ⚠️ Décommenter UNE SEULE FOIS pour régler l'heure, puis re-commenter :
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // ── GPS NEO-6M (UART2) ─────────────────────────────────────────
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("[OK] GPS NEO-6M (RX=16, TX=17)");

  // ── FC-03 — Vitesse vent ───────────────────────────────────────
  pinMode(FC03_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FC03_PIN), isrFC03, FALLING);
  Serial.println("[OK] FC-03 sur GPIO " + String(FC03_PIN));

  // ── MH-RD — Pluviomètre ───────────────────────────────────────
  pinMode(MH_RD_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MH_RD_PIN), isrPluie, CHANGE);
  t1min = millis();
  Serial.println("[OK] MH-RD sur GPIO " + String(MH_RD_PIN));

  // ── HW-038 — Niveau d'eau ─────────────────────────────────────
  // VCC connecté à 3V3 physique directement (pas sur GPIO)
  pinMode(HW038_PIN, INPUT);
  analogReadResolution(12);   // ESP32 : résolution ADC 12 bits (0-4095)
  Serial.println("[OK] HW-038 sur GPIO " + String(HW038_PIN) + " (VCC→3V3)");

  // ── LED indicateur pluie ──────────────────────────────────────
  pinMode(PIN_SIGNAL, OUTPUT);
  digitalWrite(PIN_SIGNAL, LOW);

  // ── Écran TFT ILI9341 ─────────────────────────────────────────
  tft.begin();
  tft.setRotation(1);   // paysage
  ts.begin();
  ts.setRotation(1);
  Serial.println("[OK] TFT ILI9341 + XPT2046");

  // ── Première lecture des capteurs ─────────────────────────────
  lireBME280etRTC();
  derniereVitesse = millis();

  afficherPage(pageActuelle);
  Serial.println("=== Station prête ! ===\n");
}

// ═══════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════
void loop() {
  unsigned long maintenant = millis();

  // ────────────────────────────────────────────────────────────────
  //  1. GPS (non-bloquant — lecture permanente)
  // ────────────────────────────────────────────────────────────────
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  if (maintenant - dernierAffichageGPS > 2000) {
    dernierAffichageGPS = maintenant;
    bool gpsChange = false;

    satGPS = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;

    if (gps.location.isValid()) {
      float newLat = gps.location.lat();
      float newLon = gps.location.lng();
      if (newLat != latGPS || newLon != lonGPS) {
        latGPS = newLat;
        lonGPS = newLon;
        gpsChange = true;
      }
    }

    if (gps.altitude.isValid()) {
      char newAlt[10];
      dtostrf(gps.altitude.meters(), 5, 0, newAlt);
      if (strcmp(newAlt, altGPS) != 0) {
        strcpy(altGPS, newAlt);
        gpsChange = true;
      }
    }

    // Rafraîchir page 3 si GPS a changé
    if (gpsChange && pageActuelle == 3) {
      afficherPage(3);
    }
  }

  // ────────────────────────────────────────────────────────────────
  //  2. BME280 + RTC (toutes les 5 secondes)
  // ────────────────────────────────────────────────────────────────
  if (maintenant - derniereLecture1 > 5000) {
    derniereLecture1 = maintenant;
    lireBME280etRTC();
  }

  // ────────────────────────────────────────────────────────────────
  //  3. Vitesse + Direction vent (toutes les 1 seconde)
  // ────────────────────────────────────────────────────────────────
  static uint32_t pulses_prec = 0;

  if (maintenant - derniereVitesse >= 1000) {
    uint32_t dt = maintenant - derniereVitesse;
    derniereVitesse = maintenant;

    // Snapshot atomique du compteur ISR
    noInterrupts();
    uint32_t pulses_courants = g_pulses;
    interrupts();

    uint32_t delta = pulses_courants - pulses_prec;
    pulses_prec = pulses_courants;

    // Calcul vitesse (m/s → km/h avec moyenne glissante)
    float v_ms  = (delta * DIST_PAR_PULSE) / (dt / 1000.0f);
    float v_moy = moyenneGlissante(v_ms);
    vitesseVent = v_moy * 3.6f;

    // Direction via AS5600
    majDirectionVent();
  }

  // ────────────────────────────────────────────────────────────────
  //  4. Pluviomètre (géré par ISR + vérifications loop)
  // ────────────────────────────────────────────────────────────────

  // Nouvelle bascule détectée par l'ISR
  if (nouvelleBascule) {
    nouvelleBascule = false;

    noInterrupts();
    unsigned long b1  = bascules1min;
    unsigned long cnt = compteur_pluie;
    interrupts();

    // mm total accumulé (chaque bascule = demi-basculement = MM/2)
    mmTotal      = cnt * (MM_PAR_BASCULE / 2.0f);
    // mm/h estimé sur la fenêtre glissante de 1 minute
    pluviometrie = (b1 / 2.0f) * MM_PAR_BASCULE * 60.0f;

    pluieEnCours   = true;
    tDernierePluie = maintenant;
    digitalWrite(PIN_SIGNAL, HIGH);
  }

  // Reset compteur bascules toutes les 1 minute
  if (maintenant - t1min >= 60000UL) {
    noInterrupts();
    bascules1min = 0;
    interrupts();
    t1min = maintenant;
  }

  // Fin de pluie si pas de bascule depuis TIMEOUT_PLUIE (60s)
  if (pluieEnCours && (maintenant - tDernierePluie > TIMEOUT_PLUIE)) {
    pluieEnCours = false;
    pluviometrie = 0.0;
    digitalWrite(PIN_SIGNAL, LOW);
  }

  // ────────────────────────────────────────────────────────────────
  //  5. Auto-refresh affichage (toutes les 5 secondes)
  //     → met à jour les pages 1 et 2 avec les nouvelles valeurs
  // ────────────────────────────────────────────────────────────────
  if (maintenant - dernierRefresh > 5000) {
    dernierRefresh = maintenant;
    afficherPage(pageActuelle);
  }

  // ────────────────────────────────────────────────────────────────
  //  6. Gestion tactile (navigation entre pages)
  // ────────────────────────────────────────────────────────────────
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, X_MAX, X_MIN, 0, 320);
    int y = map(p.y, Y_MAX, Y_MIN, 0, 240);

    if (y > 200) {
      if (x < 110) {
        // Bouton PREC
        pageActuelle = (pageActuelle == 1) ? 3 : pageActuelle - 1;
        afficherPage(pageActuelle);
        delay(300);
      } else if (x > 210) {
        // Bouton SUIV
        pageActuelle = (pageActuelle == 3) ? 1 : pageActuelle + 1;
        afficherPage(pageActuelle);
        delay(300);
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
//  FONCTIONS CAPTEURS
// ═══════════════════════════════════════════════════════════════════

void lireBME280etRTC() {
  temperature = bme.readTemperature();
  pression    = bme.readPressure() / 100.0F;
  humidite    = bme.readHumidity();

  DateTime now = rtc.now();
  snprintf(dateStr,  sizeof(dateStr),  "%02d/%02d/%02d",
           now.day(), now.month(), now.year() % 100);
  snprintf(heureStr, sizeof(heureStr), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());
}

uint16_t lireAngleBrut() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_ANGLE_MSB);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  uint16_t msb = Wire.read();
  uint16_t lsb = Wire.read();
  return ((msb << 8) | lsb) & 0x0FFF;   // 12 bits
}

void majDirectionVent() {
  static const char* CARDINAUX[16] = {
    "N","NNE","NE","ENE","E","ESE","SE","SSE",
    "S","SSO","SO","OSO","O","ONO","NO","NNO"
  };
  uint16_t brut    = lireAngleBrut();
  float    dir_deg = (brut / 4096.0f) * 360.0f;
  uint8_t  idx     = (uint8_t)((dir_deg + 11.25f) / 22.5f) % 16;
  strncpy(directionVent, CARDINAUX[idx], 7);
  directionVent[7] = '\0';
}

float moyenneGlissante(float val) {
  buf_vitesse[idx_moy % MOYS_NB] = val;
  idx_moy++;
  float somme = 0;
  for (uint8_t i = 0; i < MOYS_NB; i++) somme += buf_vitesse[i];
  return somme / MOYS_NB;
}

// ═══════════════════════════════════════════════════════════════════
//  ICÔNES (dessinées par primitives GFX — aucune lib bitmap)
// ═══════════════════════════════════════════════════════════════════

void iconThermometre(int cx, int cy, uint16_t c) {
  tft.fillRoundRect(cx-4, cy-16, 8, 24, 4, C_DKGREY);
  tft.drawRoundRect(cx-4, cy-16, 8, 24, 4, c);
  tft.fillRect(cx-2, cy, 4, 8, c);
  tft.fillCircle(cx, cy+13, 8, c);
  tft.drawCircle(cx, cy+13, 9, c);
  tft.drawLine(cx+4, cy-8, cx+8, cy-8, c);
  tft.drawLine(cx+4, cy-2, cx+7, cy-2, c);
}

void iconGoutte(int cx, int cy, uint16_t c) {
  tft.fillTriangle(cx, cy-16, cx-9, cy-3, cx+9, cy-3, c);
  tft.fillCircle(cx, cy+3, 10, c);
  tft.fillCircle(cx-3, cy-3, 3, C_LGREY);
}

void iconJauge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);
  tft.drawLine(cx-13, cy,    cx-9, cy,   c);
  tft.drawLine(cx,    cy-13, cx,   cy-9, c);
  tft.drawLine(cx+13, cy,    cx+9, cy,   c);
  tft.drawLine(cx, cy, cx-7, cy-10, C_RED);
  tft.drawLine(cx, cy, cx-8, cy-9,  C_RED);
  tft.fillCircle(cx, cy, 2, C_WHITE);
}

void iconVent(int cx, int cy, uint16_t c) {
  tft.drawLine(cx-13, cy-7, cx+9,  cy-7, c);
  tft.drawLine(cx-13, cy,   cx+12, cy,   c);
  tft.drawLine(cx-13, cy+7, cx+5,  cy+7, c);
  tft.drawCircle(cx+9,  cy-10, 3, c);
  tft.drawCircle(cx+12, cy+3,  3, c);
  tft.drawCircle(cx+5,  cy+10, 3, c);
}

void iconBoussole(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);
  tft.fillTriangle(cx, cy-12, cx-5, cy,   cx+5, cy,   C_RED);
  tft.fillTriangle(cx, cy+12, cx-5, cy+1, cx+5, cy+1, C_DKGREY);
  tft.fillRect(cx-1, cy-14, 3, 3, c);
}

void iconPluie(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx-7,  cy-8,  8, c);
  tft.fillCircle(cx+4,  cy-10, 9, c);
  tft.fillCircle(cx+12, cy-6,  7, c);
  tft.fillRect(cx-14, cy-8, 30, 10, c);
  tft.fillRoundRect(cx-9, cy+5, 3, 8, 1, C_CYAN);
  tft.fillRoundRect(cx-1, cy+8, 3, 8, 1, C_CYAN);
  tft.fillRoundRect(cx+7, cy+5, 3, 8, 1, C_CYAN);
}

void iconCalendrier(int cx, int cy, uint16_t c) {
  tft.drawRoundRect(cx-13, cy-10, 26, 23, 2, c);
  tft.fillRoundRect(cx-13, cy-10, 26,  8, 2, c);
  tft.fillRect(cx-8, cy-14, 4, 6, c);
  tft.fillRect(cx+4, cy-14, 4, 6, c);
  tft.drawLine(cx,    cy-2, cx,    cy+11, C_GREY);
  tft.drawLine(cx-13, cy+4, cx+13, cy+4,  C_GREY);
}

void iconHorloge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 14, c);
  tft.drawCircle(cx, cy, 13, c);
  tft.drawLine(cx, cy, cx,   cy-8, c);
  tft.drawLine(cx, cy, cx+7, cy+4, c);
  tft.fillCircle(cx, cy, 2, c);
  tft.fillRect(cx-1, cy-14, 3, 3, c);
}

void iconGPS(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx, cy-7, 10, c);
  tft.fillTriangle(cx-7, cy-2, cx+7, cy-2, cx, cy+14, c);
  tft.fillCircle(cx, cy-7, 5, C_CARD);
  tft.drawCircle(cx, cy-7, 13, c);
}

void drawIcon(int type, int cx, int cy, uint16_t c) {
  switch (type) {
    case 0: iconThermometre(cx, cy, c); break;
    case 1: iconGoutte(cx, cy, c);      break;
    case 2: iconJauge(cx, cy, c);       break;
    case 4: iconVent(cx, cy, c);        break;
    case 5: iconBoussole(cx, cy, c);    break;
    case 6: iconPluie(cx, cy, c);       break;
    case 7: iconCalendrier(cx, cy, c);  break;
    case 8: iconHorloge(cx, cy, c);     break;
    case 9: iconGPS(cx, cy, c);         break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  COMPOSANTS D'INTERFACE
// ═══════════════════════════════════════════════════════════════════

void dessinerTitre(const char* titre, uint16_t bg) {
  tft.fillRect(0, 0, 320, 33, bg);
  tft.fillRect(0, 0, 4, 33, C_CYAN);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 9);
  tft.print(titre);
  tft.fillRoundRect(264, 5, 52, 22, 5, C_CYAN);
  tft.setTextColor(C_NAVY);
  tft.setTextSize(1);
  tft.setCursor(272, 12);
  tft.print("Page ");
  tft.print(pageActuelle);
}

void dessinerBoutons() {
  tft.fillRect(0, 200, 320, 40, C_NAVY);
  tft.drawLine(0, 200, 320, 200, C_CYAN);
  tft.fillRoundRect(8,   207, 90, 26, 5, C_CARD);
  tft.drawRoundRect(8,   207, 90, 26, 5, C_CYAN);
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(15, 213);
  tft.print("< PREC");
  for (int i = 1; i <= 3; i++) {
    int dx = 140 + (i - 1) * 14;
    if (i == pageActuelle) tft.fillCircle(dx, 220, 5, C_CYAN);
    else                   tft.drawCircle(dx, 220, 5, C_GREY);
  }
  tft.fillRoundRect(222, 207, 90, 26, 5, C_CARD);
  tft.drawRoundRect(222, 207, 90, 26, 5, C_CYAN);
  tft.setTextColor(C_CYAN);
  tft.setCursor(230, 213);
  tft.print("SUIV >");
}

// Carte standard [icone | label / valeur / unite]
void dessinerCarte(int x, int y, int w, int h,
                   const char* label, const char* val, const char* unit,
                   uint16_t col, int iconType) {
  tft.fillRoundRect(x, y, w, h, 8, C_CARD);
  tft.drawRoundRect(x, y, w, h, 8, col);
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);
  drawIcon(iconType, x+27, y + h/2, col);
  tft.drawLine(x+52, y+10, x+52, y+h-10, C_DKGREY);
  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(x+57, y+10);
  tft.print(label);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(x+57, y+27);
  tft.print(val);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(x+57, y+52);
  tft.print(unit);
}

// Carte pleine largeur [icone | label / ligne1 / ligne2]
void dessinerCartePleine(int y, uint16_t col, int iconType,
                          const char* label,
                          const char* line1, const char* line2) {
  tft.fillRoundRect(5, y, 310, 78, 8, C_CARD);
  tft.drawRoundRect(5, y, 310, 78, 8, col);
  tft.fillRoundRect(13, y, 294, 3, 1, col);
  drawIcon(iconType, 36, y + 39, col);
  tft.drawLine(58, y+10, 58, y+68, C_DKGREY);
  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(66, y+10);
  tft.print(label);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(66, y+26);
  tft.print(line1);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(66, y+52);
  tft.print(line2);
}

// Carte GPS — affiche position + indicateur fix vert/rouge
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt) {
  int x = 5, y = y_start, w = 310, h = 95;
  uint16_t col    = ILI9341_CYAN;
  uint16_t colFix = gps.location.isValid() ? ILI9341_GREEN : ILI9341_RED;

  tft.fillRoundRect(x, y, w, h, 8, C_CARD);
  tft.drawRoundRect(x, y, w, h, 8, col);
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);
  drawIcon(9, x+30, y + h/2, colFix);
  tft.drawLine(x+58, y+10, x+58, y+h-10, C_DKGREY);

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(x+65, y+8);
  tft.print("Localisation GPS");

  if (gps.location.isValid()) {
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(x+65, y+24);
    tft.print("Lat: ");
    tft.print(lat, 4);
    tft.print(" N");
    tft.setCursor(x+65, y+50);
    tft.print("Lon: ");
    tft.print(lon, 4);
    tft.print(" E");
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(x+65, y+80);
    tft.print("Alt: ");
    tft.print(alt);
    tft.print(" m   Sat: ");
    tft.print(satGPS);
  } else {
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(x+65, y+30);
    tft.print("Recherche satellites...");
    tft.setCursor(x+65, y+48);
    tft.print("Sat detectes : ");
    tft.print(satGPS);
    tft.setCursor(x+65, y+66);
    tft.print("Placez l'antenne vers le ciel");
  }
}

// ═══════════════════════════════════════════════════════════════════
//  PAGES
// ═══════════════════════════════════════════════════════════════════

void afficherPage(int num) {
  tft.fillScreen(C_BG);
  dessinerBoutons();

  char buf[16];

  // ── PAGE 1 : CLIMAT ──────────────────────────────────────────
  if (num == 1) {
    dessinerTitre("CLIMAT", C_TITLE_P1);

    dtostrf(temperature, 4, 1, buf);
    dessinerCarte(5,   36, 155, 78, "Temperature", buf, "deg C", C_ORANGE,      0);

    dtostrf(humidite, 4, 1, buf);
    dessinerCarte(163, 36, 152, 78, "Humidite",    buf, "%",     ILI9341_CYAN,  1);

    dtostrf(pression, 6, 1, buf);
    dessinerCarte(5,  118, 310, 78, "Pression",    buf, "hPa",  ILI9341_GREEN, 2);
  }

  // ── PAGE 2 : VENT & PLUIE ────────────────────────────────────
  else if (num == 2) {
    dessinerTitre("VENT & PLUIE", C_TITLE_P2);

    dtostrf(vitesseVent, 4, 1, buf);
    dessinerCarte(5,   36, 155, 78, "Vit. Vent", buf,           "km/h", C_ORANGE,      4);
    dessinerCarte(163, 36, 152, 78, "Direction", directionVent, "",     ILI9341_WHITE, 5);

    dtostrf(pluviometrie, 4, 1, buf);
    dessinerCartePleine(118, ILI9341_BLUE, 6,
                        "Pluviometrie",
                        buf,
                        "mm / heure");
  }

  // ── PAGE 3 : GPS & TEMPS ─────────────────────────────────────
  else if (num == 3) {
    dessinerTitre("GPS & TEMPS", C_TITLE_P3);

    dessinerCarte(5,   36, 153, 62, "Date",  dateStr,  "", ILI9341_WHITE,  7);
    dessinerCarte(162, 36, 153, 62, "Heure", heureStr, "", ILI9341_YELLOW, 8);

    dessinerCarteGPS(102, latGPS, lonGPS, altGPS);
  }
}
