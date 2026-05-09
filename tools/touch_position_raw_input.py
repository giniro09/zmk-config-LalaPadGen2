"""Windows Raw Input viewer for LaLaPad IQS9151 touch-position diagnosis.

Run this on Windows while the left-side firmware has
CONFIG_INPUT_IQS9151_TOUCH_POSITION_DIAG=y.

The firmware emits a small relative-mouse protocol:
  (-range, -range)              frame reset
  (finger1_x_1_to_range, y)     finger1, when valid
  (finger2_x_1_to_range, y)     finger2, when valid

This tool reads Raw Input directly, filters for that protocol, and renders up
to two touch points. It avoids browser Pointer Lock and normal cursor position.
"""

from __future__ import annotations

import ctypes
import tkinter as tk
from ctypes import wintypes
from dataclasses import dataclass
from time import perf_counter


RANGE = 800
WM_INPUT = 0x00FF
RID_INPUT = 0x10000003
RIM_TYPEMOUSE = 0
RIDEV_INPUTSINK = 0x00000100
GWL_WNDPROC = -4

ULONG_PTR = wintypes.WPARAM
LONG_PTR = wintypes.LPARAM
LRESULT = wintypes.LPARAM
WNDPROC = ctypes.WINFUNCTYPE(LRESULT, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)


class RAWINPUTDEVICE(ctypes.Structure):
    _fields_ = [
        ("usUsagePage", wintypes.USHORT),
        ("usUsage", wintypes.USHORT),
        ("dwFlags", wintypes.DWORD),
        ("hwndTarget", wintypes.HWND),
    ]


class RAWINPUTHEADER(ctypes.Structure):
    _fields_ = [
        ("dwType", wintypes.DWORD),
        ("dwSize", wintypes.DWORD),
        ("hDevice", wintypes.HANDLE),
        ("wParam", wintypes.WPARAM),
    ]


class RAWMOUSE_BUTTONS_STRUCT(ctypes.Structure):
    _fields_ = [
        ("usButtonFlags", wintypes.USHORT),
        ("usButtonData", wintypes.USHORT),
    ]


class RAWMOUSE_BUTTONS_UNION(ctypes.Union):
    _fields_ = [
        ("ulButtons", wintypes.ULONG),
        ("buttons", RAWMOUSE_BUTTONS_STRUCT),
    ]


class RAWMOUSE(ctypes.Structure):
    _anonymous_ = ("button_union",)
    _fields_ = [
        ("usFlags", wintypes.USHORT),
        ("button_union", RAWMOUSE_BUTTONS_UNION),
        ("ulRawButtons", wintypes.ULONG),
        ("lLastX", wintypes.LONG),
        ("lLastY", wintypes.LONG),
        ("ulExtraInformation", wintypes.ULONG),
    ]


class RAWINPUTDATA(ctypes.Union):
    _fields_ = [("mouse", RAWMOUSE)]


class RAWINPUT(ctypes.Structure):
    _fields_ = [
        ("header", RAWINPUTHEADER),
        ("data", RAWINPUTDATA),
    ]


