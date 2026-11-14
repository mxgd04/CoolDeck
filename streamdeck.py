import serial
import serial.tools.list_ports
import subprocess
import time
import sys
import os
import ctypes
import threading
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
#  pip install pyserial pycaw comtypes pywin32 psutil
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

def get_mic_volume_interface(flow=eCapture, quiet=False):
    """IAudioEndpointVolume del micro (eCapture) o altavoz (eRender) por defecto"""
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
        mic_device = enumerator.GetDefaultAudioEndpoint(flow, eConsole)
        vol = mic_device.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        # QueryInterface (no cast): cast duplica la referencia sin AddRef y
        # se libera dos veces, lo que acaba en "access violation"
        return vol.QueryInterface(IAudioEndpointVolume)
    except Exception as e:
        if not quiet:
            print(f"  [MIC] get_mic_volume_interface error: {e}")
        return None


def get_mute(flow):
    """True/False si el dispositivo por defecto está muteado, None si no se sabe"""
    vol = get_mic_volume_interface(flow, quiet=True)
    try:
        return bool(vol.GetMute()) if vol else None
    except Exception:
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
def is_camera_open():
    """True si la app Cámara de Windows está abierta, None si no se puede saber"""
    try:
        import psutil
    except ImportError:
        return None
    try:
        return any((p.info["name"] or "").lower() == "windowscamera.exe"
                   for p in psutil.process_iter(["name"]))
    except Exception:
        return None

def toggle_camera():
    if not is_camera_open():
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
#  ESTADÍSTICAS DEL PC -> PÁGINA DE INFO DE LA ESP32
# ============================================================
STATS_INTERVAL = 1.0  # segundos entre envíos

try:
    import psutil
except ImportError:
    psutil = None
    print("[AVISO] psutil no instalado: pip install psutil (sin datos de CPU/RAM/red)")


LHM_URL = "http://localhost:8085/data.json"
CPU_TEMP_NAMES = ("CPU Package", "Core (Tctl/Tdie)", "Core (Tctl)")


def read_cpu_temp_lhm_web():
    """Lee la temperatura de la CPU del servidor web de LibreHardwareMonitor"""
    import json
    import re
    import urllib.request
    try:
        with urllib.request.urlopen(LHM_URL, timeout=1) as r:
            root = json.load(r)
    except Exception:
        return None

    found = {}  # nombre -> ºC, solo sensores de temperatura de la CPU

    def walk(node, in_cpu):
        sid = node.get("SensorId", "")
        in_cpu = in_cpu or "cpu" in node.get("ImageURL", "").lower() or "cpu/" in sid
        val = node.get("Value", "")
        if in_cpu and "°C" in val:
            m = re.search(r"-?\d+(?:[.,]\d+)?", val)
            if m:
                found[node.get("Text", "")] = float(m.group().replace(",", "."))
        for ch in node.get("Children", []):
            walk(ch, in_cpu)

    walk(root, False)
    for name in CPU_TEMP_NAMES:
        if name in found:
            return found[name]
    cores = [v for n, v in found.items() if n.startswith("CPU Core") or n.startswith("Core")]
    return max(cores) if cores else None


def read_cpu_temp():
    """Temperatura de la CPU en ºC, o None.

    Windows no la expone sin privilegios, así que se lee de
    LibreHardwareMonitor si está abierto:
      1) su servidor web (Opciones > Servidor web remoto), versiones actuales
      2) su WMI (solo versiones antiguas / OpenHardwareMonitor)
    Como último recurso prueba la zona térmica ACPI (requiere admin).
    """
    t = read_cpu_temp_lhm_web()
    if t is not None:
        return t

    import win32com.client
    for ns in ("root\\LibreHardwareMonitor", "root\\OpenHardwareMonitor"):
        try:
            svc = win32com.client.GetObject(f"winmgmts:{ns}")
            temps = [(s.Name, float(s.Value)) for s in svc.ExecQuery(
                "SELECT Name, Value FROM Sensor WHERE SensorType='Temperature'")
                if s.Value is not None]
            # Preferimos "CPU Package" (Intel) o "Core (Tctl/Tdie)" (AMD)
            for name, val in temps:
                if name in CPU_TEMP_NAMES:
                    return val
            cores = [v for n, v in temps if n.startswith("CPU Core") or n.startswith("Core #")]
            if cores:
                return max(cores)
        except Exception:
            pass
    try:
        svc = win32com.client.GetObject("winmgmts:root\\wmi")
        for z in svc.InstancesOf("MSAcpi_ThermalZoneTemperature"):
            return z.CurrentTemperature / 10.0 - 273.15
    except Exception:
        pass
    return None


