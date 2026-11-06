#!/bin/sh
# Fence-token driver for the Colorer fence tokens (story markdown-code-highlight).
# Builds the driver, then runs it.  R1: no --case runs all cases.  The plugin
# cases need the full build: HIGH_BUILD names it, default
# ../far2l.markdown-code-highlight.build, falling back to ../far2l.build.
set -u
script_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$script_dir/../.." && pwd)
build_dir=${HIGH_BUILD:-../far2l.markdown-code-highlight.build}
app_dir=$(cd "$project_dir/$build_dir" 2>/dev/null && pwd)
if [ -z "$app_dir" ]; then
  build_dir=../far2l.build
  app_dir=$(cd "$project_dir/$build_dir" 2>/dev/null && pwd)
fi
if [ -z "$app_dir" ]; then
  echo "no build dir: run the check (see AGENTS.md) first"
  exit 2
fi
build="$app_dir"

mkdir -p "$build/testing"
cand="$build/highlight_driver"
src="$script_dir/highlight_driver.cpp"
if [ ! -f "$cand" ] || [ "$src" -nt "$cand" ]; then
  # -Wl,-u pins each WINPORT_* entry point so the plugin's dynamic links find
  # them in the driver image.
  undefs=""
  for sym in $(nm "$build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide" | awk '/U _WINPORT_/ {print $2}'); do
    undefs="$undefs -Wl,-u,$sym"
  done
  c++ -std=c++17 -o "$cand" -I "$project_dir/far2l/far2sdk" -I "$project_dir/WinPort" \
    "$script_dir/highlight_driver.cpp" \
    $undefs "$build/WinPort/libWinPort.a" "$build/utils/libutils.a" \
    "$build/WinPort/wineguts/libwineguts.a" -framework CoreFoundation \
    2>"$build/testing/highlight-driver.log" || {
    echo "FAILED build: $(cat "$build/testing/highlight-driver.log")"; exit 1; }
fi

# The app exports its symbols on macOS; the plugin links them from the binary.
export FAR2L_APP="$build/install/far2l.app/Contents/MacOS/far2l"
export FAR2L_PLUGIN="$build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide"
if [ ! -f "$FAR2L_APP" ] || [ ! -f "$FAR2L_PLUGIN" ]; then
  echo "no app or plugin under $build/install: run cmake --build $build --target all install first"
  exit 2
fi

"$cand" "$@" || { echo "FAILED: $1 $2"; exit 1; }
echo "DONE"
