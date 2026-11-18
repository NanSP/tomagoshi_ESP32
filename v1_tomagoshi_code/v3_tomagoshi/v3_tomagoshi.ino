#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <EEPROM.h>
#include <stdio.h>

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// ILI9341 em landscape: 320x240
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Botões e buzzer
const int pinoBotaoNavegar = 15;
const int pinoBotaoSelect  = 17;
const int pinoBuzzer       = 32;

struct ButtonState {
  bool rawPressed;
  bool stablePressed;
  bool pressEvent;
  bool shortPress;
  bool longPress;
  bool longHandled;
  unsigned long rawChangedAt;
  unsigned long pressedAt;

  void update(int pin, unsigned long now) {
    pressEvent = false;
    shortPress = false;
    longPress = false;

    bool pressed = digitalRead(pin) == LOW;
    if (pressed != rawPressed) {
      rawPressed = pressed;
      rawChangedAt = now;
    }

    if (pressed != stablePressed && now - rawChangedAt >= 20UL) {
      stablePressed = pressed;
      if (pressed) {
        pressEvent = true;
        pressedAt = now;
        longHandled = false;
      } else if (!longHandled) {
        shortPress = true;
      }
    }

    if (stablePressed && rawPressed && !longHandled && now - pressedAt >= 600UL) {
      longPress = true;
      longHandled = true;
    }
  }

  bool clicked() {
    return pressEvent || shortPress;
  }

  void consume() {
    pressEvent = false;
    shortPress = false;
    longHandled = true;
  }
};

ButtonState navigateButton = {};
ButtonState selectButton = {};

// Cores ILI9341 (mesmos valores RGB565 que ST7735)
#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0

// Aliases ST7735 → ILI9341 para compatibilidade
#define ST7735_BLACK   ILI9341_BLACK
#define ST7735_WHITE   ILI9341_WHITE
#define ST7735_RED     ILI9341_RED
#define ST7735_GREEN   ILI9341_GREEN
#define ST7735_BLUE    ILI9341_BLUE
#define ST7735_CYAN    ILI9341_CYAN
#define ST7735_MAGENTA ILI9341_MAGENTA
#define ST7735_YELLOW  ILI9341_YELLOW

#define COR_FUNDO      ILI9341_BLACK
#define COR_TEXTO      ILI9341_WHITE
#define COR_BARRA      ILI9341_GREEN
#define COR_PERSONAGEM ILI9341_YELLOW
#define COR_ASFALTO    0x2104
#define COR_GRAMA      0x03E0

struct TamagotchiState {
  int energia;
  int felicidade;
  int fome;
  int high_score;
  byte magicByte;
};

struct StateIntegrity {
  uint32_t magic;
  uint32_t checksum;
};

const int integrityAddress = 64;
const uint32_t integrityMagic = 0x54414D31UL;

uint32_t stateChecksum(int energiaValue, int felicidadeValue, int fomeValue,
                       int highScoreValue, byte magicValue) {
  uint32_t hash = 2166136261UL;
  const int values[] = { energiaValue, felicidadeValue, fomeValue, highScoreValue };
  for (int i = 0; i < 4; i++) {
    hash = (hash ^ (uint32_t)values[i]) * 16777619UL;
  }
  return (hash ^ magicValue) * 16777619UL;
}

#define MODE_TAMAGOTCHI 0
#define MODE_GAME       1
#define MODE_MENU       2
#define MODE_GAME_PAUSED 3
#define MODE_GAME_OVER 4
#define MODE_GAME_SELECT 5
#define MODE_CLOCK_SET 6

#define GAME_JUMP 0
#define GAME_SHIP 1
#define GAME_CAR  2

int currentMode = MODE_TAMAGOTCHI;
int currentGame = GAME_JUMP;

// Menu
int menuOpcao = 0;
int lastMenuOpcao = -1;
const int totalOpcoes = 4;
bool menuInitialized = false;
int gameMenuOpcao = 0;
int lastGameMenuOpcao = -1;
bool gameMenuInitialized = false;

// Status
int energia = 100;
int felicidade = 100;
int fome = 100;
bool estaDormindo = false;

int lastEnergia = 100;
int lastFelicidade = 100;
int lastFome = 100;

int gameHour = 8;
int gameMinute = 0;
int lastHour = 8;
int lastMinute = 0;
bool clockConfigured = false;
int clockEditHour = 8;
int clockEditMinute = 0;
int clockEditField = 0;

unsigned long previousMillisStats = 0;
const long intervalStats = 30000;
unsigned long previousMillisSleep = 0;
const unsigned long sleepInterval = 5000UL;
const int sleepEnergyGain = 2;

unsigned long previousMillisSave = 0;
const long saveInterval = 300000;

unsigned long previousMillisClock = 0;
unsigned long lastRedrawTime = 0;
bool messageActive = false;
unsigned long messageStarted = 0;
const unsigned long messageDuration = 2000;
int lastNeed = -1;

// Jogo
int score = 0;
int high_score = 0;

#define PLAYER_X 50
#define PLAYER_SIZE 20
#define GROUND_Y 220
#define GRAVITY -1.0
#define JUMP_FORCE 14

float player_y = GROUND_Y - PLAYER_SIZE;
float player_vel_y = 0;
bool player_is_jumping = false;

int obstacle_x = SCREEN_WIDTH;
int last_obstacle_x = SCREEN_WIDTH;
int obstacle_width = 16;
int obstacle_height = 32;

int game_speed = 2;
int last_game_speed = 2;
int last_score = -1;
float last_player_y = GROUND_Y - PLAYER_SIZE;
const int gameEnergyCost = 5;

// Jogo da nave
#define SHIP_WIDTH  20
#define SHIP_HEIGHT 12
#define SHIP_Y (SCREEN_HEIGHT - 20)
#define SHIP_SPEED  5
#define MAX_ENEMIES 4
#define MAX_BULLETS 4
#define ENEMY_WIDTH  16
#define ENEMY_HEIGHT 12
#define BULLET_WIDTH  4
#define BULLET_HEIGHT  8

struct Enemy {
  int x, y;
  int lastX, lastY;
  bool active;
  bool wasDrawn;
};

struct Bullet {
  int x, y;
  int lastX, lastY;
  bool active;
  bool wasDrawn;
};

Enemy enemies[MAX_ENEMIES];
Bullet bullets[MAX_BULLETS];
int ship_x = SCREEN_WIDTH / 2 - SHIP_WIDTH / 2;
int last_ship_x = ship_x;
int ship_score = 0;
int last_ship_score = -1;
int high_score_ship = 0;
unsigned long lastEnemySpawn = 0;
unsigned long lastAutoFire = 0;
unsigned long bothPressedAt = 0;
long enemySpawnInterval = 1100;
const long autoFireInterval = 450;
int ship_enemy_speed = 2;

// Jogo do carro
#define ROAD_X  80
#define ROAD_W 160
#define CAR_W   16
#define CAR_H   26
#define CAR_Y   190
#define CAR_SPEED 5
#define DASH_X (ROAD_X + ROAD_W / 2 - 2)
#define MAX_OBS 3
#define MAX_COINS 2

struct Obstacle {
  int x, y;
  int lastX, lastY;
  bool active;
  bool wasDrawn;
  uint16_t color;
};

struct Coin {
  int x, y;
  int lastX, lastY;
  bool active;
  bool wasDrawn;
};

Obstacle obs[MAX_OBS];
Coin coins[MAX_COINS];
const uint16_t obsColors[4] = { ILI9341_RED, 0xFD20, ILI9341_YELLOW, ILI9341_MAGENTA };

