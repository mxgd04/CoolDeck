#include <Arduino.h>
#include <LovyanGFX.hpp>

// ============================================================
//  CONFIGURACIÓN PANTALLA — MSP4021 ST7796 + XPT2046
//  Touch en SPI3 independiente (pines verificados funcionando)
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796   _panel_instance;
  lgfx::Bus_SPI        _bus_instance;
  lgfx::Light_PWM      _light_instance;
  lgfx::Touch_XPT2046  _touch_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 12;
      cfg.pin_mosi    = 11;
      cfg.pin_miso    = 13;
      cfg.pin_dc      = 9;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 10;
      cfg.pin_rst          = 14;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 21;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min           = 438;
      cfg.x_max           = 3628;
      cfg.y_min           = 3785;
      cfg.y_max           = 513;
      cfg.pin_int         = 16;
      cfg.bus_shared      = false;
      cfg.offset_rotation = 0;
      cfg.spi_host        = SPI3_HOST;
      cfg.freq            = 1000000;
      cfg.pin_sclk        = 40;
      cfg.pin_mosi        = 41;
      cfg.pin_miso        = 42;
      cfg.pin_cs          = 15;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

// ============================================================
//  CONFIGURACIÓN DEL STREAM DECK
// ============================================================
#define COLS        3
#define ROWS        4
#define NUM_BUTTONS (COLS * ROWS)

#define MARGIN_X    12
#define MARGIN_Y    12
#define GAP         8
#define SCREEN_W    480
#define SCREEN_H    320
#define HEADER_H    10
#define BTN_W       ((SCREEN_W - MARGIN_X * 2 - GAP * (COLS - 1)) / COLS)
#define BTN_H       ((SCREEN_H - MARGIN_Y * 2 - GAP * (ROWS - 1)) / ROWS)

// Gestos
#define SWIPE_START_DX  25   // px de movimiento horizontal para considerar que es un deslizamiento
#define SWIPE_MIN_DX    80   // px mínimos para cambiar de página al soltar
#define RELEASE_FRAMES  3    // lecturas seguidas sin toque para dar por soltado (antirrebote)
#define LOOP_DELAY_MS   15

#define C_BG        0x1082
#define C_BTN       0x2124
#define C_BTN_PR    0x4228
#define C_BORDER    0x4A69
#define C_TEXT      0xFFFF
#define C_ICON      0x07FF
#define C_GREEN     0x07E0
#define C_RED       0xF800
#define C_YELLOW    0xFFE0
#define C_PURPLE    0xA81F
#define C_ORANGE    0xFD20
#define C_BLUE      0x001F
#define C_GREY      0x7BEF

// ============================================================
//  DEFINICIÓN DE BOTONES Y PÁGINAS
// ============================================================
struct Button {
  const char* label;     // nullptr = hueco vacío
  const char* sublabel;
  uint16_t    color;
  uint8_t     icon;      // 0=mic 1=discord 2=music 3=vol 4=prev 5=next
                         // 6=play/pause 7=cam-obs 8=mute 9=webcam
                         // 10=terminal 11=power 12=screenshot 13=browser
                         // 14=carpeta 15=tareas 16=escritorio 17=candado 18=engranaje
  const char* cmd;       // texto que se envía por serie al PC
  bool        toggle;    // mantiene estado visual encendido/apagado
};

#define EMPTY { nullptr, nullptr, 0, 0, nullptr, false }

