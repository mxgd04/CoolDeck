import serial
import serial.tools.list_ports
import subprocess
import time
import sys
import os
import ctypes
from ctypes import cast, POINTER
import win32api
import win32con
import win32gui
import win32process

# Evita UnicodeEncodeError (✓, —, tildes) si la salida no es una consola UTF-8
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# ============================================================
#  INSTALACIÓN:
#  pip install pyserial pycaw comtypes pywin32
# ============================================================


# ============================================================
#  MICRÓFONO — directo por Windows Core Audio API con ctypes
#  Sin depender de pycaw.GetMicrophone() que falla en W11
# ============================================================
# GUIDs necesarios
import comtypes
from comtypes import GUID

CLSID_MMDeviceEnumerator = GUID('{BCDE0395-E52F-467C-8E3D-C4579291692E}')
IID_IMMDeviceEnumerator  = GUID('{A95664D2-9614-4F35-A746-DE8DB63617E6}')
IID_IAudioEndpointVolume = GUID('{5CDF2C82-841E-4546-9722-0CF74078229A}')

eRender  = 0
eCapture = 1
eConsole = 0

mic_muted = False

def get_mic_volume_interface():
    """Devuelve el IAudioEndpointVolume del micrófono por defecto"""
    try:
        import comtypes.client
        # IMMDeviceEnumerator
        enumerator = comtypes.client.CreateObject(
            CLSID_MMDeviceEnumerator,
            interface=comtypes.IUnknown
        )
        # QueryInterface a IMMDeviceEnumerator
        from pycaw.pycaw import IMMDeviceEnumerator, IAudioEndpointVolume
        from comtypes import CLSCTX_ALL
        enumerator = enumerator.QueryInterface(IMMDeviceEnumerator)
        mic_device = enumerator.GetDefaultAudioEndpoint(eCapture, eConsole)
        vol = mic_device.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        return cast(vol, POINTER(IAudioEndpointVolume))
    except Exception as e:
        print(f"  [MIC] get_mic_volume_interface error: {e}")
        return None

def toggle_mic():
    global mic_muted
    vol = get_mic_volume_interface()
    if vol:
        try:
            current = vol.GetMute()
            vol.SetMute(not current, None)
            mic_muted = not current
            print(f"  [MIC] {'MUTEADO' if mic_muted else 'ACTIVO'}")
            return
        except Exception as e:
            print(f"  [MIC] SetMute error: {e}")
    print("  [MIC] No se pudo cambiar el estado del micrófono")


# ============================================================
#  DISCORD — buscar ventana por proceso y usar SendKeys
# ============================================================
discord_muted = False

