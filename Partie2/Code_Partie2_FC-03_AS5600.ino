// ============================================
//   Anémomètre DIY — ESP32 + Capteur Photoélectrique
//   Chaque impulsion = 12° de rotation (1/30 de tour)
//   Bras de coupelles : 3 cm (rayon = 0.03 m)
// ============================================

// ——— CONFIGURATION ———————————————————————————
#define PIN_CAPTEUR          4        // GPIO connecté au capteur
#define RAYON_BRAS_M         0.03     // Rayon en mètres (3 cm → 0.03)
#define DEGRES_PAR_IMPULSION 12.0     // Chaque impulsion = 12°
#define INTERVALLE_MS        2000     // Calcul toutes les 2 secondes

// ——— CONSTANTE : fraction de tour par impulsion ———
// 12° / 360° = 0.03333... tour par impulsion
const float FRACTION_TOUR   = DEGRES_PAR_IMPULSION / 360.0;
const float CIRCONFERENCE   = 2.0 * PI * RAYON_BRAS_M;  // ≈ 0.1885 m

// ——— VARIABLES ————————————————————————————————
volatile uint32_t compteur = 0;
float vitesse_ms  = 0.0;
float vitesse_kmh = 0.0;
uint32_t dernier_calcul = 0;

// ——— INTERRUPTION ————————————————————————————
void IRAM_ATTR compterImpulsion() {
  compteur++;
}

// ——— SETUP ———————————————————————————————————
void setup() {
  Serial.begin(115200);
  pinMode(PIN_CAPTEUR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_CAPTEUR), compterImpulsion, FALLING);

  Serial.println("Anémomètre ESP32 démarré");
  Serial.printf("Rayon        : %.3f m\n", RAYON_BRAS_M);
  Serial.printf("Circonférence: %.5f m\n", CIRCONFERENCE);
  Serial.printf("Par impulsion: %.4f tour (%.1f°)\n", FRACTION_TOUR, DEGRES_PAR_IMPULSION);

  dernier_calcul = millis();
}

// ——— LOOP ————————————————————————————————————
void loop() {
  uint32_t maintenant = millis();
  uint32_t elapsed    = maintenant - dernier_calcul;

  if (elapsed >= INTERVALLE_MS) {

    noInterrupts();
    uint32_t impulsions = compteur;
    compteur = 0;
    interrupts();

    // Nombre de tours réels = impulsions × (12° / 360°)
    float tours_reels   = impulsions * FRACTION_TOUR;
    float tours_par_sec = tours_reels / (elapsed / 1000.0);

    vitesse_ms  = tours_par_sec * CIRCONFERENCE;
    vitesse_kmh = vitesse_ms * 3.6;

    const char* beaufort = getBeaufort(vitesse_kmh);

    Serial.printf("[%6lu ms] Impulsions: %3lu | Tours réels: %.2f | %5.2f m/s | %5.1f km/h | %s\n",
                  maintenant, impulsions, tours_reels, vitesse_ms, vitesse_kmh, beaufort);

    dernier_calcul = maintenant;
  }
}

// ——— ECHELLE DE BEAUFORT ————————————————————
const char* getBeaufort(float kmh) {
  if (kmh <   1)  return "B0 - Calme";
  if (kmh <   6)  return "B1 - Très légère brise";
  if (kmh <  12)  return "B2 - Légère brise";
  if (kmh <  20)  return "B3 - Petite brise";
  if (kmh <  29)  return "B4 - Jolie brise";
  if (kmh <  39)  return "B5 - Brise fraîche";
  if (kmh <  50)  return "B6 - Brise forte";
  if (kmh <  62)  return "B7 - Grand frais";
  if (kmh <  75)  return "B8 - Coup de vent";
  if (kmh <  89)  return "B9 - Fort coup de vent";
  if (kmh < 103)  return "B10 - Tempête";
  if (kmh < 118)  return "B11 - Violente tempête";
                   return "B12 - Ouragan !";
}