// Para añadir más páginas: añade otro bloque de 12 botones y el
// comando correspondiente en COMMANDS de streamdeck.py.
const Button PAGES[][NUM_BUTTONS] = {
  // ---------------- PÁGINA 1 ----------------
  {
    { "MIC",       "Mutear",   C_RED,    0,  "CMD:MIC_TOGGLE",    true  },
    { "DISCORD",   "Mutear",   C_PURPLE, 1,  "CMD:DISCORD_MUTE",  true  },
    { "CAMARA",    "On/Off",   C_BLUE,   9,  "CMD:CAMERA_TOGGLE", true  },

    { "ANTERIOR",  "Spotify",  C_GREEN,  4,  "CMD:SPOTIFY_PREV",  false },
    { "PLAY",      "Spotify",  C_GREEN,  6,  "CMD:SPOTIFY_PLAY",  true  },
    { "SIGUIENTE", "Spotify",  C_GREEN,  5,  "CMD:SPOTIFY_NEXT",  false },

    { "VOL -",     "Sistema",  C_YELLOW, 3,  "CMD:VOL_DOWN",      false },
    { "VOL +",     "Sistema",  C_YELLOW, 3,  "CMD:VOL_UP",        false },
    { "CAPTURA",   "Pantalla", C_ORANGE, 12, "CMD:SCREENSHOT",    false },

    { "NAVEGADOR", "Abrir",    C_ICON,   13, "CMD:OPEN_BROWSER",  false },
    { "CMD",       "Terminal", C_ICON,   10, "CMD:OPEN_TERMINAL", false },
    { "APAGAR",    "Monitor",  C_RED,    11, "CMD:MONITOR_OFF",   false },
  },
  // ---------------- PÁGINA 2 ----------------
  {
    { "SILENCIAR",  "Sistema",  C_YELLOW, 8,  "CMD:VOL_MUTE",      true  },
    { "EXPLORADOR", "Archivos", C_ORANGE, 14, "CMD:OPEN_EXPLORER", false },
    { "TAREAS",     "Admin.",   C_GREEN,  15, "CMD:TASK_MANAGER",  false },

    { "ESCRITORIO", "Mostrar",  C_ICON,   16, "CMD:SHOW_DESKTOP",  false },
    { "BLOQUEAR",   "PC",       C_RED,    17, "CMD:LOCK_PC",       false },
    { "AJUSTES",    "Windows",  C_PURPLE, 18, "CMD:OPEN_SETTINGS", false },

    EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY,
  },
};

const int NUM_PAGES = sizeof(PAGES) / sizeof(PAGES[0]);

int  currentPage = 0;
bool toggleState[NUM_PAGES][NUM_BUTTONS] = {};

