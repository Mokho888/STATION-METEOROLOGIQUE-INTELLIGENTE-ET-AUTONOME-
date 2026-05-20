#include <WiFi.h>              // Bibliothèque WiFi pour ESP32
#include <WebServer.h>         // Serveur web local sur port 80
#include <HTTPClient.h>        // Envoi de requêtes HTTP vers Firebase
#include <Adafruit_GFX.h>      // Librairie graphique de base pour OLED
#include <Adafruit_SSD1306.h>  // Librairie spécifique écran OLED SSD1306
#include <Wire.h>              // Communication I2C (OLED)
#include <DHT.h>               // Librairie capteur DHT11/DHT22
#include <WiFiClientSecure.h>  // Client WiFi sécurisé pour connexions HTTPS

// ===== DHT22 =====
#define DHTPIN  5              // Pin GPIO5 connectée au DATA du DHT22
#define DHTTYPE DHT22          // Type de capteur : DHT22

// ===== OLED =====
#define SCREEN_WIDTH 128       // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT 64       // Hauteur de l'écran OLED en pixels

// ===== WIFI =====
const char* ssid     = "MEZX";           // Nom du réseau WiFi
const char* password = "12312312345";    // Mot de passe du réseau WiFi

// ===== FIREBASE =====
const char* firebaseURL = "https://meteo-f6152-default-rtdb.europe-west1.firebasedatabase.app/weather.json";
// URL HTTPS de la base de données Firebase Realtime

// ===== OBJETS =====
DHT dht(DHTPIN, DHTTYPE);               // Objet DHT22 sur la pin définie
WebServer server(80);                    // Serveur web écoutant sur le port 80
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Objet écran OLED I2C

// ===== VARIABLES =====
float temperature;            // Température lue par le DHT22 en °C
float humidite;               // Humidité lue par le DHT22 en %
unsigned long lastSend = 0;   // Timestamp du dernier envoi Firebase (ms)

// ===== PAGE WEB =====
void handleRoot() {
  String page = "<!DOCTYPE html><html>";                     // Début document HTML
  page += "<head><meta http-equiv='refresh' content='2'/>";  // Rafraîchissement auto toutes les 2s
  page += "<meta charset='UTF-8'>";                          // Encodage UTF-8 pour les accents
  page += "<title>ESP32 DHT22</title></head>";               // Titre de l'onglet navigateur
  page += "<body style='text-align:center;font-family:Arial'>"; // Corps centré police Arial
  page += "<h1>Station Meteo ESP32</h1>";                    // Titre principal de la page
  page += "<h2>&#127777; Temperature : " + String(temperature, 1) + " &deg;C</h2>"; // Affiche température
  page += "<h2>&#128167; Humidite    : " + String(humidite, 1)    + " %</h2>";      // Affiche humidité
  page += "</body></html>";                                  // Fin du document HTML
  server.send(200, "text/html", page);                       // Envoie la page au client
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);   // Initialise la communication série à 115200 bauds
  Wire.begin();           // Initialise le bus I2C pour l'OLED

  // ===== OLED =====
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Démarre l'OLED à l'adresse I2C 0x3C
    Serial.println("Erreur OLED !");                 // Message d'erreur si OLED non trouvé
    for (;;);                                        // Bloque le programme si OLED absent
  }
  display.clearDisplay();           // Efface l'écran OLED
  display.setTextSize(1);           // Taille du texte : petite (1x)
  display.setTextColor(WHITE);      // Couleur du texte : blanc
  display.setCursor(0, 0);          // Curseur en haut à gauche
  display.println("Demarrage...");  // Affiche message de démarrage
  display.display();                // Envoie le buffer vers l'écran

  // ===== DHT22 =====
  dht.begin();                      // Initialise le capteur DHT22
  Serial.println("DHT22 OK");       // Confirmation dans le moniteur série

  // ===== WiFi =====
  display.clearDisplay();               // Efface l'écran avant affichage WiFi
  display.setCursor(0, 0);              // Curseur en haut à gauche
  display.println("Connexion WiFi...");  // Affiche message de connexion
  display.display();                    // Actualise l'écran OLED

  WiFi.begin(ssid, password);           // Lance la connexion WiFi
  Serial.print("Connexion WiFi");       // Affiche dans le moniteur série

  while (WiFi.status() != WL_CONNECTED) { // Attend tant que non connecté
    delay(500);                            // Pause 500ms entre chaque tentative
    Serial.print(".");                     // Affiche un point à chaque tentative
  }

  Serial.println("\nWiFi connecte !");  // Confirme la connexion WiFi
  Serial.print("IP: ");                 // Affiche le label IP
  Serial.println(WiFi.localIP());       // Affiche l'adresse IP obtenue

  // ===== OLED afficher IP =====
  display.clearDisplay();           // Efface l'écran
  display.setCursor(0, 0);          // Curseur en haut à gauche
  display.println("WiFi OK");       // Affiche confirmation WiFi
  display.setCursor(0, 20);         // Curseur à la ligne suivante
  display.print("IP:");             // Label IP
  display.println(WiFi.localIP());  // Affiche l'adresse IP sur l'OLED
  display.display();                // Actualise l'écran
  delay(3000);                      // Affiche l'IP pendant 3 secondes

  // ===== Web server =====
  server.on("/", handleRoot);  // Associe l'URL "/" à la fonction handleRoot
  server.begin();              // Démarre le serveur web
}