int car_x = ROAD_X + ROAD_W / 2 - CAR_W / 2;
int last_car_x = car_x;
int car_score = 0;
int last_car_score = -1;
int high_score_car = 0;
int car_speed = 3;
long obsSpawnInterval = 1000;
long coinSpawnInterval = 3000;
unsigned long carScoreTick = 0;
unsigned long lastSpeedUpTime = 0;
unsigned long lastObsSpawn = 0;
unsigned long lastCoinSpawn = 0;
unsigned long lastCarUpdate = 0;
int dashOffset = 0;

struct ShipRecord {
  int high_score;
  byte magicByte;
};

struct CarRecord {
  int high_score;
  byte magicByte;
};

// ================= EEPROM =================

void saveData() {
  TamagotchiState state = { energia, felicidade, fome, high_score, 0xAD };
  EEPROM.put(0, state);
  ShipRecord shipRecord = { high_score_ship, 0xB1 };
  EEPROM.put(sizeof(TamagotchiState), shipRecord);
  CarRecord carRecord = { high_score_car, 0xC2 };
  EEPROM.put(sizeof(TamagotchiState) + sizeof(ShipRecord), carRecord);
  StateIntegrity integrity = { integrityMagic,
    stateChecksum(state.energia, state.felicidade, state.fome, state.high_score, state.magicByte) };
  EEPROM.put(integrityAddress, integrity);
  EEPROM.commit();
}

void loadData() {
  EEPROM.begin(512);

  TamagotchiState state;
  EEPROM.get(0, state);

  StateIntegrity integrity;
  EEPROM.get(integrityAddress, integrity);
  bool hasChecksum = integrity.magic == integrityMagic;

  bool stateValid = state.magicByte == 0xAD &&
      state.energia >= 0 && state.energia <= 100 &&
      state.felicidade >= 0 && state.felicidade <= 100 &&
      state.fome >= 0 && state.fome <= 100 &&
      state.high_score >= 0 &&
      (!hasChecksum || integrity.checksum == stateChecksum(
        state.energia, state.felicidade, state.fome, state.high_score, state.magicByte));
  if (stateValid) {
    energia = state.energia;
    felicidade = state.felicidade;
    fome = state.fome;
    high_score = state.high_score;
  } else {
    energia = 100;
    felicidade = 100;
    fome = 100;
    high_score = 0;
  }

  ShipRecord shipRecord;
  EEPROM.get(sizeof(TamagotchiState), shipRecord);
  bool shipRecordValid = shipRecord.magicByte == 0xB1 && shipRecord.high_score >= 0;
  high_score_ship = shipRecordValid ? shipRecord.high_score : 0;

  CarRecord carRecord;
  EEPROM.get(sizeof(TamagotchiState) + sizeof(ShipRecord), carRecord);
  bool carRecordValid = carRecord.magicByte == 0xC2 && carRecord.high_score >= 0;
  high_score_car = carRecordValid ? carRecord.high_score : 0;

  if (!stateValid || !shipRecordValid || !carRecordValid || !hasChecksum) saveData();

  lastEnergia = energia;
  lastFelicidade = felicidade;
  lastFome = fome;
}

// ================= SOM =================

void play_tone(int freq, int duration) {
  tone(pinoBuzzer, freq, duration);
  delay(duration);
  noTone(pinoBuzzer);
}

// ================= INTERFACE =================

void drawStatusBar(int x, int y, const char* label, int valor) {
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(x, y);
  tft.print(label);

  tft.drawRect(x + 24, y, 70, 14, COR_TEXTO);

  int largura = map(valor, 0, 100, 0, 66);
  tft.fillRect(x + 25, y + 2, largura, 10, COR_BARRA);
}

void drawClock() {
  tft.fillRect(0, 222, SCREEN_WIDTH, 18, COR_FUNDO);
  tft.setCursor(10, 224);
  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  if (clockConfigured) {
    if (gameHour < 10) tft.print("0");
    tft.print(gameHour);
    tft.print(":");
    if (gameMinute < 10) tft.print("0");
    tft.print(gameMinute);
  } else {
    tft.print("--:--");
  }
  tft.setCursor(120, 224);
  tft.print("SEL AJUSTA HORA");
  lastHour = gameHour;
  lastMinute = gameMinute;
}

int currentNeed() {
  if (fome < 20) return 1;
  if (energia < 10) return 2;
  if (felicidade < 20) return 3;
  if (fome < 40) return 4;
  return 0;
}

void updateNeed() {
  int need = currentNeed();
  if (need == lastNeed) return;
  tft.fillRect(0, 20, SCREEN_WIDTH, 20, COR_FUNDO);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW, COR_FUNDO);
  tft.setCursor(10, 22);
  if (need == 1) tft.print("PRECISA COMER");
  else if (need == 2) tft.print("PRECISA DORMIR");
  else if (need == 3) tft.print("PRECISA BRINCAR");
  else if (need == 4) tft.print("POUCA SACIEDADE");
  lastNeed = need;
}

void showMessage(const char* message) {
  tft.fillRect(0, 222, SCREEN_WIDTH, 18, COR_FUNDO);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
  tft.setCursor(8, 224);
  tft.print(message);
  messageActive = true;
  messageStarted = millis();
}

void fullRedraw(bool showClock);

void drawClockSetup() {
  tft.fillScreen(COR_FUNDO);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
  tft.setCursor(16, 20);
  tft.print("AJUSTAR HORA");

  tft.setTextSize(4);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(70, 90);
  if (clockEditHour < 10) tft.print("0");
  tft.print(clockEditHour);
  tft.print(":");
  if (clockEditMinute < 10) tft.print("0");
  tft.print(clockEditMinute);
  tft.drawRect(clockEditField == 0 ? 64 : 172, 86, 80, 44, ILI9341_YELLOW);

  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(10, 160);
  tft.print("NAV +1  SEG +5");
  tft.setCursor(10, 185);
  tft.print("SEL PROX / CONFIRMA");
  tft.setCursor(10, 210);
  tft.print("SEG SEL: CANCELA");
}

void enterClockSetup() {
  clockEditHour = gameHour;
  clockEditMinute = gameMinute;
  clockEditField = 0;
  currentMode = MODE_CLOCK_SET;
  selectButton.consume(); // ignora a soltura que abriu esta tela
  drawClockSetup();
}

void clock_setup_loop() {
  if (selectButton.longPress) {
    selectButton.consume();
    currentMode = MODE_TAMAGOTCHI;
    fullRedraw(true);
    return;
  }

  if (navigateButton.shortPress || navigateButton.longPress) {
    int step = navigateButton.longPress ? 5 : 1;
    navigateButton.consume();
    if (clockEditField == 0) clockEditHour = (clockEditHour + step) % 24;
    else clockEditMinute = (clockEditMinute + step) % 60;
    drawClockSetup();
  }

  if (selectButton.clicked()) {
    selectButton.consume();
    if (clockEditField == 0) {
      clockEditField = 1;
      drawClockSetup();
    } else {
      gameHour = clockEditHour;
      gameMinute = clockEditMinute;
      clockConfigured = true;
      previousMillisClock = millis();
      currentMode = MODE_TAMAGOTCHI;
      tone(pinoBuzzer, 880, 100);
      fullRedraw(true);
    }
  }
}

