// ╔══════════════════════════════════════════════════════════════════╗
// ║  STATION MÉTÉO — CODE COMPLET                                    ║
// ║  ESP32                                                           ║
// ║  BME280 + DS3231 + FC-03 + AS5600 + MH-RD + HW-038               ║
// ║  GPS NEO-6M + TFT ILI9341 + WiFi + Firebase HTTPS                ║
// ╠══════════════════════════════════════════════════════════════════╣
// ║  BROCHES FINALES :                                               ║
// ║   TFT_RST  : GPIO 13                                             ║
// ║   TOUCH_CS : GPIO 25                                             ║
// ║   TOUCH_IRQ: GPIO 26                                             ║
// ║   FC-03    : GPIO 14                                             ║
// ║   MH-RD    : GPIO 32                                             ║
// ║   LED pluie: GPIO 33                                             ║
// ║   HW-038 VCC: 3V3 direct (pas sur GPIO)                          ║
// ╚══════════════════════════════════════════════════════════════════╝

// ─── Includes ──────────────────────────────────────────────────────
#include <SPI.h>                   // Communication SPI (TFT + Tactile)
#include <Wire.h>                  // Communication I2C (BME280 + DS3231 + AS5600)
#include <WiFi.h>                  // Connexion WiFi ESP32
#include <WiFiClientSecure.h>      // Client WiFi sécurisé SSL/TLS pour HTTPS
#include <HTTPClient.h>            // Envoi de requêtes HTTP/HTTPS
#include <WebServer.h>             // Serveur web local sur port 80
#include <Adafruit_GFX.h>          // Librairie graphique de base pour TFT
#include <Adafruit_ILI9341.h>      // Librairie écran TFT ILI9341
#include <XPT2046_Touchscreen.h>   // Librairie tactile XPT2046
#include <TinyGPS++.h>             // Décodage trames GPS NMEA
#include <HardwareSerial.h>        // UART matériel pour GPS
#include <Adafruit_Sensor.h>       // Dépendance Adafruit capteurs
#include <Adafruit_BME280.h>       // Capteur température/humidité/pression
#include <RTClib.h>                // Horloge temps réel DS3231

// ═══════════════════════════════════════════════════════════════════
//  BROCHES — AFFECTATION FINALE
// ═══════════════════════════════════════════════════════════════════

// ── Écran TFT ILI9341 (SPI) ──────────────────────────────────────
#define TFT_DC    2                // Data/Command TFT
#define TFT_CS    15               // Chip Select TFT
#define TFT_RST   13               // Reset TFT (était 4 → conflit résolu)
// SPI partagé : SCK=18, MOSI=23, MISO=19

// ── Tactile XPT2046 (SPI partagé) ────────────────────────────────
#define TOUCH_CS  25               // Chip Select tactile (était 21 → conflit I2C)
#define TOUCH_IRQ 26               // IRQ tactile (était 22 → conflit I2C)

// ── I2C partagé : BME280 (0x76) + DS3231 (0x68) + AS5600 (0x36) ─
#define SDA_PIN   21               // Broche données I2C
#define SCL_PIN   22               // Broche horloge I2C

// ── GPS NEO-6M (UART2) ────────────────────────────────────────────
#define GPS_RX    16               // RX ESP32 ← TX GPS
#define GPS_TX    17               // TX ESP32 → RX GPS

// ── FC-03 — Capteur vitesse vent ─────────────────────────────────
#define FC03_PIN  14               // Sortie impulsions anémomètre (était 4)

// ── MH-RD — Pluviomètre à bascule ────────────────────────────────
#define MH_RD_PIN 32               // Sortie numérique pluviomètre (était 4)

// ── HW-038 — Niveau d'eau (ADC) ──────────────────────────────────
#define HW038_PIN 35               // ADC input-only (VCC → 3V3 physique)

// ── LED indicateur pluie ─────────────────────────────────────────
#define PIN_SIGNAL 33              // LED allumée quand pluie détectée (était 5)

// ═══════════════════════════════════════════════════════════════════
//  WIFI + FIREBASE
// ═══════════════════════════════════════════════════════════════════
const char* ssid        = "MEZX";           // Nom du réseau WiFi
const char* password    = "12312312345";    // Mot de passe WiFi
const char* firebaseURL = "https://meteo-f6152-default-rtdb.europe-west1.firebasedatabase.app/weather.json";
// URL HTTPS Firebase Realtime Database

// ═══════════════════════════════════════════════════════════════════
//  OBJETS
// ═══════════════════════════════════════════════════════════════════
Adafruit_ILI9341    tft(TFT_CS, TFT_DC, TFT_RST); // Écran TFT ILI9341
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);        // Écran tactile XPT2046
TinyGPSPlus         gps;                            // Décodeur GPS NMEA
HardwareSerial      GPS_Serial(2);                  // UART2 pour GPS
Adafruit_BME280     bme;                            // Capteur BME280 I2C
RTC_DS3231          rtc;                            // Horloge DS3231 I2C
WebServer           server(80);                     // Serveur web port 80

// ═══════════════════════════════════════════════════════════════════
//  CALIBRATION TACTILE
// ═══════════════════════════════════════════════════════════════════
#define X_MIN 370                  // Valeur brute X minimum du tactile
#define X_MAX 3800                 // Valeur brute X maximum du tactile
#define Y_MIN 260                  // Valeur brute Y minimum du tactile
#define Y_MAX 3600                 // Valeur brute Y maximum du tactile

