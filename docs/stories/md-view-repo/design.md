---
description: How the md-view repo is cut out of far2l and laid out, and the choices behind it.
---

# Design: md-view-repo

Why this file exists: the story makes a new repo, so the layout and the install shape are architecture.

## Module shape

```
bin/md-view        new, 3-line launcher     exec "<share>/md-view" "$@"
lib/md-view        moved script, stateful   the md-open.sh contract with new names
lib/*.js *.css     moved, vendored          read from the folder of lib/md-view
tests/*.sh         moved and new            sh tests, MD_OPEN env picks the script
Makefile           new                      install, uninstall, check
```

Dependencies point one way: `bin/md-view -> lib/md-view -> lib/vendored files`. Tests call `lib/md-view` directly.

## Decisions

1. **Rewrite the paths in history with `git filter-repo --path-rename`.** The history then shows the final layout from the first commit. Rejected: keep the far2l paths and move them in one last commit, because then `git log -- lib/md-view` needs `--follow` forever.
2. **Run filter-repo on a fresh `git clone --no-local` of the far2l repo, not on the far2l checkout.** filter-repo rewrites the repo it runs in. Rejected: run it in place, because it would destroy the far2l history.
3. **`bin/md-view` is a small launcher that runs `lib/md-view` with `exec`.** The script finds its vendored files from `dirname "$0"`. A symlink in `bin/` gives `$0` in `bin/`. Rejected: a symlink with link resolution in the script, because `readlink -f` is not on older macOS, and the loop adds code.
4. **The vendored files stay beside the script in `lib/`.** The script reads them from its own folder, so this needs no code change. Rejected: a `vendor/` folder, because it changes the lookup code and the error tests.
5. **The `far2l-md-*` names in the page and in localStorage become `md-view-*`.** md-view must not name far2l. A reader loses the saved width and theme once. Rejected: keep the old keys, because they name far2l for good.
6. **License GPL-2.0.** The script came from far2l, which is GPL-2.0. The vendored files are MIT and BSD-3, which GPL-2.0 allows. Rejected: MIT, because the owner did not relicense the far2l code.
7. **`--print-association` keeps only its far2l entry.** It is help text. far2l does not read it. The `mc.ext` rule is in the README. Rejected: an `mc.ext` block in the script, because it is new behavior with no need.

## Risks for review

- The filter-repo command: a wrong `--path` list drops a commit or keeps a far2l file.
- Missed old names: grep for `far2l`, `md-open` and `settings/md-view.ini` in `lib/` and `tests/`.
- The launcher must pass every argument unchanged, including names with spaces.
