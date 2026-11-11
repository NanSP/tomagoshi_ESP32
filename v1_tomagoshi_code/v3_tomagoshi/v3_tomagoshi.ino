#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>
#include <stdio.h>

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Botões e buzzer
const int pinoBotaoNavegar = 15;
const int pinoBotaoSelect  = 33;
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

    if (pressed != stablePressed && now - rawChangedAt >= 30UL) {
      stablePressed = pressed;
      if (pressed) {
        pressEvent = true;
        pressedAt = now;
        longHandled = false;
      } else if (!longHandled) {
        shortPress = true;
      }
    }

    if (stablePressed && rawPressed && !longHandled && now - pressedAt >= 700UL) {
      longPress = true;
      longHandled = true;
    }
  }
};

ButtonState navigateButton = {};
ButtonState selectButton = {};

// Cores ST7735
#define COR_FUNDO      ST7735_BLACK
#define COR_TEXTO      ST7735_WHITE
#define COR_BARRA      ST7735_GREEN
#define COR_PERSONAGEM ST7735_YELLOW

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

#define PLAYER_X 25
#define PLAYER_SIZE 10
#define GROUND_Y 115
#define GRAVITY -0.5
#define JUMP_FORCE 7

float player_y = GROUND_Y - PLAYER_SIZE;
float player_vel_y = 0;
bool player_is_jumping = false;

int obstacle_x = SCREEN_WIDTH;
int last_obstacle_x = SCREEN_WIDTH;
int obstacle_width = 8;
int obstacle_height = 16;

int game_speed = 2;
int last_game_speed = 2;
int last_score = -1;
float last_player_y = GROUND_Y - PLAYER_SIZE;
const int gameEnergyCost = 5;

// Jogo da nave
#define SHIP_WIDTH 10
#define SHIP_HEIGHT 6
#define SHIP_Y (SCREEN_HEIGHT - 10)
#define SHIP_SPEED 3
#define MAX_ENEMIES 4
#define MAX_BULLETS 4
#define ENEMY_WIDTH 8
#define ENEMY_HEIGHT 6
#define BULLET_WIDTH 2
#define BULLET_HEIGHT 4

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

struct ShipRecord {
  int high_score;
  byte magicByte;
};

// ================= EEPROM =================

void saveData() {
  TamagotchiState state = { energia, felicidade, fome, high_score, 0xAD };
  EEPROM.put(0, state);
  ShipRecord shipRecord = { high_score_ship, 0xB1 };
  EEPROM.put(sizeof(TamagotchiState), shipRecord);
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
  if (!stateValid || !shipRecordValid || !hasChecksum) saveData();

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

  tft.drawRect(x + 12, y, 35, 7, COR_TEXTO);

  int largura = map(valor, 0, 100, 0, 33);
  tft.fillRect(x + 13, y + 1, largura, 5, COR_BARRA);
}

void drawClock() {
  tft.fillRect(0, 116, SCREEN_WIDTH, 12, COR_FUNDO);
  tft.setCursor(5, 118);
  tft.setTextSize(1);
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
  tft.setCursor(61, 118);
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
  tft.fillRect(0, 11, SCREEN_WIDTH, 10, COR_FUNDO);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
  tft.setCursor(5, 12);
  if (need == 1) tft.print("PRECISA COMER");
  else if (need == 2) tft.print("PRECISA DORMIR");
  else if (need == 3) tft.print("PRECISA BRINCAR");
  else if (need == 4) tft.print("POUCA SACIEDADE");
  lastNeed = need;
}

void showMessage(const char* message) {
  tft.fillRect(0, 116, SCREEN_WIDTH, 12, COR_FUNDO);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN, COR_FUNDO);
  tft.setCursor(4, 118);
  tft.print(message);
  messageActive = true;
  messageStarted = millis();
}

void fullRedraw(bool showClock);

