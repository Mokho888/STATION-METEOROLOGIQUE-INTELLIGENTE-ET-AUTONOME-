// ╔══════════════════════════════════════════════════════════╗
// ║  STATION MÉTÉO — Interface Locale avec Icônes            ║
// ║  Binôme 4 — Partie 4 (GPS, Énergie, Interface)           ║
// ╚══════════════════════════════════════════════════════════╝

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// ─── Broches (inchangées) ────────────────────────────────
#define TFT_DC    2
#define TFT_CS    15
#define TFT_RST   4
#define TOUCH_CS  21
#define TOUCH_IRQ 22

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ─── Calibration tactile (inchangée) ─────────────────────
#define X_MIN 370
#define X_MAX 3800
#define Y_MIN 260
#define Y_MAX 3600

// ════════════════════════════════════════════════════════════
//  VARIABLES PARTAGÉES — CHAQUE BINÔME REMPLIT SA PARTIE
// ════════════════════════════════════════════════════════════

// ── PARTIE 1 — BME280 (Température / Humidité / Pression) ──
// Partie 1 met à jour ces 3 variables dans leur loop() :
//   temperature = bme.readTemperature();
//   humidite    = bme.readHumidity();
//   pression    = bme.readPressure() / 100.0F;
float temperature = 24.5;
float humidite    = 45.0;
float pression    = 1013.0;

// ── PARTIE 1 — RTC DS3231 (Date / Heure) ──────────────────
// Partie 1 met à jour ces 2 variables dans leur loop() :
//   DateTime now = rtc.now();
//   sprintf(dateStr,  "%02d/%02d/%02d", now.day(), now.month(), now.year() % 100);
//   sprintf(heureStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
char dateStr[12]  = "17/04/26";
char heureStr[12] = "19:30:00";

// ── PARTIE 2 — Anémomètre / Girouette ─────────────────────
// Partie 2 met à jour :
//   vitesseVent  = (valeur calculée) en km/h
//   directionVent = "N", "N-E", "E", "S-E", "S", "S-O", "O", "N-O"
float vitesseVent       = 18.2;
char  directionVent[8]  = "N-E";

// ── PARTIE 3 — Pluviomètre ────────────────────────────────
// Partie 3 met à jour :
//   pluviometrie = (valeur calculée) en mm/h
float pluviometrie = 2.5;

// ── PARTIE 4 — GPS NEO-6M ────────────────────────────────
// À remplacer par :
//   latGPS = gps.location.lat();
//   lonGPS = gps.location.lng();
//   sprintf(altGPS, "%.0f", gps.altitude.meters());
float latGPS      = 36.7538;
float lonGPS      =  3.0588;
char  altGPS[10]  = "---";

// ════════════════════════════════════════════════════════════

// ─── Palette de couleurs ─────────────────────────────────
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

int pageActuelle = 1;

// ════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  ts.begin();
  ts.setRotation(1);
  afficherPage(pageActuelle);
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, X_MAX, X_MIN, 0, 320);
    int y = map(p.y, Y_MAX, Y_MIN, 0, 240);
    if (y > 200) {
      if (x < 110) {
        pageActuelle = (pageActuelle == 1) ? 3 : pageActuelle - 1;
        afficherPage(pageActuelle);
        delay(300);
      } else if (x > 210) {
        pageActuelle = (pageActuelle == 3) ? 1 : pageActuelle + 1;
        afficherPage(pageActuelle);
        delay(300);
      }
    }
  }
  // Plus tard : lire les capteurs ici et rappeler afficherPage()
}

// ════════════════════════════════════════════════════════
//  ICÔNES
// ════════════════════════════════════════════════════════

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

// ════════════════════════════════════════════════════════
//  COMPOSANTS D'INTERFACE
// ════════════════════════════════════════════════════════

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

// Carte pleine largeur
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

// Carte GPS dédiée
void dessinerCarteGPS(int y_start, float lat, float lon, const char* alt) {
  int x = 5, y = y_start, w = 310, h = 95;
  uint16_t col = ILI9341_CYAN;
  tft.fillRoundRect(x, y, w, h, 8, C_CARD);
  tft.drawRoundRect(x, y, w, h, 8, col);
  tft.fillRoundRect(x+8, y, w-16, 3, 1, col);
  drawIcon(9, x+30, y + h/2, col);
  tft.drawLine(x+58, y+10, x+58, y+h-10, C_DKGREY);
  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(x+65, y+8);
  tft.print("Localisation GPS");
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
  tft.print(" m");
}

// ════════════════════════════════════════════════════════
//  PAGES
// ════════════════════════════════════════════════════════

void afficherPage(int num) {
  tft.fillScreen(C_BG);
  dessinerBoutons();

  char buf[16];

  // ── PAGE 1 : CLIMAT (BME280 — Partie 1) ──────────────
  //
  //  ┌─────────────────────┬───────────────────┐  y=36  h=78
  //  │ 🌡  Température      │ 💧 Humidité        │
  //  ├────────────────────────────────────────  ┤  y=118 h=78
  //  │ ⚙️  Pression  (pleine largeur)            │
  //  └──────────────────────────────────────────┘  y=196
  if (num == 1) {
    dessinerTitre("CLIMAT", C_TITLE_P1);

    dtostrf(temperature, 4, 1, buf);
    dessinerCarte(5,   36, 155, 78, "Temperature", buf, "deg C", C_ORANGE,     0);

    dtostrf(humidite, 4, 1, buf);
    dessinerCarte(163, 36, 152, 78, "Humidite",    buf, "%",     ILI9341_CYAN, 1);

    dtostrf(pression, 6, 1, buf);
    dessinerCarte(5,  118, 310, 78, "Pression",    buf, "hPa",   ILI9341_GREEN, 2);
  }

  // ── PAGE 2 : VENT & PLUIE (Parties 2 & 3) ───────────
  else if (num == 2) {
    dessinerTitre("VENT & PLUIE", C_TITLE_P2);

    dtostrf(vitesseVent, 4, 1, buf);
    dessinerCarte(5,   36, 155, 78, "Vit. Vent", buf,          "km/h", C_ORANGE,      4);
    dessinerCarte(163, 36, 152, 78, "Direction", directionVent, "",    ILI9341_WHITE, 5);

    dtostrf(pluviometrie, 4, 1, buf);
    dessinerCartePleine(118, ILI9341_BLUE, 6,
                        "Pluviometrie",
                        buf,
                        "mm / heure");
  }

  // ── PAGE 3 : GPS & TEMPS (Partie 4 + RTC Partie 1) ──
  //
  //  ┌───────────────────┬─────────────────────┐  y=36  h=62
  //  │ 📅 Date: 17/04/26  │ 🕐 Heure: 19:30:00  │
  //  ├────────────────────────────────────────  ┤  y=102 h=95
  //  │ 📍 Lat: 36.7538 N                         │
  //  │    Lon:  3.0588 E                         │
  //  │    Alt: --- m                             │
  //  └──────────────────────────────────────────┘  y=197
  else if (num == 3) {
    dessinerTitre("GPS & TEMPS", C_TITLE_P3);

    // Date w=153 → zone texte = 153-57 = 96px → "17/04/26" (8×12px) tient
    dessinerCarte(5,   36, 153, 62, "Date",  dateStr,  "", ILI9341_WHITE,  7);
    dessinerCarte(162, 36, 153, 62, "Heure", heureStr, "", ILI9341_YELLOW, 8);

    dessinerCarteGPS(102, latGPS, lonGPS, altGPS);
  }
}
