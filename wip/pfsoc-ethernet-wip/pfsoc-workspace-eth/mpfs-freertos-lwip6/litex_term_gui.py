#!/usr/bin/env python3
"""
Graphical User Interface (GUI) for PolarFire SoC / LiteX Serialboot & Flash Utility.
Features:
 - Interactive terminal window (direct keypress forwarding to UART)
 - Standalone '⚡ IAP Programming' Tab for .spi bitstream upload (0x00100000 default offset)
 - Full System Controller IAP suite (Program, Verify, AutoUpdate)
 - Strict /dev/ttyUSB* port filtering on Linux
 - Full Board Identity display upon connection
"""

import sys
import os
import glob
import time
import serial
import threading
import argparse
import json
import socket
import queue
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
from collections import deque

try:
    import serial.tools.list_ports
    HAS_PYSERIAL = True
except ImportError:
    HAS_PYSERIAL = False

# Console Handling ---------------------------------------------------------------------------------

if sys.platform == "win32":
    import ctypes
    import msvcrt
    class Console:
        def configure(self):
            kernel32 = ctypes.windll.kernel32
            kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

        def unconfigure(self):
            pass

        def getkey(self):
            return msvcrt.getch()

        def escape_char(self, b):
            return b == b"\xe0"

        def handle_escape(self, b):
            return {
                b"H" : b"\x1b[A", b"P" : b"\x1b[B", b"K" : b"\x1b[D", b"M" : b"\x1b[C",
                b"G" : b"\x1b[H", b"O" : b"\x1b[F", b"R" : b"\x1b[2~", b"S" : b"\x1b[3~",
            }.get(b, None)
else:
    import termios
    import pty
    class Console:
        def __init__(self):
            try:
                self.fd = sys.stdin.fileno()
                self.is_tty = sys.stdin.isatty()
            except Exception:
                self.is_tty = False

            if self.is_tty:
                self.default_settings = termios.tcgetattr(self.fd)

        def configure(self):
            if getattr(self, "is_tty", False):
                settings = termios.tcgetattr(self.fd)
                settings[3] = settings[3] & ~termios.ICANON & ~termios.ECHO
                settings[6][termios.VMIN] = 1
                settings[6][termios.VTIME] = 0
                termios.tcsetattr(self.fd, termios.TCSANOW, settings)

        def unconfigure(self):
            if getattr(self, "is_tty", False) and hasattr(self, 'default_settings'):
                termios.tcsetattr(self.fd, termios.TCSAFLUSH, self.default_settings)

        def getkey(self):
            return os.read(self.fd, 1)

        def escape_char(self, b):
            return False

        def handle_escape(self, b):
            return None

# SFL Framing & CRC Protocol ----------------------------------------------------------------------

sfl_prompt_req = b"F7:    boot from serial\n"
sfl_prompt_ack = b"\x06"
sfl_magic_req  = b"sL5DdSMmkekro\n"
sfl_magic_ack  = b"z6IHG7cYDID6o\n"

sfl_payload_length      = 255
sfl_address_length      = 4
sfl_data_length         = sfl_payload_length - sfl_address_length
sfl_safe_data_length    = 64
sfl_default_outstanding = 8

sfl_cmd_abort           = b"\x00"
sfl_cmd_load            = b"\x01"
sfl_cmd_jump            = b"\x02"
sfl_cmd_flash           = b"\x03"
sfl_cmd_done            = b"\x04"

sfl_ack_success  = b"K"
sfl_ack_crcerror = b"C"
sfl_ack_unknown  = b"U"
sfl_ack_error    = b"E"

crc16_table = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
]

def crc16(l):
    crc = 0
    for d in l:
        crc = crc16_table[((crc >> 8) ^ d) & 0xff] ^ (crc << 8)
    return crc & 0xffff

class SFLUploadError(Exception):
    def __init__(self, message, reply=None):
        super().__init__(message)
        self.reply = reply

class SFLFrame:
    def __init__(self):
        self.cmd = bytes()
        self.payload = bytes()

    def compute_crc(self):
        return crc16(self.cmd + self.payload)

    def encode(self):
        packet = bytes([len(self.payload)])
        packet += self.compute_crc().to_bytes(2, "big")
        packet += self.cmd
        packet += self.payload
        return packet

