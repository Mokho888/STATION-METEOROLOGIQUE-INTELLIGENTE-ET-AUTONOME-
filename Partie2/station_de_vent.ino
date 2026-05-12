/*
 * ============================================================
 *  STATION DE MESURE VENT — ESP32
 *  Vitesse : FC-03 + roue codeuse 30 fentes (rayon bras = 4 cm)
 *  Direction : encodeur magnétique AS5600 (I²C)
 * ============================================================
 *  Auteur  : generated for ESP32 / Arduino framework
 *  Révision: 2025
 * ============================================================
 *
 *  Formule vitesse :
 *    chaque impulsion FC-03 = 360/30 = 12° de rotation
 *    => 1 tour complet = 30 impulsions
 *    distance / tour = 2π × r = 2π × 0.04 m ≈ 0.2513 m
 *    distance / impulsion = 0.2513 / 30 ≈ 8.378 mm
 *    V (m/s) = (nb_impulsions × dist_par_impulsion) / temps_s
 *
 *  AS5600 :
 *    Registre angle brut = 0x0E (2 octets, 12 bits → 0..4095)
 *    Angle (°) = (raw / 4096) × 360
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>

/* ─── Broches ─────────────────────────────────────────── */
#define FC03_PIN       4     // entrée digitale FC-03 (IRAM-safe, pull-up externe si nécessaire)
#define SDA_PIN        21     // I²C SDA → AS5600
#define SCL_PIN        22     // I²C SCL → AS5600

/* ─── Paramètres anémomètre ───────────────────────────── */
#define NB_FENTES      30                             // nombre de fentes sur la roue
#define RAYON_M        0.04f                          // rayon du bras de coupelles (mètres)
#define DIST_PAR_PULSE (TWO_PI * RAYON_M / NB_FENTES) // ≈ 0.008378 m

/* ─── Paramètres mesure ───────────────────────────────── */
#define INTERVALLE_MS  1000   // fenêtre de comptage (ms) — 1 s
#define MOYS_NB        5      // nombre de mesures pour la moyenne glissante

/* ─── Adresse / registres AS5600 ─────────────────────── */
#define AS5600_ADDR    0x36
#define REG_ANGLE_MSB  0x0E   // registre 12-bit angle brut (2 octets Big-Endian)
#define REG_STATUS     0x0B   // registre statut (bit 5 = MD = magnet detected)

/* ─── Variables globales ─────────────────────────────── */
volatile uint32_t g_pulses = 0;   // compteur d'impulsions (mis à jour par ISR)

float   buf_vitesse[MOYS_NB] = {0};   // tampon moyenne glissante
uint8_t idx_moy = 0;

/* ─── Prototypes ─────────────────────────────────────── */
void    IRAM_ATTR isrFC03();
bool    as5600Detecte();
uint16_t lireAngleBrut();
float   brut2Degres(uint16_t raw);
String  degreVersCardinal(float deg);
float   calcVitesse(uint32_t impulsions, uint32_t dt_ms);
float   moyenneGlissante(float nouvelle_valeur);
void    afficherJSON(float vitesse_ms, float vitesse_kmh, float vitesse_bf,
                     float dir_deg, const String& cardinal);
uint8_t echelleBeaufort(float v_ms);

/* ===========================================================
   SETUP
   =========================================================== */
void setup()
{
    Serial.begin(115200);
    delay(500);

    /* ── FC-03 ── */
    pinMode(FC03_PIN, INPUT);                                 // pull-up externe recommandé
    attachInterrupt(digitalPinToInterrupt(FC03_PIN),
                    isrFC03, FALLING);                        // front descendant = fente traversée

    /* ── I²C / AS5600 ── */
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);   // mode Fast I²C

    Serial.println("\n========================================");
    Serial.println("  STATION VENT  —  ESP32");
    Serial.println("========================================");

    if (as5600Detecte()) {
        Serial.println("[AS5600]  capteur détecté ✓");
    } else {
        Serial.println("[AS5600]  ERREUR : capteur non trouvé !");
        Serial.println("          Vérifiez câblage SDA/SCL et alimentation 3.3 V.");
    }

    Serial.println("[FC-03]   interruption configurée sur GPIO " + String(FC03_PIN));
    Serial.printf("[Anémomètre] %d fentes, rayon %.0f mm, dist/pulse = %.4f m\n",
                  NB_FENTES, RAYON_M * 1000.0f, DIST_PAR_PULSE);
    Serial.println("----------------------------------------");
    Serial.println("Format : JSON compact, 1 ligne par seconde");
    Serial.println("----------------------------------------\n");
}

