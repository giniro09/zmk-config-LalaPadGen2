#!/usr/bin/env python3
"""Focused keyboard event logger for comparing perceived input lag.

This does not measure physical switch-to-OS latency. It measures when this
focused Tk window receives key events, which is useful for comparing keyboards
on the same PC and looking for event gaps, drops, or odd hold/release timing.
"""

from __future__ import annotations

import csv
import statistics
import time
import tkinter as tk
from dataclasses import dataclass, field
from pathlib import Path
from tkinter import filedialog, ttk


WATCH_KEYS = ("space", "BackSpace", "Return", "Tab", "s", "o")
MAX_ROWS = 800


def key_name(event: tk.Event) -> str:
    if event.keysym == "space":
        return "space"
    return event.keysym or event.char or "Unknown"


@dataclass
class KeyStats:
    downs: int = 0
    ups: int = 0
    repeats: int = 0
    down_intervals_ms: list[float] = field(default_factory=list)
    hold_ms: list[float] = field(default_factory=list)


class KeyboardLatencyLogger:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Keyboard Latency Logger")
        self.root.geometry("980x640")
        self.root.minsize(820, 520)

        self.events: list[dict[str, str | float | int]] = []
        self.stats: dict[str, KeyStats] = {}
        self.pressed_at: dict[str, float] = {}
        self.last_down_at: dict[str, float] = {}
        self.last_event_at: float | None = None
        self.start_at = time.perf_counter()

        self.status_var = tk.StringVar(value="Click here, then type. Tab/Enter/Backspace are captured.")
        self.summary_var = tk.StringVar(value="")

        self._build_ui()
        self._bind_keys()
        self._refresh_summary()

    def _build_ui(self) -> None:
        root = self.root

        header = ttk.Frame(root, padding=(10, 8))
        header.pack(fill=tk.X)

        ttk.Label(header, textvariable=self.status_var).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(header, text="Clear", command=self.clear).pack(side=tk.RIGHT, padx=(8, 0))
        ttk.Button(header, text="Save CSV", command=self.save_csv).pack(side=tk.RIGHT)

        help_text = (
            "Focus this window and compare LaLaPad vs another keyboard. "
            "The important columns are dt_ms, hold_ms, repeat, and the per-key summary below."
        )
        ttk.Label(root, text=help_text, padding=(10, 0)).pack(fill=tk.X)

        columns = ("#", "t_ms", "dt_ms", "event", "key", "hold_ms", "repeat")
        self.tree = ttk.Treeview(root, columns=columns, show="headings", height=20)
        widths = (60, 100, 100, 90, 140, 100, 80)
        for col, width in zip(columns, widths):
            self.tree.heading(col, text=col)
            self.tree.column(col, width=width, anchor=tk.E if col.endswith("ms") or col == "#" else tk.W)

        yscroll = ttk.Scrollbar(root, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=yscroll.set)

        table = ttk.Frame(root, padding=(10, 8))
        table.pack(fill=tk.BOTH, expand=True)
        self.tree.pack(in_=table, side=tk.LEFT, fill=tk.BOTH, expand=True)
        yscroll.pack(in_=table, side=tk.RIGHT, fill=tk.Y)

        ttk.Label(root, textvariable=self.summary_var, padding=(10, 8), justify=tk.LEFT).pack(fill=tk.X)

        focus_box = tk.Frame(root, height=54, borderwidth=1, relief=tk.SUNKEN, takefocus=True)
        focus_box.pack(fill=tk.X, padx=10, pady=(0, 10))
        focus_box.pack_propagate(False)
        ttk.Label(
            focus_box,
            text="Input capture area. Click here if key events stop. Keys are swallowed; Tab should not move focus.",
            padding=(8, 12),
        ).pack(anchor=tk.W)
        focus_box.focus_set()
        focus_box.bind("<Button-1>", lambda _event: focus_box.focus_set())
        self.focus_box = focus_box

    def _bind_keys(self) -> None:
        self.root.bind_all("<KeyPress>", self.on_key_down, add=False)
        self.root.bind_all("<KeyRelease>", self.on_key_up, add=False)

    def on_key_down(self, event: tk.Event) -> str:
        name = key_name(event)
        now = time.perf_counter()
        stat = self.stats.setdefault(name, KeyStats())

        repeat = name in self.pressed_at
        if repeat:
            stat.repeats += 1
        else:
            self.pressed_at[name] = now

        last_down = self.last_down_at.get(name)
        if last_down is not None:
            stat.down_intervals_ms.append((now - last_down) * 1000.0)
        self.last_down_at[name] = now
        stat.downs += 1

        self._append_event(now, "down", name, hold_ms=None, repeat=repeat)
        return "break"

    def on_key_up(self, event: tk.Event) -> str:
        name = key_name(event)
        now = time.perf_counter()
        stat = self.stats.setdefault(name, KeyStats())

        down_at = self.pressed_at.pop(name, None)
        hold_ms = None
        if down_at is not None:
            hold_ms = (now - down_at) * 1000.0
            stat.hold_ms.append(hold_ms)
        stat.ups += 1

        self._append_event(now, "up", name, hold_ms=hold_ms, repeat=False)
        return "break"

    def _append_event(self, now: float, event_type: str, name: str, hold_ms: float | None, repeat: bool) -> None:
        elapsed_ms = (now - self.start_at) * 1000.0
        dt_ms = None if self.last_event_at is None else (now - self.last_event_at) * 1000.0
        self.last_event_at = now

        row = {
            "#": len(self.events) + 1,
            "t_ms": elapsed_ms,
            "dt_ms": "" if dt_ms is None else dt_ms,
            "event": event_type,
            "key": name,
            "hold_ms": "" if hold_ms is None else hold_ms,
            "repeat": "yes" if repeat else "",
        }
        self.events.append(row)

        values = (
            row["#"],
            f"{elapsed_ms:.1f}",
            "" if dt_ms is None else f"{dt_ms:.1f}",
            event_type,
            name,
            "" if hold_ms is None else f"{hold_ms:.1f}",
            row["repeat"],
        )
        self.tree.insert("", tk.END, values=values)
        if len(self.tree.get_children()) > MAX_ROWS:
            self.tree.delete(self.tree.get_children()[0])
        self.tree.yview_moveto(1.0)
        self._refresh_summary()

    def _refresh_summary(self) -> None:
        names = [name for name in WATCH_KEYS if name in self.stats]
        names += sorted(name for name in self.stats if name not in WATCH_KEYS)
        lines = []
        for name in names[:12]:
            stat = self.stats[name]
            interval = self._fmt_samples(stat.down_intervals_ms)
            hold = self._fmt_samples(stat.hold_ms)
            lines.append(
                f"{name:>10}: down={stat.downs:3d} up={stat.ups:3d} repeat={stat.repeats:3d} "
                f"down-interval {interval} hold {hold}"
            )
        self.summary_var.set("\n".join(lines) if lines else "No key events yet.")

    @staticmethod
    def _fmt_samples(samples: list[float]) -> str:
        if not samples:
            return "n/a"
        avg = statistics.fmean(samples)
        worst = max(samples)
        if len(samples) >= 2:
            stdev = statistics.pstdev(samples)
            return f"avg={avg:.1f}ms max={worst:.1f}ms sd={stdev:.1f}"
        return f"avg={avg:.1f}ms max={worst:.1f}ms"

    def clear(self) -> None:
        self.events.clear()
        self.stats.clear()
        self.pressed_at.clear()
        self.last_down_at.clear()
        self.last_event_at = None
        self.start_at = time.perf_counter()
        for item in self.tree.get_children():
            self.tree.delete(item)
        self._refresh_summary()
        self.status_var.set("Cleared. Type again while this window is focused.")

    def save_csv(self) -> None:
        default = Path.cwd() / "keyboard_latency_log.csv"
        path = filedialog.asksaveasfilename(
            title="Save keyboard latency log",
            defaultextension=".csv",
            initialfile=default.name,
            filetypes=(("CSV files", "*.csv"), ("All files", "*.*")),
        )
        if not path:
            return
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=["#", "t_ms", "dt_ms", "event", "key", "hold_ms", "repeat"])
            writer.writeheader()
            for row in self.events:
                out = dict(row)
                for field in ("t_ms", "dt_ms", "hold_ms"):
                    if isinstance(out[field], float):
                        out[field] = f"{out[field]:.3f}"
                writer.writerow(out)
        self.status_var.set(f"Saved: {path}")


def main() -> None:
    root = tk.Tk()
    KeyboardLatencyLogger(root)
    root.mainloop()


if __name__ == "__main__":
    main()
