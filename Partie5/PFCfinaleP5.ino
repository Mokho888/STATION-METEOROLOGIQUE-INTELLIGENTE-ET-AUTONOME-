// ╔══════════════════════════════════════════════════════════════════╗
// ║  STATION MÉTÉO COMPLÈTE — ESP32 DevKit V1                        ║
// ║  Capteurs  : BME280 · DS3231 · FC-03 · AS5600                    ║
// ║              MH-RD · HW-038 · GPS NEO-6M                         ║
// ║  Affichage : TFT ILI9341 tactile (3 pages)                       ║
// ║  Réseau    : WiFi · Serveur Web local · Firebase RTDB            ║
// ╚══════════════════════════════════════════════════════════════════╝

// ===== BIBLIOTHÈQUES =====
#include <SPI.h>                   // Communication SPI (TFT + tactile)
#include <Wire.h>                  // Communication I2C (BME280 + DS3231 + AS5600)
#include <WiFi.h>                  // Connexion WiFi de l'ESP32
#include <WiFiClientSecure.h>      // Client TCP sécurisé TLS/SSL pour les connexions HTTPS
#include <WebServer.h>             // Création d'un serveur web local sur port 80
#include <HTTPClient.h>            // Envoi de requêtes HTTPS vers Firebase
#include <Adafruit_GFX.h>          // Bibliothèque graphique de base (texte, formes)
#include <Adafruit_ILI9341.h>      // Pilote pour l'écran TFT ILI9341 2.8"
#include <XPT2046_Touchscreen.h>   // Pilote pour le contrôleur tactile XPT2046
#include <TinyGPS++.h>             // Décodage des trames NMEA du module GPS NEO-6M
#include <HardwareSerial.h>        // UART matériel pour la communication GPS
#include <Adafruit_Sensor.h>       // Couche d'abstraction commune Adafruit (requis par BME280)
#include <Adafruit_BME280.h>       // Pilote capteur BME280 (température, humidité, pression)
#include <RTClib.h>                // Pilote horloge temps réel DS3231

// ===== WIFI =====
const char* ssid     = "MEZX";            // Nom du réseau WiFi (SSID)
const char* password = "12312312345";     // Mot de passe du réseau WiFi

// ===== FIREBASE =====
const char* firebaseURL =
  "https://meteo-f6152-default-rtdb.europe-west1.firebasedatabase.app/weather.json";
// URL complète de la base de données Firebase Realtime Database
// La requête PUT enverra toutes les mesures à ce nœud "weather"

// ===== BROCHES TFT ILI9341 (bus SPI) =====
#define TFT_DC   2    // Data/Command : indique au TFT si on envoie une commande ou des données
#define TFT_CS   15   // Chip Select TFT : active l'écran sur le bus SPI
#define TFT_RST  13   // Reset TFT : réinitialise l'écran au démarrage
// SCK  = GPIO 18 (horloge SPI, câblage fixe ESP32)
// MOSI = GPIO 23 (données vers le TFT, câblage fixe ESP32)
// MISO = GPIO 19 (non utilisé par le TFT mais partagé avec le tactile)

// ===== BROCHES TACTILE XPT2046 (bus SPI partagé) =====
#define TOUCH_CS  25   // Chip Select tactile : active le contrôleur XPT2046 sur le SPI
#define TOUCH_IRQ 26   // Interrupt Request : passe à LOW quand l'écran est touché

// ===== BROCHES I2C (partagées : BME280 + DS3231 + AS5600) =====
#define SDA_PIN   21   // Ligne de données I2C
#define SCL_PIN   22   // Ligne d'horloge I2C

// ===== BROCHES GPS NEO-6M (UART2 matériel) =====
#define GPS_RX    16   // Réception ESP32 ← TX du module GPS
#define GPS_TX    17   // Émission  ESP32 → RX du module GPS (non utilisé ici)

// ===== BROCHES CAPTEURS EXTERNES =====
#define FC03_PIN   14   // FC-03 : sortie numérique de l'anémomètre (impulsions)
#define MH_RD_PIN  32   // MH-RD : sortie numérique du détecteur de pluie (bascule)
#define HW038_PIN  35   // HW-038 : sortie analogique du capteur de niveau d'eau
                        // ⚠️ GPIO 35 = entrée ADC uniquement (pas de sortie possible)
                        // ⚠️ VCC du HW-038 → broche 3V3 physique (pas sur un GPIO)
#define PIN_SIGNAL 33   // LED indicateur : s'allume quand de la pluie est détectée

// ===== OBJETS PRINCIPAUX =====
Adafruit_ILI9341    tft(TFT_CS, TFT_DC, TFT_RST);  // Objet écran TFT avec ses 3 broches de contrôle
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);         // Objet tactile avec CS et IRQ
TinyGPSPlus         gps;                             // Objet décodeur GPS (parse les trames NMEA)
HardwareSerial      GPS_Serial(2);                   // UART2 matériel dédié au GPS
Adafruit_BME280     bme;                             // Objet capteur BME280 via I2C
RTC_DS3231          rtc;                             // Objet horloge DS3231 via I2C
WebServer           server(80);                      // Serveur web sur le port HTTP standard 80

// ===== CALIBRATION TACTILE =====
// Ces valeurs brutes (0-4095) correspondent aux bords physiques de l'écran
// Elles sont utilisées dans map() pour convertir en pixels (0-320 et 0-240)
#define X_MIN 370
#define X_MAX 3800
#define Y_MIN 260
#define Y_MAX 3600

// ===== VARIABLES CAPTEURS — PARTIE 1 (BME280 + DS3231) =====
float temperature = 0.0;          // Temperature lue par le BME280 en degres Celsius
float humidite    = 0.0;          // Humidite relative lue par le BME280 en %
float pression    = 0.0;          // Pression atmospherique en hPa (hectopascals)
char  dateStr[12] = "--/--/----"; // Date formatee "JJ/MM/AAAA" lue depuis le DS3231
char  heureStr[9] = "--:--:--";   // Heure formatee "HH:MM:SS" lue depuis le DS3231

// ===== VARIABLES CAPTEURS — PARTIE 2 (FC-03 + AS5600) =====
float vitesseVent      = 0.0;   // Vitesse du vent calculee en km/h
char  directionVent[8] = "---"; // Direction du vent (ex: "NNE", "SO") via AS5600

// ===== VARIABLES CAPTEURS — PARTIE 3 (MH-RD + HW-038) =====
float pluviometrie = 0.0;   // Quantite de pluie estimee en mm/heure

// ===== VARIABLES CAPTEURS — PARTIE 4 (GPS NEO-6M) =====
float latGPS     = 0.0;      // Latitude GPS en degres decimaux
float lonGPS     = 0.0;      // Longitude GPS en degres decimaux
char  altGPS[10] = "---";    // Altitude GPS en metres (format texte)
int   satGPS     = 0;        // Nombre de satellites GPS visibles

