#include "FormatUtils.h"

namespace FormatUtils {

QString formatRuntime(qint64 seconds)
{
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        qint64 minutes = seconds / 60;
        qint64 secs = seconds % 60;
        return QString("%1m %2s").arg(minutes).arg(secs);
    } else if (seconds < 86400) {
        qint64 hours = seconds / 3600;
        qint64 minutes = (seconds % 3600) / 60;
        return QString("%1h %2m").arg(hours).arg(minutes);
    } else {
        qint64 days = seconds / 86400;
        qint64 hours = (seconds % 86400) / 3600;
        return QString("%1d %2h").arg(days).arg(hours);
    }
}

QString formatMemory(double megabytes)
{
    if (megabytes < 1024.0) {
        return QString("%1 MB").arg(megabytes, 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(megabytes / 1024.0, 0, 'f', 1);
    }
}

QString formatCpu(double percent)
{
    return QString("%1%").arg(percent, 0, 'f', 1);
}

QString getNumericTooltip(double cpu, double memoryMb, qint64 runtimeSeconds)
{
    return QString("CPU: %1%, Memory: %2 MB, Runtime: %3 seconds")
        .arg(cpu, 0, 'f', 3)
        .arg(memoryMb, 0, 'f', 1)
        .arg(runtimeSeconds);
}

}
