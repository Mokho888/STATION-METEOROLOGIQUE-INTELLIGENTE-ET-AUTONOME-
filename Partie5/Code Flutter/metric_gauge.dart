// ============================================================
// metric_gauge.dart — Jauge circulaire animée (arc semi-circulaire)
// Utilisée sur l'écran Accueil pour afficher Température et Humidité.
// Le dessin de l'arc est fait via CustomPainter (_ArcPainter).
// ============================================================

import 'dart:math' as math;
import 'package:flutter/material.dart';
import '../theme/app_theme.dart';

/// Widget de jauge circulaire.
/// Affiche une valeur numérique sur un arc allant de [min] à [max],
/// avec un dégradé de couleurs [gradient] et une icône [icon].
class MetricGauge extends StatefulWidget {
  final double value;          // valeur actuelle à afficher
  final double min;            // valeur minimale de la jauge
  final double max;            // valeur maximale de la jauge
  final String label;          // étiquette affiché sous l'arc (ex : "TEMPÉRATURE")
  final String unit;           // unité affiché après la valeur (ex : "°C", "%")
  final LinearGradient gradient; // dégradé de couleur de l'arc rempli
  final IconData icon;         // icône affiché au centre de la jauge
  final Color iconColor;       // couleur de l'icône et du point lumineux à l'extrémité

  const MetricGauge({
    super.key,
    required this.value,
    required this.min,
    required this.max,
    required this.label,
    required this.unit,
    required this.gradient,
    required this.icon,
    required this.iconColor,
  });

  @override
  State<MetricGauge> createState() => _MetricGaugeState();
}