void drawClockSetup() {
  tft.fillScreen(COR_FUNDO);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN, COR_FUNDO);
  tft.setCursor(8, 10);
  tft.print("AJUSTAR HORA");

  tft.setTextSize(3);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(35, 50);
  if (clockEditHour < 10) tft.print("0");
  tft.print(clockEditHour);
  tft.print(":");
  if (clockEditMinute < 10) tft.print("0");
  tft.print(clockEditMinute);
  tft.drawRect(clockEditField == 0 ? 32 : 86, 46, 40, 29, ST7735_YELLOW);

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(5, 94);
  tft.print("NAV +1  SEG +5");
  tft.setCursor(5, 106);
  tft.print("SEL PROX / CONFIRMA");
  tft.setCursor(5, 118);
  tft.print("SEG SEL: CANCELA");
}

void enterClockSetup() {
  clockEditHour = gameHour;
  clockEditMinute = gameMinute;
  clockEditField = 0;
  currentMode = MODE_CLOCK_SET;
  selectButton.longHandled = true; // ignora a soltura que abriu esta tela
  drawClockSetup();
}

void clock_setup_loop() {
  if (selectButton.longPress) {
    currentMode = MODE_TAMAGOTCHI;
    fullRedraw(true);
    return;
  }

  if (navigateButton.shortPress || navigateButton.longPress) {
    int step = navigateButton.longPress ? 5 : 1;
    if (clockEditField == 0) clockEditHour = (clockEditHour + step) % 24;
    else clockEditMinute = (clockEditMinute + step) % 60;
    drawClockSetup();
  }

  if (selectButton.shortPress) {
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
    tft.fillRect(14, 2, 33, 5, COR_FUNDO);
    int largura = map(energia, 0, 100, 0, 33);
    tft.fillRect(14, 2, largura, 5, COR_BARRA);
    lastEnergia = energia;
  }

  if (felicidade != lastFelicidade) {
    tft.fillRect(67, 2, 33, 5, COR_FUNDO);
    int largura = map(felicidade, 0, 100, 0, 33);
    tft.fillRect(67, 2, largura, 5, COR_BARRA);
    lastFelicidade = felicidade;
  }

  if (fome != lastFome) {
    tft.fillRect(120, 2, 33, 5, COR_FUNDO);
    int largura = map(fome, 0, 100, 0, 33);
    tft.fillRect(120, 2, largura, 5, COR_BARRA);
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

int eyeW = 40;
int eyeH = 35;
int eyeY = 60;

int leftEyeX = 25;
int rightEyeX = 95;

void drawEyesPose(int pose) {
  tft.fillRect(0, 20, 160, 96, COR_FUNDO);

  if (pose == EYES_BLINK) {
    tft.drawLine(leftEyeX, eyeY + 18, leftEyeX + eyeW, eyeY + 18, COR_TEXTO);
    tft.drawLine(rightEyeX, eyeY + 18, rightEyeX + eyeW, eyeY + 18, COR_TEXTO);
    return;
  }

  int pupilOffset = pose == EYES_LEFT ? -6 : (pose == EYES_RIGHT ? 6 : 0);
  if (pose == EYES_WINK) {
    tft.drawLine(leftEyeX, eyeY + 18, leftEyeX + eyeW, eyeY + 18, COR_TEXTO);
  } else {
    tft.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);
    tft.fillRoundRect(leftEyeX + 15 + pupilOffset, eyeY + 8,
                      10, 18, 5, COR_FUNDO);
  }
  tft.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);
  tft.fillRoundRect(rightEyeX + 15 + pupilOffset, eyeY + 8,
                    10, 18, 5, COR_FUNDO);
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

  drawStatusBar(1, 1, "EN", energia);
  drawStatusBar(54, 1, "FE", felicidade);
  drawStatusBar(107, 1, "SA", fome);
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
  tft.drawLine(30, 75, 45, 60, COR_TEXTO);
  tft.drawLine(45, 60, 60, 75, COR_TEXTO);

  tft.drawLine(100, 75, 115, 60, COR_TEXTO);
  tft.drawLine(115, 60, 130, 75, COR_TEXTO);

  // sorriso
  tft.drawLine(60, 100, 75, 110, COR_TEXTO);
  tft.drawLine(75, 110, 85, 110, COR_TEXTO);
  tft.drawLine(85, 110, 100, 100, COR_TEXTO);

  tone(pinoBuzzer, 1000, 100);
  delay(1000);
}