// ============================================================
//  DIBUJO DE ICONOS
// ============================================================
void drawIcon(int cx, int cy, uint8_t icon, uint16_t color, bool active) {
  uint16_t c = active ? color : tft.color565(120, 120, 120);
  int s = 14;

  switch (icon) {
    case 0: // Micrófono
      tft.fillRoundRect(cx-5, cy-s, 10, s*1.2, 4, c);
      tft.drawArc(cx, cy+2, 11, 8, 0, 180, c);
      tft.drawFastVLine(cx, cy+11, 6, c);
      tft.drawFastHLine(cx-6, cy+17, 12, c);
      break;
    case 1: // Discord (gamepad simplificado)
      tft.drawRoundRect(cx-s, cy-8, s*2, 16, 5, c);
      tft.fillCircle(cx-5, cy, 3, c);
      tft.fillCircle(cx+5, cy, 3, c);
      tft.fillRect(cx-2, cy-s, 4, 8, c);
      break;
    case 2: // Música
      tft.fillRect(cx-2, cy-s+2, 4, s, c);
      tft.fillRect(cx+2, cy-s, 8, 4, c);
      tft.fillRect(cx+6, cy-s+2, 4, 8, c);
      tft.fillCircle(cx-4, cy+2, 5, c);
      tft.fillCircle(cx+4, cy, 5, c);
      break;
    case 3: // Volumen
      tft.fillTriangle(cx-s+4, cy-6, cx-s+4, cy+6, cx-2, cy, c);
      tft.fillRect(cx-s, cy-4, 6, 8, c);
      tft.drawArc(cx+1, cy, 8, 6, 300, 60, c);
      tft.drawArc(cx+1, cy, 13, 11, 300, 60, c);
      break;
    case 4: // Anterior
      tft.fillTriangle(cx+2, cy-s+4, cx+2, cy+s-4, cx-8, cy, c);
      tft.fillTriangle(cx+s-4, cy-s+4, cx+s-4, cy+s-4, cx+4, cy, c);
      tft.fillRect(cx-s+2, cy-s+4, 4, (s-4)*2, c);
      break;
    case 5: // Siguiente
      tft.fillTriangle(cx-2, cy-s+4, cx-2, cy+s-4, cx+8, cy, c);
      tft.fillTriangle(cx-s+4, cy-s+4, cx-s+4, cy+s-4, cx-4, cy, c);
      tft.fillRect(cx+s-6, cy-s+4, 4, (s-4)*2, c);
      break;
    case 6: // Play/Pause
      if (active) {
        tft.fillRect(cx-8, cy-s+4, 5, (s-4)*2, c);
        tft.fillRect(cx+3, cy-s+4, 5, (s-4)*2, c);
      } else {
        tft.fillTriangle(cx-6, cy-s+4, cx-6, cy+s-4, cx+s-2, cy, c);
      }
      break;
    case 7: // Cámara/escena (no usado ya, mantenido por compatibilidad)
      tft.fillRoundRect(cx-s, cy-8, s*1.5, 16, 3, c);
      tft.fillTriangle(cx+8, cy-6, cx+8, cy+6, cx+s+2, cy, c);
      tft.fillCircle(cx-3, cy, 4, C_BTN);
      tft.drawCircle(cx-3, cy, 4, c);
      break;
    case 8: // Mute
      tft.fillTriangle(cx-s+4, cy-6, cx-s+4, cy+6, cx-2, cy, c);
      tft.fillRect(cx-s, cy-4, 6, 8, c);
      tft.drawLine(cx+2, cy-8, cx+s-2, cy+8, c);
      tft.drawLine(cx+2, cy+8, cx+s-2, cy-8, c);
      break;
    case 9: // Cámara web
      tft.drawRoundRect(cx-s, cy-8, s*2, 16, 3, c);
      tft.fillCircle(cx, cy, 5, c);
      tft.fillCircle(cx, cy, 2, C_BTN);
      tft.fillRect(cx-3, cy-s-2, 6, 6, c);
      break;
    case 10: // Terminal (>_)
      tft.drawRoundRect(cx-s, cy-s+4, s*2, (s-4)*2, 3, c);
      // Símbolo ">"
      tft.drawLine(cx-8, cy-4, cx-2, cy, c);
      tft.drawLine(cx-8, cy+4, cx-2, cy, c);
      // Símbolo "_"
      tft.drawFastHLine(cx+1, cy+6, 8, c);
      break;
    case 11: // Power off
      tft.drawArc(cx, cy, s-2, s-6, 45, 315, c);
      tft.drawFastVLine(cx, cy-s, s/2+2, c);
      break;
    case 12: // Captura de pantalla (marco con esquinas tipo recorte)
      {
        int r = s;          // radio aprox
        int len = 8;        // longitud de cada esquina
        // esquina superior izquierda
        tft.drawFastHLine(cx-r, cy-r, len, c);
        tft.drawFastVLine(cx-r, cy-r, len, c);
        // esquina superior derecha
        tft.drawFastHLine(cx+r-len+1, cy-r, len, c);
        tft.drawFastVLine(cx+r, cy-r, len, c);
        // esquina inferior izquierda
        tft.drawFastHLine(cx-r, cy+r-1, len, c);
        tft.drawFastVLine(cx-r, cy+r-len+1, len, c);
        // esquina inferior derecha
        tft.drawFastHLine(cx+r-len+1, cy+r-1, len, c);
        tft.drawFastVLine(cx+r, cy+r-len+1, len, c);
        // circulo central (lente)
        tft.drawCircle(cx, cy, 5, c);
      }
      break;
    case 13: // Navegador (globo terráqueo simplificado)
      tft.drawCircle(cx, cy, s, c);
      tft.drawEllipse(cx, cy, s/2, s, c);
      tft.drawFastHLine(cx-s, cy, s*2, c);
      tft.drawFastHLine(cx-s+2, cy-7, s*2-4, c);
      tft.drawFastHLine(cx-s+2, cy+7, s*2-4, c);
      break;
    case 14: // Carpeta
      tft.fillRoundRect(cx-s, cy-s+4, 12, 6, 2, c);
      tft.fillRoundRect(cx-s, cy-s+7, s*2, s+6, 3, c);
      break;
    case 15: // Administrador de tareas (gráfica de barras)
      tft.fillRect(cx-12, cy+2, 6, 10, c);
      tft.fillRect(cx-3,  cy-8, 6, 20, c);
      tft.fillRect(cx+6,  cy-3, 6, 15, c);
      tft.drawFastHLine(cx-s, cy+13, s*2, c);
      break;
    case 16: // Escritorio (monitor)
      tft.drawRoundRect(cx-s, cy-s+2, s*2, 18, 2, c);
      tft.fillRect(cx-2, cy+6, 4, 5, c);
      tft.fillRect(cx-7, cy+11, 14, 3, c);
      break;
    case 17: // Candado
      tft.drawRoundRect(cx-7, cy-s, 14, 16, 7, c);
      tft.drawRoundRect(cx-6, cy-s+1, 12, 14, 6, c);
      tft.fillRoundRect(cx-10, cy-3, 20, 15, 3, c);
      tft.fillCircle(cx, cy+3, 2, C_BTN);
      break;
    case 18: // Engranaje
      for (int a = 0; a < 360; a += 45) {
        float r = a * DEG_TO_RAD;
        tft.fillCircle(cx + cosf(r) * 10, cy + sinf(r) * 10, 3, c);
      }
      tft.fillCircle(cx, cy, 9, c);
      tft.fillCircle(cx, cy, 4, C_BTN);
      break;
  }
}