// ═══════════════════════════════════════════════════════════════════
//  VARIABLES — DONNÉES CAPTEURS
// ═══════════════════════════════════════════════════════════════════

// ── BME280 + DS3231 ───────────────────────────────────────────────
float temperature = 0.0;           // Température en °C
float humidite    = 0.0;           // Humidité relative en %
float pression    = 0.0;           // Pression atmosphérique en hPa
char  dateStr[12] = "--/--/--";    // Date formatée "JJ/MM/AA"
char  heureStr[12] = "--:--:--";   // Heure formatée "HH:MM:SS"

// ── FC-03 + AS5600 ────────────────────────────────────────────────
float vitesseVent      = 0.0;      // Vitesse du vent en km/h
char  directionVent[8] = "---";    // Direction du vent (N, NE, E, etc.)

// ── MH-RD + HW-038 ───────────────────────────────────────────────
float pluviometrie = 0.0;          // Pluviométrie estimée en mm/heure

// ── GPS NEO-6M ────────────────────────────────────────────────────
float latGPS     = 0.0;            // Latitude GPS en degrés décimaux
float lonGPS     = 0.0;            // Longitude GPS en degrés décimaux
char  altGPS[10] = "---";          // Altitude GPS en mètres
int   satGPS     = 0;              // Nombre de satellites GPS visibles

// ═══════════════════════════════════════════════════════════════════
//  PALETTE DE COULEURS TFT
// ═══════════════════════════════════════════════════════════════════
#define C_BG      0x0000           // Fond noir
#define C_CARD    0x1082           // Gris très foncé (cartes)
#define C_DKGREY  0x2104           // Gris foncé (séparateurs)
#define C_GREY    0x8410           // Gris moyen
#define C_WHITE   ILI9341_WHITE    // Blanc
#define C_CYAN    ILI9341_CYAN     // Cyan (accents)
#define C_ORANGE  ILI9341_ORANGE   // Orange (température)
#define C_YELLOW  ILI9341_YELLOW   // Jaune (heure)
#define C_GREEN   ILI9341_GREEN    // Vert (pression / GPS fix)
#define C_BLUE    ILI9341_BLUE     // Bleu (pluie)
#define C_RED     ILI9341_RED      // Rouge (erreur / aiguille)
#define C_NAVY    ILI9341_NAVY     // Bleu marine (boutons)
#define C_LGREY   ILI9341_LIGHTGREY // Gris clair

#define C_TITLE_P1  0x0010         // Couleur titre page 1 (Climat)
#define C_TITLE_P2  0x3000         // Couleur titre page 2 (Vent & Pluie)
#define C_TITLE_P3  0x0010         // Couleur titre page 3 (GPS & Temps)

// ═══════════════════════════════════════════════════════════════════
//  CONSTANTES ANÉMOMÈTRE FC-03 / AS5600
// ═══════════════════════════════════════════════════════════════════
#define NB_FENTES      30          // Nombre de fentes du disque FC-03
#define RAYON_M        0.04f       // Rayon de l'anémomètre en mètres
#define DIST_PAR_PULSE (TWO_PI * RAYON_M / NB_FENTES)  // Distance par impulsion ≈ 8.38mm
#define MOYS_NB        5           // Nombre de valeurs pour la moyenne glissante vitesse

#define AS5600_ADDR    0x36        // Adresse I2C du capteur magnétique AS5600
#define REG_ANGLE_MSB  0x0E        // Registre MSB de l'angle brut AS5600

volatile uint32_t g_pulses = 0;    // Compteur d'impulsions FC-03 (ISR)
float   buf_vitesse[MOYS_NB] = {0}; // Buffer moyenne glissante vitesse
uint8_t idx_moy = 0;               // Index courant du buffer moyenne

// ═══════════════════════════════════════════════════════════════════
//  CONSTANTES PLUVIOMÈTRE MH-RD
// ═══════════════════════════════════════════════════════════════════
#define MM_PAR_BASCULE  0.2794f    // mm de pluie par bascule complète
#define DEBOUNCE_MS     200        // Anti-rebond en millisecondes
#define TIMEOUT_PLUIE   60000UL    // Délai sans pluie avant remise à zéro (60s)

volatile unsigned long compteur_pluie      = 0;    // Compteur total de bascules (ISR)
volatile unsigned long dernierTemps_pluie  = 0;    // Timestamp dernière bascule (ISR)
volatile bool          nouvelleBascule     = false; // Flag nouvelle bascule détectée
volatile unsigned long bascules1min        = 0;    // Bascules sur la dernière minute

float         mmTotal        = 0.0;   // Total mm de pluie accumulés
bool          pluieEnCours   = false; // Indicateur pluie active
unsigned long t1min          = 0;     // Timestamp dernière remise à zéro 1min
unsigned long tDernierePluie = 0;     // Timestamp dernière bascule détectée

// ═══════════════════════════════════════════════════════════════════
//  VARIABLES DE TIMING ET NAVIGATION
// ═══════════════════════════════════════════════════════════════════
int           pageActuelle        = 1;     // Page TFT affichée (1, 2 ou 3)
unsigned long dernierAffichageGPS = 0;     // Dernière mise à jour GPS (toutes les 2s)
unsigned long derniereLecture1    = 0;     // Dernière lecture BME280 (toutes les 5s)
unsigned long derniereVitesse     = 0;     // Dernier calcul vitesse vent (toutes les 1s)
unsigned long dernierRefresh      = 0;     // Dernier rafraîchissement TFT (toutes les 5s)
unsigned long lastFirebase        = 0;     // Dernier envoi Firebase (toutes les 10s)

