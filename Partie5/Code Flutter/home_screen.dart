// ============================================================
// home_screen.dart — Écran principal "Accueil" (MeteoConnect)
// Organisation : Header | Statut + GPS | Conditions Actuelles
//                (Pluie + Température) | Vent + Pression + Humidité
// ============================================================

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:intl/intl.dart';
import '../services/weather_service.dart';
import '../theme/app_theme.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen>
    with SingleTickerProviderStateMixin {
  // ─── Service météo (lecture Firebase RTDB) ────────────────
  final WeatherService _service = WeatherService();

  // ─── Variables de chaque capteur (toutes nullables) ───────
  double? _temperature;    // Température en °C
  double? _humidity;       // Humidité relative en %
  double? _pressure;       // Pression atmosphérique en hPa
  double? _windSpeed;      // Vitesse du vent en km/h
  String? _windDirection;  // Direction du vent (ex. N-E, S-O…)
  double? _rain;           // Cumul de pluie en mm
  double? _latitude;       // Latitude GPS en degrés décimaux
  double? _longitude;      // Longitude GPS en degrés décimaux


  // ─── État de connexion Firebase ───────────────────────────
  bool _isConnected = false;
  DateTime? _lastUpdate; // Horodatage de la dernière donnée reçue

  // ─── Horloge temps réel (mise à jour chaque seconde) ──────
  late Timer _clockTimer;
  DateTime _now = DateTime.now();

  // ─── Animation de pulsation du badge de statut ────────────
  late AnimationController _pulseCtrl;
  late Animation<double> _pulseAnim;

  @override
  void initState() {
    super.initState();

    // Horloge : mise à jour chaque seconde pour affichage temps réel
    _clockTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (mounted) setState(() => _now = DateTime.now());
    });

    // Animation de pulsation sur le badge de statut (connecté = pulse)
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat(reverse: true);
    _pulseAnim = Tween<double>(begin: 0.97, end: 1.0).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );

    // Abonnement au stream Firebase (onValue du nœud /weather/)
    _service.weatherStream.listen((data) {
      if (!mounted) return;
      setState(() {
        _temperature   = data['temperature']   as double?; // °C
        _humidity      = data['humidity']      as double?; // %
        _pressure      = data['pressure']      as double?; // hPa
        _windSpeed     = data['windSpeed']     as double?; // km/h
        _windDirection = data['windDirection'] as String?; // cardinal
        _rain          = data['rain']          as double?; // mm
        _latitude      = data['latitude']      as double?; // °
        _longitude     = data['longitude']     as double?; // °

        // Connecté si au moins température ET humidité disponibles
        _isConnected   = _temperature != null && _humidity != null;
        if (_isConnected) _lastUpdate = DateTime.now();
      });

      // Enregistrement historique conditionnel toutes les 30 min
      _service.maybeRecord(data);
    }, onError: (_) {
      if (!mounted) return;
      setState(() => _isConnected = false);
    });
  }

  @override
  void dispose() {
    _clockTimer.cancel();
    _pulseCtrl.dispose();
    super.dispose();
  }

  // ─────────────────────────────────────────────────────────
  // BUILD PRINCIPAL
  // ─────────────────────────────────────────────────────────
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(gradient: AppTheme.bgGradient),
        child: SafeArea(
          child: Column(
            children: [
              // ── En-tête ──────────────────────────────────
              _buildHeader(),

              // ── Contenu scrollable ────────────────────────
              Expanded(
                child: SingleChildScrollView(
                  physics: const BouncingScrollPhysics(),
                  padding: const EdgeInsets.fromLTRB(14, 8, 14, 16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      // Ligne : Statut Firebase + GPS
                      _buildStatusAndGpsRow(),
                      const SizedBox(height: 12),

                      // Titre de section "Conditions Actuelles"
                      _buildSectionLabel(),
                      const SizedBox(height: 10),

                      // Ligne 1 : Pluie + Température
                      _buildMainRow(),
                      const SizedBox(height: 10),

                      // Ligne 2 : Vent + Pression + Humidité
                      _buildSecondaryRow(),
                      const SizedBox(height: 14),

                      // Pied : Dernière mise à jour
                      _buildLastUpdate(),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  // ─────────────────────────────────────────────────────────
  // EN-TÊTE : Logo | Titre | Date + Heure
  // ─────────────────────────────────────────────────────────
  Widget _buildHeader() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.45),
        border: Border(
          bottom: BorderSide(
            color: AppTheme.primary.withValues(alpha: 0.12),
          ),
        ),
      ),
      child: Row(
        children: [
          // ── Logo circulaire ───────────────────────────────
          Container(
            width: 58,
            height: 58,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: Colors.white,
              boxShadow: [
                BoxShadow(
                  color: AppTheme.primary.withValues(alpha: 0.2),
                  blurRadius: 12,
                  spreadRadius: 2,
                ),
              ],
            ),
            child: ClipOval(
              child: Image.asset(
                'assets/images/logo.png',
                fit: BoxFit.cover,
              ),
            ),
          ),
          const SizedBox(width: 10),

          // ── Nom de l'application ──────────────────────────
          Expanded(
            child: Text(
              'Station Météo\nIntelligente',
              style: GoogleFonts.outfit(
                color: AppTheme.textPrimary,
                fontSize: 16,
                fontWeight: FontWeight.w800,
                height: 1.3,
              ),
            ),
          ),

          // ── Date + Heure en direct ────────────────────────
          Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              // Date : jour Mois année
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.calendar_today_rounded,
                      size: 13, color: AppTheme.primary),
                  const SizedBox(width: 5),
                  Text(
                    'Date: ${DateFormat('dd MMMM yyyy', 'fr_FR').format(_now)}',
                    style: GoogleFonts.outfit(
                      color: AppTheme.textPrimary,
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 4),
              // Heure : HH:mm:ss actualisé chaque seconde
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.access_time_rounded,
                      size: 13, color: AppTheme.secondary),
                  const SizedBox(width: 5),
                  Text(
                    'Heure: ${DateFormat('HH:mm:ss').format(_now)}',
                    style: GoogleFonts.outfit(
                      color: AppTheme.textSecondary,
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ],
      ),
    );
  }

  // ─────────────────────────────────────────────────────────
  // LIGNE : Statut Firebase (gauche) + GPS (droite)
  // ─────────────────────────────────────────────────────────
  Widget _buildStatusAndGpsRow() {
    final statusColor = _isConnected
        ? const Color(0xFF1565C0)
        : const Color(0xFFC62828);
    final statusBg = _isConnected
        ? Colors.white.withValues(alpha: 0.7)
        : const Color(0xFFFFEBEE);

    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // ── Badge statut avec pulsation ───────────────────
        Expanded(
          child: AnimatedBuilder(
            animation: _pulseAnim,
            builder: (_, __) => Transform.scale(
              scale: _isConnected ? _pulseAnim.value : 1.0,
              child: Container(
                padding: const EdgeInsets.symmetric(
                    horizontal: 14, vertical: 10),
                decoration: BoxDecoration(
                  color: statusBg,
                  borderRadius: BorderRadius.circular(24),
                  border: Border.all(
                      color: statusColor.withValues(alpha: 0.3)),
                  boxShadow: [
                    BoxShadow(
                      color: statusColor.withValues(alpha: 0.08),
                      blurRadius: 8,
                      offset: const Offset(0, 2),
                    ),
                  ],
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    // Point lumineux (connecté / déconnecté)
                    Container(
                      width: 10,
                      height: 10,
                      decoration: BoxDecoration(
                        color: statusColor,
                        shape: BoxShape.circle,
                        boxShadow: [
                          BoxShadow(
                            color: statusColor.withValues(alpha: 0.4),
                            blurRadius: 6,
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 10),
                    Text(
                      _isConnected
                          ? 'statut: connecté'
                          : 'statut: en attente...',
                      style: GoogleFonts.outfit(
                        color: statusColor,
                        fontSize: 13,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
        const SizedBox(width: 10),

        // ── Localisation GPS (lat / lon) ──────────────────
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: 0.7),
            borderRadius: BorderRadius.circular(16),
            border: Border.all(
                color: AppTheme.gpsColor.withValues(alpha: 0.25)),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.location_on_rounded,
                  color: AppTheme.gpsColor, size: 20),
              const SizedBox(width: 8),
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Localisation',
                    style: GoogleFonts.outfit(
                      color: AppTheme.gpsColor,
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  // Latitude GPS
                  Text(
                    'Lat: ${_latitude != null ? _latitude!.toStringAsFixed(4) + '°' : '--'}',
                    style: GoogleFonts.outfit(
                      color: AppTheme.gpsColor,
                      fontSize: 11,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                  // Longitude GPS
                  Text(
                    'Lon: ${_longitude != null ? _longitude!.toStringAsFixed(4) + '°' : '--'}',
                    style: GoogleFonts.outfit(
                      color: AppTheme.gpsColor,
                      fontSize: 11,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }

  // ─────────────────────────────────────────────────────────
  // LABEL DE SECTION "Conditions Actuelles"
  // ─────────────────────────────────────────────────────────
  Widget _buildSectionLabel() {
    return Row(
      children: [
        Icon(Icons.thermostat_rounded,
            color: AppTheme.tempColor, size: 22),
        const SizedBox(width: 8),
        Text(
          'Conditions Actuelles :',
          style: GoogleFonts.outfit(
            color: AppTheme.textPrimary,
            fontSize: 15,
            fontWeight: FontWeight.w800,
          ),
        ),
      ],
    );
  }

  // ─────────────────────────────────────────────────────────
  // LIGNE PRINCIPALE : Pluie | Température
  // ─────────────────────────────────────────────────────────
  Widget _buildMainRow() {
    return IntrinsicHeight(
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // ── Pluie ─────────────────────────────────────────
          Expanded(child: _buildRainCard()),
          const SizedBox(width: 10),
          // ── Température ───────────────────────────────────
          Expanded(child: _buildTempCard()),
        ],
      ),
    );
  }

  // Carte Pluie — cumul en mm mesuré par le pluviomètre
  Widget _buildRainCard() {
    return _MeteoCard(
      borderColor: AppTheme.rainColor,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          // Icône nuage + pluie
          Icon(Icons.grain_rounded,
              color: AppTheme.rainColor, size: 44),
          const SizedBox(height: 10),
          Text(
            'Pluie',
            style: GoogleFonts.outfit(
              color: AppTheme.rainColor,
              fontSize: 17,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 8),
          // Valeur en mm (-- si capteur absent)
          Text(
            _rain != null ? '${_rain!.toStringAsFixed(1)} mm' : '--',
            style: GoogleFonts.outfit(
              color: AppTheme.rainColor,
              fontSize: 26,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 4),
          Icon(Icons.water_drop_rounded,
              color: AppTheme.rainColor.withValues(alpha: 0.35), size: 22),
        ],
      ),
    );
  }

  // Carte Température — mesure en °C avec label qualitatif
  Widget _buildTempCard() {
    return _MeteoCard(
      borderColor: AppTheme.tempColor,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          // Icône soleil + thermomètre
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.wb_sunny_rounded,
                  color: Colors.amber, size: 32),
              const SizedBox(width: 4),
              Icon(Icons.thermostat_rounded,
                  color: AppTheme.tempColor, size: 32),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            'Température',
            style: GoogleFonts.outfit(
              color: AppTheme.tempColor,
              fontSize: 17,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 8),
          // Valeur en °C (-- si capteur absent)
          Text(
            _temperature != null
                ? '${_temperature!.toStringAsFixed(1)} °C'
                : '--',
            style: GoogleFonts.outfit(
              color: AppTheme.tempColor,
              fontSize: 26,
              fontWeight: FontWeight.w800,
            ),
          ),
          // Label qualitatif (Froid / Agréable / Chaud…)
          if (_temperature != null)
            Padding(
              padding: const EdgeInsets.only(top: 4),
              child: Text(
                _tempLabel(_temperature!),
                style: GoogleFonts.outfit(
                  color: AppTheme.tempColor.withValues(alpha: 0.7),
                  fontSize: 12,
                ),
              ),
            ),
        ],
      ),
    );
  }

  // ─────────────────────────────────────────────────────────
  // LIGNE SECONDAIRE : Vent | Pression | Humidité
  // ─────────────────────────────────────────────────────────
  Widget _buildSecondaryRow() {
    return IntrinsicHeight(
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // ── Vent ──────────────────────────────────────────
          Expanded(child: _buildWindCard()),
          const SizedBox(width: 8),
          // ── Pression ──────────────────────────────────────
          Expanded(child: _buildPressureCard()),
          const SizedBox(width: 8),
          // ── Humidité ──────────────────────────────────────
          Expanded(child: _buildHumidityCard()),
        ],
      ),
    );
  }

  // Carte Vent — vitesse (km/h) + direction cardinale
  Widget _buildWindCard() {
    return _MeteoCard(
      borderColor: AppTheme.windColor,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.air_rounded, color: AppTheme.windColor, size: 30),
          const SizedBox(height: 6),
          Text(
            'Vent',
            style: GoogleFonts.outfit(
              color: AppTheme.windColor,
              fontSize: 13,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 6),
          // Vitesse du vent en km/h
          Text(
            _windSpeed != null
                ? '${_windSpeed!.toStringAsFixed(1)} Km/h'
                : '--',
            style: GoogleFonts.outfit(
              color: AppTheme.windColor,
              fontSize: 16,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 4),
          // Direction cardinale du vent (ex. N-E, S-O…)
          Text(
            'Direction: ${_windDirection ?? '--'}',
            style: GoogleFonts.outfit(
              color: AppTheme.windColor.withValues(alpha: 0.75),
              fontSize: 11,
              fontWeight: FontWeight.w500,
            ),
          ),
        ],
      ),
    );
  }

  // Carte Pression — pression atm. en hPa avec label (Basse/Normale/Haute)
  Widget _buildPressureCard() {
    return _MeteoCard(
      borderColor: AppTheme.pressColor,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.speed_rounded, color: AppTheme.pressColor, size: 30),
          const SizedBox(height: 6),
          Text(
            'Pression',
            style: GoogleFonts.outfit(
              color: AppTheme.pressColor,
              fontSize: 13,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 6),
          // Valeur en hPa
          Text(
            _pressure != null
                ? '${_pressure!.toStringAsFixed(0)} hPa'
                : '--',
            style: GoogleFonts.outfit(
              color: AppTheme.pressColor,
              fontSize: 16,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 4),
          // Label qualitatif (Basse / Normale / Haute / Très haute)
          Text(
            _pressure != null ? _pressLabel(_pressure!) : 'N/A',
            style: GoogleFonts.outfit(
              color: AppTheme.pressColor.withValues(alpha: 0.75),
              fontSize: 11,
            ),
          ),
        ],
      ),
    );
  }

  // Carte Humidité — humidité relative en % avec label qualitatif
  Widget _buildHumidityCard() {
    return _MeteoCard(
      borderColor: AppTheme.humColor,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.water_drop_rounded, color: AppTheme.humColor, size: 30),
          const SizedBox(height: 6),
          Text(
            'Humidité',
            style: GoogleFonts.outfit(
              color: AppTheme.humColor,
              fontSize: 13,
              fontWeight: FontWeight.w700,
            ),
          ),
          const SizedBox(height: 6),
          // Valeur en pourcentage
          Text(
            _humidity != null
                ? '${_humidity!.toStringAsFixed(1)} %'
                : '--',
            style: GoogleFonts.outfit(
              color: AppTheme.humColor,
              fontSize: 16,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(height: 4),
          // Label qualitatif (Très sec / Sec / Confortable / Humide…)
          Text(
            _humidity != null ? _humLabel(_humidity!) : 'N/A',
            style: GoogleFonts.outfit(
              color: AppTheme.humColor.withValues(alpha: 0.75),
              fontSize: 11,
            ),
          ),
        ],
      ),
    );
  }

  // ─────────────────────────────────────────────────────────
  // PIED : Dernière mise à jour Firebase
  // ─────────────────────────────────────────────────────────
  Widget _buildLastUpdate() {
    return Center(
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        decoration: BoxDecoration(
          color: Colors.white.withValues(alpha: 0.6),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
              color: AppTheme.primary.withValues(alpha: 0.15)),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.update_rounded,
                size: 14, color: AppTheme.textSecondary),
            const SizedBox(width: 6),
            Text(
              _lastUpdate != null
                  ? 'Dernière mise à jour à : ${DateFormat('HH:mm:ss').format(_lastUpdate!)}'
                  : 'En attente de données...',
              style: GoogleFonts.outfit(
                  color: AppTheme.textSecondary, fontSize: 12),
            ),
          ],
        ),
      ),
    );
  }

  // ─────────────────────────────────────────────────────────
  // LABELS QUALITATIFS
  // ─────────────────────────────────────────────────────────

  /// Étiquette qualitative pour la température en °C
  String _tempLabel(double t) {
    if (t < 0)  return 'Gel';
    if (t < 10) return 'Froid';
    if (t < 18) return 'Frais';
    if (t < 25) return 'Agréable';
    if (t < 30) return 'Chaud';
    return 'Très chaud';
  }

  /// Étiquette qualitative pour l'humidité en %
  String _humLabel(double h) {
    if (h < 30) return 'Très sec';
    if (h < 50) return 'Sec';
    if (h < 65) return 'Confortable';
    if (h < 80) return 'Humide';
    return 'Très humide';
  }

  /// Étiquette qualitative pour la pression en hPa
  String _pressLabel(double p) {
    if (p < 1000) return 'Basse';
    if (p < 1013) return 'Normale';
    if (p < 1025) return 'Haute';
    return 'Très haute';
  }
}

// ─────────────────────────────────────────────────────────
// WIDGET RÉUTILISABLE : Carte météo standard
// Fond blanc, coins arrondis, bordure colorée par métrique
// ─────────────────────────────────────────────────────────
class _MeteoCard extends StatelessWidget {
  final Widget child;
  final Color borderColor;

  const _MeteoCard({
    required this.child,
    required this.borderColor,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.85),
        borderRadius: BorderRadius.circular(22),
        border: Border.all(
          color: borderColor.withValues(alpha: 0.22),
          width: 1.5,
        ),
        boxShadow: [
          BoxShadow(
            color: borderColor.withValues(alpha: 0.1),
            blurRadius: 14,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: child,
    );
  }
}