// ============================================================
//  GEOMETRÍA
// ============================================================
inline const Button& btn(int idx) { return PAGES[currentPage][idx]; }
inline bool isEmpty(int idx)      { return btn(idx).label == nullptr; }

// Devuelve el índice del botón bajo (tx, ty) o -1
int buttonAt(int32_t tx, int32_t ty) {
  if (tx < MARGIN_X || ty < MARGIN_Y) return -1;
  int col = (tx - MARGIN_X) / (BTN_W + GAP);
  int row = (ty - MARGIN_Y) / (BTN_H + GAP);
  if (col >= COLS || row >= ROWS) return -1;

  int bx = MARGIN_X + col * (BTN_W + GAP);
  int by = MARGIN_Y + row * (BTN_H + GAP);
  if (tx > bx + BTN_W || ty > by + BTN_H) return -1;  // cayó en el hueco entre botones

  int idx = row * COLS + col;
  return isEmpty(idx) ? -1 : idx;
}

// ============================================================
//  DIBUJO DE UN BOTÓN
// ============================================================
void drawButton(int idx, bool pressed) {
  int col = idx % COLS;
  int row = idx / COLS;
  int x = MARGIN_X + col * (BTN_W + GAP);
  int y = MARGIN_Y + row * (BTN_H + GAP);

  if (isEmpty(idx)) {
    tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, C_BG);
    tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, C_BTN);
    return;
  }

  const Button& b = btn(idx);
  bool active = toggleState[currentPage][idx];
  uint16_t bgColor = pressed ? C_BTN_PR : C_BTN;

  tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, bgColor);

  if (active) {
    tft.drawRoundRect(x,   y,   BTN_W,   BTN_H,   8, b.color);
    tft.drawRoundRect(x+1, y+1, BTN_W-2, BTN_H-2, 7, b.color);
  } else {
    tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, C_BORDER);
  }

  drawIcon(x + BTN_W/2, y + BTN_H/2 - 8, b.icon, b.color, active);

  tft.setTextSize(1);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(active ? b.color : C_TEXT);
  tft.drawString(b.label, x + BTN_W/2, y + BTN_H - 14);

  tft.setTextColor(tft.color565(120, 120, 120));
  tft.drawString(b.sublabel, x + BTN_W/2, y + BTN_H - 4);
}

