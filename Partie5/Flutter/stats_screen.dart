// ============================================================
// stats_screen.dart — Écran Historique (thème clair bleu ciel)
// Affiche les statistiques et l'historique des mesures.
// ============================================================

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:intl/intl.dart';
import '../models/weather_record.dart';
import '../services/weather_service.dart';
import '../services/excel_export_service.dart';
import '../theme/app_theme.dart';

class StatsScreen extends StatefulWidget {
  const StatsScreen({super.key});

  @override
  State<StatsScreen> createState() => _StatsScreenState();
}

class _StatsScreenState extends State<StatsScreen> {
  final WeatherService _service = WeatherService();
  List<WeatherRecord> _allRecords = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    final records = await _service.getHistory();
    if (mounted) {
      setState(() {
        _allRecords = records;
        _loading = false;
      });
    }
  }

  /// Exporte tous les enregistrements vers un fichier Excel.
  Future<void> _exportExcel() async {
    if (_allRecords.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Aucune donnée à exporter.',
            style: GoogleFonts.outfit(),
          ),
          backgroundColor: AppTheme.primary,
          behavior: SnackBarBehavior.floating,
        ),
      );
      return;
    }
    try {
      await ExcelExportService.exportToExcel(_allRecords);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Row(
            children: [
              const Icon(
                Icons.check_circle_rounded,
                color: Colors.white,
                size: 18,
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  'Export Excel de ${_allRecords.length} enregistrements lancé !',
                  style: GoogleFonts.outfit(color: Colors.white),
                ),
              ),
            ],
          ),
          backgroundColor: const Color(0xFF2E7D32),
          behavior: SnackBarBehavior.floating,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        ),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Erreur lors de l\'export : $e',
            style: GoogleFonts.outfit(color: Colors.white),
          ),
          backgroundColor: Colors.red,
          behavior: SnackBarBehavior.floating,
        ),
      );
    }
  }

  List<WeatherRecord> _periodRecords(Duration d) {
    final cutoff = DateTime.now().subtract(d);
    return _allRecords.where((r) => r.timestamp.isAfter(cutoff)).toList();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(gradient: AppTheme.bgGradient),
        child: SafeArea(
          child: _loading
              ? Center(
                  child: CircularProgressIndicator(
                    color: AppTheme.primary,
                    strokeWidth: 2.5,
                  ),
                )
              : CustomScrollView(
                  physics: const BouncingScrollPhysics(),
                  slivers: [
                    SliverToBoxAdapter(child: _buildHeader()),
                    SliverToBoxAdapter(child: _buildSummaryCard()),
                    SliverToBoxAdapter(
                      child: _buildPeriodStats(
                        '24 heures',
                        _periodRecords(const Duration(hours: 24)),
                      ),
                    ),
                    SliverToBoxAdapter(
                      child: _buildPeriodStats(
                        '7 jours',
                        _periodRecords(const Duration(days: 7)),
                      ),
                    ),
                    SliverToBoxAdapter(
                      child: _buildPeriodStats(
                        '30 jours',
                        _periodRecords(const Duration(days: 30)),
                      ),
                    ),
                    SliverToBoxAdapter(child: _buildHistory()),
                    const SliverToBoxAdapter(child: SizedBox(height: 28)),
                  ],
                ),
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 10),
      child: Row(
        children: [
          Container(
            width: 42,
            height: 42,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: AppTheme.bgCard,
              boxShadow: [
                BoxShadow(
                  color: AppTheme.primary.withValues(alpha: 0.2),
                  blurRadius: 10,
                ),
              ],
            ),
            child: ClipOval(
              child: Image.asset('assets/images/logo.png', fit: BoxFit.cover),
            ),
          ),
          const SizedBox(width: 12),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Historique',
                style: GoogleFonts.outfit(
                  color: AppTheme.textPrimary,
                  fontSize: 22,
                  fontWeight: FontWeight.w700,
                ),
              ),
              Text(
                '${_allRecords.length} enregistrements · toutes les 30 min',
                style: GoogleFonts.outfit(
                  color: AppTheme.textSecondary,
                  fontSize: 11,
                ),
              ),
            ],
          ),
          const Spacer(),
          // Bouton Export Excel
          Container(
            decoration: AppTheme.cardDecoration(
              borderColor: const Color(0xFF2E7D32),
            ),
            child: IconButton(
              onPressed: _allRecords.isEmpty ? null : _exportExcel,
              icon: const Icon(
                Icons.table_chart_rounded,
                color: Color(0xFF2E7D32),
              ),
              tooltip: 'Exporter vers Excel',
            ),
          ),
          const SizedBox(width: 8),
          // Bouton Rafraîchir
          Container(
            decoration: AppTheme.cardDecoration(),
            child: IconButton(
              onPressed: _load,
              icon: Icon(Icons.refresh_rounded, color: AppTheme.primary),
              tooltip: 'Rafraîchir',
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSummaryCard() {
    if (_allRecords.isEmpty) {
      return Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Container(
          padding: const EdgeInsets.all(28),
          decoration: AppTheme.cardDecoration(),
          child: Column(
            children: [
              Icon(
                Icons.history_toggle_off_rounded,
                size: 60,
                color: AppTheme.primary.withValues(alpha: 0.3),
              ),
              const SizedBox(height: 16),
              Text(
                'Aucune donnée enregistrée\npour le moment.',
                textAlign: TextAlign.center,
                style: GoogleFonts.outfit(
                  color: AppTheme.textSecondary,
                  fontSize: 15,
                ),
              ),
              const SizedBox(height: 8),
              Text(
                'Les données seront enregistrées\nautomatiquement toutes les 30 minutes.',
                textAlign: TextAlign.center,
                style: GoogleFonts.outfit(
                  color: AppTheme.textSecondary.withValues(alpha: 0.6),
                  fontSize: 12,
                ),
              ),
            ],
          ),
        ),
      );
    }

    final last = _allRecords.last;
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
      child: Container(
        padding: const EdgeInsets.all(18),
        decoration: AppTheme.cardDecoration(borderColor: AppTheme.primary),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(
                  Icons.history_rounded,
                  size: 16,
                  color: AppTheme.textSecondary,
                ),
                const SizedBox(width: 6),
                Text(
                  'Dernière mesure',
                  style: GoogleFonts.outfit(
                    color: AppTheme.textSecondary,
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const Spacer(),
                Text(
                  DateFormat(
                    'dd MMM yyyy HH:mm',
                    'fr_FR',
                  ).format(last.timestamp),
                  style: GoogleFonts.outfit(
                    color: AppTheme.primary,
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _valueBadge(
                  '${last.temperature.toStringAsFixed(1)}°C',
                  AppTheme.tempColor,
                  Icons.thermostat_rounded,
                ),
                _valueBadge(
                  '${last.humidity.toStringAsFixed(1)}%',
                  AppTheme.humColor,
                  Icons.water_drop_rounded,
                ),
                if (last.pressure != null)
                  _valueBadge(
                    '${last.pressure!.toStringAsFixed(0)} hPa',
                    AppTheme.pressColor,
                    Icons.speed_rounded,
                  ),
                if (last.windSpeed != null)
                  _valueBadge(
                    '${last.windSpeed!.toStringAsFixed(1)} km/h',
                    AppTheme.windColor,
                    Icons.air_rounded,
                  ),
                if (last.rain != null)
                  _valueBadge(
                    '${last.rain!.toStringAsFixed(1)} mm',
                    AppTheme.rainColor,
                    Icons.grain_rounded,
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _valueBadge(String value, Color color, IconData icon) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withValues(alpha: 0.25)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 13, color: color),
          const SizedBox(width: 4),
          Text(
            value,
            style: TextStyle(
              color: color,
              fontWeight: FontWeight.w700,
              fontSize: 12,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildPeriodStats(String label, List<WeatherRecord> records) {
    if (records.isEmpty) return const SizedBox();

    final tempStats = _service.computeStats(records, (r) => r.temperature);
    final humStats = _service.computeStats(records, (r) => r.humidity);

    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 4),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 4, bottom: 8),
            child: Row(
              children: [
                Container(
                  width: 3,
                  height: 14,
                  decoration: BoxDecoration(
                    color: AppTheme.primary,
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  'Période : $label  (${records.length} pts)',
                  style: GoogleFonts.outfit(
                    color: AppTheme.textSecondary,
                    fontSize: 13,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ),
          ),
          Row(
            children: [
              _statCard(
                'T° Min',
                '${tempStats['min']!.toStringAsFixed(1)}°C',
                Icons.arrow_downward_rounded,
                const Color(0xFF1976D2),
                AppTheme.tempColor,
              ),
              const SizedBox(width: 8),
              _statCard(
                'T° Max',
                '${tempStats['max']!.toStringAsFixed(1)}°C',
                Icons.arrow_upward_rounded,
                const Color(0xFFE53935),
                AppTheme.tempColor,
              ),
              const SizedBox(width: 8),
              _statCard(
                'T° Moy',
                '${tempStats['avg']!.toStringAsFixed(1)}°C',
                Icons.show_chart_rounded,
                const Color(0xFF388E3C),
                AppTheme.tempColor,
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              _statCard(
                'H% Min',
                '${humStats['min']!.toStringAsFixed(1)}%',
                Icons.arrow_downward_rounded,
                const Color(0xFF1976D2),
                AppTheme.humColor,
              ),
              const SizedBox(width: 8),
              _statCard(
                'H% Max',
                '${humStats['max']!.toStringAsFixed(1)}%',
                Icons.arrow_upward_rounded,
                const Color(0xFFE53935),
                AppTheme.humColor,
              ),
              const SizedBox(width: 8),
              _statCard(
                'H% Moy',
                '${humStats['avg']!.toStringAsFixed(1)}%',
                Icons.show_chart_rounded,
                const Color(0xFF388E3C),
                AppTheme.humColor,
              ),
            ],
          ),
          const SizedBox(height: 10),
          Divider(color: AppTheme.primary.withValues(alpha: 0.1)),
        ],
      ),
    );
  }

  Widget _statCard(
    String label,
    String value,
    IconData icon,
    Color color,
    Color borderColor,
  ) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: borderColor.withValues(alpha: 0.15)),
          boxShadow: [
            BoxShadow(
              color: AppTheme.primary.withValues(alpha: 0.06),
              blurRadius: 6,
            ),
          ],
        ),
        child: Column(
          children: [
            Icon(icon, size: 16, color: color),
            const SizedBox(height: 6),
            Text(
              label,
              style: TextStyle(
                color: AppTheme.textSecondary,
                fontSize: 9,
                fontWeight: FontWeight.w500,
              ),
            ),
            const SizedBox(height: 3),
            Text(
              value,
              style: TextStyle(
                color: color,
                fontSize: 13,
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildHistory() {
    if (_allRecords.isEmpty) return const SizedBox();
    final display = _allRecords.reversed.take(30).toList();

    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 4, bottom: 12),
            child: Row(
              children: [
                Container(
                  width: 3,
                  height: 14,
                  decoration: BoxDecoration(
                    color: AppTheme.primary,
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    'Historique récent (30 derniers)',
                    style: GoogleFonts.outfit(
                      color: AppTheme.textSecondary,
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
                // Bouton export Excel dans la section historique
                GestureDetector(
                  onTap: _exportExcel,
                  child: Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 12,
                      vertical: 6,
                    ),
                    decoration: BoxDecoration(
                      color: const Color(0xFF2E7D32).withValues(alpha: 0.1),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: const Color(0xFF2E7D32).withValues(alpha: 0.3),
                      ),
                    ),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const Icon(
                          Icons.download_rounded,
                          size: 14,
                          color: Color(0xFF2E7D32),
                        ),
                        const SizedBox(width: 5),
                        Text(
                          'Exporter tout (${_allRecords.length})',
                          style: GoogleFonts.outfit(
                            color: const Color(0xFF2E7D32),
                            fontSize: 11,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
          Container(
            decoration: AppTheme.cardDecoration(),
            child: Column(
              children: List.generate(display.length, (i) {
                final r = display[i];
                return Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: 12,
                  ),
                  decoration: BoxDecoration(
                    border: i < display.length - 1
                        ? Border(
                            bottom: BorderSide(
                              color: AppTheme.primary.withValues(alpha: 0.08),
                            ),
                          )
                        : null,
                  ),
                  child: Row(
                    children: [
                      // Numéro
                      Container(
                        width: 30,
                        height: 30,
                        decoration: BoxDecoration(
                          color: AppTheme.primary.withValues(alpha: 0.1),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Center(
                          child: Text(
                            '${i + 1}',
                            style: TextStyle(
                              color: AppTheme.primary,
                              fontWeight: FontWeight.w700,
                              fontSize: 11,
                            ),
                          ),
                        ),
                      ),
                      const SizedBox(width: 10),
                      // Date + heure
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              DateFormat('dd/MM/yyyy').format(r.timestamp),
                              style: TextStyle(
                                color: AppTheme.textPrimary,
                                fontSize: 12,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                            Text(
                              DateFormat('HH:mm:ss').format(r.timestamp),
                              style: TextStyle(
                                color: AppTheme.textSecondary,
                                fontSize: 10,
                              ),
                            ),
                          ],
                        ),
                      ),
                      // Valeurs principales
                      Wrap(
                        spacing: 4,
                        children: [
                          _miniPill(
                            '${r.temperature.toStringAsFixed(1)}°',
                            AppTheme.tempColor,
                          ),
                          _miniPill(
                            '${r.humidity.toStringAsFixed(0)}%',
                            AppTheme.humColor,
                          ),
                          if (r.pressure != null)
                            _miniPill(
                              r.pressure!.toStringAsFixed(0),
                              AppTheme.pressColor,
                            ),
                        ],
                      ),
                    ],
                  ),
                );
              }),
            ),
          ),
          if (_allRecords.length > 30)
            Padding(
              padding: const EdgeInsets.only(top: 10),
              child: Center(
                child: Text(
                  '+ ${_allRecords.length - 30} enregistrements supplémentaires',
                  style: GoogleFonts.outfit(
                    color: AppTheme.textSecondary,
                    fontSize: 11,
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _miniPill(String text, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: color.withValues(alpha: 0.2)),
      ),
      child: Text(
        text,
        style: TextStyle(
          color: color,
          fontWeight: FontWeight.w700,
          fontSize: 11,
        ),
      ),
    );
  }
}
