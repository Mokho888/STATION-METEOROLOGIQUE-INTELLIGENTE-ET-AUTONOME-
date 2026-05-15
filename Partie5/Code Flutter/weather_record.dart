// ============================================================
// weather_record.dart — Modèle complet d'un enregistrement météo
// Tous les champs capteurs sont nullable (les capteurs ne sont pas
// encore connectés — prêts pour quand ils le seront).
// ============================================================

class WeatherRecord {
  // ─── Champs obligatoires ──────────────────────────────────
  final double temperature;   // °C
  final double humidity;      // % humidité relative

  // ─── Champs optionnels (capteurs à venir) ─────────────────
  final double? pressure;       // hPa — pression atmosphérique
  final double? windSpeed;      // km/h — vitesse du vent
  final String? windDirection;  // ex: "N-E" — direction du vent
  final double? rain;           // mm — pluviométrie
  final double? waterLevel;     // cm — niveau d'eau
  final double? latitude;       // ° — coordonnée GPS
  final double? longitude;      // ° — coordonnée GPS
  final double? altitude;       // m — altitude GPS

  // ─── Horodatage ───────────────────────────────────────────
  final DateTime timestamp;

  WeatherRecord({
    required this.temperature,
    required this.humidity,
    this.pressure,
    this.windSpeed,
    this.windDirection,
    this.rain,
    this.waterLevel,
    this.latitude,
    this.longitude,
    this.altitude,
    required this.timestamp,
  });

  // ─── Sérialisation JSON ───────────────────────────────────
  Map<String, dynamic> toJson() => {
        'temperature': temperature,
        'humidity': humidity,
        if (pressure != null) 'pressure': pressure,
        if (windSpeed != null) 'windSpeed': windSpeed,
        if (windDirection != null) 'windDirection': windDirection,
        if (rain != null) 'rain': rain,
        if (waterLevel != null) 'waterLevel': waterLevel,
        if (latitude != null) 'latitude': latitude,
        if (longitude != null) 'longitude': longitude,
        if (altitude != null) 'altitude': altitude,
        'timestamp': timestamp.millisecondsSinceEpoch,
      };

  factory WeatherRecord.fromJson(Map<String, dynamic> json) => WeatherRecord(
        temperature: (json['temperature'] as num).toDouble(),
        humidity: (json['humidity'] as num).toDouble(),
        pressure: (json['pressure'] as num?)?.toDouble(),
        windSpeed: (json['windSpeed'] as num?)?.toDouble(),
        windDirection: json['windDirection'] as String?,
        rain: (json['rain'] as num?)?.toDouble(),
        waterLevel: (json['waterLevel'] as num?)?.toDouble(),
        latitude: (json['latitude'] as num?)?.toDouble(),
        longitude: (json['longitude'] as num?)?.toDouble(),
        altitude: (json['altitude'] as num?)?.toDouble(),
        timestamp: DateTime.fromMillisecondsSinceEpoch(
            json['timestamp'] as int),
      );

  @override
  String toString() =>
      'WeatherRecord(temp: $temperature, hum: $humidity, time: $timestamp)';
}