void updateInterface() {
  if (energia != lastEnergia) {
    tft.fillRect(28, 4, 66, 10, COR_FUNDO);
    int largura = map(energia, 0, 100, 0, 66);
    tft.fillRect(28, 4, largura, 10, COR_BARRA);
    lastEnergia = energia;
  }

  if (felicidade != lastFelicidade) {
    tft.fillRect(134, 4, 66, 10, COR_FUNDO);
    int largura = map(felicidade, 0, 100, 0, 66);
    tft.fillRect(134, 4, largura, 10, COR_BARRA);
    lastFelicidade = felicidade;
  }

  if (fome != lastFome) {
    tft.fillRect(240, 4, 66, 10, COR_FUNDO);
    int largura = map(fome, 0, 100, 0, 66);
    tft.fillRect(240, 4, largura, 10, COR_BARRA);
    lastFome = fome;
  }

  if (messageActive && millis() - messageStarted >= messageDuration) {
    messageActive = false;
    drawClock();
  } else if (!messageActive && (gameHour != lastHour || gameMinute != lastMinute)) {
    drawClock();
  }
  updateNeed();
}

// ================= PERSONAGEM =================

enum EyePose { EYES_NORMAL, EYES_BLINK, EYES_LEFT, EYES_RIGHT, EYES_WINK };
EyePose eyePose = EYES_NORMAL;
unsigned long lastEyeAnimationTime = 0;
unsigned long eyePoseStarted = 0;
int nextEyeAnimation = 0;
int displayedFace = -1; // 0: normal, 1: triste, 2: doente, 3: dormindo

int eyeW = 80;
int eyeH = 70;
int eyeY = 90;

int leftEyeX = 50;
int rightEyeX = 190;

void drawEyesPose(int pose) {
  tft.fillRect(0, 40, 320, 180, COR_FUNDO);

  if (pose == EYES_BLINK) {
    tft.drawLine(leftEyeX, eyeY + 18, leftEyeX + eyeW, eyeY + 18, COR_TEXTO);
    tft.drawLine(rightEyeX, eyeY + 18, rightEyeX + eyeW, eyeY + 18, COR_TEXTO);
    return;
  }

  int pupilOffset = pose == EYES_LEFT ? -12 : (pose == EYES_RIGHT ? 12 : 0);
  if (pose == EYES_WINK) {
    tft.drawLine(leftEyeX, eyeY + 18, leftEyeX + eyeW, eyeY + 18, COR_TEXTO);
  } else {
    tft.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 16, COR_TEXTO);
    tft.fillRoundRect(leftEyeX + 30 + pupilOffset, eyeY + 16,
                      20, 36, 10, COR_FUNDO);
  }
  tft.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 16, COR_TEXTO);
  tft.fillRoundRect(rightEyeX + 30 + pupilOffset, eyeY + 16,
                    20, 36, 10, COR_FUNDO);
}

void drawEyes() {
  drawEyesPose(EYES_NORMAL);
}

void drawCharacter() {
  eyePose = EYES_NORMAL;
  lastEyeAnimationTime = millis();
  drawEyes();
}

void updateIdleEyes(unsigned long now) {
  if (eyePose != EYES_NORMAL) {
    unsigned long duration = eyePose == EYES_BLINK ? 120UL :
                             (eyePose == EYES_WINK ? 350UL : 600UL);
    if (now - eyePoseStarted >= duration) {
      eyePose = EYES_NORMAL;
      drawEyes();
    }
    return;
  }

  if (now - lastEyeAnimationTime >= 3200UL) {
    const EyePose sequence[] = { EYES_LEFT, EYES_RIGHT, EYES_WINK, EYES_BLINK };
    eyePose = sequence[nextEyeAnimation];
    nextEyeAnimation = (nextEyeAnimation + 1) % 4;
    eyePoseStarted = now;
    lastEyeAnimationTime = now;
    drawEyesPose(eyePose);
  }
}

void drawSleeping();

void fullRedraw(bool showClock) {
  tft.fillScreen(COR_FUNDO);

  drawStatusBar(2, 2, "EN", energia);
  drawStatusBar(108, 2, "FE", felicidade);
  drawStatusBar(214, 2, "SA", fome);
  messageActive = false;
  if (showClock) drawClock();

  if (estaDormindo) {
    drawSleeping();
    displayedFace = 3;
    lastRedrawTime = millis();
  } else {
    drawCharacter();
    displayedFace = 0;
  }
  lastNeed = -1;
  updateNeed();

  lastEnergia = energia;
  lastFelicidade = felicidade;
  lastFome = fome;
  lastHour = gameHour;
  lastMinute = gameMinute;
}

// ==================================================
// FELIZ
// ==================================================

void drawHappyFace() {
  tft.fillScreen(COR_FUNDO);

  // ^ ^
  tft.drawLine(60, 140, 90, 110, COR_TEXTO);
  tft.drawLine(90, 110, 120, 140, COR_TEXTO);

  tft.drawLine(200, 140, 230, 110, COR_TEXTO);
  tft.drawLine(230, 110, 260, 140, COR_TEXTO);

  // sorriso
  tft.drawLine(120, 190, 150, 210, COR_TEXTO);
  tft.drawLine(150, 210, 170, 210, COR_TEXTO);
  tft.drawLine(170, 210, 200, 190, COR_TEXTO);

  tone(pinoBuzzer, 1000, 100);
  delay(1000);
}

// ==================================================
// TRISTE
// ==================================================

void drawSadFace() {

  tone(pinoBuzzer, 200, 300);

  tft.fillRect(0, 40, 320, 180, COR_FUNDO);

  // olhos inclinados
  tft.drawLine(60, 110, 120, 140, COR_TEXTO);
  tft.drawLine(200, 140, 260, 110, COR_TEXTO);

  // boca triste
  tft.drawLine(120, 205, 150, 185, COR_TEXTO);
  tft.drawLine(150, 185, 170, 185, COR_TEXTO);
  tft.drawLine(170, 185, 200, 205, COR_TEXTO);

}

// ==================================================
// DOENTE
// ==================================================

void drawSickFace() {

  tone(pinoBuzzer, 100, 500);

  tft.fillRect(0, 40, 320, 180, COR_FUNDO);

  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(5);

  tft.setCursor(70, 100);
  tft.print("X");

  tft.setCursor(210, 100);
  tft.print("X");

  // boca torta
  tft.drawLine(130, 190, 150, 180, ILI9341_RED);
  tft.drawLine(150, 180, 170, 200, ILI9341_RED);
  tft.drawLine(170, 200, 190, 190, ILI9341_RED);

  tft.setTextColor(COR_TEXTO);
}

// ==================================================
// COMER
// ==================================================

void drawEatingFace() {
  tft.fillScreen(COR_FUNDO);
  drawEyes();

  // boca aberta
  tft.fillCircle(160, 195, 24, COR_TEXTO);
  tft.fillCircle(160, 195, 12, COR_FUNDO);

  tone(pinoBuzzer, 1200, 80);
  delay(800);
}

// ==================================================
// DORMINDO
// ==================================================

void drawSleeping() {

  tft.fillRect(0, 40, 320, 180, COR_FUNDO);

  tft.drawLine(leftEyeX, eyeY + 35,
               leftEyeX + eyeW,
               eyeY + 35,
               COR_TEXTO);

  tft.drawLine(rightEyeX, eyeY + 35,
               rightEyeX + eyeW,
               eyeY + 35,
               COR_TEXTO);

  tft.setTextColor(COR_TEXTO);
  tft.setTextSize(4);

  tft.setCursor(120, 50);
  tft.print("Zzz");
}

// ================= MENU =================

