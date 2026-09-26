#pragma once
// A trial run at pictures that do not travel with the application: the first time the game
// starts, a set is fetched and unpacked beside the game files, ready to be chosen later.
#include <filesystem>
#include <functional>

namespace pfr {

/// Where a downloaded set is unpacked to, inside the game folder.
constexpr const char* kSkinFolder = "hd";

/// Offers the set on the first start and fetches it if it is wanted. False when the offer
/// was turned down, which ends the game; true to carry on, the set being there already, or
/// fetched now, or the fetching having failed -- the game runs with its own pictures then.
/// Puts the offer to whoever is playing and waits for the answer. The game draws this in
/// its own window.
using SkinAsk = std::function<bool()>;

/// One turn of whatever the caller shows while the fetching runs, given the seconds since
/// it started. Called about sixty times a second.
using SkinWaitTick = std::function<void(double seconds)>;

bool downloadSkinOnce(const std::filesystem::path& gameDir, const SkinAsk& ask, const SkinWaitTick& tick);

}  // namespace pfr
