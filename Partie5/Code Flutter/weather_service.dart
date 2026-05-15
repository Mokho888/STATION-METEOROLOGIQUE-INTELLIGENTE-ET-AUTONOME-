// ============================================================
// weather_service.dart — Service météo (Firebase + historique)
// - Lit tous les paramètres depuis /weather/ (Firebase RTDB)
// - Enregistre un snapshot toutes les 30 min dans /history/
// - Garde aussi une copie locale (SharedPreferences) en fallback
// ============================================================

import 'dart:async';
import 'dart:convert';
import 'package:firebase_database/firebase_database.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:intl/intl.dart';
import '../models/weather_record.dart';

class WeatherService {
  // ─── Constantes ──────────────────────────────────────────
  static const String _historyKey = 'weather_history';
  // Enregistrement toutes les 30 minutes
  static const Duration _recordInterval = Duration(minutes: 30);
  // Garde 1440 points = 30j × 48 mesures/jour
  static const int _maxHistory = 1440;

  // ─── Référence Firebase ───────────────────────────────────
  final DatabaseReference _weatherRef =
      FirebaseDatabase.instance.ref('weather');
  final DatabaseReference _historyRef =
      FirebaseDatabase.instance.ref('history');

  DateTime? _lastRecordedTime;

  // ─── Stream temps réel ────────────────────────────────────
  /// Émet un Map avec tous les paramètres météo à chaque changement.
  /// Chaque valeur est nullable (null si capteur absent).
  Stream<Map<String, dynamic>> get weatherStream {
    return _weatherRef.onValue.map((event) {
      final data = event.snapshot.value as Map<dynamic, dynamic>?;
      if (data == null) return <String, dynamic>{};
      return {
        'temperature':   (data['temperature']   as num?)?.toDouble(),
        'humidity':      (data['humidity']      as num?)?.toDouble(),
        'pressure':      (data['pressure']      as num?)?.toDouble(),
        'windSpeed':     (data['windSpeed']     as num?)?.toDouble(),
        'windDirection': data['windDirection']  as String?,
        'rain':          (data['rain']          as num?)?.toDouble(),
        'waterLevel':    (data['waterLevel']    as num?)?.toDouble(),
        'latitude':      (data['latitude']      as num?)?.toDouble(),
        'longitude':     (data['longitude']     as num?)?.toDouble(),
        'altitude':      (data['altitude']      as num?)?.toDouble(),
      };
    });
  }

  // ─── Enregistrement conditionnel ─────────────────────────
  /// Appelé à chaque mise à jour Firebase.
  /// Enregistre uniquement si 30 min se sont écoulées.
  /// Requiert au minimum temperature + humidity.
  Future<void> maybeRecord(Map<String, dynamic> data) async {
    final temp = data['temperature'] as double?;
    final hum  = data['humidity']   as double?;
    if (temp == null || hum == null) return;

    final now = DateTime.now();
    if (_lastRecordedTime != null &&
        now.difference(_lastRecordedTime!) < _recordInterval) return;

    final record = WeatherRecord(
      temperature:   temp,
      humidity:      hum,
      pressure:      data['pressure']      as double?,
      windSpeed:     data['windSpeed']     as double?,
      windDirection: data['windDirection'] as String?,
      rain:          data['rain']          as double?,
      waterLevel:    data['waterLevel']    as double?,
      latitude:      data['latitude']      as double?,
      longitude:     data['longitude']     as double?,
      altitude:      data['altitude']      as double?,
      timestamp:     now,
    );

    // Sauvegarde locale (fallback hors-ligne)
    await _saveLocalRecord(record);
    // Sauvegarde cloud Firebase (pour graphes & prédictions)
    await _saveToFirebase(record);
    _lastRecordedTime = now;
  }

  // ─── Sauvegarde locale ────────────────────────────────────
  Future<void> _saveLocalRecord(WeatherRecord record) async {
    final prefs = await SharedPreferences.getInstance();
    final history = await getHistory();
    history.add(record);
    if (history.length > _maxHistory) {
      history.removeRange(0, history.length - _maxHistory);
    }
    final encoded = jsonEncode(history.map((r) => r.toJson()).toList());
    await prefs.setString(_historyKey, encoded);
  }

  // ─── Sauvegarde Firebase History ─────────────────────────
  /// Écrit dans /history/YYYY-MM-DD/HH-MM/ pour organiser par date.
  Future<void> _saveToFirebase(WeatherRecord record) async {
    try {
      final dateKey = DateFormat('yyyy-MM-dd').format(record.timestamp);
      final timeKey = DateFormat('HH-mm').format(record.timestamp);
      await _historyRef.child(dateKey).child(timeKey).set(record.toJson());
    } catch (_) {
      // Silencieux — le stockage local est le fallback
    }
  }

  // ─── Lecture historique local ─────────────────────────────
  Future<List<WeatherRecord>> getHistory() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_historyKey);
    if (raw == null) return [];
    try {
      final list = jsonDecode(raw) as List<dynamic>;
      return list
          .map((e) => WeatherRecord.fromJson(e as Map<String, dynamic>))
          .toList();
    } catch (_) {
      return [];
    }
  }

  Future<List<WeatherRecord>> getHistoryForPeriod(Duration period) async {
    final all = await getHistory();
    final cutoff = DateTime.now().subtract(period);
    return all.where((r) => r.timestamp.isAfter(cutoff)).toList();
  }

  // ─── Lecture historique Firebase ─────────────────────────
  /// Lit les enregistrements d'un jour donné depuis Firebase.
  Future<List<WeatherRecord>> getFirebaseHistoryForDay(DateTime date) async {
    try {
      final dateKey = DateFormat('yyyy-MM-dd').format(date);
      final snap = await _historyRef.child(dateKey).get();
      if (!snap.exists || snap.value == null) return [];
      final map = snap.value as Map<dynamic, dynamic>;
      final records = <WeatherRecord>[];
      for (final entry in map.values) {
        try {
          final r = WeatherRecord.fromJson(
              Map<String, dynamic>.from(entry as Map));
          records.add(r);
        } catch (_) {}
      }
      records.sort((a, b) => a.timestamp.compareTo(b.timestamp));
      return records;
    } catch (_) {
      return [];
    }
  }

  /// Lit les enregistrements Firebase pour les [days] derniers jours.
  Future<List<WeatherRecord>> getFirebaseHistoryForPeriod(int days) async {
    final allRecords = <WeatherRecord>[];
    for (int i = 0; i < days; i++) {
      final date = DateTime.now().subtract(Duration(days: i));
      final dayRecords = await getFirebaseHistoryForDay(date);
      allRecords.addAll(dayRecords);
    }
    allRecords.sort((a, b) => a.timestamp.compareTo(b.timestamp));
    return allRecords;
  }

  // ─── Statistiques ─────────────────────────────────────────
  Map<String, double> computeStats(
      List<WeatherRecord> records,
      double? Function(WeatherRecord) getValue) {
    if (records.isEmpty) return {'min': 0, 'max': 0, 'avg': 0};
    final values = records.map(getValue).whereType<double>().toList();
    if (values.isEmpty) return {'min': 0, 'max': 0, 'avg': 0};
    return {
      'min': values.reduce((a, b) => a < b ? a : b),
      'max': values.reduce((a, b) => a > b ? a : b),
      'avg': values.reduce((a, b) => a + b) / values.length,
    };
  }
}