def find_discord_hwnd():
    """Busca la ventana principal de Discord por nombre de proceso"""
    result = []
    def callback(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        try:
            _, pid = win32process.GetWindowThreadProcessId(hwnd)
            handle = win32api.OpenProcess(
                win32con.PROCESS_QUERY_INFORMATION | win32con.PROCESS_VM_READ,
                False, pid
            )
            exe = win32process.GetModuleFileNameEx(handle, 0)
            if "discord.exe" in exe.lower():
                title = win32gui.GetWindowText(hwnd)
                if title:
                    result.append((hwnd, title))
        except:
            pass
    win32gui.EnumWindows(callback, None)
    return result

def toggle_discord_mute():
    global discord_muted
    discord_muted = not discord_muted

    windows = find_discord_hwnd()
    if not windows:
        print("  [DISCORD] Discord no encontrado — ¿está abierto?")
        return

    hwnd, title = windows[0]
    print(f"  [DISCORD] Ventana encontrada: '{title}'")

    try:
        import win32com.client
        shell = win32com.client.Dispatch("WScript.Shell")

        prev_hwnd = win32gui.GetForegroundWindow()

        # Activar por título exacto de la ventana
        activated = shell.AppActivate(title)
        if not activated:
            # Intentar con "Discord" genérico
            activated = shell.AppActivate("Discord")

        if activated:
            time.sleep(0.3)
            shell.SendKeys("^+m")
            time.sleep(0.15)
            if prev_hwnd:
                try:
                    win32gui.SetForegroundWindow(prev_hwnd)
                except:
                    pass
            print(f"  [DISCORD] {'MUTEADO' if discord_muted else 'ACTIVO'}")
        else:
            print("  [DISCORD] No se pudo activar la ventana de Discord")
    except Exception as e:
        print(f"  [DISCORD] Error: {e}")


# ============================================================
#  VOLUMEN — teclas multimedia globales
# ============================================================
def press_media_key(vk_code):
    win32api.keybd_event(vk_code, 0, 0, 0)
    time.sleep(0.05)
    win32api.keybd_event(vk_code, 0, win32con.KEYEVENTF_KEYUP, 0)

def volume_up():
    press_media_key(0xAF)
    print("  [VOL] Subiendo")

def volume_down():
    press_media_key(0xAE)
    print("  [VOL] Bajando")


# ============================================================
#  SPOTIFY — teclas multimedia
# ============================================================
def spotify_play_pause():
    press_media_key(0xB3)
    print("  [SPOTIFY] Play/Pause")

def spotify_next():
    press_media_key(0xB0)
    print("  [SPOTIFY] Siguiente")

def spotify_prev():
    press_media_key(0xB1)
    print("  [SPOTIFY] Anterior")


# ============================================================
#  CÁMARA, TERMINAL, CAPTURA, NAVEGADOR, MONITOR
# ============================================================
camera_open = False
def toggle_camera():
    global camera_open
    camera_open = not camera_open
    if camera_open:
        subprocess.Popen(["explorer", "microsoft.windows.camera:"])
        print("  [CAMARA] Abriendo")
    else:
        subprocess.run(["taskkill", "/f", "/im", "WindowsCamera.exe"], capture_output=True)
        print("  [CAMARA] Cerrando")

def open_terminal():
    subprocess.Popen(["wt"])
    print("  [TERMINAL] Abriendo")

def press_hotkey(*keys):
    """Pulsa una combinación de teclas (códigos VK) y la suelta en orden inverso"""
    for k in keys:
        win32api.keybd_event(k, 0, 0, 0)
    time.sleep(0.05)
    for k in reversed(keys):
        win32api.keybd_event(k, 0, win32con.KEYEVENTF_KEYUP, 0)

def take_screenshot():
    press_hotkey(win32con.VK_LWIN, win32con.VK_SHIFT, ord('S'))
    print("  [CAPTURA] Herramienta de recorte abierta")

def open_browser():
    os.startfile("https://www.google.com")
    print("  [NAVEGADOR] Abriendo")

def monitor_off():
    # PostMessage (no SendMessage): SendMessage a HWND_BROADCAST se queda
    # bloqueado si alguna ventana no responde.
    ctypes.windll.user32.PostMessageW(0xFFFF, 0x0112, 0xF170, 2)
    print("  [MONITOR] Apagando pantalla")


# ============================================================
#  PÁGINA 2
# ============================================================
def volume_mute():
    press_media_key(0xAD)  # VK_VOLUME_MUTE
    print("  [VOL] Silenciar/Activar")

def open_explorer():
    subprocess.Popen(["explorer"])
    print("  [EXPLORADOR] Abriendo")

def open_task_manager():
    # Ctrl+Shift+Esc: evita el error de elevación (UAC) de lanzar taskmgr.exe con Popen
    press_hotkey(win32con.VK_CONTROL, win32con.VK_SHIFT, win32con.VK_ESCAPE)
    print("  [TAREAS] Abriendo administrador de tareas")

def show_desktop():
    press_hotkey(win32con.VK_LWIN, ord('D'))
    print("  [ESCRITORIO] Mostrar/ocultar")

def lock_pc():
    ctypes.windll.user32.LockWorkStation()
    print("  [BLOQUEAR] PC bloqueado")

def open_settings():
    os.startfile("ms-settings:")
    print("  [AJUSTES] Abriendo")


# ============================================================
#  TABLA DE COMANDOS
# ============================================================
COMMANDS = {
    "CMD:MIC_TOGGLE":    toggle_mic,
    "CMD:DISCORD_MUTE":  toggle_discord_mute,
    "CMD:CAMERA_TOGGLE": toggle_camera,
    "CMD:SPOTIFY_PREV":  spotify_prev,
    "CMD:SPOTIFY_PLAY":  spotify_play_pause,
    "CMD:SPOTIFY_NEXT":  spotify_next,
    "CMD:VOL_DOWN":      volume_down,
    "CMD:VOL_UP":        volume_up,
    "CMD:SCREENSHOT":    take_screenshot,
    "CMD:OPEN_BROWSER":  open_browser,
    "CMD:OPEN_TERMINAL": open_terminal,
    "CMD:MONITOR_OFF":   monitor_off,
    # Página 2
    "CMD:VOL_MUTE":      volume_mute,
    "CMD:OPEN_EXPLORER": open_explorer,
    "CMD:TASK_MANAGER":  open_task_manager,
    "CMD:SHOW_DESKTOP":  show_desktop,
    "CMD:LOCK_PC":       lock_pc,
    "CMD:OPEN_SETTINGS": open_settings,
}


# ============================================================
#  BUSCAR PUERTO ESP32
# ============================================================
def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.vid == 0x303A:  # Espressif (USB nativo de la ESP32-S3)
            return port.device
    for port in ports:
        desc = port.description.lower()
        if any(x in desc for x in ["cp210", "ch340", "ch9102", "usb serial", "uart", "cdc"]):
            return port.device
    if ports:
        print("Puertos disponibles:")
        for p in ports:
            print(f"  {p.device} — {p.description}")
    return None


# ============================================================
#  BUCLE PRINCIPAL
# ============================================================
def main():
    print("=" * 45)
    print("       CoolDeck — Python Bridge")
    print("=" * 45)

    port = find_esp32_port()
    if port is None:
        port = input("\nNo se encontró la ESP32. Introduce el puerto (ej: COM4): ").strip()

    print(f"\nConectando a {port} a 115200 baudios...")
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        print(f"Conectado a {port} ✓")
        print("Esperando comandos...\n")
    except serial.SerialException as e:
        print(f"Error al abrir {port}: {e}")
        sys.exit(1)

    while True:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            print(f"[SERIE] {line}")

            if line in COMMANDS:
                try:
                    COMMANDS[line]()
                except Exception as e:
                    print(f"  [ERROR] {e}")
            elif line.startswith("CMD:"):
                print(f"  [AVISO] Comando no reconocido: {line}")

        except serial.SerialException:
            print("\n[ERROR] Conexión perdida. Reintentando cada 3s (Ctrl+C para salir)...")
            try:
                ser.close()
            except Exception:
                pass
            # La ESP32 puede tardar en volver a aparecer (p. ej. tras subir firmware)
            # y a veces con otro número de COM, así que se reintenta sin límite.
            while True:
                time.sleep(3)
                try:
                    new_port = find_esp32_port() or port
                    ser = serial.Serial(new_port, 115200, timeout=1)
                    port = new_port
                    print(f"Reconectado a {port} ✓")
                    break
                except serial.SerialException:
                    pass

        except KeyboardInterrupt:
            print("\n\nCerrando CoolDeck...")
            ser.close()
            sys.exit(0)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nCerrando CoolDeck...")