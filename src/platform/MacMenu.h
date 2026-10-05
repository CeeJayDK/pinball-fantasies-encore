#pragma once
// macOS only: the menu bar SDL gives the game, made cheaper to have.

namespace encore {

/// Makes the Window menu an ordinary menu with the same commands, so the system no longer
/// keeps it up to date with what it knows of windows and nearby devices.
void plainWindowMenu();

}  // namespace encore