class _MetricGaugeState extends State<MetricGauge>
    with SingleTickerProviderStateMixin {
  // Contrôleur d'animation (0 → 1) déclenché au montage et à chaque changement de valeur
  late AnimationController _controller;
  // Courbe de l'animation : décélération progressive
  late Animation<double> _animation;

  @override
  void initState() {
    super.initState();

    // Crée l'animation sur 1.2 seconde avec décélération cubique
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1200),
    );
    _animation = Tween<double>(begin: 0, end: 1).animate(
      CurvedAnimation(parent: _controller, curve: Curves.easeOutCubic),
    );
    // Démarre l'animation au montage pour l'effet "remplissage de l'arc"
    _controller.forward();
  }

  @override
  void didUpdateWidget(MetricGauge oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Si la valeur change (nouvelle donnée Firebase), rejoue l'animation depuis 0
    if (oldWidget.value != widget.value) {
      _controller.forward(from: 0);
    }
  }

  @override
  void dispose() {
    _controller.dispose(); // Libère l'AnimationController pour éviter les fuites
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    // Normalise la valeur entre 0.0 et 1.0 pour l'arc (clamp pour rester dans [0,1])
    final progress =
        ((widget.value - widget.min) / (widget.max - widget.min))
            .clamp(0.0, 1.0);

    // Reconstruit à chaque tick d'animation pour une jauge fluide
    return AnimatedBuilder(
      animation: _animation,
      builder: (context, child) {
        return Container(
          decoration: BoxDecoration(
            color: AppTheme.bgCard,
            borderRadius: BorderRadius.circular(24),
            // Bordure fine de la même couleur que l'icône, atténuée
            border: Border.all(
              color: widget.iconColor.withValues(alpha: 0.15),
            ),
            // Halo lumineux subtil de la couleur de la métrique
            boxShadow: [
              BoxShadow(
                color: widget.iconColor.withValues(alpha: 0.12),
                blurRadius: 24,
                spreadRadius: 2,
              ),
            ],
          ),
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                // ── Arc dessiné via CustomPaint ─────────
                SizedBox(
                  width: 160,
                  height: 100,
                  child: CustomPaint(
                    // Passe la progression animée au painter
                    painter: _ArcPainter(
                      progress: progress * _animation.value, // progression animée 0→progress
                      gradient: widget.gradient,
                      iconColor: widget.iconColor,
                    ),
                    // Contenu au centre de l'arc : icône + valeur numérique
                    child: Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.end,
                        children: [
                          // Icône de la métrique (thermostat ou goutte d'eau)
                          Icon(widget.icon,
                              color: widget.iconColor, size: 22),
                          const SizedBox(height: 4),
                          // Valeur + unité animées (interpolation numérique)
                          RichText(
                            text: TextSpan(
                              children: [
                                TextSpan(
                                  // Interpole numériquement de min → valeur selon l'animation
                                  text: (widget.value * _animation.value +
                                          widget.min * (1 - _animation.value))
                                      .toStringAsFixed(1),
                                  style: const TextStyle(
                                    color: AppTheme.textPrimary,
                                    fontSize: 28,
                                    fontWeight: FontWeight.w700,
                                    height: 1,
                                  ),
                                ),
                                TextSpan(
                                  text: widget.unit,
                                  style: const TextStyle(
                                    color: AppTheme.textSecondary,
                                    fontSize: 14,
                                    fontWeight: FontWeight.w500,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 14),
                // ── Label de la métrique ────────────────
                Text(
                  widget.label,
                  style: const TextStyle(
                    color: AppTheme.textSecondary,
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                    letterSpacing: 1.0,
                  ),
                ),
                const SizedBox(height: 8),
                // ── Bornes min / max sous l'arc ─────────
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    // Borne minimale à gauche
                    Text(
                      '${widget.min.toStringAsFixed(0)}${widget.unit}',
                      style: const TextStyle(
                          color: AppTheme.textMuted, fontSize: 10),
                    ),
                    // Borne maximale à droite
                    Text(
                      '${widget.max.toStringAsFixed(0)}${widget.unit}',
                      style: const TextStyle(
                          color: AppTheme.textMuted, fontSize: 10),
                    ),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

// ─── Painter personnalisé ───────────────────────────────────
/// Dessine l'arc de fond (gris) et l'arc de progression (coloré + dégradé).
/// Ajoute un point lumineux à l'extrémité de l'arc pour un effet premium.
class _ArcPainter extends CustomPainter {
  final double progress;          // progression entre 0.0 et 1.0
  final LinearGradient gradient;  // dégradé de couleur de l'arc actif
  final Color iconColor;          // couleur du point à l'extrémité

  _ArcPainter({
    required this.progress,
    required this.gradient,
    required this.iconColor,
  });

  @override
  void paint(Canvas canvas, Size size) {
    // Centre de l'arc : bas du canvas (semi-cercle de la moitié inférieure)
    final center = Offset(size.width / 2, size.height * 0.85);
    final radius = size.width * 0.46; // rayon de l'arc

    // L'arc commence à gauche (π = 180°) et s'étend sur 180° vers la droite
    const startAngle = math.pi;
    const sweepAngle = math.pi;

    // ── Arc de fond (piste grise) ─────────────────────
    final trackPaint = Paint()
      ..color = AppTheme.textMuted.withValues(alpha: 0.15) // gris très atténué
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10
      ..strokeCap = StrokeCap.round; // extrémités arrondies

    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      startAngle,
      sweepAngle,
      false, // false = arc (pas de secteur fermé)
      trackPaint,
    );

    // ── Arc de progression (coloré) ───────────────────
    if (progress > 0) {
      // Crée un shader à partir du dégradé (adapté à la taille du canvas)
      final rect = Rect.fromCircle(center: center, radius: radius);
      final shader = gradient.createShader(rect);

      final arcPaint = Paint()
        ..shader = shader              // applique le dégradé
        ..style = PaintingStyle.stroke
        ..strokeWidth = 10
        ..strokeCap = StrokeCap.round;

      // Dessine l'arc de gauche (startAngle) jusqu'à la progression actuelle
      canvas.drawArc(
          rect, startAngle, sweepAngle * progress, false, arcPaint);

      // ── Point lumineux à l'extrémité de l'arc ────────
      // Calcule les coordonnées de l'extrémité en coordonnées polaires → cartésiennes
      final tipAngle = startAngle + sweepAngle * progress;
      final tipX = center.dx + radius * math.cos(tipAngle);
      final tipY = center.dy + radius * math.sin(tipAngle);

      // Halo extérieur (glow)
      final glowPaint = Paint()
        ..color = iconColor.withValues(alpha: 0.3)
        ..style = PaintingStyle.fill
        ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 6);
      canvas.drawCircle(Offset(tipX, tipY), 9, glowPaint);

      // Point plein de couleur
      final dotPaint = Paint()
        ..color = iconColor
        ..style = PaintingStyle.fill;
      canvas.drawCircle(Offset(tipX, tipY), 5.5, dotPaint);
    }
  }

  /// Indique à Flutter de ne redessiner que si la progression a changé
  @override
  bool shouldRepaint(_ArcPainter old) => old.progress != progress;
}