// ===== PALETTE DE COULEURS TFT (format RGB565) =====
#define C_BG      0x0000               // Fond noir
#define C_CARD    0x1082               // Gris tres fonce pour les cartes
#define C_DKGREY  0x2104               // Gris fonce pour les separateurs
#define C_GREY    0x8410               // Gris moyen
#define C_WHITE   ILI9341_WHITE        // Blanc pur
#define C_CYAN    ILI9341_CYAN         // Cyan (couleur principale de l'interface)
#define C_ORANGE  ILI9341_ORANGE       // Orange (temperature)
#define C_YELLOW  ILI9341_YELLOW       // Jaune (heure)
#define C_GREEN   ILI9341_GREEN        // Vert (pression, GPS fixe)
#define C_BLUE    ILI9341_BLUE         // Bleu (pluie)
#define C_RED     ILI9341_RED          // Rouge (alertes, aiguille jauge)
#define C_NAVY    ILI9341_NAVY         // Bleu marine (barre de navigation)
#define C_LGREY   ILI9341_LIGHTGREY    // Gris clair (reflet icone goutte)

#define C_TITLE_P1  0x0010   // Couleur de fond du titre de la page 1
#define C_TITLE_P2  0x3000   // Couleur de fond du titre de la page 2
#define C_TITLE_P3  0x0010   // Couleur de fond du titre de la page 3

// ===== CONSTANTES ANEMOMETRE FC-03 + AS5600 =====
#define NB_FENTES       30                               // Nombre de fentes du disque de l'anemometre
#define RAYON_M         0.04f                            // Rayon de rotation des pales en metres
#define DIST_PAR_PULSE  (TWO_PI * RAYON_M / NB_FENTES)  // Distance parcourue par impulsion ~0.00838m
#define MOYS_NB         5                                // Taille du buffer de moyenne glissante

#define AS5600_ADDR   0x36   // Adresse I2C du capteur magnetique AS5600 (direction vent)
#define REG_ANGLE_MSB 0x0E   // Registre du byte de poids fort de l'angle brut (12 bits)

volatile uint32_t g_pulses = 0;          // Compteur d'impulsions FC-03 (incremente par l'ISR)
float   buf_vitesse[MOYS_NB] = {0};      // Buffer circulaire pour la moyenne glissante
uint8_t idx_moy = 0;                     // Index courant dans le buffer de moyenne

// ===== CONSTANTES PLUVIOMETRE MH-RD =====
#define MM_PAR_BASCULE  0.2794f   // Equivalent pluviometrique d'un basculement complet en mm
#define DEBOUNCE_MS     200        // Delai anti-rebond en millisecondes
#define TIMEOUT_PLUIE   60000UL    // Duree sans bascule avant de considerer la pluie terminee (60s)

volatile unsigned long compteur_pluie     = 0;      // Nombre total de basculements depuis le demarrage
volatile unsigned long dernierTemps_pluie = 0;      // Timestamp du dernier basculement (anti-rebond)
volatile bool          nouvelleBascule    = false;  // Drapeau mis a true par l'ISR, lu dans loop()
volatile unsigned long bascules1min       = 0;      // Basculements dans la derniere minute

float         mmTotal        = 0.0;   // Total cumule de pluie en mm depuis le demarrage
bool          pluieEnCours   = false; // Vrai si de la pluie a ete detectee recemment
unsigned long t1min          = 0;     // Timestamp du debut de la fenetre de 1 minute
unsigned long tDernierePluie = 0;     // Timestamp de la derniere bascule (fin de pluie)

// ===== VARIABLES DE TIMING ET NAVIGATION =====
int           pageActuelle        = 1;   // Page TFT affichee (1=Climat, 2=Vent/Pluie, 3=GPS/Temps)
unsigned long dernierAffichageGPS = 0;   // Dernier rafraichissement GPS (toutes les 2s)
unsigned long derniereLecture1    = 0;   // Derniere lecture BME280 + DS3231 (toutes les 5s)
unsigned long derniereVitesse     = 0;   // Derniere mesure vitesse vent FC-03 (toutes les 1s)
unsigned long dernierRefresh      = 0;   // Dernier rafraichissement ecran TFT (toutes les 5s)
unsigned long dernierFirebase     = 0;   // Dernier envoi vers Firebase (toutes les 5s)

bool wifiConnecte = false;   // Indique si la connexion WiFi a reussi au demarrage

// ===== ISR — FC-03 (Anemometre) =====
// Declenchee a chaque front descendant = passage d'une fente devant le capteur
// IRAM_ATTR force le code a rester en RAM rapide pour reponse immediate
void IRAM_ATTR isrFC03() {
  g_pulses++;   // Incremente le compteur global d'impulsions
}

// ===== ISR — MH-RD (Pluviometre a bascule) =====
// Declenchee sur CHANGE (montee ET descente) = 1 appel par demi-basculement
// Une bascule complete = 2 appels ISR → on divise par 2 dans les calculs
void IRAM_ATTR isrPluie() {
  unsigned long maintenant = millis();
  if (maintenant - dernierTemps_pluie > DEBOUNCE_MS) {   // Filtre anti-rebond
    dernierTemps_pluie = maintenant;  // Memorise le temps du dernier evenement valide
    compteur_pluie++;                 // Incremente le compteur total de demi-basculements
    bascules1min++;                   // Incremente le compteur de la fenetre 1 minute
    nouvelleBascule = true;           // Signale a loop() qu'une nouvelle bascule a eu lieu
  }
}

// ===== PROTOTYPES =====
void lireBME280etRTC();
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
                          const char* label, const char* line1, const char* line2);
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt);
void drawIcon(int type, int cx, int cy, uint16_t c);
void envoyerFirebase();
void handleRoot();
void connecterWiFi();