void drawMenuItem(int index, bool selected) {
  int x = 16 + (index * 76);
  int y = 80;

  tft.fillRect(x - 4, y - 4, 68, 96, COR_FUNDO);

  if (selected) {
    tft.drawRect(x - 4, y - 4, 68, 68, ILI9341_YELLOW);
  } else {
    tft.drawRect(x, y, 60, 60, COR_TEXTO);
  }

  if (index == 0) {
    tft.fillCircle(x + 30, y + 30, 18, ILI9341_GREEN);
    tft.fillRect(x + 24, y + 24, 12, 12, COR_FUNDO);
  } else if (index == 1) {
    tft.fillCircle(x + 30, y + 30, 18, ILI9341_YELLOW);
    tft.fillRect(x + 22, y + 26, 16, 8, COR_FUNDO);
  } else if (index == 2) {
    tft.fillCircle(x + 30, y + 30, 18, ILI9341_BLUE);
    tft.setCursor(x + 20, y + 16);
    tft.setTextSize(3);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.print("Z");
  } else if (index == 3) {
    tft.fillCircle(x + 30, y + 30, 18, ILI9341_RED);
    tft.fillRect(x + 22, y + 28, 16, 6, COR_FUNDO);
    tft.fillRect(x + 28, y + 22, 6, 16, COR_FUNDO);
  }

  const char* opcoes[] = {"COM", "JOG", estaDormindo ? "ACO" : "DOR", "CUR"};

  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(x + 10, y + 72);
  tft.print(opcoes[index]);
}

void drawMenu() {
  if (!menuInitialized) {
    tft.fillScreen(COR_FUNDO);

    tft.setTextSize(3);
    tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
    tft.setCursor(110, 20);
    tft.print("MENU");

    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);

    for (int i = 0; i < totalOpcoes; i++) {
      drawMenuItem(i, false);
    }

    tft.setTextSize(2);
    tft.setCursor(10, 200);
    tft.print("NAV PROX.  SEL OK");
    tft.setCursor(10, 220);
    tft.print("SEGURE NAV: VOLTAR");

    menuInitialized = true;
    lastMenuOpcao = -1;
  }

  if (lastMenuOpcao != menuOpcao) {
    if (lastMenuOpcao >= 0) {
      drawMenuItem(lastMenuOpcao, false);
    }

    drawMenuItem(menuOpcao, true);
    const char* nomes[] = {"ALIMENTAR", "JOGAR", estaDormindo ? "ACORDAR" : "DORMIR", "CURAR"};
    tft.fillRect(0, 175, SCREEN_WIDTH, 20, COR_FUNDO);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_YELLOW, COR_FUNDO);
    tft.setCursor(80, 177);
    tft.print(nomes[menuOpcao]);
    lastMenuOpcao = menuOpcao;
  }
}

void handleAlimentar();
void handleDormir();
void handleCurar();
void startGame();

void menu_loop() {
  drawMenu();

  if (navigateButton.longPress) {
    navigateButton.consume();
    menuInitialized = false;
    currentMode = MODE_TAMAGOTCHI;
    fullRedraw(true);
    tone(pinoBuzzer, 330, 80);
    return;
  }

  if (navigateButton.shortPress) {
    navigateButton.consume();
    menuOpcao = (menuOpcao + 1) % totalOpcoes;
    tone(pinoBuzzer, 440, 50);
  }

  if (selectButton.clicked()) {
    selectButton.consume();
    tone(pinoBuzzer, 880, 100);

    menuInitialized = false;

    switch (menuOpcao) {
      case 0:
        handleAlimentar();
        currentMode = MODE_TAMAGOTCHI;
        break;

      case 1:
        gameMenuOpcao = 0;
        gameMenuInitialized = false;
        currentMode = MODE_GAME_SELECT;
        break;

      case 2:
        handleDormir();
        currentMode = MODE_TAMAGOTCHI;
        break;

      case 3:
        handleCurar();
        currentMode = MODE_TAMAGOTCHI;
        break;
    }
  }
}

void drawGameMenuItem(int index, bool selected) {
  int x = 16 + index * 100;
  int y = 55;
  tft.fillRect(x - 4, y - 4, 92, 124, COR_FUNDO);
  tft.drawRect(x - 2, y - 2, 88, 92, selected ? ILI9341_YELLOW : COR_TEXTO);

  if (index == GAME_JUMP) {
    tft.fillRect(x + 32, y + 52, 20, 20, ILI9341_GREEN);
    tft.drawLine(x + 42, y + 14, x + 42, y + 46, COR_TEXTO);
    tft.drawLine(x + 34, y + 24, x + 42, y + 14, COR_TEXTO);
    tft.drawLine(x + 50, y + 24, x + 42, y + 14, COR_TEXTO);
  } else if (index == GAME_SHIP) {
    tft.fillTriangle(x + 42, y + 16, x + 20, y + 72,
                     x + 64, y + 72, ILI9341_CYAN);
  } else {
    tft.fillRoundRect(x + 28, y + 16, 28, 52, 4, ILI9341_YELLOW);
    tft.fillRect(x + 34, y + 30, 16, 10, 0x0000);
    tft.fillRect(x + 34, y + 52, 16, 6, 0x0000);
    tft.fillRect(x + 24, y + 22, 4, 12, 0xFFFF);
    tft.fillRect(x + 56, y + 22, 4, 12, 0xFFFF);
    tft.fillRect(x + 24, y + 48, 4, 12, 0xFFFF);
    tft.fillRect(x + 56, y + 48, 4, 12, 0xFFFF);
  }

  tft.setTextSize(2);
  tft.setTextColor(selected ? ILI9341_YELLOW : COR_TEXTO, COR_FUNDO);
  if (index == GAME_JUMP) {
    tft.setCursor(x + 20, y + 98);
    tft.print("PULO");
  } else if (index == GAME_SHIP) {
    tft.setCursor(x + 20, y + 98);
    tft.print("NAVE");
  } else {
    tft.setCursor(x + 14, y + 98);
    tft.print("CARRO");
  }
}

void drawGameSelectMenu() {
  if (!gameMenuInitialized) {
    tft.fillScreen(COR_FUNDO);
    tft.setTextSize(3);
    tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
    tft.setCursor(76, 16);
    tft.print("ESCOLHA O JOGO");
    drawGameMenuItem(GAME_JUMP, false);
    drawGameMenuItem(GAME_SHIP, false);
    drawGameMenuItem(GAME_CAR, false);
    tft.setTextSize(2);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(10, 196);
    tft.print("NAV PROX.  SEL OK");
    tft.setCursor(10, 218);
    tft.print("SEGURE NAV: VOLTAR");
    gameMenuInitialized = true;
    lastGameMenuOpcao = -1;
  }

  if (lastGameMenuOpcao != gameMenuOpcao) {
    if (lastGameMenuOpcao >= 0) drawGameMenuItem(lastGameMenuOpcao, false);
    drawGameMenuItem(gameMenuOpcao, true);
    tft.fillRect(0, 170, SCREEN_WIDTH, 20, COR_FUNDO);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_YELLOW, COR_FUNDO);
    tft.setCursor(4, 172);
    if (gameMenuOpcao == GAME_SHIP) tft.print("NAV< SEL> AMBOS PAUSA");
    else if (gameMenuOpcao == GAME_CAR) tft.print("NAV< SEL> AMBOS PAUSA");
    else tft.print("NAV PULA SEL PAUSA");
    lastGameMenuOpcao = gameMenuOpcao;
  }
}

void game_select_loop() {
  drawGameSelectMenu();
  if (navigateButton.longPress) {
    navigateButton.consume();
    gameMenuInitialized = false;
    menuInitialized = false;
    currentMode = MODE_MENU;
    tone(pinoBuzzer, 330, 80);
    return;
  }
  if (navigateButton.shortPress) {
    navigateButton.consume();
    gameMenuOpcao = (gameMenuOpcao + 1) % 3;
    tone(pinoBuzzer, 440, 50);
  }
  if (selectButton.clicked()) {
    selectButton.consume();
    gameMenuInitialized = false;
    currentGame = gameMenuOpcao;
    startGame();
  }
}

