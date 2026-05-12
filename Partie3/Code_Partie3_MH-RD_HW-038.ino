// ============================================================
//  PLUVIOMETRE INTELLIGENT - ESP32
//  MH-RD + Capteur niveau
// ============================================================
//  ATTENTION : ESP32 = 3.3V — toutes les broches sont 3.3V
//  Diviseur de tension obligatoire sur GPIO 35
// ============================================================

// ---- Broches ESP32 -----------------------------------------
const int PIN_MHRD    = 4;    // MH-RD → GPIO 4 (interrupt)
const int PIN_SIGNAL  = 5;    // LED pluie en cours → GPIO 5
const int sensorPower = 15;   // Alim. capteur niveau → GPIO 15
const int sensorPin   = 35;   // Signal niveau → GPIO 35 (ADC)

// ---- Calibration -------------------------------------------
const float MM_PAR_BASCULE = 0.2794;
const int   DEBOUNCE_MS    = 200;
const long  TIMEOUT_PLUIE  = 60000;

// ---- Seuils niveau eau (recalibrer pour 3.3V = 4095 max) ---
const int NIVEAU_SEC     = 400;
const int NIVEAU_HUMIDE  = 1600;
const int NIVEAU_PLEIN   = 3200;

// ---- Variables bascule -------------------------------------
volatile unsigned long compteur        = 0;
volatile unsigned long dernierTemps    = 0;
volatile unsigned long intervalleMs    = 0;
volatile bool          nouvelleBascule = false;
volatile bool          etatMHRD        = false;

// ---- Variables calcul --------------------------------------
unsigned long bascules1min = 0;
unsigned long t1min        = 0;
bool          pluieEnCours = false;
float         mmTotal      = 0;
float         mmhActuel    = 0;
int           niveauRaw    = 0;

// ============================================================
//  Lecture capteur niveau
// ============================================================
int readSensor() {
  digitalWrite(sensorPower, HIGH);
  delay(10);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return val;
}

// ============================================================
String intensitePluie(float mmh) {
  if      (mmh == 0)   return "Aucune";
  else if (mmh < 1.0)  return "Bruine";
  else if (mmh < 5.0)  return "Faible";
  else if (mmh < 15.0) return "Moderee";
  else if (mmh < 30.0) return "Forte";
  else                 return "Tres forte";
}

// ============================================================
//  ISR — IRAM_ATTR obligatoire sur ESP32 !
// ============================================================
void IRAM_ATTR ISR_falling() {
  unsigned long maintenant = millis();
  if (maintenant - dernierTemps > DEBOUNCE_MS) {
    intervalleMs    = maintenant - dernierTemps;
    dernierTemps    = maintenant;
    compteur++;
    bascules1min++;
    nouvelleBascule = true;
    etatMHRD        = false;
  }
}

void IRAM_ATTR ISR_rising() {
  unsigned long maintenant = millis();
  if (maintenant - dernierTemps > DEBOUNCE_MS) {
    intervalleMs    = maintenant - dernierTemps;
    dernierTemps    = maintenant;
    compteur++;
    bascules1min++;
    nouvelleBascule = true;
    etatMHRD        = true;
  }
}

// ============================================================
void setup() {
  Serial.begin(115200);

  // ---- Broches --------------------------------------------
  pinMode(PIN_MHRD,    INPUT_PULLUP);
  pinMode(PIN_SIGNAL,  OUTPUT);
  pinMode(sensorPower, OUTPUT);
  pinMode(sensorPin,   INPUT);        // GPIO 35 = input only
  digitalWrite(PIN_SIGNAL,  LOW);
  digitalWrite(sensorPower, LOW);

  // ---- ADC ESP32 : résolution 12 bits ---------------------
  analogReadResolution(12);

  // ---- Interruptions deux sens ----------------------------
  attachInterrupt(digitalPinToInterrupt(PIN_MHRD), ISR_falling, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_MHRD), ISR_rising,  RISING);

  t1min = millis();

  Serial.println("============================================");
  Serial.println("   PLUVIOMETRE ESP32 - Pret                ");
  Serial.println("============================================");
  Serial.println("# | Sens | Intervalle | mm total | mm/h | Intensite | Niveau");
}
// ============================================================
void loop() {
  unsigned long maintenant = millis();

  niveauRaw = readSensor();

  noInterrupts();
  unsigned long b1 = bascules1min;
  interrupts();
  mmhActuel = (b1 / 2.0) * MM_PAR_BASCULE * 60.0;

  // ---- Nouvelle bascule -----------------------------------
  if (nouvelleBascule) {
    nouvelleBascule = false;

    noInterrupts();
    unsigned long cnt  = compteur;
    unsigned long iMs  = intervalleMs;
    bool          sens = etatMHRD;
    interrupts();

    mmTotal      = cnt * (MM_PAR_BASCULE / 2.0);
    pluieEnCours = true;
    digitalWrite(PIN_SIGNAL, HIGH);

    Serial.print("#");
    Serial.print(cnt);
    Serial.print(" | ");
    Serial.print(sens ? ">>> RETOUR" : "<<< DEPART");
    Serial.print(" | ");
    if (cnt <= 1) { Serial.print("------"); }
    else if (iMs < 1000) { Serial.print(iMs); Serial.print("ms"); }
    else { Serial.print(iMs / 1000.0, 1); Serial.print("s"); }
    Serial.print(" | ");
    Serial.print(mmTotal, 3);
    Serial.print("mm | ");
    Serial.print(mmhActuel, 1);
    Serial.print("mm/h | ");
    Serial.print(intensitePluie(mmhActuel));
    Serial.print(" | Niv:");
    Serial.println(niveauRaw);
  }

  // ---- Reset compteur 1 minute ----------------------------
  if (maintenant - t1min >= 60000UL) {
    noInterrupts();
    bascules1min = 0;
    interrupts();
    t1min = maintenant;
  }

  // ---- Fin de pluie ---------------------------------------
  if (pluieEnCours && (maintenant - dernierTemps > TIMEOUT_PLUIE)) {
    pluieEnCours = false;
    digitalWrite(PIN_SIGNAL, LOW);
    Serial.println(">>> FIN DE PLUIE | Total : " + String(mmTotal, 3) + " mm");
  }
}