// ==================================================
// TRISTE
// ==================================================

void drawSadFace() {

  tone(pinoBuzzer, 200, 300);

  tft.fillRect(0, 20, 160, 96, COR_FUNDO);

  // olhos inclinados
  tft.drawLine(30, 60, 60, 75, COR_TEXTO);
  tft.drawLine(100, 75, 130, 60, COR_TEXTO);

  // boca triste
  tft.drawLine(60, 110, 75, 100, COR_TEXTO);
  tft.drawLine(75, 100, 85, 100, COR_TEXTO);
  tft.drawLine(85, 100, 100, 110, COR_TEXTO);

}

// ==================================================
// DOENTE
// ==================================================

void drawSickFace() {

  tone(pinoBuzzer, 100, 500);

  tft.fillRect(0, 20, 160, 96, COR_FUNDO);

  tft.setTextColor(ST7735_RED);
  tft.setTextSize(3);

  tft.setCursor(35, 60);
  tft.print("X");

  tft.setCursor(105, 60);
  tft.print("X");

  // boca torta
  tft.drawLine(65, 100, 75, 95, ST7735_RED);
  tft.drawLine(75, 95, 85, 105, ST7735_RED);
  tft.drawLine(85, 105, 95, 100, ST7735_RED);

  tft.setTextColor(COR_TEXTO);
}

// ==================================================
// COMER
// ==================================================

void drawEatingFace() {
  tft.fillScreen(COR_FUNDO);
  drawEyes();

  // boca aberta
  tft.fillCircle(80, 105, 12, COR_TEXTO);
  tft.fillCircle(80, 105, 6, COR_FUNDO);

  tone(pinoBuzzer, 1200, 80);
  delay(800);
}

// ==================================================
// DORMINDO
// ==================================================

void drawSleeping() {

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

  tft.drawLine(leftEyeX, eyeY + 18,
               leftEyeX + eyeW,
               eyeY + 18,
               COR_TEXTO);

  tft.drawLine(rightEyeX, eyeY + 18,
               rightEyeX + eyeW,
               eyeY + 18,
               COR_TEXTO);

  tft.setTextColor(COR_TEXTO);
  tft.setTextSize(2);

  tft.setCursor(60, 30);
  tft.print("Zzz");
}

// ================= MENU =================

void drawMenuItem(int index, bool selected) {
  int x = 8 + (index * 38);
  int y = 42;

  tft.fillRect(x - 2, y - 2, 34, 48, COR_FUNDO);

  if (selected) {
    tft.drawRect(x - 2, y - 2, 34, 34, ST7735_YELLOW);
  } else {
    tft.drawRect(x, y, 30, 30, COR_TEXTO);
  }

  if (index == 0) {
    tft.fillCircle(x + 15, y + 15, 9, ST7735_GREEN);
    tft.fillRect(x + 12, y + 12, 6, 6, COR_FUNDO);
  } else if (index == 1) {
    tft.fillCircle(x + 15, y + 15, 9, ST7735_YELLOW);
    tft.fillRect(x + 11, y + 13, 8, 4, COR_FUNDO);
  } else if (index == 2) {
    tft.fillCircle(x + 15, y + 15, 9, ST7735_BLUE);
    tft.setCursor(x + 10, y + 8);
    tft.setTextSize(2);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.print("Z");
  } else if (index == 3) {
    tft.fillCircle(x + 15, y + 15, 9, ST7735_RED);
    tft.fillRect(x + 11, y + 14, 8, 3, COR_FUNDO);
    tft.fillRect(x + 14, y + 11, 3, 8, COR_FUNDO);
  }

  const char* opcoes[] = {"COM", "JOG", estaDormindo ? "ACO" : "DOR", "CUR"};

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(x + 5, y + 36);
  tft.print(opcoes[index]);
}

