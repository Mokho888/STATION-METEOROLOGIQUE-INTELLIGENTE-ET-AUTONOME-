// ============================================================
// graphs_screen.dart — Écran Graphiques (thème clair bleu ciel)
// Permet de visualiser l'évolution de chaque métrique sur
// différentes périodes (Firebase + local).
// ============================================================

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../models/weather_record.dart';
import '../services/weather_service.dart';
import '../theme/app_theme.dart';
import '../widgets/weather_chart.dart';

class GraphsScreen extends StatefulWidget {
  const GraphsScreen({super.key});

  @override
  State<GraphsScreen> createState() => _GraphsScreenState();
}

class _GraphsScreenState extends State<GraphsScreen>
    with SingleTickerProviderStateMixin {
  final WeatherService _service = WeatherService();
  List<WeatherRecord> _records = [];
  bool _loading = true;
  int _periodIndex = 0;
  int _metricIndex = 0; // métrique sélectionnée

  late AnimationController _fadeCtrl;
  late Animation<double> _fadeAnim;

  static const _periods = [
    Duration(hours: 24),
    Duration(days: 7),
    Duration(days: 30),
    Duration(days: 9999),
  ];
  static const _periodLabels = ['24h', '7j', '30j', 'Tout'];

  // Métriques disponibles (label, extractor, couleur, unité)
  static final _metrics = [
    _Metric('Température', (r) => r.temperature, AppTheme.tempColor, '°C', Icons.thermostat_rounded),
    _Metric('Humidité', (r) => r.humidity, AppTheme.humColor, '%', Icons.water_drop_rounded),
    _Metric('Pression', (r) => r.pressure, AppTheme.pressColor, 'hPa', Icons.speed_rounded),
    _Metric('Pluie', (r) => r.rain, AppTheme.rainColor, 'mm', Icons.grain_rounded),
    _Metric('Vent', (r) => r.windSpeed, AppTheme.windColor, 'km/h', Icons.air_rounded),
  ];

  @override
  void initState() {
    super.initState();
    _fadeCtrl = AnimationController(
        vsync: this, duration: const Duration(milliseconds: 300));
    _fadeAnim = Tween<double>(begin: 0.0, end: 1.0).animate(
        CurvedAnimation(parent: _fadeCtrl, curve: Curves.easeIn));
    _load();
  }

  @override
  void dispose() {
    _fadeCtrl.dispose();
    super.dispose();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    _fadeCtrl.reset();
    final records = await _service.getHistoryForPeriod(_periods[_periodIndex]);
    if (mounted) {
      setState(() {
        _records = records;
        _loading = false;
      });
      _fadeCtrl.forward();
    }
  }

  @override
  Widget build(BuildContext context) {
    final metric = _metrics[_metricIndex];
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(gradient: AppTheme.bgGradient),
        child: SafeArea(
          child: Column(
            children: [
              _buildHeader(),
              _buildPeriodSelector(),
              _buildMetricSelector(),
              Expanded(child: _buildChart(metric)),
              _buildStats(metric),
              const SizedBox(height: 12),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
      child: Row(
        children: [
          Container(
            width: 42,
            height: 42,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: AppTheme.bgCard,
              boxShadow: [BoxShadow(color: AppTheme.primary.withValues(alpha: 0.2), blurRadius: 10)],
            ),
            child: ClipOval(
                child: Image.asset('assets/images/logo.png', fit: BoxFit.cover)),
          ),
          const SizedBox(width: 12),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Graphiques',
                  style: GoogleFonts.outfit(
                      color: AppTheme.textPrimary,
                      fontSize: 22,
                      fontWeight: FontWeight.w700)),
              Text('${_records.length} enregistrement(s)',
                  style: GoogleFonts.outfit(
                      color: AppTheme.textSecondary, fontSize: 12)),
            ],
          ),
          const Spacer(),
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

  Widget _buildPeriodSelector() {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      child: Container(
        padding: const EdgeInsets.all(4),
        decoration: AppTheme.cardDecoration(),
        child: Row(
          children: List.generate(_periodLabels.length, (i) {
            final sel = i == _periodIndex;
            return Expanded(
              child: GestureDetector(
                onTap: () {
                  if (_periodIndex == i) return;
                  setState(() => _periodIndex = i);
                  _load();
                },
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 220),
                  padding: const EdgeInsets.symmetric(vertical: 9),
                  decoration: BoxDecoration(
                    color: sel ? AppTheme.primary : Colors.transparent,
                    borderRadius: BorderRadius.circular(12),
                    boxShadow: sel
                        ? [BoxShadow(
                            color: AppTheme.primary.withValues(alpha: 0.3),
                            blurRadius: 8,
                            offset: const Offset(0, 2))]
                        : null,
                  ),
                  child: Text(
                    _periodLabels[i],
                    textAlign: TextAlign.center,
                    style: GoogleFonts.outfit(
                      color: sel ? Colors.white : AppTheme.textSecondary,
                      fontSize: 13,
                      fontWeight: sel ? FontWeight.w700 : FontWeight.w400,
                    ),
                  ),
                ),
              ),
            );
          }),
        ),
      ),
    );
  }

  Widget _buildMetricSelector() {
    return SizedBox(
      height: 44,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
        itemCount: _metrics.length,
        separatorBuilder: (context, index) => const SizedBox(width: 8),
        itemBuilder: (ctx, i) {
          final m = _metrics[i];
          final sel = i == _metricIndex;
          return GestureDetector(
            onTap: () => setState(() => _metricIndex = i),
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 200),
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              decoration: BoxDecoration(
                color: sel ? m.color.withValues(alpha: 0.15) : AppTheme.bgCard,
                borderRadius: BorderRadius.circular(20),
                border: Border.all(
                  color: sel ? m.color.withValues(alpha: 0.5) : AppTheme.primary.withValues(alpha: 0.1),
                ),
              ),
              child: Row(
                children: [
                  Icon(m.icon, size: 14, color: sel ? m.color : AppTheme.textSecondary),
                  const SizedBox(width: 5),
                  Text(
                    m.label,
                    style: GoogleFonts.outfit(
                      color: sel ? m.color : AppTheme.textSecondary,
                      fontSize: 12,
                      fontWeight: sel ? FontWeight.w700 : FontWeight.w500,
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }

  Widget _buildChart(_Metric metric) {
    if (_loading) {
      return Center(
          child: CircularProgressIndicator(color: AppTheme.primary, strokeWidth: 2.5));
    }
    return FadeTransition(
      opacity: _fadeAnim,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
        child: Container(
          padding: const EdgeInsets.fromLTRB(8, 16, 14, 8),
          decoration: AppTheme.cardDecoration(borderColor: metric.color),
          child: WeatherChart(
            records: _records,
            valueExtractor: metric.extractor,
            lineColor: metric.color,
            unit: metric.unit,
          ),
        ),
      ),
    );
  }

  Widget _buildStats(_Metric metric) {
    if (_records.isEmpty) return const SizedBox();
    final stats = _service.computeStats(_records, metric.extractor);
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
      child: Row(
        children: [
          _statPill('Min', '${stats['min']!.toStringAsFixed(1)}${metric.unit}',
              const Color(0xFF1976D2), metric.color),
          const SizedBox(width: 8),
          _statPill('Max', '${stats['max']!.toStringAsFixed(1)}${metric.unit}',
              const Color(0xFFE53935), metric.color),
          const SizedBox(width: 8),
          _statPill('Moy', '${stats['avg']!.toStringAsFixed(1)}${metric.unit}',
              const Color(0xFF388E3C), metric.color),
        ],
      ),
    );
  }

  Widget _statPill(String label, String value, Color valueColor, Color border) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 10),
        decoration: BoxDecoration(
          color: AppTheme.bgCard,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: border.withValues(alpha: 0.2)),
          boxShadow: [BoxShadow(
              color: AppTheme.primary.withValues(alpha: 0.06),
              blurRadius: 8)],
        ),
        child: Column(
          children: [
            Text(label,
                style: TextStyle(
                    color: AppTheme.textSecondary,
                    fontSize: 11,
                    fontWeight: FontWeight.w500)),
            const SizedBox(height: 4),
            Text(value,
                style: TextStyle(
                    color: valueColor,
                    fontSize: 14,
                    fontWeight: FontWeight.w800)),
          ],
        ),
      ),
    );
  }
}

// ─── Modèle de métrique ───────────────────────────────────────
class _Metric {
  final String label;
  final double? Function(WeatherRecord) extractor;
  final Color color;
  final String unit;
  final IconData icon;
  const _Metric(this.label, this.extractor, this.color, this.unit, this.icon);
}