/* ===========================================================
   LOOP
   =========================================================== */
void loop()
{
    static uint32_t t_dernier   = 0;
    static uint32_t pulses_prec = 0;

    uint32_t t_courant = millis();

    if (t_courant - t_dernier >= INTERVALLE_MS)
    {
        uint32_t dt = t_courant - t_dernier;
        t_dernier   = t_courant;

        /* ── Snapshot atomique du compteur ISR ── */
        noInterrupts();
        uint32_t pulses_courants = g_pulses;
        interrupts();

        uint32_t delta = pulses_courants - pulses_prec;
        pulses_prec    = pulses_courants;

        /* ── Vitesse ── */
        float v_ms   = calcVitesse(delta, dt);
        float v_moy  = moyenneGlissante(v_ms);
        float v_kmh  = v_moy * 3.6f;
        uint8_t bf   = echelleBeaufort(v_moy);

        /* ── Direction ── */
        uint16_t brut    = lireAngleBrut();
        float    dir_deg = brut2Degres(brut);
        String   cardinal = degreVersCardinal(dir_deg);

        /* ── Sortie ── */
        afficherJSON(v_moy, v_kmh, (float)bf, dir_deg, cardinal);
    }
}

/* ===========================================================
   ISR  —  FC-03
   Déclenchée sur chaque front descendant (passage d'une fente)
   =========================================================== */
void IRAM_ATTR isrFC03()
{
    g_pulses++;
}

/* ===========================================================
   AS5600 — Détection
   =========================================================== */
bool as5600Detecte()
{
    /* Tentative de communication I²C */
    Wire.beginTransmission(AS5600_ADDR);
    if (Wire.endTransmission() != 0) return false;

    /* Vérification bit MD (Magnet Detected) dans le registre STATUS */
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(REG_STATUS);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_ADDR, (uint8_t)1);
    if (!Wire.available()) return false;
    uint8_t status = Wire.read();
    return (status >> 5) & 0x01;   // bit 5 = MD
}

/* ===========================================================
   AS5600 — Lecture angle brut (12 bits)
   =========================================================== */
uint16_t lireAngleBrut()
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(REG_ANGLE_MSB);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_ADDR, (uint8_t)2);

    if (Wire.available() < 2) return 0;

    uint16_t msb = Wire.read();
    uint16_t lsb = Wire.read();
    return ((msb << 8) | lsb) & 0x0FFF;   // 12 bits valides
}

/* ===========================================================
   Conversion brut → degrés (0..360)
   =========================================================== */
float brut2Degres(uint16_t raw)
{
    return (raw / 4096.0f) * 360.0f;
}

/* ===========================================================
   Direction en point cardinal (16 secteurs de 22.5°)
   Système : Nord = 0°/360°, sens horaire
   =========================================================== */
String degreVersCardinal(float deg)
{
    static const char* CARDINAUX[16] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSO","SO","OSO","O","ONO","NO","NNO"
    };
    uint8_t idx = (uint8_t)((deg + 11.25f) / 22.5f) % 16;
    return String(CARDINAUX[idx]);
}

/* ===========================================================
   Calcul vitesse du vent en m/s
   V = (n × dist_par_pulse) / dt
   =========================================================== */
float calcVitesse(uint32_t impulsions, uint32_t dt_ms)
{
    if (dt_ms == 0) return 0.0f;
    float dt_s = dt_ms / 1000.0f;
    return (impulsions * DIST_PAR_PULSE) / dt_s;
}

/* ===========================================================
   Moyenne glissante sur MOYS_NB mesures
   =========================================================== */
float moyenneGlissante(float val)
{
    buf_vitesse[idx_moy % MOYS_NB] = val;
    idx_moy++;
    float somme = 0;
    for (uint8_t i = 0; i < MOYS_NB; i++) somme += buf_vitesse[i];
    return somme / MOYS_NB;
}

/* ===========================================================
   Échelle de Beaufort (0..12) à partir de la vitesse en m/s
   =========================================================== */
uint8_t echelleBeaufort(float v_ms)
{
    static const float SEUILS[12] = {
        0.3f, 1.5f, 3.4f, 5.4f, 7.9f, 10.7f,
        13.8f, 17.1f, 20.7f, 24.4f, 28.4f, 32.7f
    };
    for (uint8_t bf = 0; bf < 12; bf++) {
        if (v_ms < SEUILS[bf]) return bf;
    }
    return 12;
}

