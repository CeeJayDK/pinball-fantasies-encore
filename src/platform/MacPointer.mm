#include "platform/MacPointer.h"

#import <AppKit/AppKit.h>

namespace encore {

void macHidePointer(bool hide) {
  // NSCursor counts its hides, each to be undone by a show: only the changes are passed on.
  static bool hidden = false;
  if (hide == hidden) return;
  hidden = hide;
  if (hide)
    [NSCursor hide];
  else
    [NSCursor unhide];
}

}  // namespace encore
