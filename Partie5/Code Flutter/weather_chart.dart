// ============================================================
// weather_chart.dart — Graphique linéaire (thème clair)
// Utilise fl_chart. Accepte un extracteur de valeur générique
// pour afficher n'importe quelle métrique.
// ============================================================

import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:intl/intl.dart';
import '../models/weather_record.dart';
import '../theme/app_theme.dart';

class WeatherChart extends StatelessWidget {
  final List<WeatherRecord> records;
  final double? Function(WeatherRecord) valueExtractor;
  final Color lineColor;
  final String unit;

  const WeatherChart({
    super.key,
    required this.records,
    required this.valueExtractor,
    required this.lineColor,
    required this.unit,
  });

  @override
  Widget build(BuildContext context) {
    // Filtre les points avec une valeur valide
    final validRecords = records
        .where((r) => valueExtractor(r) != null)
        .toList();

    if (validRecords.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.bar_chart_rounded,
                size: 52, color: AppTheme.primary.withValues(alpha: 0.3)),
            const SizedBox(height: 12),
            Text(
              'Pas encore de données.\nLes capteurs seront bientôt connectés !',
              textAlign: TextAlign.center,
              style: GoogleFonts.outfit(
                  color: AppTheme.textSecondary, fontSize: 14),
            ),
          ],
        ),
      );
    }

    final spots = <FlSpot>[];
    for (int i = 0; i < validRecords.length; i++) {
      final v = valueExtractor(validRecords[i])!;
      spots.add(FlSpot(i.toDouble(), v));
    }

    return LineChart(
      LineChartData(
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: _computeYInterval(spots),
          getDrawingHorizontalLine: (_) => FlLine(
            color: AppTheme.primary.withValues(alpha: 0.08),
            strokeWidth: 1,
          ),
        ),
        titlesData: FlTitlesData(
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 40,
              getTitlesWidget: (v, _) => Text(
                '${v.toStringAsFixed(0)}$unit',
                style: TextStyle(
                    color: AppTheme.textSecondary.withValues(alpha: 0.7),
                    fontSize: 9),
              ),
            ),
          ),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 32,
              interval: _computeXInterval(validRecords.length),
              getTitlesWidget: (v, _) {
                final idx = v.toInt();
                if (idx < 0 || idx >= validRecords.length) {
                  return const SizedBox();
                }
                return Padding(
                  padding: const EdgeInsets.only(top: 4),
                  child: Text(
                    DateFormat('HH:mm\ndd/MM').format(validRecords[idx].timestamp),
                    textAlign: TextAlign.center,
                    style: TextStyle(
                        color: AppTheme.textSecondary.withValues(alpha: 0.7),
                        fontSize: 8),
                  ),
                );
              },
            ),
          ),
          topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
          rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
        ),
        borderData: FlBorderData(show: false),
        lineTouchData: LineTouchData(
          touchTooltipData: LineTouchTooltipData(
            getTooltipColor: (_) => AppTheme.bgCard,
            getTooltipItems: (spots) => spots.map((s) => LineTooltipItem(
              '${s.y.toStringAsFixed(1)}$unit',
              TextStyle(
                  color: lineColor,
                  fontWeight: FontWeight.bold,
                  fontSize: 13),
            )).toList(),
          ),
        ),
        lineBarsData: [
          LineChartBarData(
            spots: spots,
            isCurved: true,
            curveSmoothness: 0.3,
            color: lineColor,
            barWidth: 2.5,
            dotData: FlDotData(
              show: spots.length < 30,
              getDotPainter: (spot, percent, bar, index) => FlDotCirclePainter(
                radius: 3,
                color: lineColor,
                strokeColor: Colors.white,
                strokeWidth: 2,
              ),
            ),
            belowBarData: BarAreaData(
              show: true,
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: [
                  lineColor.withValues(alpha: 0.25),
                  lineColor.withValues(alpha: 0.0),
                ],
              ),
            ),
          ),
        ],
      ),
      duration: const Duration(milliseconds: 400),
    );
  }

  double _computeYInterval(List<FlSpot> spots) {
    if (spots.isEmpty) return 10;
    final min = spots.map((s) => s.y).reduce((a, b) => a < b ? a : b);
    final max = spots.map((s) => s.y).reduce((a, b) => a > b ? a : b);
    final range = (max - min).abs();
    if (range < 5) return 1;
    if (range < 20) return 5;
    return 10;
  }

  double _computeXInterval(int count) {
    if (count <= 12) return 1;
    if (count <= 48) return 4;
    return 12;
  }
}