// ================= JOGO =================

void game_reset() {
  tone(pinoBuzzer, 600, 100);

  score = 0;
  last_score = -1;

  game_speed = 2;
  last_game_speed = 2;

  player_y = GROUND_Y - PLAYER_SIZE;
  last_player_y = player_y;

  player_vel_y = 0;
  player_is_jumping = false;

  obstacle_x = SCREEN_WIDTH;
  last_obstacle_x = SCREEN_WIDTH;

  tft.fillScreen(COR_FUNDO);
  tft.drawLine(0, GROUND_Y, SCREEN_WIDTH, GROUND_Y, COR_TEXTO);
  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(8, 222);
  tft.print("NAV PULA  SEL PAUSA");
}

void ship_game_reset();
void car_game_reset();

void startGame() {
  if (energia < gameEnergyCost) {
    currentMode = MODE_TAMAGOTCHI;
    fullRedraw(false);
    showMessage("SEM ENERGIA PARA JOGAR");
    return;
  }
  energia -= gameEnergyCost;
  currentMode = MODE_GAME;
  if (currentGame == GAME_SHIP) ship_game_reset();
  else if (currentGame == GAME_CAR) car_game_reset();
  else game_reset();
}

void showGameOver(int finalScore, int& highScore) {
  tone(pinoBuzzer, 200, 500);

  int oldFelicidade = felicidade;
  felicidade = min(100, felicidade + min(10, finalScore + 2));
  int gainedFelicidade = felicidade - oldFelicidade;
  bool newRecord = finalScore > highScore;
  if (newRecord) highScore = finalScore;
  saveData();

  tft.fillScreen(COR_FUNDO);

  tft.setTextSize(4);
  tft.setTextColor(ILI9341_RED, COR_FUNDO);
  tft.setCursor(50, 30);
  tft.print("GAME OVER");

  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(110, 90);
  if (currentGame == GAME_SHIP) tft.print("NAVE");
  else if (currentGame == GAME_CAR) tft.print("CARRO");
  else tft.print("PULO");
  tft.setCursor(84, 115);
  tft.print("Score: ");
  tft.print(finalScore);
  tft.setCursor(84, 140);
  tft.print("Recorde: ");
  tft.print(highScore);
  tft.setCursor(52, 165);
  tft.print("FEL +");
  tft.print(gainedFelicidade);
  tft.print("   EN -");
  tft.print(gameEnergyCost);

  if (newRecord) {
    tft.setCursor(80, 190);
    tft.setTextColor(ILI9341_YELLOW, COR_FUNDO);
    tft.print("NOVO RECORDE!");
  }
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(56, 210);
  tft.print("NAV: VOLTAR");
  tft.setCursor(56, 228);
  tft.print("SEL: REPETIR");
  currentMode = MODE_GAME_OVER;
}

void game_over() {
  showGameOver(score, high_score);
}

void updateGameDisplay() {
  if (last_player_y != player_y) {
    tft.fillRect(PLAYER_X, last_player_y, PLAYER_SIZE, PLAYER_SIZE, COR_FUNDO);
  }

  if (last_obstacle_x != obstacle_x) {
    tft.fillRect(last_obstacle_x, GROUND_Y - obstacle_height, obstacle_width, obstacle_height, COR_FUNDO);
  }

  tft.drawLine(0, GROUND_Y, SCREEN_WIDTH, GROUND_Y, COR_TEXTO);

  tft.fillRect(PLAYER_X, player_y, PLAYER_SIZE, PLAYER_SIZE, ST7735_GREEN);
  tft.fillRect(obstacle_x, GROUND_Y - obstacle_height, obstacle_width, obstacle_height, ST7735_RED);

  if (score != last_score) {
    tft.fillRect(10, 20, 160, 22, COR_FUNDO);
    tft.setTextSize(2);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(10, 20);
    tft.print("Score: ");
    tft.print(score);
    last_score = score;
  }

  last_player_y = player_y;
  last_obstacle_x = obstacle_x;
}

void pauseGame() {
  currentMode = MODE_GAME_PAUSED;
  tft.fillScreen(COR_FUNDO);
  tft.setTextSize(4);
  tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
  tft.setCursor(74, 50);
  tft.print("PAUSADO");
  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(106, 120);
  if (currentGame == GAME_SHIP) tft.print("NAVE");
  else if (currentGame == GAME_CAR) tft.print("CARRO");
  else tft.print("PULO");
  tft.setCursor(56, 155);
  tft.print("SEL: CONTINUAR");
  tft.setCursor(56, 185);
  tft.print("NAV: SAIR");
}

void updateShipDisplay();
void updateCarDisplay(int oldDashOffset);
void fillRectClip(int x, int y, int w, int h, uint16_t c);

void resumeGame() {
  currentMode = MODE_GAME;
  tft.fillScreen(COR_FUNDO);
  if (currentGame == GAME_SHIP) {
    last_ship_score = -1;
    updateShipDisplay();
    return;
  }
  if (currentGame == GAME_CAR) {
    last_car_score = -1;
    last_car_x = car_x;
    tft.fillScreen(COR_GRAMA);
    tft.fillRect(ROAD_X, 0, ROAD_W, SCREEN_HEIGHT, COR_ASFALTO);
    tft.fillRect(ROAD_X - 4, 0, 4, SCREEN_HEIGHT, COR_TEXTO);
    tft.fillRect(ROAD_X + ROAD_W, 0, 4, SCREEN_HEIGHT, COR_TEXTO);
    for (int y = -40; y < SCREEN_HEIGHT; y += 40) {
      fillRectClip(DASH_X, y, 4, 20, COR_TEXTO);
    }
    updateCarDisplay(dashOffset);
    return;
  }
  last_player_y = player_y;
  last_obstacle_x = obstacle_x;
  last_score = -1;
  updateGameDisplay();
  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(8, 222);
  tft.print("NAV PULA  SEL PAUSA");
}

void jump_game_loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastGameUpdate = 0;

  if (selectButton.clicked()) {
    selectButton.consume();
    pauseGame();
    return;
  }

  if (navigateButton.clicked() && !player_is_jumping) {
    navigateButton.consume();
    player_vel_y = JUMP_FORCE;
    player_is_jumping = true;
    tone(pinoBuzzer, 800, 50);
  }

  if (currentMillis - lastGameUpdate < 25) {
    return;
  }

  lastGameUpdate = currentMillis;

  player_y -= player_vel_y;
  player_vel_y += GRAVITY;

  if (player_y > GROUND_Y - PLAYER_SIZE) {
    player_y = GROUND_Y - PLAYER_SIZE;
    player_vel_y = 0;
    player_is_jumping = false;
  }

  obstacle_x -= game_speed;

  if (obstacle_x < -obstacle_width) {
    obstacle_x = SCREEN_WIDTH;
    score++;

    if (score % 5 == 0 && game_speed < 8) {
      game_speed++;
    }
  }

  if (
    PLAYER_X + PLAYER_SIZE > obstacle_x &&
    PLAYER_X < obstacle_x + obstacle_width &&
    player_y + PLAYER_SIZE > GROUND_Y - obstacle_height
  ) {
    game_over();
    return;
  }

  updateGameDisplay();
}

