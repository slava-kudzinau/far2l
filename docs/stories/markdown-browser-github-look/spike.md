---
description: Spike of task 1 of the GitHub look story. Do two local pages with different names share localStorage.
---

# Spike: localStorage on two local pages

The test page has 17 lines. It reads the key `spikeKey` from `localStorage`. The copy named `a.html` writes `value-from-a` to the key. The copy named `b.html` only reads it. The two copies are in one folder and open as `file://` URLs.

## Chrome

- Browser: Google Chrome.
- Same value seen: yes. `b.html` read `value-from-a`, and `location.origin` was `file://` on both pages.
- Version: 156.0.8078.80.
- Date: 2026-11-07.

Method: the shell cannot read a result from a window that `open -a` opens. Thus the test ran Chrome headless with a new empty profile folder, once for each page, with the same profile: `Google Chrome --headless --user-data-dir=<new folder> --dump-dom file://<folder>/a.html`, then the same for `b.html`. The test read the result from the dumped DOM.

## Firefox

- Browser: Mozilla Firefox.
- Same value seen: manual, pending the owner. The shell could not drive Firefox here. `firefox --headless --profile <new folder> --screenshot` wrote no file and did not stop in 180 s.
- Version: 159.0.
- Date: 2026-11-07.

Manual steps for the owner:

1. Copy the 17-line page (below) to `a.html` and `b.html` in one new folder.
2. Run `open -a Firefox a.html`. The page shows `RESULT page=a.html before=null ... wrote after=value-from-a`.
3. Run `open -a Firefox b.html`. If the page shows `before=value-from-a`, the second copy sees the value. If it shows `before=null`, it does not.

The page:

```html
<!doctype html>
<html><head><meta charset="utf-8"><title>ls spike</title></head>
<body><pre id="out">no script</pre>
<script>
var name = location.pathname.split('/').pop();
var out = document.getElementById('out');
var r = '';
try {
  var before = localStorage.getItem('spikeKey');
  r += 'page=' + name + ' before=' + before + ' origin=' + location.origin;
  if (name === 'a.html') { localStorage.setItem('spikeKey', 'value-from-a'); r += ' wrote'; }
  r += ' after=' + localStorage.getItem('spikeKey');
} catch (e) { r += ' error=' + e.name + ':' + e.message; }
out.textContent = 'RESULT ' + r;
document.title = 'RESULT ' + r;
</script>
</body></html>
```