def read_nvidia_gpu():
    """(uso %, temp ºC, VRAM usada MB, VRAM total MB) o None"""
    try:
        out = subprocess.run(
            ["nvidia-smi",
             "--query-gpu=utilization.gpu,temperature.gpu,memory.used,memory.total",
             "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=3,
            creationflags=subprocess.CREATE_NO_WINDOW,
        ).stdout.strip().splitlines()[0]
        return tuple(float(x) for x in out.split(","))
    except Exception:
        return None


# Reproducción multimedia (Spotify, etc.): opcional, requiere
#   pip install winrt-Windows.Media.Control winrt-Windows.Foundation
try:
    import asyncio
    from winrt.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager as _MediaManager)
except Exception:
    _MediaManager = None


def is_media_playing():
    """True si la sesión multimedia actual está reproduciendo, None si no se sabe"""
    if _MediaManager is None:
        return None

    async def query():
        mgr = await _MediaManager.request_async()
        s = mgr.get_current_session()
        if s is None:
            return False
        return int(s.get_playback_info().playback_status) == 4  # 4 = PLAYING

    try:
        return asyncio.run(query())
    except Exception:
        return None


def read_button_states():
    """Estado real de cada botón con toggle.

    Las claves son el comando sin el prefijo "CMD:"; la ESP actualiza todos
    los botones cuyo comando coincida. Discord no se puede consultar, así
    que ese botón sigue cambiando solo al pulsarlo.
    """
    states = {
        "MIC_TOGGLE":    get_mute(eCapture),
        "VOL_MUTE":      get_mute(eRender),
        "CAMERA_TOGGLE": is_camera_open(),
        "SPOTIFY_PLAY":  is_media_playing(),
    }
    return {k: v for k, v in states.items() if v is not None}


def stats_loop(get_serial):
    """Hilo que envía 'STAT:...' a la ESP32 cada STATS_INTERVAL segundos"""
    import pythoncom
    pythoncom.CoInitialize()  # necesario para WMI desde un hilo

    have_nvidia = read_nvidia_gpu() is not None
    cpu_temp_ok = read_cpu_temp() is not None
    if not cpu_temp_ok:
        print("[INFO] Temperatura de CPU no disponible: abre LibreHardwareMonitor "
              "como administrador y activa Opciones > Servidor web remoto > Ejecutar "
              "(puerto 8085) para verla en la pantalla.")

    last_net = psutil.net_io_counters() if psutil else None
    last_t = time.time()
    if psutil:
        psutil.cpu_percent(None)  # la primera lectura siempre da 0

    temp_check = 0
    while True:
        time.sleep(STATS_INTERVAL)
        fields = {}

        if psutil:
            fields["cpu"] = psutil.cpu_percent(None)
            vm = psutil.virtual_memory()
            fields["ram"] = vm.percent
            fields["ramu"] = vm.used / 1024**3
            fields["ramt"] = vm.total / 1024**3

            now = time.time()
            net = psutil.net_io_counters()
            dt = max(now - last_t, 0.001)
            fields["down"] = (net.bytes_recv - last_net.bytes_recv) / 1024 / dt
            fields["up"] = (net.bytes_sent - last_net.bytes_sent) / 1024 / dt
            last_net, last_t = net, now

        # Si no había temperatura, reintenta cada 10 s (por si abren LHM después)
        t = None
        if cpu_temp_ok or temp_check <= 0:
            t = read_cpu_temp()
            cpu_temp_ok = t is not None
            temp_check = 10
        else:
            temp_check -= 1
        fields["ctemp"] = t if t is not None else -1

        if have_nvidia:
            gpu = read_nvidia_gpu()
            if gpu:
                util, gtemp, vused, vtotal = gpu
                fields.update(gpu=util, gtemp=gtemp,
                              vram=vused / vtotal * 100 if vtotal else -1,
                              vramu=vused / 1024, vramt=vtotal / 1024)

        line = "STAT:" + ",".join(f"{k}={v:.1f}" for k, v in fields.items()) + "\n"

        # Estado real de los botones con toggle (la ESP los redibuja si cambian)
        states = read_button_states()
        if states:
            line += "STATE:" + ",".join(f"{k}={int(v)}" for k, v in states.items()) + "\n"

        ser = get_serial()
        if ser is None:
            continue
        try:
            ser.write(line.encode("ascii"))
        except Exception:
            pass  # el bucle principal se encarga de reconectar