void ship_game_reset() {
  tone(pinoBuzzer, 600, 100);
  ship_score = 0;
  last_ship_score = -1;
  ship_x = SCREEN_WIDTH / 2 - SHIP_WIDTH / 2;
  last_ship_x = ship_x;
  ship_enemy_speed = 2;
  enemySpawnInterval = 1100;
  bothPressedAt = 0;

  for (int i = 0; i < MAX_ENEMIES; i++) {
    enemies[i].active = false;
    enemies[i].wasDrawn = false;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    bullets[i].active = false;
    bullets[i].wasDrawn = false;
  }

  lastEnemySpawn = millis();
  lastAutoFire = millis();
  tft.fillScreen(COR_FUNDO);
}

void ship_game_over() {
  showGameOver(ship_score, high_score_ship);
}

void updateShipDisplay() {
  if (last_ship_x != ship_x) {
    tft.fillRect(last_ship_x, SHIP_Y, SHIP_WIDTH, SHIP_HEIGHT, COR_FUNDO);
  }
  tft.fillTriangle(ship_x + SHIP_WIDTH / 2, SHIP_Y,
                   ship_x, SHIP_Y + SHIP_HEIGHT - 1,
                   ship_x + SHIP_WIDTH - 1, SHIP_Y + SHIP_HEIGHT - 1,
                   ST7735_CYAN);
  last_ship_x = ship_x;

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].wasDrawn) {
      tft.fillRect(bullets[i].lastX, bullets[i].lastY,
                   BULLET_WIDTH, BULLET_HEIGHT, COR_FUNDO);
    }
    if (bullets[i].active) {
      tft.fillRect(bullets[i].x, bullets[i].y,
                   BULLET_WIDTH, BULLET_HEIGHT, ST7735_YELLOW);
      bullets[i].lastX = bullets[i].x;
      bullets[i].lastY = bullets[i].y;
      bullets[i].wasDrawn = true;
    } else {
      bullets[i].wasDrawn = false;
    }
  }

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].wasDrawn) {
      tft.fillRect(enemies[i].lastX, enemies[i].lastY,
                   ENEMY_WIDTH, ENEMY_HEIGHT, COR_FUNDO);
    }
    if (enemies[i].active) {
      tft.fillRect(enemies[i].x, enemies[i].y,
                   ENEMY_WIDTH, ENEMY_HEIGHT, ST7735_RED);
      enemies[i].lastX = enemies[i].x;
      enemies[i].lastY = enemies[i].y;
      enemies[i].wasDrawn = true;
    } else {
      enemies[i].wasDrawn = false;
    }
  }

  if (ship_score != last_ship_score) {
    tft.fillRect(8, 8, 200, 22, COR_FUNDO);
    tft.setTextSize(2);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(8, 8);
    tft.print("Score: ");
    tft.print(ship_score);
    last_ship_score = ship_score;
  }
}

void ship_game_loop() {
  unsigned long now = millis();
  static unsigned long lastShipUpdate = 0;

  if (navigateButton.stablePressed && selectButton.stablePressed) {
    if (bothPressedAt == 0) bothPressedAt = now;
    if (now - bothPressedAt >= 250UL) {
      navigateButton.longHandled = true;
      selectButton.longHandled = true;
      bothPressedAt = 0;
      pauseGame();
      return;
    }
  } else {
    bothPressedAt = 0;
  }

  if (now - lastShipUpdate < 30UL) return;
  lastShipUpdate = now;

  if (navigateButton.stablePressed && !selectButton.stablePressed) ship_x -= SHIP_SPEED;
  else if (selectButton.stablePressed && !navigateButton.stablePressed) ship_x += SHIP_SPEED;
  ship_x = constrain(ship_x, 0, SCREEN_WIDTH - SHIP_WIDTH);

  if (now - lastAutoFire >= autoFireInterval) {
    lastAutoFire = now;
    for (int i = 0; i < MAX_BULLETS; i++) {
      if (!bullets[i].active) {
        bullets[i].active = true;
        bullets[i].x = ship_x + SHIP_WIDTH / 2 - BULLET_WIDTH / 2;
        bullets[i].y = SHIP_Y - BULLET_HEIGHT;
        tone(pinoBuzzer, 900, 30);
        break;
      }
    }
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    bullets[i].y -= 5;
    if (bullets[i].y < 18) bullets[i].active = false;
  }

  if (now - lastEnemySpawn >= (unsigned long)enemySpawnInterval) {
    lastEnemySpawn = now;
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (!enemies[i].active) {
        enemies[i].active = true;
        enemies[i].x = random(0, SCREEN_WIDTH - ENEMY_WIDTH);
        enemies[i].y = 18;
        break;
      }
    }
    if (enemySpawnInterval > 500) enemySpawnInterval -= 20;
  }

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    enemies[i].y += ship_enemy_speed;
    if (enemies[i].y > SCREEN_HEIGHT) {
      enemies[i].active = false;
      continue;
    }
    if (enemies[i].y + ENEMY_HEIGHT > SHIP_Y &&
        enemies[i].y < SHIP_Y + SHIP_HEIGHT &&
        enemies[i].x < ship_x + SHIP_WIDTH &&
        enemies[i].x + ENEMY_WIDTH > ship_x) {
      ship_game_over();
      return;
    }
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!enemies[j].active) continue;
      if (bullets[i].x < enemies[j].x + ENEMY_WIDTH &&
          bullets[i].x + BULLET_WIDTH > enemies[j].x &&
          bullets[i].y < enemies[j].y + ENEMY_HEIGHT &&
          bullets[i].y + BULLET_HEIGHT > enemies[j].y) {
        bullets[i].active = false;
        enemies[j].active = false;
        ship_score++;
        tone(pinoBuzzer, 1200, 40);
        if (ship_score % 5 == 0 && ship_enemy_speed < 4) ship_enemy_speed++;
        break;
      }
    }
  }

  updateShipDisplay();
}

// ---------- JOGO 3: CARRO ----------

void car_game_over() {
  showGameOver(car_score, high_score_car);
}

// Desenha apenas a parte visível do retângulo.
void fillRectClip(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
  if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
  if (w <= 0 || h <= 0) return;
  tft.fillRect(x, y, w, h, c);
}

// Restaura o asfalto e o tracejado que passa por baixo de um sprite apagado.
void drawRoadBackgroundRect(int x, int y, int w, int h) {
  fillRectClip(x, y, w, h, COR_ASFALTO);

  int stripeLeft = (x > DASH_X) ? x : DASH_X;
  int stripeRight = (x + w < DASH_X + 4) ? x + w : DASH_X + 4;
  if (stripeLeft >= stripeRight) return;

  int stripeY = -40 + dashOffset;
  while (stripeY + 20 <= y) stripeY += 40;
  for (; stripeY < y + h; stripeY += 40) {
    int top = (y > stripeY) ? y : stripeY;
    int bottom = (y + h < stripeY + 20) ? y + h : stripeY + 20;
    if (top < bottom) {
      fillRectClip(stripeLeft, top, stripeRight - stripeLeft, bottom - top, COR_TEXTO);
    }
  }
}

void drawCarSprite(int x, int y, uint16_t body) {
  fillRectClip(x, y, CAR_W, CAR_H, body);
  fillRectClip(x + 3, y + 6, CAR_W - 6, 6, ILI9341_BLACK);   // para-brisa
  fillRectClip(x + 3, y + 16, CAR_W - 6, 5, ILI9341_BLACK); // vidro traseiro
}

// Desenha uma moeda 8x8.
void drawCoinSprite(int x, int y) {
  fillRectClip(x, y, 8, 8, ILI9341_YELLOW);
  fillRectClip(x + 2, y + 2, 4, 4, ILI9341_BLACK);
}

