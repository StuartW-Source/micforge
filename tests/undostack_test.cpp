#include "undostack.h"

#include <cstdio>

static int failures = 0;
static void check(bool ok, const char *what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++failures;
}

int main() {
    UndoStack u;
    // A slider drag: many changes within 700 ms -> one step from the first to the last value.
    u.record("compRatio", 3, 3.5, 1000, "Compressor ratio");
    u.record("compRatio", 3.5, 4, 1100, "Compressor ratio");
    u.record("compRatio", 4, 6, 1500, "Compressor ratio");
    check(u.canUndo() && !u.canRedo(), "drag recorded");
    // A pause, then another change to the same setting: a new step.
    u.record("compRatio", 6, 8, 3000, "Compressor ratio");
    auto s2 = u.takeUndo();
    check(s2.changes.size() == 1 && s2.changes[0].before == 6 && s2.changes[0].after == 8, "after a pause: separate step");
    auto s1 = u.takeUndo();
    check(s1.changes[0].before == 3 && s1.changes[0].after == 6, "drag merged into one step (3 -> 6)");
    check(!u.canUndo() && u.canRedo() && u.redoLabel() == "Compressor ratio", "everything undone; redo available");
    auto r = u.takeRedo();
    check(r.changes[0].after == 6, "redo re-applies");
    u.record("reverbMix", 10, 20, 9000, "Reverb");
    check(!u.canRedo(), "a new change clears redo");

    // Grouped operation: one step with every changed setting.
    u.beginGroup("Load preset “Studio”");
    u.record("reverbMix", 20, 30, 9500, "");
    u.record("echoMix", 0, 10, 9500, "");
    u.record("reverbMix", 30, 35, 9500, "");
    u.endGroup(9500);
    auto g = u.takeUndo();
    check(g.label == "Load preset “Studio”" && g.changes.size() == 2, "preset load is one step");
    check(g.changes[0].before == 20 && g.changes[0].after == 35, "a setting changed twice inside a group keeps its first 'before'");

    // Dragging back to the start leaves nothing to undo.
    UndoStack v;
    v.record("outputGain", 0, 3, 100, "Output");
    v.record("outputGain", 3, 0, 200, "Output");
    check(!v.canUndo(), "returning to the original value cancels the step");

    // History is bounded.
    UndoStack w;
    for (int i = 0; i < 250; ++i) w.record("k" + QString::number(i), 0, 1, i * 1000, "x");
    int n = 0;
    while (w.canUndo()) { w.takeUndo(); ++n; }
    check(n == UndoStack::kMaxSteps, "history keeps the last 100 steps");
    std::printf("\nundo tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