// ═══════════════════════════════════════════════════════════════════
//  ISR (Interrupt Service Routines)
// ═══════════════════════════════════════════════════════════════════

// FC-03 : chaque fente du disque génère 1 impulsion FALLING
void IRAM_ATTR isrFC03() {
  g_pulses++;                      // Incrémente compteur impulsions vent
}

// MH-RD : CHANGE = montée + descente = 1 bascule complète du pluviomètre
// ✅ Une seule ISR CHANGE évite le bug de deux attachInterrupt sur la même pin
void IRAM_ATTR isrPluie() {
  unsigned long maintenant = millis();
  if (maintenant - dernierTemps_pluie > DEBOUNCE_MS) { // Anti-rebond 200ms
    dernierTemps_pluie = maintenant;  // Mémorise le temps de la bascule
    compteur_pluie++;                 // Incrémente compteur total
    bascules1min++;                   // Incrémente compteur 1 minute
    nouvelleBascule = true;           // Signale nouvelle bascule au loop
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
void envoyerFirebase();
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
void handleRoot();

// ═══════════════════════════════════════════════════════════════════
//  PAGE WEB LOCALE
// ═══════════════════════════════════════════════════════════════════
void handleRoot() {
  String page = "<!DOCTYPE html><html>";                      // Début document HTML
  page += "<head><meta http-equiv='refresh' content='5'/>";   // Rafraîchissement auto 5s
  page += "<meta charset='UTF-8'>";                           // Encodage UTF-8
  page += "<title>Station Meteo ESP32</title></head>";        // Titre onglet navigateur
  page += "<body style='text-align:center;font-family:Arial'>"; // Centré police Arial
  page += "<h1>Station Meteo ESP32</h1>";                     // Titre principal

  page += "<h2>&#128197; " + String(dateStr) + " &nbsp; &#128336; " + String(heureStr) + "</h2>";
  // Affiche date et heure avec icônes

  page += "<h2>&#127777; Temperature : " + String(temperature, 1) + " &deg;C</h2>"; // Température
  page += "<h2>&#128167; Humidite    : " + String(humidite, 1)    + " %</h2>";      // Humidité
  page += "<h2>&#127786; Pression    : " + String(pression, 1)    + " hPa</h2>";    // Pression
  page += "<h2>&#127788; Vent        : " + String(vitesseVent, 1) + " km/h " + String(directionVent) + "</h2>"; // Vent
  page += "<h2>&#127783; Pluie       : " + String(pluviometrie, 1) + " mm/h</h2>"; // Pluie

  page += "</body></html>";                  // Fin document HTML
  server.send(200, "text/html", page);       // Envoie la page au client
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);             // Moniteur série 115200 bauds
  delay(500);
  Serial.println("\n=== STATION MÉTÉO — Démarrage ===");

  // ── I2C partagé (BME280 + DS3231 + AS5600) ────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);     // Démarre I2C sur GPIO 21/22
  Wire.setClock(400000);            // Mode Fast I2C 400kHz

  // ── BME280 ─────────────────────────────────────────────────────
  if (!bme.begin(0x76) && !bme.begin(0x77)) {  // Essai adresse 0x76 puis 0x77
    Serial.println("[ERREUR] BME280 non trouvé ! Vérifiez SDA=21 SCL=22");
  } else {
    Serial.println("[OK] BME280");
  }

  // ── RTC DS3231 ─────────────────────────────────────────────────
  if (!rtc.begin()) {
    Serial.println("[ERREUR] DS3231 non trouvé ! Vérifiez SDA=21 SCL=22");
  } else {
    Serial.println("[OK] RTC DS3231");
    // ⚠️ Décommenter une seule fois pour régler l'heure, puis re-commenter :
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // ── GPS NEO-6M (UART2) ─────────────────────────────────────────
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX); // UART2 9600 bauds
  Serial.println("[OK] GPS NEO-6M (RX=16, TX=17)");

  // ── FC-03 — Vitesse vent ───────────────────────────────────────
  pinMode(FC03_PIN, INPUT);         // Pin FC-03 en entrée numérique
  attachInterrupt(digitalPinToInterrupt(FC03_PIN), isrFC03, FALLING); // ISR sur front descendant
  Serial.println("[OK] FC-03 sur GPIO " + String(FC03_PIN));

  // ── MH-RD — Pluviomètre ───────────────────────────────────────
  pinMode(MH_RD_PIN, INPUT_PULLUP); // Pull-up interne activé
  attachInterrupt(digitalPinToInterrupt(MH_RD_PIN), isrPluie, CHANGE); // ISR sur tout changement
  t1min = millis();                 // Initialise le timer 1 minute
  Serial.println("[OK] MH-RD sur GPIO " + String(MH_RD_PIN));

  // ── HW-038 — Niveau d'eau ─────────────────────────────────────
  pinMode(HW038_PIN, INPUT);        // Pin ADC en entrée (VCC → 3V3 physique)
  analogReadResolution(12);         // Résolution ADC 12 bits (0–4095)
  Serial.println("[OK] HW-038 sur GPIO " + String(HW038_PIN) + " (VCC→3V3)");

  // ── LED indicateur pluie ──────────────────────────────────────
  pinMode(PIN_SIGNAL, OUTPUT);      // LED en sortie
  digitalWrite(PIN_SIGNAL, LOW);    // LED éteinte au démarrage

  // ── WiFi ──────────────────────────────────────────────────────
  Serial.print("Connexion WiFi");
  WiFi.begin(ssid, password);       // Lance la connexion WiFi
  while (WiFi.status() != WL_CONNECTED) { // Attend la connexion
    delay(500);
    Serial.print(".");              // Point d'attente à chaque tentative
  }
  Serial.println("\n[OK] WiFi connecte — IP: " + WiFi.localIP().toString());

  // ── Serveur web local ─────────────────────────────────────────
  server.on("/", handleRoot);       // Route "/" vers la page web
  server.begin();                   // Démarre le serveur web
  Serial.println("[OK] Serveur web sur port 80");

  // ── Écran TFT ILI9341 ─────────────────────────────────────────
  tft.begin();                      // Initialise le TFT
  tft.setRotation(1);               // Mode paysage
  ts.begin();                       // Initialise le tactile
  ts.setRotation(1);                // Mode paysage pour le tactile
  Serial.println("[OK] TFT ILI9341 + XPT2046");

  // ── Première lecture capteurs + affichage ─────────────────────
  lireBME280etRTC();                // Lit BME280 et DS3231 immédiatement
  derniereVitesse = millis();       // Initialise le timer vitesse vent

  afficherPage(pageActuelle);       // Affiche la page 1 au démarrage
  Serial.println("=== Station prête ! ===\n");
}

// ═══════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════
void loop() {
  unsigned long maintenant = millis(); // Horodatage du cycle courant

  // ── Serveur web local ─────────────────────────────────────────
  server.handleClient();            // Traite les requêtes HTTP entrantes

  // ────────────────────────────────────────────────────────────────
  //  1. GPS (non-bloquant — lecture permanente du buffer UART)
  // ────────────────────────────────────────────────────────────────
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read()); // Décode les trames NMEA octet par octet
  }

  if (maintenant - dernierAffichageGPS > 2000) { // Mise à jour GPS toutes les 2s
    dernierAffichageGPS = maintenant;
    bool gpsChange = false;

    satGPS = gps.satellites.isValid() ? (int)gps.satellites.value() : 0; // Nb satellites

    if (gps.location.isValid()) {                // Coordonnées valides ?
      float newLat = gps.location.lat();
      float newLon = gps.location.lng();
      if (newLat != latGPS || newLon != lonGPS) { // Changement de position ?
        latGPS = newLat;                          // Met à jour latitude
        lonGPS = newLon;                          // Met à jour longitude
        gpsChange = true;
      }
    }

    if (gps.altitude.isValid()) {               // Altitude valide ?
      char newAlt[10];
      dtostrf(gps.altitude.meters(), 5, 0, newAlt);
      if (strcmp(newAlt, altGPS) != 0) {        // Changement d'altitude ?
        strcpy(altGPS, newAlt);                 // Met à jour altitude
        gpsChange = true;
      }
    }

    if (gpsChange && pageActuelle == 3) {        // Rafraîchit page 3 si GPS changé
      afficherPage(3);
    }
  }

  // ────────────────────────────────────────────────────────────────
  //  2. BME280 + RTC (toutes les 5 secondes)
  // ────────────────────────────────────────────────────────────────
  if (maintenant - derniereLecture1 > 5000) {   // Toutes les 5 secondes
    derniereLecture1 = maintenant;
    lireBME280etRTC();                           // Lit température, humidité, pression, date/heure
  }

  // ────────────────────────────────────────────────────────────────
  //  3. Vitesse + Direction vent (toutes les 1 seconde)
  // ────────────────────────────────────────────────────────────────
  static uint32_t pulses_prec = 0;              // Valeur précédente du compteur

  if (maintenant - derniereVitesse >= 1000) {   // Toutes les 1 seconde
    uint32_t dt = maintenant - derniereVitesse;
    derniereVitesse = maintenant;

    noInterrupts();                              // Lecture atomique du compteur ISR
    uint32_t pulses_courants = g_pulses;
    interrupts();

    uint32_t delta = pulses_courants - pulses_prec; // Impulsions sur la dernière seconde
    pulses_prec = pulses_courants;

    float v_ms  = (delta * DIST_PAR_PULSE) / (dt / 1000.0f); // Vitesse en m/s
    float v_moy = moyenneGlissante(v_ms);        // Lisse sur 5 mesures
    vitesseVent = v_moy * 3.6f;                  // Convertit m/s → km/h

    majDirectionVent();                          // Lit l'angle AS5600 → direction cardinale
  }

  // ────────────────────────────────────────────────────────────────
  //  4. Pluviomètre (géré par ISR + vérifications loop)
  // ────────────────────────────────────────────────────────────────
  if (nouvelleBascule) {                         // Bascule signalée par l'ISR ?
    nouvelleBascule = false;

    noInterrupts();                              // Lecture atomique des compteurs ISR
    unsigned long b1  = bascules1min;
    unsigned long cnt = compteur_pluie;
    interrupts();

    mmTotal      = cnt * (MM_PAR_BASCULE / 2.0f);          // Total mm accumulés
    pluviometrie = (b1 / 2.0f) * MM_PAR_BASCULE * 60.0f;   // Débit mm/h estimé

    pluieEnCours   = true;                       // Marque la pluie comme active
    tDernierePluie = maintenant;                 // Mémorise l'heure de la dernière bascule
    digitalWrite(PIN_SIGNAL, HIGH);              // Allume la LED pluie
  }

  if (maintenant - t1min >= 60000UL) {           // Remet à zéro le compteur 1min
    noInterrupts();
    bascules1min = 0;
    interrupts();
    t1min = maintenant;
  }

  if (pluieEnCours && (maintenant - tDernierePluie > TIMEOUT_PLUIE)) { // Fin de pluie ?
    pluieEnCours = false;                        // Désactive l'indicateur pluie
    pluviometrie = 0.0;                          // Remet le débit à zéro
    digitalWrite(PIN_SIGNAL, LOW);               // Éteint la LED pluie
  }

  // ────────────────────────────────────────────────────────────────
  //  5. Envoi Firebase HTTPS (toutes les 10 secondes)
  // ────────────────────────────────────────────────────────────────
  if (maintenant - lastFirebase > 10000) {       // Toutes les 10 secondes
    lastFirebase = maintenant;
    envoyerFirebase();                           // Envoie toutes les données vers Firebase
  }

  // ────────────────────────────────────────────────────────────────
  //  6. Auto-refresh TFT (toutes les 5 secondes)
  // ────────────────────────────────────────────────────────────────
  if (maintenant - dernierRefresh > 5000) {      // Toutes les 5 secondes
    dernierRefresh = maintenant;
    afficherPage(pageActuelle);                  // Redessine la page active
  }

  // ────────────────────────────────────────────────────────────────
  //  7. Gestion tactile (navigation entre pages)
  // ────────────────────────────────────────────────────────────────
  if (ts.touched()) {                            // Écran touché ?
    TS_Point p = ts.getPoint();
    int x = map(p.x, X_MAX, X_MIN, 0, 320);     // Convertit coordonnée brute X
    int y = map(p.y, Y_MAX, Y_MIN, 0, 240);     // Convertit coordonnée brute Y

    if (y > 200) {                               // Zone boutons en bas de l'écran
      if (x < 110) {                             // Bouton PRÉCÉDENT
        pageActuelle = (pageActuelle == 1) ? 3 : pageActuelle - 1;
        afficherPage(pageActuelle);
        delay(300);                              // Anti-rebond tactile
      } else if (x > 210) {                      // Bouton SUIVANT
        pageActuelle = (pageActuelle == 3) ? 1 : pageActuelle + 1;
        afficherPage(pageActuelle);
        delay(300);                              // Anti-rebond tactile
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
//  ENVOI FIREBASE HTTPS
// ═══════════════════════════════════════════════════════════════════
void envoyerFirebase() {
  if (WiFi.status() != WL_CONNECTED) {           // Vérifie la connexion WiFi
    Serial.println("[Firebase] WiFi non connecté, envoi annulé");
    return;
  }

  WiFiClientSecure client;                       // Client WiFi sécurisé SSL/TLS
  client.setInsecure();                          // Désactive vérif. certificat (projet perso)

  HTTPClient https;                              // Client HTTPS
  https.begin(client, firebaseURL);              // Connexion HTTPS vers Firebase
  https.addHeader("Content-Type", "application/json"); // Type JSON

  // ── Construction du JSON avec les noms exacts Firebase ────────
  String jsonData = "{";
  jsonData += "\"temperature\":"   + String(temperature,  1) + ",";  // °C
  jsonData += "\"humidite\":"      + String(humidite,     1) + ",";  // %
  jsonData += "\"pression\":"      + String(pression,     1) + ",";  // hPa
  jsonData += "\"vitesseVent\":"   + String(vitesseVent,  1) + ",";  // km/h
  jsonData += "\"directionVent\":\"" + String(directionVent) + "\","; // Cardinal
  jsonData += "\"quantitePluie\":" + String(pluviometrie, 1) + ",";  // mm/h
  jsonData += "\"latitude\":"      + String(latGPS,       4) + ",";  // degrés
  jsonData += "\"longitude\":"     + String(lonGPS,       4) + ",";  // degrés
  jsonData += "\"altitude\":\""    + String(altGPS)          + "\""; // m
  jsonData += "}";

  int code = https.PUT(jsonData);                // Envoi PUT vers Firebase
  Serial.print("[Firebase] Code HTTP: ");
  Serial.println(code);                         // 200 = succès

  if (code > 0) {
    Serial.println("[Firebase] Envoi HTTPS réussi !");
  } else {
    Serial.print("[Firebase] Erreur: ");
    Serial.println(https.errorToString(code));   // Détail de l'erreur si échec
  }

  https.end();                                   // Ferme la connexion HTTPS
}

// ═══════════════════════════════════════════════════════════════════
//  FONCTIONS CAPTEURS
// ═══════════════════════════════════════════════════════════════════

void lireBME280etRTC() {
  temperature = bme.readTemperature();           // Lecture température BME280 en °C
  pression    = bme.readPressure() / 100.0F;    // Lecture pression BME280 Pa → hPa
  humidite    = bme.readHumidity();             // Lecture humidité BME280 en %

  DateTime now = rtc.now();                      // Récupère l'heure courante du DS3231
  snprintf(dateStr,  sizeof(dateStr),  "%02d/%02d/%02d",
           now.day(), now.month(), now.year() % 100); // Formate date JJ/MM/AA
  snprintf(heureStr, sizeof(heureStr), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());   // Formate heure HH:MM:SS
}

uint16_t lireAngleBrut() {
  Wire.beginTransmission(AS5600_ADDR);           // Démarre transmission I2C vers AS5600
  Wire.write(REG_ANGLE_MSB);                     // Sélectionne le registre angle MSB
  Wire.endTransmission(false);                   // Restart condition (pas de STOP)
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);     // Demande 2 octets (MSB + LSB)
  if (Wire.available() < 2) return 0;            // Retourne 0 si lecture impossible
  uint16_t msb = Wire.read();                    // Octet de poids fort
  uint16_t lsb = Wire.read();                    // Octet de poids faible
  return ((msb << 8) | lsb) & 0x0FFF;           // Angle 12 bits (0–4095)
}

void majDirectionVent() {
  static const char* CARDINAUX[16] = {
    "N","NNE","NE","ENE","E","ESE","SE","SSE",
    "S","SSO","SO","OSO","O","ONO","NO","NNO"  // 16 directions cardinales
  };
  uint16_t brut    = lireAngleBrut();             // Angle brut 0–4095
  float    dir_deg = (brut / 4096.0f) * 360.0f;  // Convertit en degrés 0–360°
  uint8_t  idx     = (uint8_t)((dir_deg + 11.25f) / 22.5f) % 16; // Indice directionnel
  strncpy(directionVent, CARDINAUX[idx], 7);      // Copie le nom de la direction
  directionVent[7] = '\0';                        // Termine la chaîne
}

float moyenneGlissante(float val) {
  buf_vitesse[idx_moy % MOYS_NB] = val;           // Stocke la nouvelle valeur dans le buffer
  idx_moy++;                                      // Avance l'index circulaire
  float somme = 0;
  for (uint8_t i = 0; i < MOYS_NB; i++) somme += buf_vitesse[i]; // Somme des 5 valeurs
  return somme / MOYS_NB;                         // Retourne la moyenne
}

// ═══════════════════════════════════════════════════════════════════
//  ICÔNES TFT (primitives GFX — sans bitmap)
// ═══════════════════════════════════════════════════════════════════

void iconThermometre(int cx, int cy, uint16_t c) {
  tft.fillRoundRect(cx-4, cy-16, 8, 24, 4, C_DKGREY);  // Corps du thermomètre
  tft.drawRoundRect(cx-4, cy-16, 8, 24, 4, c);          // Contour corps
  tft.fillRect(cx-2, cy, 4, 8, c);                       // Mercure (colonne)
  tft.fillCircle(cx, cy+13, 8, c);                       // Bulbe mercure
  tft.drawCircle(cx, cy+13, 9, c);                       // Contour bulbe
  tft.drawLine(cx+4, cy-8, cx+8, cy-8, c);               // Graduation haute
  tft.drawLine(cx+4, cy-2, cx+7, cy-2, c);               // Graduation basse
}

void iconGoutte(int cx, int cy, uint16_t c) {
  tft.fillTriangle(cx, cy-16, cx-9, cy-3, cx+9, cy-3, c); // Pointe de la goutte
  tft.fillCircle(cx, cy+3, 10, c);                         // Corps de la goutte
  tft.fillCircle(cx-3, cy-3, 3, C_LGREY);                  // Reflet de lumière
}

void iconJauge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);                  // Cadran de la jauge
  tft.drawLine(cx-13, cy,    cx-9, cy,   c);      // Graduation gauche
  tft.drawLine(cx,    cy-13, cx,   cy-9, c);      // Graduation haut
  tft.drawLine(cx+13, cy,    cx+9, cy,   c);      // Graduation droite
  tft.drawLine(cx, cy, cx-7, cy-10, C_RED);       // Aiguille (axe principal)
  tft.drawLine(cx, cy, cx-8, cy-9,  C_RED);       // Aiguille (épaisseur)
  tft.fillCircle(cx, cy, 2, C_WHITE);             // Centre de l'aiguille
}