// Atualiza o tracejado central na nova posição.
void moveDashes(int oldOff, int newOff) {
  (void)oldOff;
  fillRectClip(DASH_X, 0, 4, SCREEN_HEIGHT, COR_ASFALTO);
  for (int y = -40 + newOff; y < SCREEN_HEIGHT; y += 40) {
    fillRectClip(DASH_X, y, 4, 20, COR_TEXTO);
  }
}

void car_game_reset() {
  play_tone(600, 100);

  car_score = 0;
  last_car_score = -1;
  car_x = ROAD_X + ROAD_W / 2 - CAR_W / 2;
  last_car_x = car_x;
  car_speed = 3;
  obsSpawnInterval = 1000;
  coinSpawnInterval = 3000;
  dashOffset = 0;
  bothPressedAt = 0;

  for (int i = 0; i < MAX_OBS; i++) {
    obs[i].active = false;
    obs[i].wasDrawn = false;
  }

  for (int i = 0; i < MAX_COINS; i++) {
    coins[i].active = false;
    coins[i].wasDrawn = false;
  }

  unsigned long now = millis();
  carScoreTick = now;
  lastSpeedUpTime = now;
  lastObsSpawn = now;
  lastCoinSpawn = now;
  lastCarUpdate = now;

  tft.fillScreen(COR_GRAMA);
  tft.fillRect(ROAD_X, 0, ROAD_W, SCREEN_HEIGHT, COR_ASFALTO);
  tft.fillRect(ROAD_X - 4, 0, 4, SCREEN_HEIGHT, COR_TEXTO);
  tft.fillRect(ROAD_X + ROAD_W, 0, 4, SCREEN_HEIGHT, COR_TEXTO);

  for (int y = -40; y < SCREEN_HEIGHT; y += 40) {
    fillRectClip(DASH_X, y, 4, 20, COR_TEXTO);
  }
}

void updateCarDisplay(int oldDashOffset) {
  moveDashes(oldDashOffset, dashOffset);

  // Apaga as posições anteriores e restaura a pista sob cada sprite.
  drawRoadBackgroundRect(last_car_x, CAR_Y, CAR_W, CAR_H);
  for (int i = 0; i < MAX_COINS; i++) {
    if (coins[i].wasDrawn) {
      drawRoadBackgroundRect(coins[i].lastX, coins[i].lastY, 8, 8);
      coins[i].wasDrawn = false;
    }
  }
  for (int i = 0; i < MAX_OBS; i++) {
    if (obs[i].wasDrawn) {
      drawRoadBackgroundRect(obs[i].lastX, obs[i].lastY, CAR_W, CAR_H);
      obs[i].wasDrawn = false;
    }
  }

  // Desenha moedas, obstáculos e o carro do jogador nessa ordem.
  for (int i = 0; i < MAX_COINS; i++) {
    if (!coins[i].active) continue;
    drawCoinSprite(coins[i].x, coins[i].y);
    coins[i].lastX = coins[i].x;
    coins[i].lastY = coins[i].y;
    coins[i].wasDrawn = true;
  }
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;
    drawCarSprite(obs[i].x, obs[i].y, obs[i].color);
    obs[i].lastX = obs[i].x;
    obs[i].lastY = obs[i].y;
    obs[i].wasDrawn = true;
  }

  drawCarSprite(car_x, CAR_Y, ILI9341_CYAN);
  last_car_x = car_x;

  if (car_score != last_car_score) {
    tft.fillRect(0, 0, ROAD_X - 4, 90, COR_GRAMA);
    tft.setTextColor(COR_TEXTO, COR_GRAMA);
    tft.setTextSize(2);
    tft.setCursor(4, 15);
    tft.print("PONTOS");
    tft.setTextSize(3);
    tft.setCursor(4, 40);
    tft.print(car_score);
    last_car_score = car_score;
  }
}

void car_game_loop() {
  unsigned long now = millis();

  // Pausa ao manter os dois botões pressionados.
  if (navigateButton.stablePressed && selectButton.stablePressed) {
    if (bothPressedAt == 0) bothPressedAt = now;
    if (now - bothPressedAt >= 250UL) {
      navigateButton.longHandled = true;
      selectButton.longHandled = true;
      bothPressedAt = 0;
      pauseGame();
      return;
    }
  } else {
    bothPressedAt = 0;
  }

  if (now - lastCarUpdate < 30UL) return;
  lastCarUpdate = now;

  if (digitalRead(pinoBotaoNavegar) == LOW) car_x -= CAR_SPEED;
  if (digitalRead(pinoBotaoSelect) == LOW) car_x += CAR_SPEED;

  if (car_x < ROAD_X + 4) car_x = ROAD_X + 4;
  if (car_x > ROAD_X + ROAD_W - 4 - CAR_W) {
    car_x = ROAD_X + ROAD_W - 4 - CAR_W;
  }

  // Pontos por tempo, sem perder contagem se um quadro atrasar.
  if (now - carScoreTick >= 500UL) {
    unsigned long gainedPoints = (now - carScoreTick) / 500UL;
    carScoreTick += gainedPoints * 500UL;
    car_score += (int)gainedPoints;
  }

  // Acelera a cada cinco segundos até a velocidade máxima.
  if (now - lastSpeedUpTime >= 5000UL) {
    lastSpeedUpTime = now;
    if (car_speed < 12) {
      car_speed += 2;
      if (car_speed > 12) car_speed = 12;
    }
    if (obsSpawnInterval > 400) {
      obsSpawnInterval -= 150;
      if (obsSpawnInterval < 400) obsSpawnInterval = 400;
    }
  }

  // Gera obstáculos em três faixas, mantendo espaço para reagir.
  if (now - lastObsSpawn >= (unsigned long)obsSpawnInterval) {
    int minGap = CAR_H + 30 + car_speed * 6;
    bool gapOk = true;

    for (int i = 0; i < MAX_OBS; i++) {
      if (obs[i].active && obs[i].y < minGap) gapOk = false;
    }

    if (gapOk) {
      for (int i = 0; i < MAX_OBS; i++) {
        if (obs[i].active) continue;

        static int lastSection = -1;
        int section = random(0, 3);
        if (section == lastSection) {
          section = (section + 1 + random(0, 2)) % 3;
        }
        int sectionWidth = ROAD_W / 3;
        int minX = ROAD_X + 4 + section * sectionWidth;
        int maxX = minX + sectionWidth - CAR_W - 4;
        int roadMaxX = ROAD_X + ROAD_W - 4 - CAR_W;
        if (maxX > roadMaxX) maxX = roadMaxX;
        if (maxX < minX) maxX = minX;

        obs[i].active = true;
        obs[i].x = random(minX, maxX + 1);
        obs[i].y = -(CAR_H * 2);
        obs[i].color = obsColors[random(0, 4)];
        lastObsSpawn = now;
        lastSection = section;
        break;
      }
    }
  }

  // Gera moedas em posições que permanecem dentro da pista.
  if (now - lastCoinSpawn >= (unsigned long)coinSpawnInterval) {
    lastCoinSpawn = now;
    for (int i = 0; i < MAX_COINS; i++) {
      if (coins[i].active) continue;

      int coinSection = random(0, 3);
      int sectionWidth = ROAD_W / 3;
      int minX = ROAD_X + 6 + coinSection * sectionWidth;
      int maxX = minX + sectionWidth - 12;
      int roadMaxX = ROAD_X + ROAD_W - 4 - 8;
      if (maxX > roadMaxX) maxX = roadMaxX;
      if (maxX < minX) maxX = minX;

      coins[i].active = true;
      coins[i].x = random(minX, maxX + 1);
      coins[i].y = -20;
      break;
    }
  }

  // Move moedas e concede pontos ao coletá-las.
  for (int i = 0; i < MAX_COINS; i++) {
    if (!coins[i].active) continue;

    coins[i].y += car_speed;
    if (coins[i].y > SCREEN_HEIGHT) {
      coins[i].active = false;
      continue;
    }

    if (car_x < coins[i].x + 8 &&
        car_x + CAR_W > coins[i].x &&
        CAR_Y < coins[i].y + 8 &&
        CAR_Y + CAR_H > coins[i].y) {
      coins[i].active = false;
      car_score += 5;
      tone(pinoBuzzer, 1500, 40);
    }
  }

  // Move obstáculos e encerra a partida ao colidir.
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;

    obs[i].y += car_speed;
    if (obs[i].y > SCREEN_HEIGHT) {
      obs[i].active = false;
      continue;
    }

    if (car_x + 2 < obs[i].x + CAR_W - 2 &&
        car_x + CAR_W - 2 > obs[i].x + 2 &&
        CAR_Y + 2 < obs[i].y + CAR_H - 2 &&
        CAR_Y + CAR_H - 2 > obs[i].y + 2) {
      car_game_over();
      return;
    }
  }

  int oldDashOffset = dashOffset;
  dashOffset = (dashOffset + car_speed) % 40;
  updateCarDisplay(oldDashOffset);
}
// ---------- DISPATCHER ----------
void game_loop() {
  if (currentGame == GAME_JUMP) jump_game_loop();
  else if (currentGame == GAME_SHIP) ship_game_loop();
  else car_game_loop();
}

