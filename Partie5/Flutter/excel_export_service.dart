// ============================================================
// excel_export_service.dart — Export des données vers Excel
// Multi-plateforme : Web (téléchargement navigateur) et
// Android / iOS / desktop (fichier temporaire + partage).
// ============================================================

import 'dart:io';
import 'dart:typed_data';

import 'package:excel/excel.dart';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../models/weather_record.dart';

// Import conditionnel : uniquement chargé sur Web
import 'excel_export_web.dart'
    if (dart.library.io) 'excel_export_stub.dart' as web_download;

class ExcelExportService {
  /// Génère un fichier Excel à partir de [records] et :
  ///  - Sur **Web**   : déclenche le téléchargement dans le navigateur.
  ///  - Sur **Mobile/Desktop** : sauvegarde dans le répertoire temp et
  ///    ouvre la feuille de partage native.
  static Future<void> exportToExcel(List<WeatherRecord> records) async {
    // ── 1. Créer le classeur et la feuille ──────────────────
    final excel = Excel.createExcel();
    final sheet = excel['Historique Météo'];

    // ── 2. En-têtes avec style ──────────────────────────────
    final headers = [
      'Date',
      'Heure',
      'Température (°C)',
      'Humidité (%)',
      'Pression (hPa)',
      'Vent (km/h)',
      'Direction vent',
      'Pluie (mm)',
      'Latitude',
      'Longitude',
      'Altitude (m)',
    ];

    final headerStyle = CellStyle(
      bold: true,
      fontColorHex: ExcelColor.fromHexString('#FFFFFF'),
      backgroundColorHex: ExcelColor.fromHexString('#1565C0'),
      horizontalAlign: HorizontalAlign.Center,
      verticalAlign: VerticalAlign.Center,
    );

    final evenStyle = CellStyle(
      backgroundColorHex: ExcelColor.fromHexString('#F0F4FF'),
    );

    final numberStyle = CellStyle(
      horizontalAlign: HorizontalAlign.Right,
    );

    for (var col = 0; col < headers.length; col++) {
      final cell = sheet.cell(
          CellIndex.indexByColumnRow(columnIndex: col, rowIndex: 0));
      cell.value = TextCellValue(headers[col]);
      cell.cellStyle = headerStyle;
    }

    // ── 3. Données ──────────────────────────────────────────
    final sorted = List<WeatherRecord>.from(records)
      ..sort((a, b) => b.timestamp.compareTo(a.timestamp));

    final dateFmt = DateFormat('dd/MM/yyyy');
    final timeFmt = DateFormat('HH:mm:ss');

    for (var i = 0; i < sorted.length; i++) {
      final r = sorted[i];
      final rowIndex = i + 1;
      final isEven = i % 2 == 0;

      void writeText(int col, String? value) {
        final cell = sheet.cell(
            CellIndex.indexByColumnRow(columnIndex: col, rowIndex: rowIndex));
        cell.value = TextCellValue(value ?? '');
        if (isEven) cell.cellStyle = evenStyle;
      }

      void writeNum(int col, double? value) {
        final cell = sheet.cell(
            CellIndex.indexByColumnRow(columnIndex: col, rowIndex: rowIndex));
        if (value != null) {
          cell.value = DoubleCellValue(value);
          cell.cellStyle = isEven
              ? CellStyle(
                  backgroundColorHex: ExcelColor.fromHexString('#F0F4FF'),
                  horizontalAlign: HorizontalAlign.Right,
                )
              : numberStyle;
        } else {
          cell.value = TextCellValue('—');
          if (isEven) cell.cellStyle = evenStyle;
        }
      }

      writeText(0,  dateFmt.format(r.timestamp));
      writeText(1,  timeFmt.format(r.timestamp));
      writeNum(2,   r.temperature);
      writeNum(3,   r.humidity);
      writeNum(4,   r.pressure);
      writeNum(5,   r.windSpeed);
      writeText(6,  r.windDirection);
      writeNum(7,   r.rain);
      writeNum(8,   r.latitude);
      writeNum(9,   r.longitude);
      writeNum(10,  r.altitude);
    }

    // ── 4. Largeur des colonnes ─────────────────────────────
    final colWidths = [12, 10, 18, 14, 16, 14, 16, 12, 12, 12, 14];
    for (var col = 0; col < colWidths.length; col++) {
      sheet.setColumnWidth(col, colWidths[col].toDouble());
    }

    excel.delete('Sheet1');

    // ── 5. Encodage ─────────────────────────────────────────
    final Uint8List bytes = Uint8List.fromList(excel.encode()!);
    final fileName =
        'station_meteo_${DateFormat('yyyyMMdd_HHmm').format(DateTime.now())}.xlsx';

    // ── 6. Plateforme ────────────────────────────────────────
    if (kIsWeb) {
      // Web : téléchargement via le navigateur
      web_download.downloadFile(bytes, fileName);
    } else {
      // Mobile / Desktop : sauvegarde dans le dossier temporaire + partage
      final dir = await getTemporaryDirectory();
      final file = File('${dir.path}/$fileName');
      await file.writeAsBytes(bytes, flush: true);

      await SharePlus.instance.share(
        ShareParams(
          files: [XFile(file.path, mimeType: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet')],
          subject: 'Export Station Météo',
          text: 'Données météo exportées le ${DateFormat('dd/MM/yyyy HH:mm').format(DateTime.now())}',
        ),
      );
    }
  }
}