void iconVent(int cx, int cy, uint16_t c) {
  tft.drawLine(cx-13, cy-7, cx+9,  cy-7, c);      // Ligne vent haute
  tft.drawLine(cx-13, cy,   cx+12, cy,   c);      // Ligne vent centrale
  tft.drawLine(cx-13, cy+7, cx+5,  cy+7, c);      // Ligne vent basse
  tft.drawCircle(cx+9,  cy-10, 3, c);             // Tourbillon haut
  tft.drawCircle(cx+12, cy+3,  3, c);             // Tourbillon milieu
  tft.drawCircle(cx+5,  cy+10, 3, c);             // Tourbillon bas
}

void iconBoussole(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);                           // Cadran boussole
  tft.fillTriangle(cx, cy-12, cx-5, cy,   cx+5, cy,   C_RED);     // Aiguille Nord (rouge)
  tft.fillTriangle(cx, cy+12, cx-5, cy+1, cx+5, cy+1, C_DKGREY);  // Aiguille Sud (gris)
  tft.fillRect(cx-1, cy-14, 3, 3, c);             // Repère Nord
}

void iconPluie(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx-7,  cy-8,  8, c);             // Nuage gauche
  tft.fillCircle(cx+4,  cy-10, 9, c);             // Nuage centre
  tft.fillCircle(cx+12, cy-6,  7, c);             // Nuage droite
  tft.fillRect(cx-14, cy-8, 30, 10, c);           // Base du nuage
  tft.fillRoundRect(cx-9, cy+5, 3, 8, 1, C_CYAN); // Goutte pluie gauche
  tft.fillRoundRect(cx-1, cy+8, 3, 8, 1, C_CYAN); // Goutte pluie centre
  tft.fillRoundRect(cx+7, cy+5, 3, 8, 1, C_CYAN); // Goutte pluie droite
}

