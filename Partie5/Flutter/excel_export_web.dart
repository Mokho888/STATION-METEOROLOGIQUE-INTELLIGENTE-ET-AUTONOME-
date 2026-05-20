// ============================================================
// excel_export_web.dart — Implémentation Web du téléchargement
// Ce fichier est importé UNIQUEMENT sur la plateforme Web.
// ============================================================

import 'dart:js_interop';
import 'dart:typed_data';
import 'package:web/web.dart' as web;

/// Déclenche le téléchargement d'un fichier binaire dans le navigateur.
void downloadFile(Uint8List bytes, String fileName) {
  final jsArray = bytes.toJS;

  final blob = web.Blob(
    [jsArray].toJS,
    web.BlobPropertyBag(
      type: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
    ),
  );

  final url = web.URL.createObjectURL(blob);

  final anchor = web.HTMLAnchorElement()
    ..href = url
    ..download = fileName
    ..style.display = 'none';

  web.document.body!.append(anchor);
  anchor.click();
  anchor.remove();
  web.URL.revokeObjectURL(url);
}