// ================= AÇÕES =================

void handleCurar() {
  if (energia < 50 || fome < 50 || felicidade < 50) {
    energia = 100;
    fome = 100;
    felicidade = 100;

    drawHappyFace();

    play_tone(1000, 100);
    play_tone(1500, 200);
    fullRedraw(false);
    showMessage("CURADO: EN/FE/SA 100");
    saveData();
  } else {
    tone(pinoBuzzer, 200, 100);
    fullRedraw(false);
    showMessage("CURA NAO NECESSARIA");
  }
}

void handleAlimentar()
{
    int oldFome = fome;
    int oldFelicidade = felicidade;
    fome = 100;

    felicidade += 5;

    if(felicidade > 100)
        felicidade = 100;

    drawEatingFace();
    fullRedraw(false);
    char result[30];
    snprintf(result, sizeof(result), "SA +%d  FE +%d", fome - oldFome,
             felicidade - oldFelicidade);
    showMessage(result);
    saveData();
}

void handleDormir() {
  estaDormindo = !estaDormindo;

  if (estaDormindo) {
    previousMillisSleep = millis();
    fullRedraw(false);
    showMessage("DORMINDO: EN +2/5S");
  } else {
    fullRedraw(false);
    showMessage("ACORDOU");
  }
  saveData();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  pinMode(pinoBotaoNavegar, INPUT_PULLUP);
  pinMode(pinoBotaoSelect, INPUT_PULLUP);
  pinMode(pinoBuzzer, OUTPUT);
  randomSeed(micros());

  // Display ILI9341 240x320
  tft.begin();

  // Paisagem: 320x240
  tft.setRotation(1);

  tft.fillScreen(COR_FUNDO);

  loadData();

  // Splash
  tft.fillScreen(COR_FUNDO);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_CYAN, COR_FUNDO);
  tft.setCursor(40, 70);
  tft.print("TAMAGOTCHI");

  tft.setCursor(110, 130);
  tft.print("ESP32");

  tft.setTextSize(2);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(40, 190);
  tft.print("EN ENERG FE FEL SA SAC");

  play_tone(500, 100);
  play_tone(800, 200);

  delay(1500);

  fullRedraw(true);

  Serial.println("Tamagotchi ESP32 ILI9341 iniciado!");
}

// ================= LOOP =================

void loop() {
  unsigned long currentMillis = millis();
  navigateButton.update(pinoBotaoNavegar, currentMillis);
  selectButton.update(pinoBotaoSelect, currentMillis);

  // Relógio interno
  unsigned long elapsedMinutes = (currentMillis - previousMillisClock) / 60000UL;
  if (clockConfigured && elapsedMinutes > 0) {
    previousMillisClock += elapsedMinutes * 60000UL;
    unsigned long minutesOfDay = (gameHour * 60UL + gameMinute + elapsedMinutes) % 1440UL;
    gameHour = minutesOfDay / 60;
    gameMinute = minutesOfDay % 60;
  }

  // Decaimento dos status
  unsigned long elapsedStats = (currentMillis - previousMillisStats) / intervalStats;
  if (elapsedStats > 0) {
    previousMillisStats += elapsedStats * intervalStats;

    if (!estaDormindo) {
      energia = max(0, energia - (int)elapsedStats);
      felicidade = max(0, felicidade - (int)elapsedStats);
      fome = max(0, fome - (int)elapsedStats);
    }
  }

  if (estaDormindo) {
    unsigned long elapsedSleep = (currentMillis - previousMillisSleep) / sleepInterval;
    if (elapsedSleep > 0) {
      previousMillisSleep += elapsedSleep * sleepInterval;
      int gain = elapsedSleep >= 50 ? 100 : (int)elapsedSleep * sleepEnergyGain;
      energia = min(100, energia + gain);
    }
  }

  // Salvar dados
  if (currentMillis - previousMillisSave >= saveInterval) {
    previousMillisSave += ((currentMillis - previousMillisSave) / saveInterval) * saveInterval;
    saveData();
  }

  switch (currentMode) {
    case MODE_TAMAGOTCHI:
      if (navigateButton.clicked()) {
        navigateButton.consume();
        currentMode = MODE_MENU;
        menuInitialized = false;
        tone(pinoBuzzer, 400, 50);
        break;
      }

      if (selectButton.clicked()) {
        selectButton.consume();
        enterClockSetup();
        break;
      }

      if (!estaDormindo) {
        if (fome < 20 || felicidade < 20 || energia < 10) {
          if (displayedFace != 2) {
            drawSickFace();
            displayedFace = 2;
          }
        } else if (fome < 40) {
          if (displayedFace != 1) {
            drawSadFace();
            displayedFace = 1;
          }
        } else {
          if (displayedFace != 0) {
            drawCharacter();
            displayedFace = 0;
          }
          updateIdleEyes(currentMillis);
        }
        updateInterface();
      } else {
        if (currentMillis - lastRedrawTime > 1000) {
          lastRedrawTime = currentMillis;
          drawSleeping();
        }
        updateInterface();
      }
      break;

    case MODE_GAME:
      game_loop();
      break;

    case MODE_GAME_SELECT:
      game_select_loop();
      break;

    case MODE_GAME_PAUSED:
      if (navigateButton.clicked()) {
        navigateButton.consume();
        currentMode = MODE_TAMAGOTCHI;
        fullRedraw(true);
      } else if (selectButton.clicked()) {
        selectButton.consume();
        resumeGame();
      }
      break;

    case MODE_GAME_OVER:
      if (navigateButton.clicked()) {
        navigateButton.consume();
        currentMode = MODE_TAMAGOTCHI;
        fullRedraw(true);
      } else if (selectButton.clicked()) {
        selectButton.consume();
        startGame();
      }
      break;

    case MODE_MENU:
      menu_loop();
      break;

    case MODE_CLOCK_SET:
      clock_setup_loop();
      break;
  }

  delay(10);
}