void iconCalendrier(int cx, int cy, uint16_t c) {
  tft.drawRoundRect(cx-13, cy-10, 26, 23, 2, c);  // Cadre calendrier
  tft.fillRoundRect(cx-13, cy-10, 26,  8, 2, c);  // En-tête coloré
  tft.fillRect(cx-8, cy-14, 4, 6, c);              // Anneau gauche
  tft.fillRect(cx+4, cy-14, 4, 6, c);              // Anneau droit
  tft.drawLine(cx,    cy-2, cx,    cy+11, C_GREY); // Grille verticale
  tft.drawLine(cx-13, cy+4, cx+13, cy+4,  C_GREY); // Grille horizontale
}

void iconHorloge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 14, c);                   // Cadran extérieur
  tft.drawCircle(cx, cy, 13, c);                   // Cadran intérieur (épaisseur)
  tft.drawLine(cx, cy, cx,   cy-8, c);             // Aiguille des minutes (12h)
  tft.drawLine(cx, cy, cx+7, cy+4, c);             // Aiguille des heures
  tft.fillCircle(cx, cy, 2, c);                    // Centre de l'horloge
  tft.fillRect(cx-1, cy-14, 3, 3, c);              // Repère 12h
}

void iconGPS(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx, cy-7, 10, c);                 // Tête du marqueur GPS
  tft.fillTriangle(cx-7, cy-2, cx+7, cy-2, cx, cy+14, c); // Queue du marqueur
  tft.fillCircle(cx, cy-7, 5, C_CARD);             // Point intérieur (trou)
  tft.drawCircle(cx, cy-7, 13, c);                 // Cercle extérieur décoratif
}