// ===== LOOP =====
void loop() {
  server.handleClient();  // Traite les requêtes web entrantes

  // ===== Lecture DHT22 =====
  float t = dht.readTemperature();  // Lit la température en °C
  float h = dht.readHumidity();     // Lit l'humidité en %

  if (!isnan(t) && !isnan(h)) {  // Vérifie que les valeurs sont valides (pas NaN)
    temperature = t;               // Sauvegarde la température valide
    humidite    = h;               // Sauvegarde l'humidité valide
  } else {
    Serial.println("Erreur lecture DHT22 !"); // Signale une lecture invalide
  }

  // ===== SERIAL =====
  Serial.println("-------- Mesures --------");       // Séparateur dans le moniteur série
  Serial.printf("Temp : %.1f C\n", temperature);     // Affiche température avec 1 décimale
  Serial.printf("Hum  : %.1f %%\n", humidite);       // Affiche humidité avec 1 décimale
  Serial.println("-------------------------");        // Séparateur de fin

  // ===== OLED =====
  display.clearDisplay();  // Efface l'écran avant redessin

  display.drawLine(0, 10, 127, 10, WHITE);  // Trace une ligne séparatrice horizontale

  display.setTextSize(1);           // Taille texte petite
  display.setCursor(20, 0);         // Curseur centré en haut
  display.print("Station Meteo");   // Titre affiché en haut de l'écran

  // --- Température ---
  display.setCursor(0, 18);             // Curseur pour le label température
  display.print("Temp :");              // Label température
  display.setTextSize(2);               // Grande taille pour la valeur
  display.setCursor(48, 16);            // Curseur pour la valeur numérique
  display.printf("%.1f", temperature);  // Affiche valeur avec 1 décimale
  display.setTextSize(1);               // Revient à petite taille
  display.setCursor(110, 16);           // Curseur pour l'unité
  display.print("\xF8""C");             // Affiche le symbole °C

  // --- Humidité ---
  display.setCursor(0, 42);           // Curseur pour le label humidité
  display.print("Hum  :");            // Label humidité
  display.setTextSize(2);             // Grande taille pour la valeur
  display.setCursor(48, 40);          // Curseur pour la valeur numérique
  display.printf("%.1f", humidite);   // Affiche valeur avec 1 décimale
  display.setTextSize(1);             // Revient à petite taille
  display.setCursor(110, 40);         // Curseur pour l'unité
  display.print("%");                 // Affiche le symbole %

  display.display();  // Envoie le buffer vers l'écran OLED

  // ===== ENVOI FIREBASE HTTPS =====
  if (millis() - lastSend > 5000) {  // Vérifie si 5 secondes se sont écoulées
    lastSend = millis();              // Met à jour le timestamp du dernier envoi

    if (WiFi.status() == WL_CONNECTED) {  // Vérifie que le WiFi est encore connecté

      WiFiClientSecure client;             // Crée un client WiFi sécurisé SSL/TLS
      client.setInsecure();                // Désactive la vérification du certificat SSL (simplifié)

      HTTPClient https;                               // Crée un objet client HTTPS
      https.begin(client, firebaseURL);               // Initialise la connexion HTTPS sécurisée
      https.addHeader("Content-Type", "application/json"); // Définit le type de contenu JSON

      String jsonData = "{";                                        // Début du JSON
      jsonData += "\"temperature\":" + String(temperature, 1) + ","; // Champ température
      jsonData += "\"humidite\":"    + String(humidite, 1);           // Champ humidité
      jsonData += "}";                                              // Fin du JSON

      int httpResponseCode = https.PUT(jsonData);  // Envoie les données par requête PUT sécurisée
      Serial.print("Firebase HTTPS code: ");       // Affiche le label du code retour
      Serial.println(httpResponseCode);            // Affiche le code HTTP reçu (200 = OK)

      if (httpResponseCode > 0) {                        // Vérifie que la réponse est valide
        Serial.println("Envoi HTTPS reussi !");          // Confirme l'envoi réussi
      } else {
        Serial.print("Erreur HTTPS : ");                 // Signale une erreur d'envoi
        Serial.println(https.errorToString(httpResponseCode)); // Affiche le détail de l'erreur
      }

      https.end();   // Ferme la connexion HTTPS
    }
  }

  delay(2000);  // Pause 2 secondes avant la prochaine mesure
}