// ============================================================
//  CABECERA CON INDICADOR DE PÁGINA
// ============================================================
void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, tft.color565(20, 20, 40));
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(80, 80, 120));
  tft.setTextDatum(ML_DATUM);
  tft.drawString("ESP32-S3 STREAM DECK", 6, 5);

  if (NUM_PAGES > 1) {
    // Un punto por página; el de la actual, relleno
    const int spacing = 10;
    int x0 = SCREEN_W - 8 - (NUM_PAGES - 1) * spacing;
    for (int p = 0; p < NUM_PAGES; p++) {
      int px = x0 + p * spacing;
      if (p == currentPage) tft.fillCircle(px, 5, 3, C_ICON);
      else                  tft.drawCircle(px, 5, 3, C_GREY);
    }
  }
}

// ============================================================
//  DIBUJA TODO EL DECK
// ============================================================
void drawDeck() {
  tft.startWrite();
  tft.fillScreen(C_BG);
  drawHeader();
  for (int i = 0; i < NUM_BUTTONS; i++) drawButton(i, false);
  tft.endWrite();
}

void changePage(int delta) {
  if (NUM_PAGES <= 1) return;
  currentPage = (currentPage + delta + NUM_PAGES) % NUM_PAGES;
  Serial.printf("\n>>> PAGINA %d/%d\n", currentPage + 1, NUM_PAGES);
  drawDeck();
}

// ============================================================
//  ACCIÓN AL PULSAR UN BOTÓN
// ============================================================
void onButtonPress(int idx) {
  const Button& b = btn(idx);
  Serial.printf("\n>>> P%d BOTON %d pulsado: %s\n", currentPage + 1, idx, b.label);

  if (b.toggle) toggleState[currentPage][idx] = !toggleState[currentPage][idx];
  if (b.cmd)    Serial.println(b.cmd);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== ESP32-S3 Stream Deck ===");

  tft.begin();
  tft.setRotation(1);
  tft.setBrightness(255);

  drawDeck();
  Serial.printf("Listo. %d pagina(s). Esperando toques...\n", NUM_PAGES);
}

// ============================================================
//  LOOP
//  - Toque corto sobre un botón  -> acción al SOLTAR
//  - Deslizar en horizontal      -> cambiar de página
// ============================================================
bool    touchActive  = false;
bool    swiping      = false;
int     pressedBtn   = -1;
uint8_t releaseCount = 0;
int32_t startX, startY, lastX, lastY;

void loop() {
  int32_t tx, ty;
  bool touching = tft.getTouch(&tx, &ty);

  if (touching) {
    releaseCount = 0;

    if (!touchActive) {
      // Empieza un toque
      touchActive = true;
      swiping     = false;
      startX = lastX = tx;
      startY = lastY = ty;
      pressedBtn = buttonAt(tx, ty);
      if (pressedBtn >= 0) drawButton(pressedBtn, true);
    } else {
      lastX = tx;
      lastY = ty;
      // Si el dedo se mueve en horizontal es un deslizamiento: se cancela el botón
      if (!swiping && abs(tx - startX) > SWIPE_START_DX) {
        swiping = true;
        if (pressedBtn >= 0) {
          drawButton(pressedBtn, false);
          pressedBtn = -1;
        }
      }
    }
  } else if (touchActive && ++releaseCount >= RELEASE_FRAMES) {
    // Soltado (tras varias lecturas sin toque, evita rebotes del táctil resistivo)
    touchActive = false;
    int32_t dx = lastX - startX;
    int32_t dy = lastY - startY;

    if (swiping && abs(dx) >= SWIPE_MIN_DX && abs(dx) > abs(dy)) {
      // Dedo hacia la izquierda -> página siguiente (como en el móvil)
      changePage(dx < 0 ? +1 : -1);
    } else if (pressedBtn >= 0) {
      onButtonPress(pressedBtn);
      drawButton(pressedBtn, false);
    }
    pressedBtn = -1;
  }

  delay(LOOP_DELAY_MS);
}