void drawIcon(int type, int cx, int cy, uint16_t c) {
  switch (type) {
    case 0: iconThermometre(cx, cy, c); break;  // Thermomètre
    case 1: iconGoutte(cx, cy, c);      break;  // Goutte eau (humidité)
    case 2: iconJauge(cx, cy, c);       break;  // Jauge (pression)
    case 4: iconVent(cx, cy, c);        break;  // Vent
    case 5: iconBoussole(cx, cy, c);    break;  // Boussole (direction)
    case 6: iconPluie(cx, cy, c);       break;  // Pluie
    case 7: iconCalendrier(cx, cy, c);  break;  // Calendrier (date)
    case 8: iconHorloge(cx, cy, c);     break;  // Horloge (heure)
    case 9: iconGPS(cx, cy, c);         break;  // GPS (localisation)
  }
}

// ═══════════════════════════════════════════════════════════════════
//  COMPOSANTS D'INTERFACE TFT
// ═══════════════════════════════════════════════════════════════════

void dessinerTitre(const char* titre, uint16_t bg) {
  tft.fillRect(0, 0, 320, 33, bg);               // Fond de la barre de titre
  tft.fillRect(0, 0, 4, 33, C_CYAN);             // Barre accent cyan gauche
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 9);
  tft.print(titre);                               // Texte du titre
  tft.fillRoundRect(264, 5, 52, 22, 5, C_CYAN);  // Badge numéro de page
  tft.setTextColor(C_NAVY);
  tft.setTextSize(1);
  tft.setCursor(272, 12);
  tft.print("Page ");
  tft.print(pageActuelle);                        // Numéro de page courant
}

