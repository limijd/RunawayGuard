#ifndef FORMATUTILS_H
#define FORMATUTILS_H

#include <QColor>
#include <QString>

namespace FormatUtils {

// Format runtime seconds to human-readable string
// < 60s: "45s", < 1h: "45m 30s", < 1d: "2h 15m", >= 1d: "3d 5h"
QString formatRuntime(qint64 seconds);

// Format memory to human-readable string
// < 1GB: "512.0 MB", >= 1GB: "2.3 GB"
QString formatMemory(double megabytes);

// Format CPU percentage with 1 decimal
QString formatCpu(double percent);

// Get precise tooltip for numeric values
QString getNumericTooltip(double cpu, double memoryMb, qint64 runtimeSeconds);

// Get background color for CPU value
// < 80%: transparent, 80-90%: light orange, > 90%: light red
QColor getCpuBackgroundColor(double percent);

// Get background color for memory value
// < 1GB: transparent, 1-4GB: light orange, > 4GB: light red
QColor getMemoryBackgroundColor(double megabytes);

// Get background color for process state
// R/S: transparent, D/Z: light red
QColor getStateBackgroundColor(const QString &state);

// Get text color (darker version for contrast)
QColor getTextColorForBackground(const QColor &bg);

}

#endif
