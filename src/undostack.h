// Undo/redo for sound settings. Every parameter change is recorded as (key, before, after). Changes to the
// same setting in quick succession (a slider drag) merge into one step; grouped operations (loading a
// preset, applying Setup, switching microphones) are a single step. Qt Core only (QString keys).
#pragma once

#include <QString>
#include <QVector>

#include <algorithm>
#include <cstdint>

class UndoStack {
public:
    struct Change { QString key; double before, after; };
    struct Step { QString label; QVector<Change> changes; int64_t timeMs = 0; bool grouped = false; };

    static constexpr int kMaxSteps = 100;
    static constexpr int64_t kMergeMs = 700;

    void beginGroup(const QString &label) {
        if (m_depth++ == 0) m_group = Step{label, {}, 0, true};
    }
    void endGroup(int64_t nowMs) {
        if (m_depth == 0 || --m_depth > 0) return;
        m_group.timeMs = nowMs;
        if (!m_group.changes.isEmpty()) pushStep(std::move(m_group));
        m_group = Step{};
    }

    void record(const QString &key, double before, double after, int64_t nowMs, const QString &label) {
        if (before == after) return;
        if (m_depth > 0) { add(m_group, key, before, after); return; }
        // Merge with the previous step if it's the same single setting, changed moments ago.
        if (!m_undo.isEmpty()) {
            Step &last = m_undo.last();
            if (!last.grouped && last.changes.size() == 1 && last.changes[0].key == key && nowMs - last.timeMs < kMergeMs) {
                last.changes[0].after = after;
                last.timeMs = nowMs;
                if (last.changes[0].before == after) m_undo.removeLast(); // back where it started: nothing to undo
                m_redo.clear();
                return;
            }
        }
        Step s{label, {}, nowMs, false};
        s.changes.push_back({key, before, after});
        pushStep(std::move(s));
    }

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString undoLabel() const { return m_undo.isEmpty() ? QString() : m_undo.last().label; }
    QString redoLabel() const { return m_redo.isEmpty() ? QString() : m_redo.last().label; }

    // The step to apply: undo -> set each `before` (in reverse order); redo -> set each `after`.
    Step takeUndo() { Step s = m_undo.takeLast(); m_redo.push_back(s); return s; }
    Step takeRedo() { Step s = m_redo.takeLast(); m_undo.push_back(s); return s; }
    void clear() { m_undo.clear(); m_redo.clear(); }

private:
    static void add(Step &s, const QString &key, double before, double after) {
        for (Change &c : s.changes)
            if (c.key == key) { c.after = after; return; }
        s.changes.push_back({key, before, after});
    }
    void pushStep(Step &&s) {
        m_undo.push_back(std::move(s));
        if (m_undo.size() > kMaxSteps) m_undo.removeFirst();
        m_redo.clear();
    }
    QVector<Step> m_undo, m_redo;
    Step m_group;
    int m_depth = 0;
};