void dessinerBoutons() {
  tft.fillRect(0, 200, 320, 40, C_NAVY);          // Fond barre boutons
  tft.drawLine(0, 200, 320, 200, C_CYAN);         // Séparation barre boutons
  tft.fillRoundRect(8,   207, 90, 26, 5, C_CARD); // Fond bouton PREC
  tft.drawRoundRect(8,   207, 90, 26, 5, C_CYAN); // Contour bouton PREC
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(15, 213);
  tft.print("< PREC");                            // Libellé bouton précédent
  for (int i = 1; i <= 3; i++) {                  // Indicateurs de page (3 points)
    int dx = 140 + (i - 1) * 14;
    if (i == pageActuelle) tft.fillCircle(dx, 220, 5, C_CYAN); // Page active = plein
    else                   tft.drawCircle(dx, 220, 5, C_GREY); // Autres = vide
  }
  tft.fillRoundRect(222, 207, 90, 26, 5, C_CARD); // Fond bouton SUIV
  tft.drawRoundRect(222, 207, 90, 26, 5, C_CYAN); // Contour bouton SUIV
  tft.setTextColor(C_CYAN);
  tft.setCursor(230, 213);
  tft.print("SUIV >");                            // Libellé bouton suivant
}

// Carte standard : icône | label / valeur / unité
void dessinerCarte(int x, int y, int w, int h,
                   const char* label, const char* val, const char* unit,
                   uint16_t col, int iconType) {
  tft.fillRoundRect(x, y, w, h, 8, C_CARD);       // Fond de la carte
  tft.drawRoundRect(x, y, w, h, 8, col);           // Bordure colorée
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);      // Accent couleur en haut
  drawIcon(iconType, x+27, y + h/2, col);           // Icône à gauche
  tft.drawLine(x+52, y+10, x+52, y+h-10, C_DKGREY); // Séparateur vertical
  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(x+57, y+10);
  tft.print(label);                                 // Libellé de la mesure
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(x+57, y+27);
  tft.print(val);                                   // Valeur numérique
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(x+57, y+52);
  tft.print(unit);                                  // Unité de la mesure
}

