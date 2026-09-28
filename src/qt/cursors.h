#pragma once

#include <QCursor>

namespace mandelbrotter::qt
{

/// The pick-seed mode's cursor (wx's wxCURSOR_BULLSEYE): two rings and a dot, drawn at run time,
/// with the hot spot at the center.
[[nodiscard]] QCursor bullseyeCursor();

}  // namespace mandelbrotter::qt
