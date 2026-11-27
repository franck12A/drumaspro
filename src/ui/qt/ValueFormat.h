#pragma once

#include <QString>

#include "core/ParameterRegistry.h"

namespace drumas::ui {

/// User-facing text for a parameter value, including its unit.
///
/// Percent parameters are stored in the 0..1 range but displayed as 0..100 %,
/// and trailing zeros are trimmed so a knob reads "55 %" instead of "0.55 %".
[[nodiscard]] QString formatParameter(core::ParameterUnit unit, double value);

/// Same formatting, taking the unit as the short name used by the QML models
/// ("Hz", "ms", "%", "dB", "st", "ct", "oct", "ratio" or empty).
[[nodiscard]] QString formatParameterByUnitName(const QString& unitName, double value);

}  // namespace drumas::ui
