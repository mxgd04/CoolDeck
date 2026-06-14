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
#define BTN_W       ((480 - MARGIN_X * 2 - GAP * (COLS - 1)) / COLS)
#define BTN_H       ((320 - MARGIN_Y * 2 - GAP * (ROWS - 1)) / ROWS)

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

// ============================================================
//  DEFINICIÓN DE BOTONES
// ============================================================
struct Button {
  const char* label;
  const char* sublabel;
  uint16_t    color;
  uint8_t     icon;  // 0=mic 1=discord 2=music 3=vol 4=prev 5=next
                      // 6=play/pause 7=cam-obs 8=mute 9=webcam
                      // 10=terminal 11=power 12=screenshot 13=browser
};

const Button BUTTONS[NUM_BUTTONS] = {
  // Fila 1 — Micrófono, Discord, Cámara
  { "MIC",       "Mutear",   C_RED,    0 },
  { "DISCORD",   "Mutear",   C_PURPLE, 1 },
  { "CAMARA",    "On/Off",   C_BLUE,   9 },

  // Fila 2 — Spotify
  { "ANTERIOR",  "Spotify",  C_GREEN,  4 },
  { "PLAY",      "Spotify",  C_GREEN,  6 },
  { "SIGUIENTE", "Spotify",  C_GREEN,  5 },

  // Fila 3 — Volumen y utilidades
  { "VOL -",     "Sistema",  C_YELLOW, 3 },
  { "VOL +",     "Sistema",  C_YELLOW, 3 },
  { "CAPTURA",   "Pantalla", C_ORANGE, 12 },

  // Fila 4 — Accesos rápidos
  { "NAVEGADOR", "Abrir",    C_ICON,   13 },
  { "CMD",       "Terminal", C_ICON,   10 },
  { "APAGAR",    "Monitor",  C_RED,    11 },
};

bool toggleState[NUM_BUTTONS] = {false};

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
      tft.fillCircle(cx-3, cy, 4, C_BG);
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
      tft.fillCircle(cx, cy, 2, C_BG);
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
  }
}

// ============================================================
//  DIBUJO DE UN BOTÓN
// ============================================================
void drawButton(int idx, bool pressed) {
  int col = idx % COLS;
  int row = idx / COLS;

  int x = MARGIN_X + col * (BTN_W + GAP);
  int y = MARGIN_Y + row * (BTN_H + GAP);

  bool active = toggleState[idx];
  uint16_t bgColor  = pressed ? C_BTN_PR : C_BTN;
  uint16_t accentColor = BUTTONS[idx].color;

  tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, bgColor);

  if (active) {
    tft.drawRoundRect(x,   y,   BTN_W,   BTN_H,   8, accentColor);
    tft.drawRoundRect(x+1, y+1, BTN_W-2, BTN_H-2, 7, accentColor);
  } else {
    tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, C_BORDER);
  }

  int iconY = y + BTN_H / 2 - 8;
  drawIcon(x + BTN_W/2, iconY, BUTTONS[idx].icon, accentColor, active);

  tft.setTextSize(1);
  tft.setTextColor(active ? accentColor : C_TEXT);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(BUTTONS[idx].label, x + BTN_W/2, y + BTN_H - 14);

  tft.setTextColor(tft.color565(120, 120, 120));
  tft.setTextDatum(BC_DATUM);
  tft.drawString(BUTTONS[idx].sublabel, x + BTN_W/2, y + BTN_H - 4);
}

// ============================================================
//  DIBUJA TODO EL DECK
// ============================================================
void drawDeck() {
  tft.fillScreen(C_BG);

  tft.fillRect(0, 0, 480, 10, tft.color565(20, 20, 40));
  tft.setTextColor(tft.color565(80, 80, 120));
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.drawString("ESP32-S3 STREAM DECK", 6, 5);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    drawButton(i, false);
  }
}

// ============================================================
//  ACCIÓN AL PULSAR UN BOTÓN
// ============================================================
void onButtonPress(int idx) {
  Serial.printf("\n>>> BOTON %d pulsado: %s\n", idx, BUTTONS[idx].label);

  // Botones toggle (estado visual persistente)
  bool isToggle = (idx == 0 || idx == 1 || idx == 2 || idx == 4);
  if (isToggle) {
    toggleState[idx] = !toggleState[idx];
  }

  switch (idx) {
    case 0:  Serial.println("CMD:MIC_TOGGLE");    break;
    case 1:  Serial.println("CMD:DISCORD_MUTE");  break;
    case 2:  Serial.println("CMD:CAMERA_TOGGLE"); break;
    case 3:  Serial.println("CMD:SPOTIFY_PREV");  break;
    case 4:  Serial.println("CMD:SPOTIFY_PLAY");  break;
    case 5:  Serial.println("CMD:SPOTIFY_NEXT");  break;
    case 6:  Serial.println("CMD:VOL_DOWN");      break;
    case 7:  Serial.println("CMD:VOL_UP");        break;
    case 8:  Serial.println("CMD:SCREENSHOT");    break;
    case 9:  Serial.println("CMD:OPEN_BROWSER");  break;
    case 10: Serial.println("CMD:OPEN_TERMINAL"); break;
    case 11: Serial.println("CMD:MONITOR_OFF");   break;
  }
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
  Serial.println("Listo. Esperando toques...");
}

// ============================================================
//  LOOP
// ============================================================
int  lastBtn      = -1;
bool lastTouching = false;

void loop() {
  int32_t tx, ty;
  bool touching = tft.getTouch(&tx, &ty);

  if (touching && !lastTouching) {
    int col = (tx - MARGIN_X) / (BTN_W + GAP);
    int row = (ty - MARGIN_Y) / (BTN_H + GAP);

    if (col >= 0 && col < COLS && row >= 0 && row < ROWS) {
      int bx = MARGIN_X + col * (BTN_W + GAP);
      int by = MARGIN_Y + row * (BTN_H + GAP);
      if (tx >= bx && tx <= bx + BTN_W && ty >= by && ty <= by + BTN_H) {
        int idx = row * COLS + col;
        lastBtn = idx;
        drawButton(idx, true);
        onButtonPress(idx);
      }
    }
  }

  if (!touching && lastTouching && lastBtn >= 0) {
    drawButton(lastBtn, false);
    lastBtn = -1;
  }

  lastTouching = touching;
  delay(20);
}