/* ===========================================================
   Affichage lisible sur le moniteur série
   Exemple :
   ┌─────────────────────────────────────────────┐
   │        STATION MÉTÉO — MESURE VENT          │
   ├──────────────────┬──────────────────────────┤
   │  VITESSE         │  DIRECTION               │
   │  4.215  m/s      │  247.3°  OSO             │
   │  15.17  km/h     │  [SO ← · → O]            │
   │  Beaufort : 4    │                          │
   ├──────────────────┴──────────────────────────┤
   │  Impulsions totales : 503   Uptime : 503 s  │
   └─────────────────────────────────────────────┘
   =========================================================== */

/* Retourne la description Beaufort */
static const char* descBeaufort(uint8_t bf)
{
    static const char* DESC[13] = {
        "Calme",        "Très légère brise", "Légère brise",
        "Petite brise", "Jolie brise",        "Brise fraîche",
        "Vent frais",   "Grand frais",        "Coup de vent",
        "Fort coup de vent", "Tempête",       "Violente tempête",
        "Ouragan"
    };
    return bf <= 12 ? DESC[bf] : "???";
}

/* Barre de progression vitesse (0..12 blocs pour 0..32.7 m/s) */
static String barreVitesse(float v_ms)
{
    const uint8_t MAX_BLOCS = 12;
    uint8_t blocs = (uint8_t)constrain(v_ms / 32.7f * MAX_BLOCS, 0, MAX_BLOCS);
    String b = "[";
    for (uint8_t i = 0; i < MAX_BLOCS; i++) b += (i < blocs) ? '#' : ' ';
    b += "]";
    return b;
}

/* Rose des vents ASCII 8 points */
static String roseVents(float deg)
{
    /*  Disposition fixe :
           NNO  N  NNE
           NO   +  NE
           ONO  O  ESE   etc.  — on met juste la flèche 8-dir
    */
    const uint8_t sect = (uint8_t)((deg + 22.5f) / 45.0f) % 8;
    /* 0=N 1=NE 2=E 3=SE 4=S 5=SO 6=O 7=NO */
    static const char* FLECHE[8] = {
        "  ^  ", " / ", " > ", " \\ ", "  v  ", " / ", " < ", " \\ "
    };
    static const char* LABEL[8] = { "N","NE","E","SE","S","SO","O","NO" };
    String r = "  ";
    r += LABEL[sect];
    r += "  ";
    r += FLECHE[sect];
    return r;
}

void afficherJSON(float vitesse_ms, float vitesse_kmh, float vitesse_bf,
                  float dir_deg, const String& cardinal)
{
    static uint32_t t_debut_s = 0;
    if (t_debut_s == 0) t_debut_s = millis() / 1000;

    noInterrupts();
    uint32_t ptot = g_pulses;
    interrupts();

    uint32_t uptime = millis() / 1000 - t_debut_s;
    uint8_t  bf     = (uint8_t)vitesse_bf;
    String   barre  = barreVitesse(vitesse_ms);
    String   rose   = roseVents(dir_deg);

    /* ── Effacement terminal (ANSI) ── */
    Serial.print("\033[2J\033[H");   // clear + curseur en haut

    /* ── Encadré principal ── */
    Serial.println("+-----------------------------------------+");
    Serial.println("|     STATION METEO  --  MESURE VENT     |");
    Serial.println("+-------------------+---------------------+");
    Serial.println("|     VITESSE       |     DIRECTION       |");
    Serial.println("+-------------------+---------------------+");

    /* Ligne m/s */
    Serial.printf("|  %-7.3f  m/s     |  %6.1f deg  %-4s  |\n",
                  vitesse_ms, dir_deg, cardinal.c_str());

    /* Ligne km/h + rose */
    Serial.printf("|  %-7.2f  km/h    |     %s     |\n",
                  vitesse_kmh, rose.c_str());

    /* Ligne barre + Beaufort */
    Serial.printf("|  %s       |                     |\n", barre.c_str());

    /* Ligne description Beaufort */
    char bf_buf[22];
    snprintf(bf_buf, sizeof(bf_buf), "Bf %d : %-14s", bf, descBeaufort(bf));
    Serial.printf("|  %-17s|                     |\n", bf_buf);

    Serial.println("+-------------------+---------------------+");

    /* Ligne bas : impulsions + uptime */
    Serial.printf("|  Impulsions : %-8lu   Uptime : %4lu s  |\n",
                  (unsigned long)ptot, (unsigned long)uptime);

    Serial.println("+-----------------------------------------+");
}