class LiteXTerm:
    def __init__(self, serial_boot, kernel_image, kernel_address, json_images, safe, no_boot=False, flash=False, log_fn=None, progress_fn=None):
        self.serial_boot = serial_boot
        self.no_boot = no_boot
        self.flash_mode = flash
        self.mem_regions = {}
        self.log_fn = log_fn
        self.progress_fn = progress_fn

        if kernel_image is not None:
            if flash and kernel_address == "0x40000000":
                kernel_address = "0x20000"
            self.mem_regions = {kernel_image: kernel_address}
            self.boot_address = kernel_address

        self.reader_alive = False
        self.writer_alive = False
        self.prompt_detect_buffer = bytes(len(sfl_prompt_req))
        self.magic_detect_buffer  = bytes(len(sfl_magic_req))
        self.console = Console()

        self.safe        = safe
        self.delay       = 0
        self.length      = sfl_safe_data_length if safe else sfl_data_length
        self.outstanding = 1 if safe else sfl_default_outstanding

    def log(self, msg):
        if self.log_fn:
            self.log_fn(msg)
        else:
            print(msg)

    def update_progress(self, current, total):
        if self.progress_fn:
            self.progress_fn(current, total)

    def open(self, port, baudrate):
        if hasattr(self, "port"):
            return
        self.port_url = port
        self.port = serial.serial_for_url(port, baudrate)

    def close(self):
        if not hasattr(self, "port"):
            return
        self.port.close()
        del self.port

    def read_sfl_reply(self, timeout=1.0):
        previous_timeout = self.port.timeout
        self.port.timeout = timeout
        try:
            reply = self.port.read()
        finally:
            self.port.timeout = previous_timeout
        if reply == b"":
            raise SFLUploadError("timed out waiting for device reply")
        return reply

    def write_sfl_data(self, data, timeout=1.0):
        previous_timeout = self.port.write_timeout
        self.port.write_timeout = timeout
        try:
            written = self.port.write(data)
        except serial.SerialTimeoutException as e:
            raise SFLUploadError("timed out writing device frame") from e
        finally:
            self.port.write_timeout = previous_timeout
        if written is not None and written != len(data):
            raise SFLUploadError(f"short write to device ({written}/{len(data)} bytes)")
        return written

    def drain(self):
        while self.port.in_waiting:
            self.port.read(self.port.in_waiting)

    def send_frame(self, frame, timeout=1.0, retries=16):
        encoded_frame = frame.encode()
        for _ in range(retries):
            try:
                self.write_sfl_data(encoded_frame, timeout=timeout)
                reply = self.read_sfl_reply(timeout=timeout)
            except SFLUploadError as e:
                self.log(f"[LITEX-TERM] {e}, aborting.\n")
                return 0
            if reply == sfl_ack_success:
                return 1
            elif reply == sfl_ack_crcerror:
                continue
            else:
                self.log(f"[LITEX-TERM] Got unknown reply '{reply}' from the device, aborting.\n")
                return 0
        self.log("[LITEX-TERM] Too many CRC errors from the device, aborting.\n")
        return 0

    def receive_upload_response(self, timeout=1.0):
        reply = self.read_sfl_reply(timeout=timeout)
        if reply == sfl_ack_success:
            return True
        elif reply == sfl_ack_crcerror:
            raise SFLUploadError("upload failed due to data corruption (CRC error)", reply=reply)
        elif reply == sfl_ack_error:
            raise SFLUploadError("device reported a serial frame error", reply=reply)
        elif reply == sfl_ack_unknown:
            raise SFLUploadError("device reported an unknown serial command", reply=reply)
        else:
            raise SFLUploadError(f"got unexpected response from device '{reply}'")

    def baudrate(self):
        return getattr(self.port, "baudrate", 115200) or 115200

    def frame_time(self, data_length=None):
        data_length = self.length if data_length is None else data_length
        frame_length = 1 + 2 + 1 + sfl_address_length + data_length
        return (frame_length * 10) / self.baudrate()

    def make_load_frame(self, address, data):
        frame = SFLFrame()
        frame.cmd = sfl_cmd_load
        frame.payload = address.to_bytes(4, "big") + data
        return frame

    def upload(self, filename, address):
        length = os.path.getsize(filename)
        self.log(f"[LITEX-TERM] Uploading {filename} to 0x{address:08x} ({length} bytes)...\n")
        profiles = [(128, 1)] if self.flash_mode else [(self.length, self.outstanding)]

        for n, (data_length, outstanding) in enumerate(profiles):
            try:
                uploaded = self.upload_once(filename, address, length, data_length, outstanding)
                return uploaded
            except SFLUploadError as e:
                self.log(f"\n[LITEX-TERM] Upload failed: {e}\n")
                raise e

    def upload_once(self, filename, address, length, data_length, max_outstanding):
        with open(filename, "rb") as f:
            current_address = address
            position        = 0
            start           = time.time()
            remaining       = length
            outstanding     = deque()
            last_update     = 0.0

            while remaining or outstanding:
                now = time.time()
                if length and now - last_update >= 0.05:
                    pct = 100 * position // length
                    bar = "=" * (20 * position // length)
                    space = " " * (20 - 20 * position // length)
                    self.log(f"|[LITEX-TERM] Uploading: [{bar}>{space}] {pct}%\r")
                    self.update_progress(position, length)
                    last_update = now

                while remaining and len(outstanding) < max_outstanding:
                    frame_data = f.read(min(remaining, data_length))
                    frame = self.make_load_frame(current_address, frame_data)
                    encoded_frame = frame.encode()

                    write_timeout = max(1.0, self.frame_time(data_length) + 0.5)
                    self.write_sfl_data(encoded_frame, timeout=write_timeout)

                    current_address += len(frame_data)
                    position        += len(frame_data)
                    remaining       -= len(frame_data)
                    outstanding.append({"frame": encoded_frame, "retries": 0})
                    time.sleep(self.delay)

                if not outstanding:
                    continue

                ack = self.receive_upload_response(timeout=max(1.0, self.frame_time(data_length) * (len(outstanding) + 1) + 0.5))
                if ack:
                    outstanding.popleft()

            elapsed = max(0.001, time.time() - start)
            self.update_progress(length, length)
            self.log(f"\n[LITEX-TERM] Upload complete ({length/(elapsed*1024):.1f}KB/s).\n")
            return length

    def boot(self):
        self.log("[LITEX-TERM] Sending the Boot Command to the device.\n")
        frame = SFLFrame()
        frame.cmd = sfl_cmd_jump
        frame.payload = int(self.boot_address, 16).to_bytes(4, "big")
        if not self.send_frame(frame):
            raise SFLUploadError("could not send boot command")

    def flash(self, image_size, flash_adr=0x20000):
        self.log(f"[LITEX-TERM] Requesting device to flash {image_size} bytes to offset 0x{flash_adr:08x}.\n")
        time.sleep(0.05)
        self.drain()

        frame = SFLFrame()
        frame.cmd = sfl_cmd_flash
        frame.payload = image_size.to_bytes(4, "big") + flash_adr.to_bytes(4, "big")

        if not self.send_frame(frame, timeout=30.0):
            raise SFLUploadError("could not send flash command")

    def send_done_command(self):
        self.log("[LITEX-TERM] Transmission Completed.\n")
        frame = SFLFrame()
        frame.cmd = sfl_cmd_done
        frame.payload = int(self.boot_address, 16).to_bytes(4, "big")
        if not self.send_frame(frame):
            raise SFLUploadError("could not send EOT command")

    def abort_serialboot(self):
        self.log("[LITEX-TERM] Aborting serial boot.\n")
        time.sleep(0.35)
        self.drain()

        frame = SFLFrame()
        frame.cmd = sfl_cmd_abort

        try:
            self.write_sfl_data(frame.encode(), timeout=1.0)
            reply = self.read_sfl_reply(timeout=1.0)
        except SFLUploadError:
            return False

        return reply == sfl_ack_success

    def detect_magic(self, single_char):
        if len(single_char) == 1:
            self.magic_detect_buffer = self.magic_detect_buffer[1:] + single_char
            return self.magic_detect_buffer == sfl_magic_req
        return False

    def answer_magic(self):
        self.log("\n[LITEX-TERM] Received firmware download request from the device.\n")
        if len(self.mem_regions):
            self.port.write(sfl_magic_ack)
            try:
                if self.flash_mode:
                    total_size = sum(os.path.getsize(f) for f in self.mem_regions.keys())
                    first_base = list(self.mem_regions.values())[0]
                    flash_offset = int(first_base, 16)
                    self.flash(total_size, flash_offset)

                for filename, base in self.mem_regions.items():
                    self.upload(filename, int(base, 16))

                if self.no_boot:
                    if self.flash_mode:
                        self.log("[LITEX-TERM] Direct Flash complete. Returning to prompt (--no-boot).\n")
                        self.send_done_command()
                    else:
                        self.log("[LITEX-TERM] RAM Download complete. Returning to prompt (--no-boot).\n")
                        self.abort_serialboot()
                else:
                    if self.flash_mode:
                        self.log("[LITEX-TERM] Direct Flash Programming complete. Returning to prompt (--no-boot).\n")
                        self.send_done_command()
                    else:
                        self.boot()
            except SFLUploadError as e:
                self.log(f"\n[LITEX-TERM] Serial boot failed: {e}.\n")
                self.abort_serialboot()
            finally:
                # Clear memory regions immediately so next board reboot won't re-trigger serialboot!
                self.mem_regions = {}
        else:
            self.log("[LITEX-TERM] Warning: No kernel image file loaded in GUI!\n")

# Tkinter Graphical User Interface -----------------------------------------------------------------

class LiteXTermGUI:
    def __init__(self, root, args):
        self.root = root
        self.args = args
        self.root.title("PolarFire SoC / LiteX BIOS Control Panel & Flashing Utility")
        self.root.geometry("880x660")
        self.root.minsize(780, 520)

        self.active_term = None
        self.output_queue = queue.Queue()
        self.pending_overwrite = False
        self.terminal_visible = True

        self._create_widgets()
        self.refresh_ports()

        if args.port:
            self.port_combo.set(args.port)
        if args.kernel:
            self.kernel_entry.insert(0, args.kernel)
        if args.kernel_adr:
            self.addr_entry.delete(0, tk.END)
            self.addr_entry.insert(0, args.kernel_adr)
        if args.flash:
            self.mode_var.set("flash")

        self._check_queue()

    def gui_log(self, text):
        self.output_queue.put(("LOG", text))

    def gui_progress(self, current, total):
        self.output_queue.put(("PROGRESS", current, total))

    def append_console_text(self, text):
        """Parse stream input and handle carriage returns (\r) in place to prevent line misalignments."""
        text = text.replace('\r\n', '\n')
        for char in text:
            if char == '\r':
                self.pending_overwrite = True
            elif char == '\n':
                self.pending_overwrite = False
                self.console.insert(tk.END, '\n')
            else:
                if self.pending_overwrite:
                    self.console.delete("end-1c linestart", "end-1c")
                    self.pending_overwrite = False
                self.console.insert(tk.END, char)
        self.console.see(tk.END)

    def _create_widgets(self):
        # 1. Top Serial Connection Frame
        conn_frame = ttk.LabelFrame(self.root, text=" Serial Connection ", padding=6)
        conn_frame.pack(fill="x", padx=8, pady=3)

        ttk.Label(conn_frame, text="Port:").grid(row=0, column=0, sticky="w", padx=3)
        self.port_combo = ttk.Combobox(conn_frame, width=18)
        self.port_combo.grid(row=0, column=1, sticky="w", padx=3)

        refresh_btn = ttk.Button(conn_frame, text="🔄", command=self.refresh_ports, width=3)
        refresh_btn.grid(row=0, column=2, padx=2)

        ttk.Label(conn_frame, text="Baud:").grid(row=0, column=3, sticky="w", padx=6)
        self.speed_combo = ttk.Combobox(conn_frame, values=["115200", "230400", "460800", "921600"], width=8)
        self.speed_combo.set("115200")
        self.speed_combo.grid(row=0, column=4, sticky="w", padx=3)

        self.connect_btn = ttk.Button(conn_frame, text="⚡ Connect", command=self.toggle_connection, width=12)
        self.connect_btn.grid(row=0, column=5, padx=8)

        # Board Identification Badge
        self.ident_label = ttk.Label(conn_frame, text="Board ID: Disconnected", font=("Sans", 9, "bold"), foreground="#666666")
        self.ident_label.grid(row=0, column=6, padx=10, sticky="e")
        conn_frame.columnconfigure(6, weight=1)

        # 2. Tabbed Control Notebook
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill="x", padx=8, pady=3)

        tab_sfl     = ttk.Frame(notebook, padding=6)
        tab_iap     = ttk.Frame(notebook, padding=6)
        tab_system  = ttk.Frame(notebook, padding=6)
        tab_flash   = ttk.Frame(notebook, padding=6)
        tab_memory  = ttk.Frame(notebook, padding=6)

        notebook.add(tab_sfl,     text="📁 SFL File Transfer")
        notebook.add(tab_iap,     text="⚡ IAP Programming")
        notebook.add(tab_system,  text="⚙️ System")
        notebook.add(tab_flash,   text="⚡ Boot & Flash")
        notebook.add(tab_memory,  text="🧠 Memory")

        # TAB 1: File Transfer (SFL)
        ttk.Label(tab_sfl, text="Image File:").grid(row=0, column=0, sticky="w", padx=3, pady=2)
        self.kernel_entry = ttk.Entry(tab_sfl, width=50)
        self.kernel_entry.grid(row=0, column=1, columnspan=2, sticky="ew", padx=3, pady=2)

        browse_btn = ttk.Button(tab_sfl, text="Browse...", command=lambda: self.browse_file(self.kernel_entry), width=9)
        browse_btn.grid(row=0, column=3, padx=3, pady=2)

        ttk.Label(tab_sfl, text="Target Mode:").grid(row=1, column=0, sticky="w", padx=3, pady=4)
        self.mode_var = tk.StringVar(value="flash")
        ram_radio = ttk.Radiobutton(tab_sfl, text="RAM (serialboot)", value="ram", variable=self.mode_var, command=self.on_mode_change)
        ram_radio.grid(row=1, column=1, sticky="w", padx=3)

        flash_radio = ttk.Radiobutton(tab_sfl, text="SPI Flash (flashwrite)", value="flash", variable=self.mode_var, command=self.on_mode_change)
        flash_radio.grid(row=1, column=2, sticky="w", padx=3)

        ttk.Label(tab_sfl, text="Address Offset:").grid(row=2, column=0, sticky="w", padx=3, pady=2)
        self.addr_entry = ttk.Entry(tab_sfl, width=18)
        self.addr_entry.insert(0, "0x00020000")
        self.addr_entry.grid(row=2, column=1, sticky="w", padx=3, pady=2)

        self.noboot_var = tk.BooleanVar(value=True)
        noboot_chk = ttk.Checkbutton(tab_sfl, text="No Boot (--no-boot)", variable=self.noboot_var)
        noboot_chk.grid(row=2, column=2, sticky="w", padx=3, pady=2)

        progress_frame = ttk.LabelFrame(tab_sfl, text=" File Transfer Progress ", padding=4)
        progress_frame.grid(row=3, column=0, columnspan=4, sticky="ew", pady=4)

        self.progress_bar = ttk.Progressbar(progress_frame, orient="horizontal", mode="determinate")
        self.progress_bar.pack(fill="x", expand=True, padx=3, pady=1)

        self.progress_label = ttk.Label(progress_frame, text="Idle - 0.0% (0 KB / 0 KB)", font=("Sans", 8))
        self.progress_label.pack(anchor="w", padx=3, pady=1)

        sfl_btn = ttk.Button(tab_sfl, text="▶ Start SFL File Upload", command=self.start_sfl_upload)
        sfl_btn.grid(row=4, column=0, columnspan=4, pady=4, sticky="ew")
        tab_sfl.columnconfigure(1, weight=1)

        # TAB 2: Dedicated IAP Programming Tab
        iap_upload_frame = ttk.LabelFrame(tab_iap, text=" 1. Flash Bitstream (.spi) to SPI Flash Memory ", padding=6)
        iap_upload_frame.pack(fill="x", pady=4)

        ttk.Label(iap_upload_frame, text="Bitstream (.spi):").grid(row=0, column=0, sticky="w", padx=3, pady=2)
        self.spi_entry = ttk.Entry(iap_upload_frame, width=45)
        self.spi_entry.grid(row=0, column=1, sticky="ew", padx=3, pady=2)
        ttk.Button(iap_upload_frame, text="Browse...", command=lambda: self.browse_file(self.spi_entry, [("SPI Bitstreams", "*.spi"), ("All Files", "*.*")]), width=8).grid(row=0, column=2, padx=3)

        ttk.Label(iap_upload_frame, text="Flash Offset:").grid(row=1, column=0, sticky="w", padx=3, pady=2)
        self.spi_off_entry = ttk.Entry(iap_upload_frame, width=18)
        self.spi_off_entry.insert(0, "0x00100000")  # Default 1MB Offset for IAP
        self.spi_off_entry.grid(row=1, column=1, sticky="w", padx=3, pady=2)

        ttk.Button(iap_upload_frame, text="▶ Upload .spi to SPI Flash (flashwrite)", command=self.upload_spi_file).grid(row=1, column=2, columnspan=2, sticky="ew", padx=3, pady=4)
        iap_upload_frame.columnconfigure(1, weight=1)

        iap_cmd_frame = ttk.LabelFrame(tab_iap, text=" 2. Execute System Controller IAP Service (sys_iap) ", padding=6)
        iap_cmd_frame.pack(fill="x", pady=6)

        ttk.Label(iap_cmd_frame, text="Target Addr / Index:").grid(row=0, column=0, sticky="w", padx=3, pady=2)
        self.iap_target_entry = ttk.Entry(iap_cmd_frame, width=18)
        self.iap_target_entry.insert(0, "0x00100000")
        self.iap_target_entry.grid(row=0, column=1, sticky="w", padx=3, pady=2)

        ttk.Label(iap_cmd_frame, text="IAP Mode:").grid(row=0, column=2, sticky="w", padx=3, pady=2)
        self.iap_mode_combo = ttk.Combobox(iap_cmd_frame, values=[
            "program_addr", "verify_addr", "program_idx", "verify_idx", "autoupdate"
        ], width=16)
        self.iap_mode_combo.set("program_addr")
        self.iap_mode_combo.grid(row=0, column=3, sticky="w", padx=3, pady=2)

        btn_box = ttk.Frame(iap_cmd_frame)
        btn_box.grid(row=1, column=0, columnspan=4, pady=6, sticky="ew")

        ttk.Button(btn_box, text="▶ Run IAP Program", command=self.run_iap_program).pack(side="left", padx=4, expand=True, fill="x")
        ttk.Button(btn_box, text="🔍 Run IAP Verify", command=self.run_iap_verify).pack(side="left", padx=4, expand=True, fill="x")
        ttk.Button(btn_box, text="🔄 AutoUpdate", command=self.run_iap_autoupdate).pack(side="left", padx=4, expand=True, fill="x")

        # TAB 3: System Commands
        sys_grid = ttk.Frame(tab_system)
        sys_grid.pack(fill="x", expand=True)

        ttk.Button(sys_grid, text="❓ help", command=lambda: self.send_cli("help")).grid(row=0, column=0, padx=3, pady=3, sticky="ew")
        ttk.Button(sys_grid, text="🆔 ident", command=lambda: self.send_cli("ident")).grid(row=0, column=1, padx=3, pady=3, sticky="ew")
        ttk.Button(sys_grid, text="🖥️ hw_info", command=lambda: self.send_cli("hw_info")).grid(row=0, column=2, padx=3, pady=3, sticky="ew")
        ttk.Button(sys_grid, text="🔄 reboot", command=lambda: self.send_cli("reboot")).grid(row=0, column=3, padx=3, pady=3, sticky="ew")

        ttk.Button(sys_grid, text="🔑 sys_serial", command=lambda: self.send_cli("sys_serial")).grid(row=1, column=0, padx=3, pady=3, sticky="ew")
        ttk.Button(sys_grid, text="📄 sys_info", command=lambda: self.send_cli("sys_info")).grid(row=1, column=1, padx=3, pady=3, sticky="ew")
        ttk.Button(sys_grid, text="🛡️ sys_digest", command=lambda: self.send_cli("sys_digest")).grid(row=1, column=2, padx=3, pady=3, sticky="ew")

        # TAB 4: Boot & Flash Operations
        fb_frame = ttk.LabelFrame(tab_flash, text=" flashboot [offset] [ram_addr] ", padding=3)
        fb_frame.pack(fill="x", pady=2)
        ttk.Label(fb_frame, text="Flash Off:").pack(side="left", padx=2)
        self.fb_off_e = ttk.Entry(fb_frame, width=12); self.fb_off_e.insert(0, "0x20000"); self.fb_off_e.pack(side="left", padx=2)
        ttk.Label(fb_frame, text="RAM Addr:").pack(side="left", padx=2)
        self.fb_ram_e = ttk.Entry(fb_frame, width=14); self.fb_ram_e.insert(0, "0x80000000"); self.fb_ram_e.pack(side="left", padx=2)
        ttk.Button(fb_frame, text="flashboot", command=lambda: self.send_cli(f"flashboot {self.fb_off_e.get().strip()} {self.fb_ram_e.get().strip()}")).pack(side="right", padx=3)

        fr_frame = ttk.LabelFrame(tab_flash, text=" flash_read <offset> [count] ", padding=3)
        fr_frame.pack(fill="x", pady=2)
        ttk.Label(fr_frame, text="Offset:").pack(side="left", padx=2)
        self.fr_off_e = ttk.Entry(fr_frame, width=12); self.fr_off_e.insert(0, "0x20000"); self.fr_off_e.pack(side="left", padx=2)
        ttk.Label(fr_frame, text="Count:").pack(side="left", padx=2)
        self.fr_cnt_e = ttk.Entry(fr_frame, width=8); self.fr_cnt_e.insert(0, "64"); self.fr_cnt_e.pack(side="left", padx=2)
        ttk.Button(fr_frame, text="flash_read", command=lambda: self.send_cli(f"flash_read {self.fr_off_e.get().strip()} {self.fr_cnt_e.get().strip()}")).pack(side="right", padx=3)

        fe_frame = ttk.LabelFrame(tab_flash, text=" flash_erase_range <offset> <count> ", padding=3)
        fe_frame.pack(fill="x", pady=2)
        ttk.Label(fe_frame, text="Offset:").pack(side="left", padx=2)
        self.fe_off_e = ttk.Entry(fe_frame, width=12); self.fe_off_e.insert(0, "0x20000"); self.fe_off_e.pack(side="left", padx=2)
        ttk.Label(fe_frame, text="Count:").pack(side="left", padx=2)
        self.fe_cnt_e = ttk.Entry(fe_frame, width=8); self.fe_cnt_e.insert(0, "0x10000"); self.fe_cnt_e.pack(side="left", padx=2)
        ttk.Button(fe_frame, text="flash_erase_range", command=lambda: self.send_cli(f"flash_erase_range {self.fe_off_e.get().strip()} {self.fe_cnt_e.get().strip()}")).pack(side="right", padx=3)

        fc_frame = ttk.LabelFrame(tab_flash, text=" flash_copy <offset> <ram_addr> [count] ", padding=3)
        fc_frame.pack(fill="x", pady=2)
        ttk.Label(fc_frame, text="Offset:").pack(side="left", padx=2)
        self.fc_off_e = ttk.Entry(fc_frame, width=10); self.fc_off_e.insert(0, "0x20000"); self.fc_off_e.pack(side="left", padx=2)
        ttk.Label(fc_frame, text="RAM:").pack(side="left", padx=2)
        self.fc_ram_e = ttk.Entry(fc_frame, width=12); self.fc_ram_e.insert(0, "0x80000000"); self.fc_ram_e.pack(side="left", padx=2)
        ttk.Label(fc_frame, text="Count:").pack(side="left", padx=2)
        self.fc_cnt_e = ttk.Entry(fc_frame, width=6); self.fc_cnt_e.insert(0, "256"); self.fc_cnt_e.pack(side="left", padx=2)
        ttk.Button(fc_frame, text="flash_copy", command=lambda: self.send_cli(f"flash_copy {self.fc_off_e.get().strip()} {self.fc_ram_e.get().strip()} {self.fc_cnt_e.get().strip()}")).pack(side="right", padx=3)

        # TAB 5: Memory Operations
        mr_frame = ttk.LabelFrame(tab_memory, text=" Memory Read & Write ", padding=4)
        mr_frame.pack(fill="x", pady=4)
        ttk.Label(mr_frame, text="Addr:").grid(row=0, column=0, padx=2)
        self.mr_addr_e = ttk.Entry(mr_frame, width=14); self.mr_addr_e.insert(0, "0x80000000"); self.mr_addr_e.grid(row=0, column=1, padx=2)
        ttk.Label(mr_frame, text="Len/Val:").grid(row=0, column=2, padx=2)
        self.mr_val_e = ttk.Entry(mr_frame, width=14); self.mr_val_e.insert(0, "64"); self.mr_val_e.grid(row=0, column=3, padx=2)
        ttk.Button(mr_frame, text="mem_read", command=lambda: self.send_cli(f"mem_read {self.mr_addr_e.get().strip()} {self.mr_val_e.get().strip()}")).grid(row=0, column=4, padx=3)
        ttk.Button(mr_frame, text="mem_write", command=lambda: self.send_cli(f"mem_write {self.mr_addr_e.get().strip()} {self.mr_val_e.get().strip()}")).grid(row=0, column=5, padx=3)

        mc_frame = ttk.LabelFrame(tab_memory, text=" Memory Copy / Compare / Test ", padding=4)
        mc_frame.pack(fill="x", pady=4)
        ttk.Label(mc_frame, text="Dst Addr:").grid(row=0, column=0, padx=2)
        self.mc_a1_e = ttk.Entry(mc_frame, width=12); self.mc_a1_e.insert(0, "0x80000000"); self.mc_a1_e.grid(row=0, column=1, padx=2)
        ttk.Label(mc_frame, text="Src Addr:").grid(row=0, column=2, padx=2)
        self.mc_a2_e = ttk.Entry(mc_frame, width=12); self.mc_a2_e.insert(0, "0x08040000"); self.mc_a2_e.grid(row=0, column=3, padx=2)
        ttk.Label(mc_frame, text="Count:").grid(row=0, column=4, padx=2)
        self.mc_cnt_e = ttk.Entry(mc_frame, width=6); self.mc_cnt_e.insert(0, "64"); self.mc_cnt_e.grid(row=0, column=5, padx=2)
        ttk.Button(mc_frame, text="mem_copy", command=lambda: self.send_cli(f"mem_copy {self.mc_a1_e.get().strip()} {self.mc_a2_e.get().strip()} {self.mc_cnt_e.get().strip()}")).grid(row=1, column=1, pady=3)
        ttk.Button(mc_frame, text="mem_cmp", command=lambda: self.send_cli(f"mem_cmp {self.mc_a1_e.get().strip()} {self.mc_a2_e.get().strip()} {self.mc_cnt_e.get().strip()}")).grid(row=1, column=3, pady=3)
        ttk.Button(mc_frame, text="mem_test", command=lambda: self.send_cli(f"mem_test {self.mc_a1_e.get().strip()}")).grid(row=1, column=5, pady=3)

        # Show / Hide Terminal Console Control Bar
        toggle_bar = ttk.Frame(self.root, padding=2)
        toggle_bar.pack(fill="x", padx=8)

        self.toggle_term_btn = ttk.Button(toggle_bar, text="👁️ Hide Terminal Console", command=self.toggle_terminal_view)
        self.toggle_term_btn.pack(side="left", padx=2)

        # Terminal Output Window & Command Line
        self.term_frame = ttk.LabelFrame(self.root, text=" Interactive Console Terminal ", padding=4)
        self.term_frame.pack(fill="both", expand=True, padx=8, pady=3)

        self.console = scrolledtext.ScrolledText(self.term_frame, bg="#121212", fg="#00FF66", insertbackground="white", font=("Courier", 10))
        self.console.pack(fill="both", expand=True, padx=2, pady=2)
        # Bind direct typing inside the console window for true interactivity
        self.console.bind("<Key>", self._on_console_key)

        cmd_bar = ttk.Frame(self.term_frame)
        cmd_bar.pack(fill="x", pady=2)

        ttk.Label(cmd_bar, text="CLI Prompt:").pack(side="left", padx=3)
        self.cli_entry = ttk.Entry(cmd_bar)
        self.cli_entry.pack(side="left", fill="x", expand=True, padx=3)
        self.cli_entry.bind("<Return>", self.send_cli_line)

        send_btn = ttk.Button(cmd_bar, text="Send Line", command=self.send_cli_line)
        send_btn.pack(side="right", padx=3)

        clear_btn = ttk.Button(cmd_bar, text="🧹 Clear", command=self.clear_terminal)
        clear_btn.pack(side="right", padx=3)

    def _on_console_key(self, event):
        """Direct terminal keypress handler: sends typed keys to UART in real-time."""
        if not self.active_term or not self.active_term.port or not self.active_term.port.is_open:
            return

        char = event.char
        keysym = event.keysym

        if keysym == "Return":
            b = b"\r"
        elif keysym == "BackSpace":
            b = b"\x08"
        elif char:
            b = char.encode("utf-8")
        else:
            return

        try:
            self.active_term.port.write(b)
        except Exception:
            pass

        return "break"

    def toggle_terminal_view(self):
        """Toggle showing or hiding the CLI Terminal Frame."""
        if self.terminal_visible:
            self.term_frame.pack_forget()
            self.terminal_visible = False
            self.toggle_term_btn.config(text="👁️ Show Terminal Console")
        else:
            self.term_frame.pack(fill="both", expand=True, padx=8, pady=3)
            self.terminal_visible = True
            self.toggle_term_btn.config(text="👁️ Hide Terminal Console")

    def refresh_ports(self):
        """Filters ONLY ttyUSB* ports when running on Linux."""
        ports = []
        if HAS_PYSERIAL:
            all_ports = [p.device for p in serial.tools.list_ports.comports()]
            if sys.platform.startswith("linux"):
                ports = [p for p in all_ports if "ttyUSB" in p]
            else:
                ports = all_ports

        if not ports and sys.platform.startswith("linux"):
            ports = glob.glob("/dev/ttyUSB*")

        if not ports:
            ports = ["/dev/ttyUSB0"] if sys.platform.startswith("linux") else ["COM1"]

        self.port_combo["values"] = ports
        self.port_combo.set(ports[0])

    def browse_file(self, target_entry, filetypes=None):
        if filetypes is None:
            filetypes = [("Binary/FBI Files", "*.bin *.fbi *.spi"), ("All Files", "*.*")]

        filename = filedialog.askopenfilename(
            title="Select File",
            filetypes=filetypes
        )
        if filename:
            target_entry.delete(0, tk.END)
            target_entry.insert(0, filename)

    def on_mode_change(self):
        self.addr_entry.delete(0, tk.END)
        if self.mode_var.get() == "flash":
            self.addr_entry.insert(0, "0x00020000")
        else:
            self.addr_entry.insert(0, "0x80000000")

    def toggle_connection(self):
        if self.active_term and self.active_term.port and self.active_term.port.is_open:
            self.stop_session()
        else:
            self.start_connection()

    def start_connection(self):
        port = self.port_combo.get().strip()
        speed = self.speed_combo.get().strip()

        if not port:
            messagebox.showerror("Error", "Please select a serial port!")
            return

        try:
            term = LiteXTerm(serial_boot=False, kernel_image=None, kernel_address="0x80000000",
                             json_images=None, safe=False, no_boot=True, flash=False, log_fn=self.gui_log, progress_fn=self.gui_progress)
            term.open(port, int(speed))
            self.active_term = term

            self.connect_btn.config(text="❌ Disconnect")
            self.ident_label.config(text=f"Board: Connected ({port})", foreground="#00aa00")
            self.output_queue.put(("LOG", f"\n[GUI] Connected to {port} @ {speed} baud.\n"))

            threading.Thread(target=self._read_uart_stream, daemon=True).start()

            # Auto-read full board identity upon connection
            self.root.after(300, lambda: self.send_cli("ident"))
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def stop_session(self):
        if self.active_term:
            try:
                self.active_term.close()
            except Exception:
                pass
            self.active_term = None
        self.connect_btn.config(text="⚡ Connect")
        self.ident_label.config(text="Board: Disconnected", foreground="#666666")
        self.output_queue.put(("LOG", "\n[GUI] Disconnected from serial port.\n"))

    def _read_uart_stream(self):
        line_buf = ""
        while self.active_term and self.active_term.port and self.active_term.port.is_open:
            try:
                if self.active_term.port.in_waiting:
                    b = self.active_term.port.read(self.active_term.port.in_waiting)
                    if b:
                        text_chunk = b.decode("utf-8", errors="replace")
                        self.output_queue.put(("LOG", text_chunk))

                        # Check for board identity output and display the entire line
                        line_buf += text_chunk
                        if "\n" in line_buf:
                            lines = line_buf.split("\n")
                            for line in lines[:-1]:
                                line_clean = line.strip()
                                if "Ident:" in line_clean or "PolarFire" in line_clean or "Platform" in line_clean:
                                    self.root.after(0, lambda l=line_clean: self.ident_label.config(text=f"Board: {l}", foreground="#00aa00"))
                            line_buf = lines[-1]

                        for char_byte in b:
                            single_char = bytes([char_byte])
                            if self.active_term.detect_magic(single_char):
                                self.active_term.answer_magic()
            except Exception:
                break
            time.sleep(0.01)

    def send_cli(self, command_str):
        if not self.active_term or not self.active_term.port or not self.active_term.port.is_open:
            self.start_connection()
            time.sleep(0.1)

        if self.active_term and self.active_term.port and self.active_term.port.is_open:
            try:
                full_cmd = command_str.strip() + "\r"
                self.active_term.port.write(full_cmd.encode("utf-8"))
            except Exception as e:
                self.console.insert(tk.END, f"\n[GUI Error] Failed to send CLI command: {e}\n")
        else:
            messagebox.showerror("Error", "Serial port is not connected!")

    def send_cli_line(self, event=None):
        cmd = self.cli_entry.get()
        if cmd:
            self.send_cli(cmd)
            self.cli_entry.delete(0, tk.END)

    def start_sfl_upload(self):
        kernel = self.kernel_entry.get().strip()
        addr = self.addr_entry.get().strip()
        is_flash = (self.mode_var.get() == "flash")
        is_noboot = self.noboot_var.get()

        if not kernel or not os.path.exists(kernel):
            messagebox.showerror("Error", "Please select a valid firmware image file (.bin / .fbi)!")
            return

        if not self.active_term or not self.active_term.port or not self.active_term.port.is_open:
            self.start_connection()
            time.sleep(0.2)

        self.progress_bar['value'] = 0
        self.progress_label.config(text="Preparing transfer...")

        self.active_term.mem_regions = {kernel: addr}
        self.active_term.boot_address = addr
        self.active_term.flash_mode = is_flash
        self.active_term.no_boot = is_noboot
        self.active_term.safe = False
        self.active_term.magic_detect_buffer = bytes(len(sfl_magic_req))

        self.console.insert(tk.END, f"\n[GUI] Configured SFL upload: {kernel} -> {addr} (Flash={is_flash})\n")

        cli_cmd = "flashwrite" if is_flash else "serialboot"
        self.console.insert(tk.END, f"[GUI] Auto-sending CLI command '{cli_cmd}' to board...\n")
        self.send_cli(cli_cmd)

    def upload_spi_file(self):
        """Uploads a .spi bitstream file directly to SPI Flash at the specified offset using flashwrite."""
        spi_file = self.spi_entry.get().strip()
        offset = self.spi_off_entry.get().strip()

        if not spi_file or not os.path.exists(spi_file):
            messagebox.showerror("Error", "Please select a valid .spi bitstream file!")
            return

        if not self.active_term or not self.active_term.port or not self.active_term.port.is_open:
            self.start_connection()
            time.sleep(0.2)

        self.progress_bar['value'] = 0
        self.progress_label.config(text="Preparing .spi bitstream transfer...")

        self.active_term.mem_regions = {spi_file: offset}
        self.active_term.boot_address = offset
        self.active_term.flash_mode = True
        self.active_term.no_boot = True
        self.active_term.safe = False
        self.active_term.magic_detect_buffer = bytes(len(sfl_magic_req))

        self.console.insert(tk.END, f"\n[GUI] Configured IAP Bitstream Upload: {spi_file} -> Flash {offset}\n")
        self.console.insert(tk.END, "[GUI] Auto-sending CLI command 'flashwrite' to board...\n")
        self.send_cli("flashwrite")

    def run_iap_program(self):
        """Dispatches IAP program command based on selected mode and target address/index."""
        target = self.iap_target_entry.get().strip()
        mode = self.iap_mode_combo.get().strip()

        if mode == "autoupdate":
            cmd = "sys_iap 0 autoupdate"
        elif "idx" in mode:
            cmd = f"sys_iap {target} {mode}"
        else:
            cmd = f"sys_iap {target} {mode}"

        self.send_cli(cmd)

    def run_iap_verify(self):
        """Dispatches IAP verify command based on selected mode and target address/index."""
        target = self.iap_target_entry.get().strip()
        mode = self.iap_mode_combo.get().strip()

        if "idx" in mode:
            cmd = f"sys_iap {target} verify_idx"
        else:
            cmd = f"sys_iap {target} verify_addr"

        self.send_cli(cmd)

    def run_iap_autoupdate(self):
        self.send_cli("sys_iap 0 autoupdate")

    def _check_queue(self):
        while not self.output_queue.empty():
            item = self.output_queue.get()
            if item is not None:
                msg_type = item[0]
                if msg_type == "LOG":
                    self.append_console_text(item[1])
                elif msg_type == "PROGRESS":
                    current, total = item[1], item[2]
                    pct = (current / total) * 100.0 if total > 0 else 0.0
                    self.progress_bar['value'] = pct
                    self.progress_label.config(text=f"Uploading: {pct:.1f}% ({current/1024:.1f} KB / {total/1024:.1f} KB)")
        self.root.after(40, self._check_queue)

    def clear_terminal(self):
        self.console.delete("1.0", tk.END)

# Entry Point --------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("port", nargs="?", default=None, help="Serial port (optional).")
    parser.add_argument("--speed", default="115200", help="Serial baudrate.")
    parser.add_argument("--kernel", default=None, help="Kernel image.")
    parser.add_argument("--kernel-adr", default="0x00020000", help="Kernel address.")
    parser.add_argument("--no-boot", action="store_true", help="Download without booting.")
    parser.add_argument("--flash", action="store_true", help="Stream directly to SPI Flash.")

    args = parser.parse_args()

    root = tk.Tk()
    app = LiteXTermGUI(root, args)
    root.mainloop()

if __name__ == "__main__":
    main()