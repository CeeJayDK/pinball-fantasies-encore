#!/bin/bash
# install.sh — put Pinball Fantasies: Encore! in your menu, for the current user only.
#
# Everything goes under ~/.local, so this needs no root and uninstalling is deleting what it
# lists. Nothing on the system is touched and no shell configuration is edited: PATH is not
# modified, because a program that rewrites your shell profile behind your back is a program
# you cannot trust with anything else.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ID="org.encore.pinball-fantasies"   # the game's app id: the menu entry is matched to its window by it
DEST="$HOME/.local/share/pinball-fantasies-encore"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor"
BIN="$HOME/.local/bin"

echo "Installing to $DEST"
mkdir -p "$DEST" "$APPS" "$BIN"
# The whole folder, not just the program: it finds SDL3 through an rpath of $ORIGIN/lib, so
# lib/ has to stay beside it, and it reads its shaders and pictures from beside itself too.
cp -R "$HERE"/. "$DEST"/
chmod +x "$DEST/Pinball Fantasies"

# A symlink is fine even though the rpath is relative: the loader resolves $ORIGIN against
# the real path, so it still finds $DEST/lib.
ln -sf "$DEST/Pinball Fantasies" "$BIN/pinball-fantasies-encore"
echo "  $BIN/pinball-fantasies-encore"

if [ -f "$HERE/icon.png" ]; then
    for px in 32 64 128 256 512; do
        d="$ICONS/${px}x${px}/apps"
        mkdir -p "$d"
        if command -v magick >/dev/null 2>&1; then
            magick "$HERE/icon.png" -resize "${px}x${px}" "$d/$ID.png"
        else
            cp "$HERE/icon.png" "$d/$ID.png"    # unscaled; the theme copes
        fi
    done
    echo "  $ICONS/*/apps/$ID.png"
fi

# Written here rather than shipped ready-made, because Exec has to be the absolute path of
# this particular install. %f is a recording dropped on the entry, which the game plays. The file is named after the app id, and StartupWMClass repeats it,
# so the desktop puts the window under this entry and its icon.
cat > "$APPS/$ID.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Pinball Fantasies: Encore!
Comment=The 1994 pinball game, natively, with remastered artwork
Exec="$DEST/Pinball Fantasies" %f
Icon=$ID
StartupWMClass=$ID
Terminal=false
Categories=Game;ArcadeGame;
EOF
echo "  $APPS/$ID.desktop"

command -v update-desktop-database >/dev/null 2>&1 \
    && update-desktop-database "$APPS" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null 2>&1 \
    && gtk-update-icon-cache -f -t "$ICONS" 2>/dev/null || true

echo "Done. To uninstall, delete the files listed above and $DEST."
