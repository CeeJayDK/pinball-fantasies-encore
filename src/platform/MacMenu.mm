#include "platform/MacMenu.h"

#import <AppKit/AppKit.h>

namespace encore {

// Every key pressed is first offered to the menu bar, in case it is one of its shortcuts, and
// macOS brings the menus up to date before it looks. The one SDL names the Window menu is
// brought up to date by asking the system which iPads are near enough to move a window to,
// and waiting for the answer: tens of milliseconds, the game held up, the music cut. A menu
// of our own with the same commands is never asked about; the window list and the iPads go.
void plainWindowMenu() {
  @autoreleasepool {
    NSMenu* windows = [NSApp windowsMenu];
    if (!windows) return;
    NSMenu* plain = [[NSMenu alloc] initWithTitle:windows.title];
    // SDL's own commands; the system's list of open windows is left behind.
    for (NSMenuItem* item in windows.itemArray)
      if (item.action != @selector(makeKeyAndOrderFront:)) [plain addItem:[item copy]];
    for (NSMenuItem* top in [NSApp mainMenu].itemArray)
      if (top.submenu == windows) top.submenu = plain;
    [NSApp setWindowsMenu:nil];
  }
}

}  // namespace encore