// ===== PAGE WEB =====
// Fonction appelee automatiquement par le serveur web quand un navigateur accede a "/"
// Genere et envoie une page HTML complete avec toutes les mesures en temps reel
void handleRoot() {

  String page = "<!DOCTYPE html><html>";

  page += "<head><meta http-equiv='refresh' content='5'/>";
  // Rafraichit automatiquement la page toutes les 5 secondes dans le navigateur

  page += "<meta charset='UTF-8'>";
  // Encodage UTF-8 pour afficher les caracteres speciaux correctement

  page += "<meta name='viewport' content='width=device-width,initial-scale=1'/>";
  // Rend la page lisible sur smartphone et tablette (responsive)

  page += "<title>Station Meteo ESP32</title>";

  // Styles CSS integres pour l'apparence sombre et moderne de la page
  page += "<style>"
          "body{font-family:Arial,sans-serif;background:#0a0a1a;color:#eee;margin:0;padding:16px;}"
          "h1{color:#00e5ff;text-align:center;margin-bottom:4px;}"
          ".sub{text-align:center;color:#888;margin-bottom:20px;font-size:13px;}"
          ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));"
          "gap:12px;max-width:800px;margin:0 auto;}"
          ".card{background:#1a1a2e;border:1px solid #00e5ff44;border-radius:10px;"
          "padding:14px 18px;}"
          ".card .label{font-size:11px;color:#00e5ff;text-transform:uppercase;"
          "letter-spacing:1px;margin-bottom:6px;}"
          ".card .val{font-size:28px;font-weight:bold;color:#fff;}"
          ".card .unit{font-size:12px;color:#aaa;margin-top:2px;}"
          ".full{grid-column:1/-1;}"   // Carte pleine largeur pour le GPS
          "</style></head><body>";

  page += "<h1>&#127782; Station Meteo ESP32</h1>";
  // &#127782; = emoji nuage avec soleil

  page += "<p class='sub'>&#128197; " + String(dateStr) +
          " &nbsp; &#128336; " + String(heureStr) + "</p>";
  // Affiche la date et l'heure issues du DS3231

  page += "<div class='grid'>";   // Debut de la grille de cartes

  // Carte Temperature
  page += "<div class='card'><div class='label'>&#127777; Temperature</div>"
          "<div class='val'>" + String(temperature, 1) + "</div>"
          "<div class='unit'>deg C</div></div>";

  // Carte Humidite
  page += "<div class='card'><div class='label'>&#128167; Humidite</div>"
          "<div class='val'>" + String(humidite, 1) + "</div>"
          "<div class='unit'>%</div></div>";

  // Carte Pression
  page += "<div class='card'><div class='label'>&#127786; Pression</div>"
          "<div class='val'>" + String(pression, 1) + "</div>"
          "<div class='unit'>hPa</div></div>";

  // Carte Vitesse du vent
  page += "<div class='card'><div class='label'>&#127788; Vitesse Vent</div>"
          "<div class='val'>" + String(vitesseVent, 1) + "</div>"
          "<div class='unit'>km/h</div></div>";

  // Carte Direction du vent
  page += "<div class='card'><div class='label'>&#129517; Direction</div>"
          "<div class='val'>" + String(directionVent) + "</div>"
          "<div class='unit'>&nbsp;</div></div>";

  // Carte Pluviometrie
  page += "<div class='card'><div class='label'>&#127783; Pluviometrie</div>"
          "<div class='val'>" + String(pluviometrie, 1) + "</div>"
          "<div class='unit'>mm/h</div></div>";

  // Carte GPS (pleine largeur)
  if (gps.location.isValid()) {
    // Le GPS a un fix : afficher les coordonnees reelles
    page += "<div class='card full'><div class='label'>&#128205; GPS</div>"
            "<div class='val' style='font-size:16px'>"
            "Lat: " + String(latGPS, 4) + " N &nbsp; Lon: " + String(lonGPS, 4) + " E"
            "</div>"
            "<div class='unit'>Alt: " + String(altGPS) +
            " m &nbsp; Sat: " + String(satGPS) + "</div></div>";
  } else {
    // Pas encore de fix GPS : afficher un message d'attente
    page += "<div class='card full'><div class='label'>&#128205; GPS</div>"
            "<div class='val' style='font-size:16px;color:#ffcc00'>"
            "Recherche satellites...</div>"
            "<div class='unit'>Sat detectes : " + String(satGPS) + "</div></div>";
  }

  page += "</div>";         // Fin de la grille
  page += "</body></html>"; // Fin de la page HTML

  server.send(200, "text/html", page);   // Envoie la page avec le code HTTP 200 (OK)
}

// ===== ENVOI FIREBASE =====
// Envoie toutes les mesures vers la base Firebase en JSON via une requete HTTPS PUT
// Utilise WiFiClientSecure pour chiffrer la communication avec TLS/SSL
// Un PUT remplace entierement le noeud "weather" avec les nouvelles valeurs
void envoyerFirebase() {

  if (!wifiConnecte || WiFi.status() != WL_CONNECTED) return;
  // Ne rien faire si le WiFi n'est pas disponible (evite un blocage)

  WiFiClientSecure client;             // Creation du client TCP securise TLS
  client.setInsecure();
  // setInsecure() : accepte le certificat SSL de Firebase sans le verifier
  // Suffisant pour un projet embarque ; pour un usage critique, utiliser setCACert()
  // avec l'empreinte du certificat racine de Google/Firebase

  HTTPClient https;                              // Creation de l'objet client HTTPS
  https.begin(client, firebaseURL);              // Associe le client securise a l'URL Firebase
  https.addHeader("Content-Type", "application/json");   // Firebase attend du JSON

  // Construction du JSON avec toutes les mesures de la station
  String jsonData = "{";
  jsonData += "\"temperature\":"  + String(temperature, 1)  + ",";   // Temperature deg C
  jsonData += "\"humidite\":"     + String(humidite, 1)      + ",";   // Humidite %
  jsonData += "\"pression\":"     + String(pression, 1)      + ",";   // Pression hPa
  jsonData += "\"vitesse_vent\":" + String(vitesseVent, 1)   + ",";   // Vitesse vent km/h
  jsonData += "\"direction\":\"" + String(directionVent)     + "\","; // Direction vent texte
  jsonData += "\"pluie_mmh\":"   + String(pluviometrie, 1)   + ",";   // Pluie mm/h
  jsonData += "\"lat\":"          + String(latGPS, 4)         + ",";   // Latitude GPS
  jsonData += "\"lon\":"          + String(lonGPS, 4)         + ",";   // Longitude GPS
  jsonData += "\"altitude\":\"" + String(altGPS)             + "\","; // Altitude texte
  jsonData += "\"satellites\":"  + String(satGPS)             + ",";   // Nb satellites
  jsonData += "\"date\":\""      + String(dateStr)            + "\","; // Date JJ/MM/AAAA
  jsonData += "\"heure\":\""     + String(heureStr)           + "\"";  // Heure HH:MM:SS
  jsonData += "}";

  int httpCode = https.PUT(jsonData);  // Envoie les donnees via HTTPS et recupere le code reponse

  Serial.print("[Firebase HTTPS] Code : ");
  Serial.println(httpCode);            // 200 = succes, -1 = erreur reseau ou TLS

  https.end();   // Libere les ressources de la connexion HTTPS
  client.stop(); // Ferme proprement la connexion TLS securisee
}

