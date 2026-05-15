// ============================================================
// app_theme.dart — Thème clair bleu ciel (MeteoConnect)
// Inspiré du design fourni : fond bleu ciel, cartes blanches,
// texte bleu profond, icônes colorées par métrique.
// ============================================================

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class AppTheme {
  // ─── Couleurs principales ────────────────────────────────
  static const Color primary      = Color(0xFF1565C0); // bleu profond
  static const Color primaryLight = Color(0xFF42A5F5); // bleu clair
  static const Color secondary    = Color(0xFF0288D1); // bleu océan
  static const Color accent       = Color(0xFFFF8F00); // ambre/orange

  // ─── Couleurs par métrique ───────────────────────────────
  static const Color tempColor    = Color(0xFFE64A19); // rouge-orange (chaleur)
  static const Color humColor     = Color(0xFF1565C0); // bleu (humidité)
  static const Color pressColor   = Color(0xFF00838F); // teal (pression)
  static const Color windColor    = Color(0xFF6A1B9A); // violet (vent)
  static const Color rainColor    = Color(0xFF0277BD); // bleu nuit (pluie)
  static const Color waterColor   = Color(0xFF00695C); // vert sombre (eau)
  static const Color gpsColor     = Color(0xFF2E7D32); // vert (GPS)
  static const Color altColor     = Color(0xFF4527A0); // indigo (altitude)

  // ─── Fonds ──────────────────────────────────────────────
  static const Color bgTop        = Color(0xFF90CAF9); // bleu ciel clair
  static const Color bgMid        = Color(0xFFBBDEFB); // bleu très pâle
  static const Color bgBottom     = Color(0xFFE3F2FD); // quasi-blanc bleuté
  static const Color bgCard       = Color(0xFFFFFFFF); // blanc pur (cartes)
  static const Color bgCardLight  = Color(0xFFF0F8FF); // alice blue (hover)

  // ─── Textes ──────────────────────────────────────────────
  static const Color textPrimary   = Color(0xFF0D47A1); // bleu très profond
  static const Color textSecondary = Color(0xFF1976D2); // bleu moyen
  static const Color textMuted     = Color(0xFF90CAF9); // bleu ciel (labels)

  // ─── Dégradés ────────────────────────────────────────────
  static const LinearGradient bgGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0xFF90CAF9), Color(0xFFBBDEFB), Color(0xFFE3F2FD)],
    stops: [0.0, 0.5, 1.0],
  );

  static const LinearGradient tempGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0xFFFF7043), Color(0xFFFF8F00)],
  );

  static const LinearGradient humGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0xFF1565C0), Color(0xFF42A5F5)],
  );

  static const LinearGradient headerGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0xFF1565C0), Color(0xFF0288D1)],
  );

  // ─── Décoration de carte standard ────────────────────────
  static BoxDecoration cardDecoration({Color? borderColor}) => BoxDecoration(
        color: bgCard,
        borderRadius: BorderRadius.circular(20),
        boxShadow: [
          BoxShadow(
            color: primary.withValues(alpha: 0.1),
            blurRadius: 16,
            offset: const Offset(0, 4),
          ),
        ],
        border: Border.all(
          color: (borderColor ?? primary).withValues(alpha: 0.15),
          width: 1.5,
        ),
      );

  // ─── ThemeData Flutter (thème clair) ─────────────────────
  static ThemeData get light {
    return ThemeData(
      brightness: Brightness.light,
      scaffoldBackgroundColor: bgBottom,
      colorScheme: const ColorScheme.light(
        primary: primary,
        secondary: secondary,
        surface: bgCard,
      ),
      textTheme: GoogleFonts.outfitTextTheme(
        const TextTheme(
          displayLarge:  TextStyle(color: textPrimary),
          displayMedium: TextStyle(color: textPrimary),
          bodyLarge:     TextStyle(color: textPrimary),
          bodyMedium:    TextStyle(color: textSecondary),
        ),
      ),
      appBarTheme: AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        titleTextStyle: GoogleFonts.outfit(
          color: textPrimary,
          fontSize: 20,
          fontWeight: FontWeight.w600,
        ),
        iconTheme: const IconThemeData(color: textPrimary),
      ),
      bottomNavigationBarTheme: const BottomNavigationBarThemeData(
        backgroundColor: bgCard,
        selectedItemColor: primary,
        unselectedItemColor: textSecondary,
        type: BottomNavigationBarType.fixed,
      ),
    );
  }
}
