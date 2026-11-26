# Plugin editor windows and out-of-process hosting: evaluation (Phase 5)

Status: **evaluation only, awaiting approval.** Nothing here is implemented yet.

## Where we are

- VST3 and LV2 plugins run **inside MicForge, on the audio thread**. A plugin that crashes takes MicForge,
  and with it the "MicForge (processed mic)" virtual mic, down mid-call. A plugin that blocks causes
  dropouts. Mitigations today: VST3 bundles are *scanned* in a helper process; a plugin that crashed while
  loading is switched off on the next start; the new block list; per-plugin CPU readout.
- Plugins are controlled through **generated sliders**, not their own windows.

## Why editor windows are hard here

| Fact | Consequence |
|---|---|
| VST3 editors on Linux attach to an **X11** window (`kPlatformTypeX11EmbedWindowID`) and need the host to provide an `IRunLoop` (file-descriptor and timer callbacks). | We need an X11 window and a run loop the plugin can register with. |
| Most LV2 UIs are X11 (some GTK). Hosting them properly needs **suil** (wrapper library). | suil would be a **new dependency**. |
| MicForge runs as a native **Wayland** client (Qt Wayland). One Qt process uses one platform plugin. | An X11 window can only come from XWayland, i.e. from a process running Qt's `xcb` platform. |

## Options

**A. Run all of MicForge under XWayland (`QT_QPA_PLATFORM=xcb`) and embed editors in-process.**
Least code. But MicForge loses native Wayland (blurrier fractional scaling, worse tray/portal integration),
and editors still crash MicForge. *Not recommended.*

**B. Editors only, in a helper process; plugins still process audio in MicForge.**
The editor needs the same plugin instance that processes audio, so the helper would have to control a
remote instance: VST3 separates processor and controller in theory, but many plugins don't support a
controller in another process. *Fragile; not recommended.*

**C. Out-of-process plugin host ("bridge"), one helper per plugin, which also shows its editor.** *(recommended)*
- `micforge --plugin-host` runs the plugin (and, under `xcb`, its editor window).
- Audio: a shared-memory block + an `eventfd`/futex pair per direction. Each block, the audio thread writes
  input, signals the helper, and waits for the result with a deadline (e.g. half a block). If the
  helper misses the deadline, that block passes through dry and the miss is counted in Diagnostics.
- Parameters and state: a small message channel (lock-free queue in the same shared memory).
- A helper that crashes or hangs is restarted (or switched off) **while MicForge and the virtual mic keep
  running**. That's the main gain.
- The same design works on Windows later (shared memory + events), which fits Phase 8–10.
- Costs: one extra context switch per block per plugin (roughly 10–50 µs on a desktop at 256 frames,
  to be measured), the helper needs real-time scheduling to avoid misses, and it's the largest
  architectural change since Phase 2: process lifecycle, state sync, crash recovery, tests.
- Rough effort: several working sessions. LV2 UIs additionally need **suil** (new dependency).

**D. Keep in-process, no editors.** Current behaviour plus this phase's safety nets (block list, CPU,
crash-on-load guard). Zero risk, but no editors and no crash isolation while running.

## Recommendation

C, in steps, each shippable on its own:
1. Out-of-process processing for VST3 and LV2 with deadline fallback and automatic restart; an option per
   plugin ("Run separately: safer, slightly more CPU"), default on for VST3. Measured latency and CPU
   overhead added to the test results before it becomes the default.
2. VST3 editor windows in the helper (X11 via XWayland).
3. LV2 editor windows via suil (needs approval for the dependency).

Until then: option D.
