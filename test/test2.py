import tkinter as tk
from tkinter import messagebox
from zeroconf import ServiceBrowser, Zeroconf
import socket

class MyListener:
    def __init__(self, listbox):
        self.listbox = listbox

    def add_service(self, zeroconf, type, name):
        info = zeroconf.get_service_info(type, name)
        if info:
            address = socket.inet_ntoa(info.addresses[0])
            self.listbox.insert(tk.END, f"{name} ({address})")

    def remove_service(self, zeroconf, type, name):
        for i in range(self.listbox.size()):
            if self.listbox.get(i).startswith(name):
                self.listbox.delete(i)
                break

def scan_devices():
    zeroconf = Zeroconf()
    listener = MyListener(devices_listbox)
    browser = ServiceBrowser(zeroconf, "_http._tcp.local.", listener)

def connect_device():
    selected_device = devices_listbox.get(tk.ACTIVE)
    if selected_device:
        address = selected_device.split('(')[-1].strip(')')
        # Here you can add your connection logic using the device address (address)
        messagebox.showinfo("Info", f"Connecting to {selected_device}")
    else:
        messagebox.showwarning("Warning", "No device selected")

# GUI setup
root = tk.Tk()
root.title("ESP32 Connector")

scan_button = tk.Button(root, text="Scan for Devices", command=scan_devices)
scan_button.pack(pady=10)

devices_listbox = tk.Listbox(root, width=50)
devices_listbox.pack(pady=10)

connect_button = tk.Button(root, text="Connect", command=connect_device)
connect_button.pack(pady=10)

root.mainloop()