void drawMenu() {
  if (!menuInitialized) {
    tft.fillScreen(COR_FUNDO);

    tft.setTextSize(2);
    tft.setTextColor(ST7735_CYAN, COR_FUNDO);
    tft.setCursor(55, 12);
    tft.print("MENU");

    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);

    for (int i = 0; i < totalOpcoes; i++) {
      drawMenuItem(i, false);
    }

    tft.setCursor(5, 109);
    tft.print("NAV PROX.  SEL OK");
    tft.setCursor(5, 119);
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
    tft.fillRect(0, 94, SCREEN_WIDTH, 12, COR_FUNDO);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
    tft.setCursor(40, 96);
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
    menuInitialized = false;
    currentMode = MODE_TAMAGOTCHI;
    fullRedraw(true);
    tone(pinoBuzzer, 330, 80);
    return;
  }

  if (navigateButton.shortPress) {
    menuOpcao = (menuOpcao + 1) % totalOpcoes;
    tone(pinoBuzzer, 440, 50);
  }

  if (selectButton.pressEvent) {
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
  int x = 16 + index * 78;
  int y = 31;
  tft.fillRect(x - 3, y - 3, 56, 68, COR_FUNDO);
  tft.drawRect(x - 2, y - 2, 54, 54, selected ? ST7735_YELLOW : COR_TEXTO);

  if (index == GAME_JUMP) {
    tft.fillRect(x + 19, y + 30, 12, 12, ST7735_GREEN);
    tft.drawLine(x + 25, y + 9, x + 25, y + 27, COR_TEXTO);
    tft.drawLine(x + 21, y + 14, x + 25, y + 9, COR_TEXTO);
    tft.drawLine(x + 29, y + 14, x + 25, y + 9, COR_TEXTO);
  } else {
    tft.fillTriangle(x + 25, y + 10, x + 12, y + 39,
                     x + 38, y + 39, ST7735_CYAN);
  }

  tft.setTextSize(1);
  tft.setTextColor(selected ? ST7735_YELLOW : COR_TEXTO, COR_FUNDO);
  tft.setCursor(x + 13, y + 56);
  tft.print(index == GAME_JUMP ? "PULO" : "NAVE");
}

void drawGameSelectMenu() {
  if (!gameMenuInitialized) {
    tft.fillScreen(COR_FUNDO);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_CYAN, COR_FUNDO);
    tft.setCursor(38, 8);
    tft.print("ESCOLHA O JOGO");
    drawGameMenuItem(GAME_JUMP, false);
    drawGameMenuItem(GAME_SHIP, false);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(5, 105);
    tft.print("NAV PROX.  SEL OK");
    tft.setCursor(5, 117);
    tft.print("SEGURE NAV: VOLTAR");
    gameMenuInitialized = true;
    lastGameMenuOpcao = -1;
  }

  if (lastGameMenuOpcao != gameMenuOpcao) {
    if (lastGameMenuOpcao >= 0) drawGameMenuItem(lastGameMenuOpcao, false);
    drawGameMenuItem(gameMenuOpcao, true);
    tft.fillRect(0, 96, SCREEN_WIDTH, 9, COR_FUNDO);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
    tft.setCursor(2, 97);
    tft.print(gameMenuOpcao == GAME_SHIP ? "NAV< SEL> AMBOS PAUSA" : "NAV PULA SEL PAUSA");
    lastGameMenuOpcao = gameMenuOpcao;
  }
}

