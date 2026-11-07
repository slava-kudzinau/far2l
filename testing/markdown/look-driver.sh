#!/bin/sh
# Look driver for the Colorer look of the F3 Markdown view (story
# markdown-colorer-look).  Builds the driver, then runs it.  R1: no --case
# runs all cases.  The plugin cases need the full build: LOOK_BUILD names it,
# default ../far2l.markdown-colorer-look.build, falling back to ../far2l.build.
# --rgb on the command line runs the driver with a truecolor backend, so the
# plugin's ReloadBase picks the rgb scheme (far2l.hrd).
set -u
script_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$script_dir/../.." && pwd)
build_dir=${LOOK_BUILD:-../far2l.markdown-colorer-look.build}
app_dir=$(cd "$build_dir" 2>/dev/null && pwd) || app_dir=""
if [ -z "$app_dir" ]; then
  app_dir=$(cd "$project_dir/$build_dir" 2>/dev/null && pwd)
fi
if [ -z "$app_dir" ]; then
  build_dir=../far2l.build
  app_dir=$(cd "$project_dir/$build_dir" 2>/dev/null && pwd)
fi
if [ -z "$app_dir" ]; then
  echo "no build dir: run the check (see AGENTS.md) first"
  exit 2
fi
build="$app_dir"

# far2l main loads the language pack before the plugin manager; the driver
# needs it too (ApplySudoConfiguration reads Msg strings).
export FAR2L_APP="$build/install/far2l.app/Contents/MacOS/far2l"

# A truecolor backend for the plugin's ReloadBase (the headless default is
# the 16-color path, which reads console.hrd, not far2l.hrd).  The cases that
# care about the 16-color path switch the backend themselves.
rgb="--rgb"
if [ "${1:-}" = "--rgb" ]; then
  shift
fi

mkdir -p "$build/testing"
cand="$build/look_driver"
src="$script_dir/look_driver.cpp"
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
  incs="-I $project_dir/far2l -I $project_dir/far2l/far2sdk -I $project_dir/far2l/src -I $project_dir/WinPort/src -I $project_dir/WinPort/src/Backend"
  for d in base mix bookmarks cfg console panels filemask hist locale macro plug vt shoco markdown; do
    incs="$incs -I $project_dir/far2l/src/$d"
  done
  obj="$build/testing/look_driver.o"
  c++ -std=c++17 -c -o "$obj" $incs \
    -I "$project_dir/far2l/src/markdown/md4c" -I "$build/far2l" \
    -I "$project_dir/WinPort" -I "$project_dir/utils/include" \
    "$script_dir/look_driver.cpp" \
    2>"$build/testing/look-driver.log" || {
    echo "FAILED compile: $(cat "$build/testing/look-driver.log")"; exit 1; }
  inputs=$(sed -e 's/^[^ ]*//' -e 's/ -o [^ ]*//' -e 's|CMakeFiles/far2l.dir/src/main.cpp.o ||' "$link_file")
  ( cd "$build/far2l" && c++ -o "$cand" "$obj" \
      $inputs -Wl,-undefined,dynamic_lookup -Wl,-undefined,dynamic_lookup ) \
    2>>"$build/testing/look-driver.log" || {
    echo "FAILED link: $(cat "$build/testing/look-driver.log")"; exit 1; }
fi

# The plugin manager scans `Plugins` next to the running binary: give the
# driver the app's tree there, so FindPlugin sees the colorer module.
ln -sfn install/far2l.app/Contents/MacOS/Plugins "$build/Plugins"

FAR2L_PLUGIN="$build/install/far2l.app/Contents/MacOS/Plugins/colorer/plug/colorer.far-plug-wide"
# Resolve to one absolute path: the core's plugin scan reaches the module
# through the Plugins symlink above, the driver through this path; dyld must
# see one path for one module (a relative path here loaded a second module
# instance and the viewer-event init crashed on the second catalog).
FAR2L_PLUGIN="$(cd "$(dirname "$FAR2L_PLUGIN")" && pwd)/$(basename "$FAR2L_PLUGIN")"
export FAR2L_PLUGIN
if [ ! -f "$FAR2L_PLUGIN" ]; then
  echo "no plugin under $build/install: run cmake --build $build first"
  exit 2
fi

"$cand" $rgb "$@" || { echo "FAILED: driver exit 1"; exit 1; }
echo "DONE"
