// ============================================================
// excel_export_stub.dart — Stub pour les plateformes non-Web
// Ce fichier est importé sur Android, iOS, Desktop.
// La logique réelle est dans ExcelExportService (path_provider +
// share_plus). Cette fonction ne devrait jamais être appelée.
// ============================================================

import 'dart:typed_data';

/// No-op sur les plateformes non-Web (le service gère la logique mobile).
void downloadFile(Uint8List bytes, String fileName) {
  // Intentionnellement vide — la branche mobile est gérée
  // directement dans ExcelExportService avec path_provider + share_plus.
  throw UnsupportedError(
    'downloadFile() ne doit pas être appelé sur une plateforme non-Web.',
  );
}