void game_select_loop() {
  drawGameSelectMenu();
  if (navigateButton.longPress) {
    gameMenuInitialized = false;
    menuInitialized = false;
    currentMode = MODE_MENU;
    tone(pinoBuzzer, 330, 80);
    return;
  }
  if (navigateButton.shortPress) {
    gameMenuOpcao = (gameMenuOpcao + 1) % 2;
    tone(pinoBuzzer, 440, 50);
  }
  if (selectButton.pressEvent) {
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
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(4, 118);
  tft.print("NAV PULA  SEL PAUSA");
}

void ship_game_reset();

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

  tft.setTextSize(2);
  tft.setTextColor(ST7735_RED, COR_FUNDO);
  tft.setCursor(25, 17);
  tft.print("GAME OVER");

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(55, 37);
  tft.print(currentGame == GAME_SHIP ? "NAVE" : "PULO");
  tft.setCursor(42, 48);
  tft.print("Score: ");
  tft.print(finalScore);
  tft.setCursor(42, 61);
  tft.print("Recorde: ");
  tft.print(highScore);
  tft.setCursor(26, 75);
  tft.print("FEL +");
  tft.print(gainedFelicidade);
  tft.print("   EN -");
  tft.print(gameEnergyCost);

  if (newRecord) {
    tft.setCursor(40, 88);
    tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
    tft.print("NOVO RECORDE!");
  }
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(28, 105);
  tft.print("NAV: VOLTAR");
  tft.setCursor(28, 117);
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
    tft.fillRect(5, 5, 80, 12, COR_FUNDO);
    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(5, 5);
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
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN, COR_FUNDO);
  tft.setCursor(37, 28);
  tft.print("PAUSADO");
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(53, 60);
  tft.print(currentGame == GAME_SHIP ? "NAVE" : "PULO");
  tft.setCursor(28, 80);
  tft.print("SEL: CONTINUAR");
  tft.setCursor(28, 98);
  tft.print("NAV: SAIR");
}

void updateShipDisplay();

void resumeGame() {
  currentMode = MODE_GAME;
  tft.fillScreen(COR_FUNDO);
  if (currentGame == GAME_SHIP) {
    last_ship_score = -1;
    updateShipDisplay();
    return;
  }
  last_player_y = player_y;
  last_obstacle_x = obstacle_x;
  last_score = -1;
  updateGameDisplay();
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(4, 118);
  tft.print("NAV PULA  SEL PAUSA");
}

void game_loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastGameUpdate = 0;

  if (selectButton.pressEvent) {
    pauseGame();
    return;
  }

  if (navigateButton.pressEvent && !player_is_jumping) {
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
    tft.fillRect(4, 4, 100, 11, COR_FUNDO);
    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(4, 4);
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

  // Display ST7735 128x160
  tft.initR(INITR_BLACKTAB);

  // Paisagem: 160x128
  tft.setRotation(1);

  tft.fillScreen(COR_FUNDO);

  loadData();

  // Splash
  tft.fillScreen(COR_FUNDO);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN, COR_FUNDO);
  tft.setCursor(18, 35);
  tft.print("TAMAGOTCHI");

  tft.setCursor(50, 65);
  tft.print("ESP32");

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(17, 95);
  tft.print("EN ENERG FE FEL SA SAC");

  play_tone(500, 100);
  play_tone(800, 200);

  delay(1500);

  fullRedraw(true);

  Serial.println("Tamagotchi ESP32 ST7735 iniciado!");
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
      if (navigateButton.shortPress) {
        currentMode = MODE_MENU;
        menuInitialized = false;
        tone(pinoBuzzer, 400, 50);
        break;
      }

      if (selectButton.pressEvent) {
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
      if (currentGame == GAME_SHIP) ship_game_loop();
      else game_loop();
      break;

    case MODE_GAME_SELECT:
      game_select_loop();
      break;

    case MODE_GAME_PAUSED:
      if (navigateButton.shortPress) {
        currentMode = MODE_TAMAGOTCHI;
        fullRedraw(true);
      } else if (selectButton.pressEvent) {
        resumeGame();
      }
      break;

    case MODE_GAME_OVER:
      if (navigateButton.shortPress) {
        currentMode = MODE_TAMAGOTCHI;
        fullRedraw(true);
      } else if (selectButton.pressEvent) {
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