// ===== CONNEXION WIFI =====
// Tente de se connecter au reseau WiFi avec un timeout de 15 secondes
// Affiche l'avancement directement sur l'ecran TFT pendant la tentative
void connecterWiFi() {

  Serial.print("[WiFi] Connexion a : ");
  Serial.println(ssid);

  // Affichage du message d'attente sur le TFT
  tft.fillScreen(C_BG);
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 80);
  tft.print("Connexion WiFi...");   // Message principal visible par l'utilisateur
  tft.setTextSize(1);
  tft.setTextColor(C_GREY);
  tft.setCursor(20, 110);
  tft.print(ssid);                  // Affiche le nom du reseau vise

  WiFi.mode(WIFI_STA);              // Mode Station : l'ESP32 se connecte a un routeur existant
  WiFi.begin(ssid, password);       // Lance la procedure de connexion WiFi

  unsigned long debut = millis();   // Memorise l'heure de debut pour calculer le timeout
  int dots = 0;                     // Compteur pour afficher les points de progression

  // Attente de la connexion avec timeout de 15 secondes
  while (WiFi.status() != WL_CONNECTED && millis() - debut < 15000) {
    delay(500);                     // Pause 500ms entre chaque verification d'etat
    tft.setTextColor(C_CYAN);
    tft.setCursor(20 + dots * 10, 130);
    tft.print(".");                 // Affiche un point de progression sur le TFT
    dots++;
    Serial.print(".");              // Meme progression sur le moniteur serie
  }

  if (WiFi.status() == WL_CONNECTED) {
    // ===== CONNEXION REUSSIE =====
    wifiConnecte = true;
    Serial.println();
    Serial.print("[WiFi] Connecte ! IP locale : ");
    Serial.println(WiFi.localIP());   // Affiche l'adresse IP attribuee par le routeur

    // Affichage de l'IP sur le TFT pour que l'utilisateur puisse y acceder
    tft.fillScreen(C_BG);
    tft.setTextColor(C_GREEN);
    tft.setTextSize(2);
    tft.setCursor(20, 60);
    tft.print("WiFi OK !");
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 95);
    tft.print("Adresse IP locale :");
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(20, 112);
    tft.print(WiFi.localIP().toString());   // Exemple : 192.168.1.45
    tft.setTextSize(1);
    tft.setTextColor(C_GREY);
    tft.setCursor(20, 145);
    tft.print("Ouvrez cette adresse dans");
    tft.setCursor(20, 157);
    tft.print("votre navigateur (port 80)");

    server.on("/", handleRoot);   // Associe l'URL "/" a la fonction handleRoot()
    server.begin();               // Lance le serveur sur le port 80
    Serial.println("[Web] Serveur demarre sur port 80");

    delay(3000);   // Laisse le temps de lire l'IP affichee sur le TFT

  } else {
    // ===== CONNEXION ECHOUEE =====
    wifiConnecte = false;
    Serial.println();
    Serial.println("[WiFi] Echec — station en mode hors-ligne");

    // Affichage du message d'erreur sur le TFT
    tft.fillScreen(C_BG);
    tft.setTextColor(C_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.print("WiFi echoue !");
    tft.setTextSize(1);
    tft.setTextColor(C_GREY);
    tft.setCursor(20, 115);
    tft.print("Mode hors-ligne actif.");
    tft.setCursor(20, 127);
    tft.print("Verifiez SSID et mot de passe.");

    delay(2500);   // Affiche l'erreur quelques secondes avant de continuer
  }
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);   // Initialise le port serie pour les messages de debug
  delay(500);             // Courte pause pour laisser le port serie se stabiliser
  Serial.println("\n=== STATION METEO — Demarrage ===");

  // ===== TFT (initialise en premier pour afficher les messages de boot) =====
  tft.begin();           // Initialise le controleur ILI9341 via SPI
  tft.setRotation(1);    // Rotation 1 = mode paysage (320x240 pixels)
  ts.begin();            // Initialise le controleur tactile XPT2046
  ts.setRotation(1);     // Meme rotation que l'ecran pour coherence des coordonnees
  tft.fillScreen(C_BG);  // Efface l'ecran avec la couleur de fond (noir)
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.print("Station Meteo");   // Titre de demarrage
  tft.setTextColor(C_GREY);
  tft.setTextSize(1);
  tft.setCursor(20, 75);
  tft.print("Initialisation des capteurs...");
  Serial.println("[OK] TFT ILI9341 + XPT2046");

  // ===== I2C =====
  Wire.begin(SDA_PIN, SCL_PIN);   // Demarre le bus I2C sur les broches definies
  Wire.setClock(400000);           // Frequence 400kHz (Fast I2C, compatible avec les 3 capteurs)

  // ===== BME280 =====
  // Le BME280 peut avoir l'adresse 0x76 (SDO a GND) ou 0x77 (SDO a VCC)
  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("[ERREUR] BME280 non trouve !");
    tft.setTextColor(C_RED);
    tft.setCursor(20, 95);
    tft.print("[ERR] BME280 absent");
  } else {
    Serial.println("[OK] BME280");
    tft.setTextColor(C_GREEN);
    tft.setCursor(20, 95);
    tft.print("[OK] BME280");
  }

  // ===== RTC DS3231 =====
  if (!rtc.begin()) {
    Serial.println("[ERREUR] DS3231 non trouve !");
    tft.setTextColor(C_RED);
    tft.setCursor(20, 107);
    tft.print("[ERR] DS3231 absent");
  } else {
    if (rtc.lostPower()) {
      // Si la pile est vide, l'heure est perdue : synchronisation avec la date de compilation
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("[RTC] Heure resynchronisee (pile vide)");
    }
    Serial.println("[OK] DS3231");
    tft.setTextColor(C_GREEN);
    tft.setCursor(20, 107);
    tft.print("[OK] DS3231");
  }

  // ===== GPS NEO-6M =====
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  // 9600 bauds = vitesse par defaut du NEO-6M
  // SERIAL_8N1 = 8 bits de donnees, pas de parite, 1 bit de stop
  Serial.println("[OK] GPS NEO-6M (RX=16, TX=17)");
  tft.setTextColor(C_GREEN);
  tft.setCursor(20, 119);
  tft.print("[OK] GPS NEO-6M");

  // ===== FC-03 (anemometre) =====
  pinMode(FC03_PIN, INPUT);   // Entree numerique pour recevoir les impulsions
  attachInterrupt(digitalPinToInterrupt(FC03_PIN), isrFC03, FALLING);
  // FALLING = declenche l'ISR sur chaque front descendant (passage d'une fente)
  Serial.println("[OK] FC-03 GPIO " + String(FC03_PIN));
  tft.setCursor(20, 131);
  tft.print("[OK] FC-03 (anemometre)");

  // ===== MH-RD (pluviometre a bascule) =====
  pinMode(MH_RD_PIN, INPUT_PULLUP);   // Resistance de tirage interne activee
  attachInterrupt(digitalPinToInterrupt(MH_RD_PIN), isrPluie, CHANGE);
  // CHANGE = declenche l'ISR a chaque changement d'etat (montee ET descente)
  t1min = millis();   // Initialise le timer de la fenetre de 1 minute
  Serial.println("[OK] MH-RD GPIO " + String(MH_RD_PIN));
  tft.setCursor(20, 143);
  tft.print("[OK] MH-RD (pluviometre)");

  // ===== HW-038 (niveau d'eau) =====
  pinMode(HW038_PIN, INPUT);   // Entree ADC (lecture analogique)
  analogReadResolution(12);     // Resolution 12 bits : valeurs de 0 a 4095
  Serial.println("[OK] HW-038 GPIO " + String(HW038_PIN));
  tft.setCursor(20, 155);
  tft.print("[OK] HW-038 (niveau eau)");

  // ===== LED indicateur pluie =====
  pinMode(PIN_SIGNAL, OUTPUT);       // Sortie numerique
  digitalWrite(PIN_SIGNAL, LOW);     // Eteinte au demarrage

  delay(1500);   // Laisse le temps de lire les resultats d'initialisation sur le TFT

  // ===== CONNEXION WIFI =====
  connecterWiFi();   // Tente la connexion et affiche le resultat sur le TFT

  // ===== PREMIERE LECTURE DES CAPTEURS =====
  lireBME280etRTC();          // Lit immediatement temperature, humidite, pression, date, heure
  derniereVitesse = millis(); // Initialise le timer de mesure de la vitesse du vent

  afficherPage(pageActuelle);   // Affiche la premiere page (Climat) sur le TFT
  Serial.println("=== Station prete ! ===\n");
}

