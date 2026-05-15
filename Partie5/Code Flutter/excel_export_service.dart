// ============================================================
// excel_export_service.dart — Export des données vers Excel
// Compatible Flutter Web moderne (dart:js_interop + package:web).
// ============================================================

import 'dart:js_interop';
import 'dart:typed_data';

import 'package:excel/excel.dart';
import 'package:intl/intl.dart';
import 'package:web/web.dart' as web;

import '../models/weather_record.dart';

class ExcelExportService {
  /// Génère un fichier Excel à partir de la liste de [records] et
  /// déclenche le téléchargement automatique dans le navigateur.
  static void exportToExcel(List<WeatherRecord> records) {
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
      'Niveau eau (cm)',
      'Latitude',
      'Longitude',
      'Altitude (m)',
    ];

    // Style d'en-tête : fond bleu foncé, texte blanc, gras
    final headerStyle = CellStyle(
      bold: true,
      fontColorHex: ExcelColor.fromHexString('#FFFFFF'),
      backgroundColorHex: ExcelColor.fromHexString('#1565C0'),
      horizontalAlign: HorizontalAlign.Center,
      verticalAlign: VerticalAlign.Center,
    );

    // Style de données pair : fond gris très clair
    final evenStyle = CellStyle(
      backgroundColorHex: ExcelColor.fromHexString('#F0F4FF'),
    );

    // Style nombre : aligné à droite
    final numberStyle = CellStyle(
      horizontalAlign: HorizontalAlign.Right,
    );

    // Écriture des en-têtes (ligne 0)
    for (var col = 0; col < headers.length; col++) {
      final cell = sheet.cell(
          CellIndex.indexByColumnRow(columnIndex: col, rowIndex: 0));
      cell.value = TextCellValue(headers[col]);
      cell.cellStyle = headerStyle;
    }

    // ── 3. Données ──────────────────────────────────────────
    // Tri du plus récent au plus ancien
    final sorted = List<WeatherRecord>.from(records)
      ..sort((a, b) => b.timestamp.compareTo(a.timestamp));

    final dateFmt = DateFormat('dd/MM/yyyy');
    final timeFmt = DateFormat('HH:mm:ss');

    for (var i = 0; i < sorted.length; i++) {
      final r = sorted[i];
      final rowIndex = i + 1; // ligne 0 = en-têtes
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
      writeNum(8,   r.waterLevel);
      writeNum(9,   r.latitude);
      writeNum(10,  r.longitude);
      writeNum(11,  r.altitude);
    }

    // ── 4. Largeur automatique des colonnes ─────────────────
    final colWidths = [12, 10, 18, 14, 16, 14, 16, 12, 17, 12, 12, 14];
    for (var col = 0; col < colWidths.length; col++) {
      sheet.setColumnWidth(col, colWidths[col].toDouble());
    }

    // Supprimer la feuille vide par défaut créée par le package
    excel.delete('Sheet1');

    // ── 5. Encodage et téléchargement (API web moderne) ─────
    final Uint8List bytes = Uint8List.fromList(excel.encode()!);

    // Convertir en JSUint8Array pour l'API Blob
    final jsArray = bytes.toJS;

    final blob = web.Blob(
      [jsArray].toJS,
      web.BlobPropertyBag(
          type: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet'),
    );

    final url = web.URL.createObjectURL(blob);
    final fileName =
        'station_meteo_${DateFormat('yyyyMMdd_HHmm').format(DateTime.now())}.xlsx';

    // Création d'un lien invisible pour déclencher le téléchargement
    final anchor = web.HTMLAnchorElement()
      ..href = url
      ..download = fileName
      ..style.display = 'none';

    web.document.body!.append(anchor);
    anchor.click();
    anchor.remove();
    web.URL.revokeObjectURL(url);
  }
}
