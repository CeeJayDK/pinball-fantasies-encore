#pragma once
// macOS only: the pointer hidden by AppKit itself.

namespace encore {

/// Hides the pointer, or shows it again, for as long as the game is the active application.
/// SDL hides it through the window's cursor rectangles, which macOS looks at again only now
/// and then: coming in over most edges of the window, and on every move full screen, the
/// pointer was shown. NSCursor's own hiding holds until it is undone.
void macHidePointer(bool hide);

}  // namespace encore