// ===== LOOP =====
void loop() {

  unsigned long maintenant = millis();   // Heure courante depuis le demarrage (en ms)

  // ===== Traitement des requetes web =====
  if (wifiConnecte) server.handleClient();
  // Verifie en permanence si un navigateur envoie une requete HTTP
  // Non bloquant : retourne immediatement s'il n'y a pas de requete en attente

  // ===== Reconnexion WiFi automatique =====
  if (wifiConnecte && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connexion perdue — tentative de reconnexion...");
    WiFi.reconnect();   // Relance la procedure de connexion sans redemarrer l'ESP32
  }

  // ===== 1. Lecture GPS (non bloquante, en continu) =====
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
    // Lit un octet a la fois sur l'UART et le passe au decodeur TinyGPS++
    // TinyGPS++ reconstruit les trames NMEA et met a jour ses variables internes
  }

  if (maintenant - dernierAffichageGPS > 2000) {
    // Toutes les 2 secondes : extraire les nouvelles valeurs du decodeur GPS
    dernierAffichageGPS = maintenant;
    bool gpsChange = false;   // Indique si une donnee GPS a change

    satGPS = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
    // Lit le nombre de satellites si la valeur est disponible, sinon 0

    if (gps.location.isValid()) {
      float newLat = gps.location.lat();   // Latitude en degres decimaux
      float newLon = gps.location.lng();   // Longitude en degres decimaux
      if (newLat != latGPS || newLon != lonGPS) {
        latGPS = newLat;
        lonGPS = newLon;
        gpsChange = true;   // La position a change
      }
    }

    if (gps.altitude.isValid()) {
      char newAlt[10];
      dtostrf(gps.altitude.meters(), 5, 0, newAlt);   // Convertit l'altitude float en texte
      if (strcmp(newAlt, altGPS) != 0) {
        strcpy(altGPS, newAlt);
        gpsChange = true;   // L'altitude a change
      }
    }

    if (gpsChange && pageActuelle == 3) afficherPage(3);
    // Rafraichit immediatement la page GPS si une donnee a change et qu'elle est visible
  }

  // ===== 2. Lecture BME280 + DS3231 (toutes les 5 secondes) =====
  if (maintenant - derniereLecture1 > 5000) {
    derniereLecture1 = maintenant;
    lireBME280etRTC();   // Met a jour les variables globales de temperature, humidite, etc.
  }

  // ===== 3. Calcul vitesse + direction vent (toutes les 1 seconde) =====
  static uint32_t pulses_prec = 0;   // Valeur du compteur a la mesure precedente

  if (maintenant - derniereVitesse >= 1000) {
    uint32_t dt = maintenant - derniereVitesse;   // Duree reelle ecoulee (peut etre > 1000ms)
    derniereVitesse = maintenant;

    // Lecture atomique du compteur ISR (interruptions desactivees le temps de la lecture)
    noInterrupts();
    uint32_t pulses_courants = g_pulses;
    interrupts();

    uint32_t delta = pulses_courants - pulses_prec;   // Impulsions pendant cette periode
    pulses_prec = pulses_courants;                     // Sauvegarde pour la prochaine fois

    // Calcul : vitesse (m/s) = distance_totale / temps_en_secondes
    float v_ms = (delta * DIST_PAR_PULSE) / (dt / 1000.0f);

    // Lissage par moyenne glissante sur les 5 dernieres secondes puis conversion km/h
    vitesseVent = moyenneGlissante(v_ms) * 3.6f;

    majDirectionVent();   // Lit l'angle AS5600 et determine le point cardinal
  }

  // ===== 4. Traitement pluviometre (declenche par l'ISR) =====
  if (nouvelleBascule) {
    nouvelleBascule = false;   // Reinitialise le drapeau

    // Lecture atomique des compteurs ISR
    noInterrupts();
    unsigned long b1  = bascules1min;    // Demi-basculements dans la derniere minute
    unsigned long cnt = compteur_pluie;  // Total des demi-basculements depuis demarrage
    interrupts();

    // Calcul du total cumule en mm (chaque demi-bascule = MM_PAR_BASCULE / 2)
    mmTotal = cnt * (MM_PAR_BASCULE / 2.0f);

    // Calcul de l'intensite actuelle en mm/h :
    // b1/2 = nombre de basculements complets dans la fenetre d'1 minute
    // * MM_PAR_BASCULE = mm pour cette minute
    // * 60 = extrapolation a 1 heure
    pluviometrie = (b1 / 2.0f) * MM_PAR_BASCULE * 60.0f;

    pluieEnCours   = true;             // Signale que la pluie est active
    tDernierePluie = maintenant;       // Memorise le moment de la derniere bascule
    digitalWrite(PIN_SIGNAL, HIGH);   // Allume la LED indicateur de pluie
  }

  // Reinitialise le compteur de la fenetre glissante toutes les 60 secondes
  if (maintenant - t1min >= 60000UL) {
    noInterrupts();
    bascules1min = 0;   // Repart a zero pour la prochaine fenetre d'1 minute
    interrupts();
    t1min = maintenant;
  }

  // Detecte la fin de pluie si aucune bascule pendant TIMEOUT_PLUIE (60s)
  if (pluieEnCours && (maintenant - tDernierePluie > TIMEOUT_PLUIE)) {
    pluieEnCours = false;           // Plus de pluie detectee
    pluviometrie = 0.0;             // Remet l'intensite a zero
    digitalWrite(PIN_SIGNAL, LOW);  // Eteint la LED indicateur
  }

  // ===== 5. Envoi Firebase (toutes les 5 secondes) =====
  if (maintenant - dernierFirebase > 5000) {
    dernierFirebase = maintenant;
    envoyerFirebase();   // Envoie toutes les mesures vers Firebase si WiFi connecte
  }

  // ===== 6. Rafraichissement ecran TFT (toutes les 5 secondes) =====
  if (maintenant - dernierRefresh > 5000) {
    dernierRefresh = maintenant;
    afficherPage(pageActuelle);   // Redessine la page courante avec les nouvelles valeurs
  }

  // ===== 7. Navigation tactile =====
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Conversion des coordonnees brutes ADC (0-4095) en pixels (0-320 et 0-240)
    int x = map(p.x, X_MAX, X_MIN, 0, 320);
    int y = map(p.y, Y_MAX, Y_MIN, 0, 240);

    if (y > 200) {
      // Zone de navigation en bas de l'ecran (boutons PREC et SUIV)
      if (x < 110) {
        // Bouton PRECEDENT : revient a la page 3 depuis la page 1 (circulaire)
        pageActuelle = (pageActuelle == 1) ? 3 : pageActuelle - 1;
        afficherPage(pageActuelle);
        delay(300);   // Anti-rebond tactile
      } else if (x > 210) {
        // Bouton SUIVANT : revient a la page 1 depuis la page 3 (circulaire)
        pageActuelle = (pageActuelle == 3) ? 1 : pageActuelle + 1;
        afficherPage(pageActuelle);
        delay(300);   // Anti-rebond tactile
      }
    }
  }
}