// Carte pleine largeur : icône | label / ligne1 / ligne2
void dessinerCartePleine(int y, uint16_t col, int iconType,
                          const char* label,
                          const char* line1, const char* line2) {
  tft.fillRoundRect(5, y, 310, 78, 8, C_CARD);     // Fond carte pleine largeur
  tft.drawRoundRect(5, y, 310, 78, 8, col);         // Bordure colorée
  tft.fillRoundRect(13, y, 294, 3, 1, col);         // Accent couleur en haut
  drawIcon(iconType, 36, y + 39, col);               // Icône à gauche
  tft.drawLine(58, y+10, 58, y+68, C_DKGREY);       // Séparateur vertical
  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(66, y+10);
  tft.print(label);                                  // Libellé
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(66, y+26);
  tft.print(line1);                                  // Ligne de données principale
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(66, y+52);
  tft.print(line2);                                  // Ligne de données secondaire
}

// Carte GPS avec indicateur fix vert/rouge
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt) {
  int x = 5, y = y_start, w = 310, h = 95;
  uint16_t col    = ILI9341_CYAN;
  uint16_t colFix = gps.location.isValid() ? ILI9341_GREEN : ILI9341_RED;
  // Vert si fix GPS obtenu, rouge si recherche en cours

  tft.fillRoundRect(x, y, w, h, 8, C_CARD);        // Fond carte GPS
  tft.drawRoundRect(x, y, w, h, 8, col);            // Bordure cyan
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);       // Accent en haut
  drawIcon(9, x+30, y + h/2, colFix);               // Icône GPS (couleur fix)
  tft.drawLine(x+58, y+10, x+58, y+h-10, C_DKGREY); // Séparateur vertical

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(x+65, y+8);
  tft.print("Localisation GPS");                     // En-tête section GPS

  if (gps.location.isValid()) {                      // Fix GPS obtenu
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(x+65, y+24);
    tft.print("Lat: ");
    tft.print(lat, 4);
    tft.print(" N");                                 // Latitude avec 4 décimales
    tft.setCursor(x+65, y+50);
    tft.print("Lon: ");
    tft.print(lon, 4);
    tft.print(" E");                                 // Longitude avec 4 décimales
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(x+65, y+80);
    tft.print("Alt: ");
    tft.print(alt);
    tft.print(" m   Sat: ");
    tft.print(satGPS);                               // Altitude + nb satellites
  } else {                                           // Pas encore de fix GPS
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(x+65, y+30);
    tft.print("Recherche satellites...");
    tft.setCursor(x+65, y+48);
    tft.print("Sat detectes : ");
    tft.print(satGPS);                               // Satellites visibles
    tft.setCursor(x+65, y+66);
    tft.print("Placez l'antenne vers le ciel");      // Conseil de placement
  }
}

// ═══════════════════════════════════════════════════════════════════
//  PAGES TFT
// ═══════════════════════════════════════════════════════════════════

void afficherPage(int num) {
  tft.fillScreen(C_BG);            // Efface l'écran (fond noir)
  dessinerBoutons();               // Dessine la barre de navigation
  char buf[16];                    // Buffer temporaire pour dtostrf

  // ── PAGE 1 : CLIMAT (BME280) ──────────────────────────────────
  if (num == 1) {
    dessinerTitre("CLIMAT", C_TITLE_P1);                    // Titre page

    dtostrf(temperature, 4, 1, buf);                         // Formate température
    dessinerCarte(5,   36, 155, 78, "Temperature", buf, "deg C", C_ORANGE,      0);

    dtostrf(humidite, 4, 1, buf);                            // Formate humidité
    dessinerCarte(163, 36, 152, 78, "Humidite",    buf, "%",     ILI9341_CYAN,  1);

    dtostrf(pression, 6, 1, buf);                            // Formate pression
    dessinerCarte(5,  118, 310, 78, "Pression",    buf, "hPa",  ILI9341_GREEN, 2);
  }

  // ── PAGE 2 : VENT & PLUIE ─────────────────────────────────────
  else if (num == 2) {
    dessinerTitre("VENT & PLUIE", C_TITLE_P2);              // Titre page

    dtostrf(vitesseVent, 4, 1, buf);                         // Formate vitesse vent
    dessinerCarte(5,   36, 155, 78, "Vit. Vent", buf,           "km/h", C_ORANGE,      4);
    dessinerCarte(163, 36, 152, 78, "Direction", directionVent, "",     ILI9341_WHITE, 5);

    dtostrf(pluviometrie, 4, 1, buf);                        // Formate pluviométrie
    dessinerCartePleine(118, ILI9341_BLUE, 6,
                        "Pluviometrie",
                        buf,
                        "mm / heure");
  }

  // ── PAGE 3 : GPS & TEMPS ──────────────────────────────────────
  else if (num == 3) {
    dessinerTitre("GPS & TEMPS", C_TITLE_P3);               // Titre page

    dessinerCarte(5,   36, 153, 62, "Date",  dateStr,  "", ILI9341_WHITE,  7); // Date DS3231
    dessinerCarte(162, 36, 153, 62, "Heure", heureStr, "", ILI9341_YELLOW, 8); // Heure DS3231
    dessinerCarteGPS(102, latGPS, lonGPS, altGPS);           // Carte GPS complète
  }
}