user32 = ctypes.windll.user32
CallWindowProc = user32.CallWindowProcW
CallWindowProc.argtypes = [LONG_PTR, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
CallWindowProc.restype = LRESULT

GetRawInputData = user32.GetRawInputData
GetRawInputData.argtypes = [
    wintypes.HANDLE,
    wintypes.UINT,
    wintypes.LPVOID,
    ctypes.POINTER(wintypes.UINT),
    wintypes.UINT,
]
GetRawInputData.restype = wintypes.UINT

RegisterRawInputDevices = user32.RegisterRawInputDevices
RegisterRawInputDevices.argtypes = [
    ctypes.POINTER(RAWINPUTDEVICE),
    wintypes.UINT,
    wintypes.UINT,
]
RegisterRawInputDevices.restype = wintypes.BOOL

SetWindowLongPtr = user32.SetWindowLongPtrW
SetWindowLongPtr.argtypes = [wintypes.HWND, ctypes.c_int, LONG_PTR]
SetWindowLongPtr.restype = LONG_PTR


@dataclass
class Point:
    x: int
    y: int


class RawTouchViewer:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("LaLaPad Touch Position Raw Input")
        self.root.geometry("900x620")
        self.root.minsize(720, 500)

        self.target_device: int | None = None
        self.points: list[Point] = []
        self.last_sample_at = 0.0
        self.events_this_second = 0
        self.events_per_second = 0
        self.raw_events = 0
        self.diag_markers = 0

        self.canvas = tk.Canvas(self.root, bg="#ffffff", highlightthickness=0)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        panel = tk.Frame(self.root, width=260, padx=10, pady=10)
        panel.pack(side=tk.RIGHT, fill=tk.Y)
        panel.pack_propagate(False)

        tk.Label(panel, text="Touch Position", font=("Segoe UI", 16, "bold"), anchor="w").pack(fill=tk.X)
        tk.Label(
            panel,
            text=(
                "Blue = finger1, orange = finger2.\n"
                "No button is needed first. Flash both halves with the "
                "diagnostic firmware, then touch the left trackpad."
            ),
            justify=tk.LEFT,
            wraplength=235,
            fg="#4b5563",
        ).pack(fill=tk.X, pady=(4, 12))

        self.state_var = tk.StringVar(value="waiting")
        self.device_var = tk.StringVar(value="device: unlocked")
        self.point_var = tk.StringVar(value="points: -")
        self.rate_var = tk.StringVar(value="events/s: 0")
        self.raw_var = tk.StringVar(value="raw mouse events: 0")
        self.marker_var = tk.StringVar(value="diag markers: 0")

        for var in (
            self.state_var,
            self.device_var,
            self.point_var,
            self.rate_var,
            self.raw_var,
            self.marker_var,
        ):
            tk.Label(panel, textvariable=var, anchor="w", font=("Consolas", 11)).pack(fill=tk.X, pady=3)

        tk.Button(panel, text="Unlock Device Filter", command=self.unlock_device).pack(fill=tk.X, pady=(12, 4))
        tk.Button(panel, text="Clear", command=self.clear_points).pack(fill=tk.X)

        self.log = tk.Text(panel, height=12, wrap=tk.WORD, font=("Consolas", 9))
        self.log.pack(fill=tk.BOTH, expand=True, pady=(12, 0))

        self.root.update_idletasks()
        self.hwnd = wintypes.HWND(self.root.winfo_id())
        self._wndproc = WNDPROC(self._handle_message)
        wndproc_ptr = ctypes.cast(self._wndproc, ctypes.c_void_p).value
        self._old_wndproc = SetWindowLongPtr(self.hwnd, GWL_WNDPROC, wndproc_ptr)

        self.register_raw_mouse()
        self.root.bind("<Configure>", lambda _event: self.draw())
        self.root.after(80, self.tick)
        self.root.after(1000, self.rate_tick)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def register_raw_mouse(self) -> None:
        rid = RAWINPUTDEVICE(0x01, 0x02, RIDEV_INPUTSINK, self.hwnd)
        ok = RegisterRawInputDevices(ctypes.byref(rid), 1, ctypes.sizeof(rid))
        if not ok:
            raise ctypes.WinError()
        self.add_log("registered raw mouse input")

    def _handle_message(self, hwnd: int, msg: int, wparam: int, lparam: int) -> int:
        if msg == WM_INPUT:
            self.handle_raw_input(lparam)
            return 0
        return CallWindowProc(self._old_wndproc, hwnd, msg, wparam, lparam)

    def handle_raw_input(self, lparam: int) -> None:
        size = wintypes.UINT(0)
        header_size = ctypes.sizeof(RAWINPUTHEADER)
        result = GetRawInputData(lparam, RID_INPUT, None, ctypes.byref(size), header_size)
        if result == wintypes.UINT(-1).value or size.value == 0:
            return

        buffer = ctypes.create_string_buffer(size.value)
        result = GetRawInputData(lparam, RID_INPUT, buffer, ctypes.byref(size), header_size)
        if result == wintypes.UINT(-1).value:
            return

        raw = ctypes.cast(buffer, ctypes.POINTER(RAWINPUT)).contents
        if raw.header.dwType != RIM_TYPEMOUSE:
            return

        device = int(raw.header.hDevice)
        dx = int(raw.data.mouse.lLastX)
        dy = int(raw.data.mouse.lLastY)
        self.raw_events += 1
        self.handle_diag_pair(device, dx, dy)

    def handle_diag_pair(self, device: int, dx: int, dy: int) -> None:
        if dx == -RANGE and dy == -RANGE:
            self.diag_markers += 1
            if self.target_device is None:
                self.target_device = device
                self.add_log(f"locked device: 0x{device:x}")
            if device == self.target_device:
                self.points = []
                self.last_sample_at = perf_counter()
                self.events_this_second += 1
            return

        if self.target_device is not None and device != self.target_device:
            return

        if 1 <= dx <= RANGE and 1 <= dy <= RANGE:
            if self.target_device is None:
                return
            if len(self.points) < 2:
                self.points.append(Point(dx, dy))
            else:
                self.points[-1] = Point(dx, dy)
            self.last_sample_at = perf_counter()
            self.events_this_second += 1
            self.draw()

    def unlock_device(self) -> None:
        self.target_device = None
        self.points = []
        self.add_log("device unlocked")
        self.draw()

    def clear_points(self) -> None:
        self.points = []
        self.last_sample_at = 0.0
        self.draw()

    def add_log(self, text: str) -> None:
        self.log.insert("1.0", text + "\n")

    def tick(self) -> None:
        if self.last_sample_at and perf_counter() - self.last_sample_at > 0.35:
            self.points = []
            self.draw()
        self.root.after(80, self.tick)

    def rate_tick(self) -> None:
        self.events_per_second = self.events_this_second
        self.events_this_second = 0
        self.rate_var.set(f"events/s: {self.events_per_second}")
        self.raw_var.set(f"raw mouse events: {self.raw_events}")
        self.marker_var.set(f"diag markers: {self.diag_markers}")
        self.root.after(1000, self.rate_tick)

    def draw(self) -> None:
        w = max(1, self.canvas.winfo_width())
        h = max(1, self.canvas.winfo_height())
        self.canvas.delete("all")

        for i in range(1, 10):
            x = w * i / 10
            y = h * i / 10
            self.canvas.create_line(x, 0, x, h, fill="#e5e7eb")
            self.canvas.create_line(0, y, w, y, fill="#e5e7eb")

        self.canvas.create_line(w / 2, 0, w / 2, h, fill="#9ca3af")
        self.canvas.create_line(0, h / 2, w, h / 2, fill="#9ca3af")

        colors = [("#0078d4", "finger1"), ("#d83b01", "finger2")]
        for point, (color, label) in zip(self.points, colors):
            x = (point.x - 1) / RANGE * w
            y = (point.y - 1) / RANGE * h
            r = 15 if label == "finger1" else 12
            self.canvas.create_oval(x - r, y - r, x + r, y + r, outline=color, width=3, fill="")
            self.canvas.create_text(x + r + 6, y, text=label, anchor=tk.W, fill=color, font=("Segoe UI", 10, "bold"))

        device = "unlocked" if self.target_device is None else f"0x{self.target_device:x}"
        self.device_var.set(f"device: {device}")
        self.state_var.set(f"state: {len(self.points)} touch{'es' if len(self.points) != 1 else ''}")
        if self.points:
            text = " ".join(f"f{i + 1}=({p.x},{p.y})" for i, p in enumerate(self.points))
        else:
            text = "points: -"
        self.point_var.set(text)

    def close(self) -> None:
        if self._old_wndproc:
            SetWindowLongPtr(self.hwnd, GWL_WNDPROC, self._old_wndproc)
        self.root.destroy()

    def run(self) -> None:
        self.draw()
        self.root.mainloop()


if __name__ == "__main__":
    RawTouchViewer().run()