// ===== LECTURE BME280 + DS3231 =====
// Lit les trois grandeurs du BME280 et la date/heure du DS3231
// Met a jour les variables globales correspondantes
void lireBME280etRTC() {

  temperature = bme.readTemperature();         // Temperature en deg C
  pression    = bme.readPressure() / 100.0F;  // Pression : Pa → hPa (diviser par 100)
  humidite    = bme.readHumidity();            // Humidite relative en %

  DateTime now = rtc.now();   // Recupere la date et l'heure courantes depuis le DS3231

  // Formate la date en "JJ/MM/AAAA" et l'heure en "HH:MM:SS" avec snprintf
  snprintf(dateStr,  sizeof(dateStr),  "%02d/%02d/%04d",
           now.day(), now.month(), now.year());
  snprintf(heureStr, sizeof(heureStr), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());
}

// ===== LECTURE ANGLE BRUT AS5600 =====
// Lit les 12 bits de l'angle magnetique depuis les registres 0x0E et 0x0F de l'AS5600
// Retourne une valeur entre 0 et 4095 (correspondant a 0° et 360°)
uint16_t lireAngleBrut() {
  Wire.beginTransmission(AS5600_ADDR);         // Demarre la communication I2C avec l'AS5600
  Wire.write(REG_ANGLE_MSB);                   // Pointe sur le registre du byte de poids fort
  Wire.endTransmission(false);                 // false = Repeated Start (maintient le bus actif)
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);  // Demande 2 octets (MSB + LSB)
  if (Wire.available() < 2) return 0;          // Retourne 0 si la lecture a echoue
  uint16_t msb = Wire.read();   // Premier octet : bits 11-8 de l'angle
  uint16_t lsb = Wire.read();   // Deuxieme octet : bits 7-0 de l'angle
  return ((msb << 8) | lsb) & 0x0FFF;   // Masque 0x0FFF pour ne garder que 12 bits
}

// ===== DIRECTION DU VENT VIA AS5600 =====
// Convertit l'angle brut de l'AS5600 en point cardinal sur 16 directions
void majDirectionVent() {

  static const char* CARDINAUX[16] = {
    "N","NNE","NE","ENE","E","ESE","SE","SSE",
    "S","SSO","SO","OSO","O","ONO","NO","NNO"
  };   // 16 points cardinaux, chacun couvre 22.5 degres (360 / 16)

  uint16_t brut    = lireAngleBrut();              // Lecture de l'angle brut (0-4095)
  float    dir_deg = (brut / 4096.0f) * 360.0f;   // Conversion en degres (0.0 a 359.9)
  uint8_t  idx     = (uint8_t)((dir_deg + 11.25f) / 22.5f) % 16;
  // +11.25° pour centrer chaque secteur de 22.5°, modulo 16 pour le bouclage

  strncpy(directionVent, CARDINAUX[idx], 7);   // Copie le nom du cardinal dans la variable globale
  directionVent[7] = '\0';                      // Assure la terminaison de la chaine de caracteres
}

// ===== MOYENNE GLISSANTE =====
// Stocke la valeur dans un buffer circulaire et retourne la moyenne des MOYS_NB dernieres valeurs
// Lisse les variations rapides de la vitesse du vent pour un affichage stable
float moyenneGlissante(float val) {
  buf_vitesse[idx_moy % MOYS_NB] = val;   // Ecrase l'ancienne valeur a cet index
  idx_moy++;                               // Avance l'index (bouclage automatique via modulo)
  float somme = 0;
  for (uint8_t i = 0; i < MOYS_NB; i++) somme += buf_vitesse[i];   // Somme des 5 valeurs
  return somme / MOYS_NB;                  // Retourne la moyenne des 5 dernieres mesures
}

// ===== ICONES TFT =====
// Chaque icone est construite uniquement avec les primitives graphiques d'Adafruit GFX
// (cercles, rectangles, triangles, lignes) — aucune image bitmap necessaire

void iconThermometre(int cx, int cy, uint16_t c) {
  tft.fillRoundRect(cx-4, cy-16, 8, 24, 4, C_DKGREY);    // Corps du thermometre (fond sombre)
  tft.drawRoundRect(cx-4, cy-16, 8, 24, 4, c);            // Contour colore du corps
  tft.fillRect(cx-2, cy, 4, 8, c);                         // Colonne de mercure
  tft.fillCircle(cx, cy+13, 8, c);                         // Reservoir (boule du bas)
  tft.drawCircle(cx, cy+13, 9, c);                         // Contour du reservoir
  tft.drawLine(cx+4, cy-8, cx+8, cy-8, c);                 // Graduation 1
  tft.drawLine(cx+4, cy-2, cx+7, cy-2, c);                 // Graduation 2
}

void iconGoutte(int cx, int cy, uint16_t c) {
  tft.fillTriangle(cx, cy-16, cx-9, cy-3, cx+9, cy-3, c); // Pointe de la goutte (partie haute)
  tft.fillCircle(cx, cy+3, 10, c);                          // Corps arrondi de la goutte
  tft.fillCircle(cx-3, cy-3, 3, C_LGREY);                  // Reflet lumineux (effet 3D)
}

void iconJauge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);                    // Cadran circulaire de la jauge
  tft.drawLine(cx-13, cy, cx-9, cy, c);             // Graduation gauche (9h)
  tft.drawLine(cx, cy-13, cx, cy-9, c);             // Graduation haut (12h)
  tft.drawLine(cx+13, cy, cx+9, cy, c);             // Graduation droite (3h)
  tft.drawLine(cx, cy, cx-7, cy-10, C_RED);         // Aiguille (couleur rouge)
  tft.drawLine(cx, cy, cx-8, cy-9, C_RED);          // Aiguille (epaisseur)
  tft.fillCircle(cx, cy, 2, C_WHITE);               // Centre de l'aiguille
}

void iconVent(int cx, int cy, uint16_t c) {
  tft.drawLine(cx-13, cy-7, cx+9,  cy-7, c);        // Ligne de vent 1 (superieure)
  tft.drawLine(cx-13, cy,   cx+12, cy,   c);        // Ligne de vent 2 (centrale, plus longue)
  tft.drawLine(cx-13, cy+7, cx+5,  cy+7, c);        // Ligne de vent 3 (inferieure)
  tft.drawCircle(cx+9,  cy-10, 3, c);               // Tourbillon en bout de ligne 1
  tft.drawCircle(cx+12, cy+3,  3, c);               // Tourbillon en bout de ligne 2
  tft.drawCircle(cx+5,  cy+10, 3, c);               // Tourbillon en bout de ligne 3
}

void iconBoussole(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 13, c);                              // Boitier de la boussole
  tft.fillTriangle(cx, cy-12, cx-5, cy,   cx+5, cy,   C_RED);     // Aiguille Nord (rouge)
  tft.fillTriangle(cx, cy+12, cx-5, cy+1, cx+5, cy+1, C_DKGREY);  // Aiguille Sud (sombre)
  tft.fillRect(cx-1, cy-14, 3, 3, c);                        // Repere du Nord
}

