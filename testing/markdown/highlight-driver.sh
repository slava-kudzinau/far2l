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
link_file="$build/far2l/CMakeFiles/far2l.dir/link.txt"
app_bin="$build/install/far2l.app/Contents/MacOS/far2l"
if [ ! -f "$cand" ] || [ "$src" -nt "$cand" ] || [ "$app_bin" -nt "$cand" ]; then
  # The driver image carries the whole far2l core (the app's own link line),
  # so CtrlObject and the plugin's dynamic links resolve inside one binary;
  # the colorer module is the only dlopen.
  undefs=""
  for sym in $(nm "$build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide" | awk '/U _WINPORT_/ {print $2}'); do
    undefs="$undefs -Wl,-u,$sym"
  done
  obj="$build/testing/highlight_driver.o"
  incs="-I $project_dir/far2l -I $project_dir/far2l/far2sdk -I $project_dir/far2l/src"
  for d in base mix bookmarks cfg console panels filemask hist locale macro plug vt shoco markdown; do
    incs="$incs -I $project_dir/far2l/src/$d"
  done
  c++ -std=c++17 -c -o "$obj" $incs \
    -I "$project_dir/far2l/src/markdown/md4c" -I "$build/far2l" \
    -I "$project_dir/WinPort" -I "$project_dir/utils/include" \
    "$script_dir/highlight_driver.cpp" \
    2>"$build/testing/highlight-driver.log" || {
    echo "FAILED compile: $(cat "$build/testing/highlight-driver.log")"; exit 1; }
  inputs=$(sed -e 's/^[^ ]*//' -e 's/ -o [^ ]*//' -e 's|CMakeFiles/far2l.dir/src/main.cpp.o ||' "$link_file")
  ( cd "$build/far2l" && c++ -o "$cand" "$obj" \
      $inputs -Wl,-undefined,dynamic_lookup -Wl,-undefined,dynamic_lookup ) \
    2>>"$build/testing/highlight-driver.log" || {
    echo "FAILED link: $(cat "$build/testing/highlight-driver.log")"; exit 1; }
fi

# The plugin manager scans `Plugins` next to the running binary: give the
# driver the app's tree there, so FindPlugin sees the colorer module.
ln -sfn install/far2l.app/Contents/MacOS/Plugins "$build/Plugins"

export FAR2L_PLUGIN="$build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide"
if [ ! -f "$FAR2L_PLUGIN" ]; then
  echo "no plugin under $build/install: run cmake --build $build first"
  exit 2
fi

"$cand" "$@" || { echo "FAILED: $1 $2"; exit 1; }
echo "DONE"
