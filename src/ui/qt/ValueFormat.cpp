#include "ValueFormat.h"

#include <cmath>

namespace drumas::ui {
namespace {

[[nodiscard]] QString trimmedFixed(double value, int decimals) {
    QString text = QString::number(value, 'f', decimals);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    if (text == QLatin1String("-0")) {
        return QStringLiteral("0");
    }
    return text;
}

[[nodiscard]] QString unitSuffix(core::ParameterUnit unit) {
    switch (unit) {
        case core::ParameterUnit::Hertz: return QStringLiteral("Hz");
        case core::ParameterUnit::Milliseconds: return QStringLiteral("ms");
        case core::ParameterUnit::Percent: return QStringLiteral("%");
        case core::ParameterUnit::Decibels: return QStringLiteral("dB");
        case core::ParameterUnit::Semitones: return QStringLiteral("st");
        case core::ParameterUnit::Cents: return QStringLiteral("ct");
        case core::ParameterUnit::Octaves: return QStringLiteral("oct");
        case core::ParameterUnit::Ratio: return QStringLiteral("ratio");
        case core::ParameterUnit::None: return {};
    }
    return {};
}

[[nodiscard]] core::ParameterUnit unitFromName(const QString& unitName) {
    if (unitName == QLatin1String("%")) {
        return core::ParameterUnit::Percent;
    }
    if (unitName == QLatin1String("Hz")) {
        return core::ParameterUnit::Hertz;
    }
    if (unitName == QLatin1String("ms")) {
        return core::ParameterUnit::Milliseconds;
    }
    if (unitName == QLatin1String("dB")) {
        return core::ParameterUnit::Decibels;
    }
    if (unitName == QLatin1String("st")) {
        return core::ParameterUnit::Semitones;
    }
    if (unitName == QLatin1String("ct")) {
        return core::ParameterUnit::Cents;
    }
    if (unitName == QLatin1String("oct")) {
        return core::ParameterUnit::Octaves;
    }
    if (unitName == QLatin1String("ratio")) {
        return core::ParameterUnit::Ratio;
    }
    return core::ParameterUnit::None;
}

}  // namespace

QString formatParameter(core::ParameterUnit unit, double value) {
    if (!std::isfinite(value)) {
        return QStringLiteral("--");
    }

    QString number;
    switch (unit) {
        case core::ParameterUnit::Percent: {
            const double scaled = value * 100.0;
            const double magnitude = std::abs(scaled);
            if (magnitude >= 100.0 || std::abs(scaled - std::round(scaled)) < 0.05) {
                number = QString::number(std::lround(scaled));
            } else {
                number = trimmedFixed(scaled, 1);
            }
            break;
        }
        case core::ParameterUnit::Hertz: {
            const double magnitude = std::abs(value);
            if (magnitude < 10.0) {
                number = trimmedFixed(value, 2);
            } else if (magnitude < 100.0) {
                number = trimmedFixed(value, 1);
            } else {
                number = QString::number(std::lround(value));
            }
            break;
        }
        case core::ParameterUnit::Milliseconds: {
            number = std::abs(value) < 10.0 ? trimmedFixed(value, 1)
                                            : QString::number(std::lround(value));
            break;
        }
        case core::ParameterUnit::Decibels:
            number = trimmedFixed(value, 1);
            break;
        case core::ParameterUnit::Semitones:
            number = QString::number(std::lround(value));
            break;
        case core::ParameterUnit::Cents:
            number = QString::number(std::lround(value));
            break;
        case core::ParameterUnit::Octaves:
            number = trimmedFixed(value, 2);
            break;
        case core::ParameterUnit::Ratio:
            number = trimmedFixed(value, 2);
            break;
        case core::ParameterUnit::None:
            number = trimmedFixed(value, 2);
            break;
    }

    const QString suffix = unitSuffix(unit);
    return suffix.isEmpty() ? number : number + QLatin1Char(' ') + suffix;
}

QString formatParameterByUnitName(const QString& unitName, double value) {
    return formatParameter(unitFromName(unitName), value);
}

}  // namespace drumas::ui