void iconPluie(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx-7,  cy-8,  8, c);    // Nuage partie gauche
  tft.fillCircle(cx+4,  cy-10, 9, c);    // Nuage partie centrale (plus haute)
  tft.fillCircle(cx+12, cy-6,  7, c);    // Nuage partie droite
  tft.fillRect(cx-14, cy-8, 30, 10, c);  // Base plate du nuage (relie les cercles)
  tft.fillRoundRect(cx-9, cy+5, 3, 8, 1, C_CYAN);   // Goutte de pluie gauche
  tft.fillRoundRect(cx-1, cy+8, 3, 8, 1, C_CYAN);   // Goutte de pluie centrale (decalee)
  tft.fillRoundRect(cx+7, cy+5, 3, 8, 1, C_CYAN);   // Goutte de pluie droite
}

void iconCalendrier(int cx, int cy, uint16_t c) {
  tft.drawRoundRect(cx-13, cy-10, 26, 23, 2, c);   // Contour du calendrier
  tft.fillRoundRect(cx-13, cy-10, 26,  8, 2, c);   // Bande d'en-tete coloree
  tft.fillRect(cx-8, cy-14, 4, 6, c);               // Anneau de reliure gauche
  tft.fillRect(cx+4, cy-14, 4, 6, c);               // Anneau de reliure droit
  tft.drawLine(cx,    cy-2, cx,    cy+11, C_GREY);  // Ligne de grille verticale
  tft.drawLine(cx-13, cy+4, cx+13, cy+4,  C_GREY); // Ligne de grille horizontale
}

void iconHorloge(int cx, int cy, uint16_t c) {
  tft.drawCircle(cx, cy, 14, c);           // Cadran exterieur
  tft.drawCircle(cx, cy, 13, c);           // Cadran interieur (double trait)
  tft.drawLine(cx, cy, cx,   cy-8, c);     // Aiguille des minutes (vers le haut)
  tft.drawLine(cx, cy, cx+7, cy+4, c);     // Aiguille des heures
  tft.fillCircle(cx, cy, 2, c);            // Centre des aiguilles
  tft.fillRect(cx-1, cy-14, 3, 3, c);     // Repere 12h
}

void iconGPS(int cx, int cy, uint16_t c) {
  tft.fillCircle(cx, cy-7, 10, c);                          // Tete ronde du marqueur GPS
  tft.fillTriangle(cx-7, cy-2, cx+7, cy-2, cx, cy+14, c);  // Pointe triangulaire
  tft.fillCircle(cx, cy-7, 5, C_CARD);                      // Trou central (effet 3D)
  tft.drawCircle(cx, cy-7, 13, c);                          // Cercle de portee GPS
}

// Dispatcher : appelle l'icone correspondant au numero de type passe en parametre
void drawIcon(int type, int cx, int cy, uint16_t c) {
  switch (type) {
    case 0: iconThermometre(cx, cy, c); break;   // 0 = thermometre (temperature)
    case 1: iconGoutte(cx, cy, c);      break;   // 1 = goutte (humidite)
    case 2: iconJauge(cx, cy, c);       break;   // 2 = jauge (pression)
    case 4: iconVent(cx, cy, c);        break;   // 4 = vent
    case 5: iconBoussole(cx, cy, c);    break;   // 5 = boussole (direction)
    case 6: iconPluie(cx, cy, c);       break;   // 6 = nuage + pluie
    case 7: iconCalendrier(cx, cy, c);  break;   // 7 = calendrier (date)
    case 8: iconHorloge(cx, cy, c);     break;   // 8 = horloge (heure)
    case 9: iconGPS(cx, cy, c);         break;   // 9 = marqueur GPS
  }
}

// ===== BARRE DE TITRE =====
// Dessine la barre superieure avec le titre, le badge de page
// et un point colore indiquant l'etat de la connexion WiFi (vert/rouge)
void dessinerTitre(const char* titre, uint16_t bg) {

  tft.fillRect(0, 0, 320, 33, bg);          // Fond de toute la barre de titre
  tft.fillRect(0, 0, 4, 33, C_CYAN);        // Fine barre decorative cyan a gauche

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 9);
  tft.print(titre);                          // Nom de la page courante

  // Badge "Page X" en haut a droite
  tft.fillRoundRect(264, 5, 52, 22, 5, C_CYAN);
  tft.setTextColor(C_NAVY);
  tft.setTextSize(1);
  tft.setCursor(272, 12);
  tft.print("Page ");
  tft.print(pageActuelle);                   // Numero de page actuelle (1, 2 ou 3)

  // Indicateur WiFi : point vert si connecte, rouge si hors-ligne
  uint16_t colWifi = (wifiConnecte && WiFi.status() == WL_CONNECTED)
                     ? C_GREEN : C_RED;
  tft.fillCircle(247, 16, 5, colWifi);       // Petit cercle colore a cote du badge
}

// ===== BARRE DE NAVIGATION =====
// Dessine les boutons PREC et SUIV ainsi que les indicateurs de pages en bas de l'ecran
void dessinerBoutons() {

  tft.fillRect(0, 200, 320, 40, C_NAVY);      // Fond bleu marine de toute la barre
  tft.drawLine(0, 200, 320, 200, C_CYAN);     // Ligne de separation cyan en haut de la barre

  // Bouton PRECEDENT (cote gauche)
  tft.fillRoundRect(8,   207, 90, 26, 5, C_CARD);
  tft.drawRoundRect(8,   207, 90, 26, 5, C_CYAN);
  tft.setTextColor(C_CYAN);
  tft.setTextSize(2);
  tft.setCursor(15, 213);
  tft.print("< PREC");

  // Indicateurs de position au centre (point plein = page active, cercle vide = autre page)
  for (int i = 1; i <= 3; i++) {
    int dx = 140 + (i - 1) * 14;              // Espacement de 14 pixels entre chaque point
    if (i == pageActuelle) tft.fillCircle(dx, 220, 5, C_CYAN);   // Page active = point plein
    else                   tft.drawCircle(dx, 220, 5, C_GREY);   // Autre page = cercle vide
  }

  // Bouton SUIVANT (cote droit)
  tft.fillRoundRect(222, 207, 90, 26, 5, C_CARD);
  tft.drawRoundRect(222, 207, 90, 26, 5, C_CYAN);
  tft.setTextColor(C_CYAN);
  tft.setCursor(230, 213);
  tft.print("SUIV >");
}