# ============================================================
#  BUSCAR PUERTO ESP32
# ============================================================
def find_esp32_port(verbose=True):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.vid == 0x303A:  # Espressif (USB nativo de la ESP32-S3)
            return port.device
    for port in ports:
        desc = port.description.lower()
        if any(x in desc for x in ["cp210", "ch340", "ch9102", "usb serial", "uart", "cdc"]):
            return port.device
    if ports and verbose:
        print("Puertos disponibles:")
        for p in ports:
            print(f"  {p.device} — {p.description}")
    return None


# ============================================================
#  BUCLE PRINCIPAL
# ============================================================
LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "streamdeck.log")
_instance_mutex = None  # se guarda para que el mutex viva mientras corre el proceso


def setup_background_mode():
    """Con pythonw (arranque automático) no hay consola: la salida va a un log"""
    if sys.stdout is not None:
        return False
    log = open(LOG_FILE, "w", encoding="utf-8", buffering=1)
    sys.stdout = sys.stderr = log
    return True


def ensure_single_instance():
    """Evita dos bridges peleándose por el mismo puerto COM"""
    global _instance_mutex
    import win32event
    import winerror
    _instance_mutex = win32event.CreateMutex(None, False, "Local\\CoolDeckBridge")
    if win32api.GetLastError() == winerror.ERROR_ALREADY_EXISTS:
        print("Ya hay un CoolDeck bridge en marcha (¿arranque automático?). "
              "Ciérralo desde el Administrador de tareas (pythonw.exe) si quieres usar este.")
        return False
    return True


def open_esp32(port_hint, interactive):
    """Abre el puerto de la ESP32, esperando a que aparezca si no está conectada"""
    port = find_esp32_port() or port_hint
    if port is None and interactive:
        port = input("\nNo se encontró la ESP32. Introduce el puerto (ej: COM4) "
                     "o pulsa Enter para esperar a que se conecte: ").strip() or None

    warned = False
    while True:
        if port:
            try:
                print(f"\nConectando a {port} a 115200 baudios...")
                ser = serial.Serial(port, 115200, timeout=1)
                time.sleep(2)
                print(f"Conectado a {port} ✓")
                return ser, port
            except serial.SerialException as e:
                print(f"No se pudo abrir {port}: {e}")
        if not warned:
            print("Esperando a que se conecte la ESP32 (reintento cada 3 s)...")
            warned = True
        time.sleep(3)
        port = find_esp32_port(verbose=False) or port


def main():
    background = setup_background_mode()
    print("=" * 45)
    print("       CoolDeck — Python Bridge")
    print("=" * 45)
    if not ensure_single_instance():
        if not background:
            time.sleep(5)
        sys.exit(0)

    ser, port = open_esp32(None, interactive=not background)
    print("Esperando comandos...\n")

    # El hilo de estadísticas lee siempre el puerto actual (cambia al reconectar)
    current = {"ser": ser}
    threading.Thread(target=stats_loop, args=(lambda: current["ser"],), daemon=True).start()

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
            current["ser"] = None
            try:
                ser.close()
            except Exception:
                pass
            # La ESP32 puede tardar en volver a aparecer (p. ej. tras subir firmware)
            # y a veces con otro número de COM, así que se reintenta sin límite.
            while True:
                time.sleep(3)
                try:
                    new_port = find_esp32_port(verbose=False) or port
                    ser = serial.Serial(new_port, 115200, timeout=1)
                    port = new_port
                    current["ser"] = ser
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