// ===== CARTE STANDARD =====
// Dessine un cadre avec icone a gauche, separateur, puis label / valeur / unite a droite
// Parametres : position (x,y), taille (w,h), textes, couleur d'accentuation, numero d'icone
void dessinerCarte(int x, int y, int w, int h,
                   const char* label, const char* val, const char* unit,
                   uint16_t col, int iconType) {

  tft.fillRoundRect(x, y, w, h, 8, C_CARD);           // Fond sombre de la carte
  tft.drawRoundRect(x, y, w, h, 8, col);               // Bordure coloree selon le type de donnee
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);          // Petite barre d'accentuation en haut

  drawIcon(iconType, x+27, y + h/2, col);               // Icone centree verticalement a gauche

  tft.drawLine(x+52, y+10, x+52, y+h-10, C_DKGREY);   // Separateur vertical entre icone et texte

  tft.setTextColor(col);     tft.setTextSize(1); tft.setCursor(x+57, y+10); tft.print(label);// Label petit
  tft.setTextColor(C_WHITE); tft.setTextSize(2); tft.setCursor(x+57, y+27); tft.print(val);  // Valeur grand
  tft.setTextColor(C_WHITE); tft.setTextSize(1); tft.setCursor(x+57, y+52); tft.print(unit); // Unite petit
}

// ===== CARTE PLEINE LARGEUR =====
// Identique a dessinerCarte() mais sur toute la largeur de l'ecran (310px)
// Utilisee pour la pluviometrie qui necessite plus d'espace
void dessinerCartePleine(int y, uint16_t col, int iconType,
                          const char* label, const char* line1, const char* line2) {

  tft.fillRoundRect(5, y, 310, 78, 8, C_CARD);
  tft.drawRoundRect(5, y, 310, 78, 8, col);
  tft.fillRoundRect(13, y, 294, 3, 1, col);

  drawIcon(iconType, 36, y + 39, col);                  // Icone centree dans la carte

  tft.drawLine(58, y+10, 58, y+68, C_DKGREY);           // Separateur vertical

  tft.setTextColor(col);     tft.setTextSize(1); tft.setCursor(66, y+10); tft.print(label);  // Label
  tft.setTextColor(C_WHITE); tft.setTextSize(2); tft.setCursor(66, y+26); tft.print(line1);  // Valeur
  tft.setTextColor(C_WHITE); tft.setTextSize(1); tft.setCursor(66, y+52); tft.print(line2);  // Unite
}

// ===== CARTE GPS =====
// Carte speciale affichant la localisation avec indicateur de fix vert (OK) ou rouge (attente)
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt) {

  int x = 5, y = y_start, w = 310, h = 95;
  uint16_t col    = ILI9341_CYAN;
  uint16_t colFix = gps.location.isValid() ? ILI9341_GREEN : ILI9341_RED;
  // L'icone GPS change de couleur : vert si position fixee, rouge si en recherche

  tft.fillRoundRect(x, y, w, h, 8, C_CARD);
  tft.drawRoundRect(x, y, w, h, 8, col);
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);

  drawIcon(9, x+30, y + h/2, colFix);             // Icone GPS avec la couleur du fix

  tft.drawLine(x+58, y+10, x+58, y+h-10, C_DKGREY);   // Separateur vertical

  tft.setTextColor(col); tft.setTextSize(1);
  tft.setCursor(x+65, y+8);
  tft.print("Localisation GPS");                   // En-tete de la carte GPS

  if (gps.location.isValid()) {
    // Fix GPS obtenu : afficher les coordonnees reelles
    tft.setTextColor(C_WHITE); tft.setTextSize(2);
    tft.setCursor(x+65, y+24);
    tft.print("Lat: "); tft.print(lat, 4); tft.print(" N");   // Latitude (4 decimales)
    tft.setCursor(x+65, y+50);
    tft.print("Lon: "); tft.print(lon, 4); tft.print(" E");   // Longitude (4 decimales)
    tft.setTextColor(C_WHITE); tft.setTextSize(1);
    tft.setCursor(x+65, y+80);
    tft.print("Alt: "); tft.print(alt);
    tft.print(" m   Sat: "); tft.print(satGPS);               // Altitude et nb satellites
  } else {
    // Pas de fix GPS : afficher un message d'attente en jaune
    tft.setTextColor(ILI9341_YELLOW); tft.setTextSize(1);
    tft.setCursor(x+65, y+30);
    tft.print("Recherche satellites...");
    tft.setCursor(x+65, y+48);
    tft.print("Sat detectes : "); tft.print(satGPS);   // Montre la progression
    tft.setCursor(x+65, y+66);
    tft.print("Placez l'antenne vers le ciel");
  }
}

// ===== AFFICHAGE DES PAGES =====
// Efface l'ecran et redessine entierement la page demandee avec les donnees actuelles
void afficherPage(int num) {

  tft.fillScreen(C_BG);    // Efface tout l'ecran (fond noir)
  dessinerBoutons();        // Dessine toujours la barre de navigation en bas (commune aux 3 pages)

  char buf[16];   // Buffer temporaire pour la conversion float → string via dtostrf

  // ===== PAGE 1 : CLIMAT =====
  // Affiche temperature, humidite et pression atmospherique
  if (num == 1) {
    dessinerTitre("CLIMAT", C_TITLE_P1);

    dtostrf(temperature, 4, 1, buf);   // Convertit le float en texte (ex: "22.5")
    dessinerCarte(5,   36, 155, 78, "Temperature", buf, "deg C", C_ORANGE,     0);

    dtostrf(humidite, 4, 1, buf);
    dessinerCarte(163, 36, 152, 78, "Humidite",    buf, "%",     ILI9341_CYAN, 1);

    dtostrf(pression, 6, 1, buf);
    dessinerCarte(5,  118, 310, 78, "Pression",    buf, "hPa",  ILI9341_GREEN,2);
  }

  // ===== PAGE 2 : VENT & PLUIE =====
  // Affiche la vitesse du vent, la direction et l'intensite de la pluie
  else if (num == 2) {
    dessinerTitre("VENT & PLUIE", C_TITLE_P2);

    dtostrf(vitesseVent, 4, 1, buf);
    dessinerCarte(5,   36, 155, 78, "Vit. Vent", buf,           "km/h", C_ORANGE,      4);
    dessinerCarte(163, 36, 152, 78, "Direction", directionVent, "",     ILI9341_WHITE, 5);
    // directionVent est deja une chaine (ex: "NNE"), pas besoin de conversion

    dtostrf(pluviometrie, 4, 1, buf);
    dessinerCartePleine(118, ILI9341_BLUE, 6, "Pluviometrie", buf, "mm / heure");
  }

  // ===== PAGE 3 : GPS & TEMPS =====
  // Affiche la date, l'heure (DS3231) et la position GPS (NEO-6M)
  else if (num == 3) {
    dessinerTitre("GPS & TEMPS", C_TITLE_P3);

    dessinerCarte(5,   36, 153, 62, "Date",  dateStr,  "", ILI9341_WHITE,  7);
    dessinerCarte(162, 36, 153, 62, "Heure", heureStr, "", ILI9341_YELLOW, 8);

    dessinerCarteGPS(102, latGPS, lonGPS, altGPS);   // Carte GPS avec coordonnees

    // Si le WiFi est connecte, afficher l'IP locale en bas pour y acceder facilement
    if (wifiConnecte && WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(C_GREEN);
      tft.setTextSize(1);
      tft.setCursor(8, 202);
      tft.print("IP: ");
      tft.print(WiFi.localIP().toString());   // Ex: 192.168.1.45
    }
  }
}
