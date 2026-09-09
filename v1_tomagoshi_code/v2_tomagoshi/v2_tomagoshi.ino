#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>

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

#define MODE_TAMAGOTCHI 0
#define MODE_GAME       1
#define MODE_MENU       2
#define MODE_GAME_SELECT 3

int currentMode = MODE_TAMAGOTCHI;

// Menu
int menuOpcao = 0;
int lastMenuOpcao = -1;
const int totalOpcoes = 4;
bool menuInitialized = false;

#define GAME_JUMP 0
#define GAME_SHIP 1

int currentGame = GAME_JUMP;
int gameMenuOpcao = 0;
int lastGameMenuOpcao = -1;
const int totalGames = 2;
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

unsigned long previousMillisStats = 0;
const long intervalStats = 30000;

unsigned long previousMillisSave = 0;
const long saveInterval = 300000;

unsigned long previousMillisClock = 0;
unsigned long lastInputTime = 0;
unsigned long lastRedrawTime = 0;

const long inputCooldown = 250;

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

// ================= JOGO 2: NAVE =================
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

struct Enemy
{
    int x, y;
    int lastX, lastY;
    bool active;
    bool wasDrawn;
};

struct Bullet
{
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
long enemySpawnInterval = 1100;
unsigned long lastAutoFire = 0;
const long autoFireInterval = 450;
int ship_enemy_speed = 2;

// Recorde separado para preservar o formato original dos dados do Tamagotchi.
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
  EEPROM.commit();
}

void loadData() {
  EEPROM.begin(512);

  TamagotchiState state;
  EEPROM.get(0, state);

  if (state.magicByte == 0xAD) {
    energia = state.energia;
    felicidade = state.felicidade;
    fome = state.fome;
    high_score = state.high_score;
  } else {
    energia = 100;
    felicidade = 100;
    fome = 100;
    high_score = 0;
    saveData();
  }

  ShipRecord shipRecord;
  EEPROM.get(sizeof(TamagotchiState), shipRecord);
  high_score_ship = (shipRecord.magicByte == 0xB1 && shipRecord.high_score >= 0)
                      ? shipRecord.high_score : 0;

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

void updateInterface() {
  if (energia != lastEnergia) {
    tft.fillRect(13, 1, 35, 7, COR_FUNDO);
    int largura = map(energia, 0, 100, 0, 33);
    tft.fillRect(14, 2, largura, 5, COR_BARRA);
    lastEnergia = energia;
  }

  if (felicidade != lastFelicidade) {
    tft.fillRect(66, 1, 35, 7, COR_FUNDO);
    int largura = map(felicidade, 0, 100, 0, 33);
    tft.fillRect(67, 2, largura, 5, COR_BARRA);
    lastFelicidade = felicidade;
  }

  if (fome != lastFome) {
    tft.fillRect(119, 1, 35, 7, COR_FUNDO);
    int largura = map(fome, 0, 100, 0, 33);
    tft.fillRect(120, 2, largura, 5, COR_BARRA);
    lastFome = fome;
  }

  if (gameHour != lastHour || gameMinute != lastMinute) {
    tft.fillRect(5, 118, 45, 10, COR_FUNDO);
    tft.setCursor(5, 118);
    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.print(gameHour);
    tft.print(":");
    if (gameMinute < 10) tft.print("0");
    tft.print(gameMinute);

    lastHour = gameHour;
    lastMinute = gameMinute;
  }
}

// ================= PERSONAGEM =================

unsigned long lastBlinkTime = 0;

int eyeW = 40;
int eyeH = 35;
int eyeY = 60;

int leftEyeX = 25;
int rightEyeX = 95;

void drawEyes() {

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

  tft.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);
  tft.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);

  tft.fillRoundRect(leftEyeX + 15, eyeY + 8, 10, 18, 5, COR_FUNDO);
  tft.fillRoundRect(rightEyeX + 15, eyeY + 8, 10, 18, 5, COR_FUNDO);
}

void drawCharacter() {
  drawEyes();
}

void fullRedraw() {
  tft.fillScreen(COR_FUNDO);

  drawStatusBar(1, 1, "E:", energia);
  drawStatusBar(54, 1, "H:", felicidade);
  drawStatusBar(107, 1, "F:", fome);

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(5, 118);
  tft.print(gameHour);
  tft.print(":");
  if (gameMinute < 10) tft.print("0");
  tft.print(gameMinute);

  drawCharacter();

  lastEnergia = energia;
  lastFelicidade = felicidade;
  lastFome = fome;
  lastHour = gameHour;
  lastMinute = gameMinute;
}

void blink() {

  tft.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 8, COR_FUNDO);
  tft.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 8, COR_FUNDO);

  delay(80);

  drawEyes();
}

// ==================================================
// FELIZ
// ==================================================

void drawHappyFace() {

  play_tone(1000, 100);

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

  // ^ ^
  tft.drawLine(30, 75, 45, 60, COR_TEXTO);
  tft.drawLine(45, 60, 60, 75, COR_TEXTO);

  tft.drawLine(100, 75, 115, 60, COR_TEXTO);
  tft.drawLine(115, 60, 130, 75, COR_TEXTO);

  // sorriso
  tft.drawLine(60, 100, 75, 110, COR_TEXTO);
  tft.drawLine(75, 110, 85, 110, COR_TEXTO);
  tft.drawLine(85, 110, 100, 100, COR_TEXTO);

  delay(1000);

  fullRedraw();
}

// ==================================================
// TRISTE
// ==================================================

void drawSadFace() {

  play_tone(200, 300);

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

  // olhos inclinados
  tft.drawLine(30, 60, 60, 75, COR_TEXTO);
  tft.drawLine(100, 75, 130, 60, COR_TEXTO);

  // boca triste
  tft.drawLine(60, 110, 75, 100, COR_TEXTO);
  tft.drawLine(75, 100, 85, 100, COR_TEXTO);
  tft.drawLine(85, 100, 100, 110, COR_TEXTO);

  delay(1200);

  fullRedraw();
}

// ==================================================
// DOENTE
// ==================================================

void drawSickFace() {

  play_tone(100, 500);

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

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

  delay(1500);

  tft.setTextColor(COR_TEXTO);

  fullRedraw();
}

// ==================================================
// COMER
// ==================================================

void drawEatingFace() {

  play_tone(1200, 80);

  tft.fillRect(0, 20, 160, 90, COR_FUNDO);

  // olhos normais
  tft.fillRoundRect(leftEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);
  tft.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 8, COR_TEXTO);

  // boca aberta
  tft.fillCircle(80, 105, 12, COR_TEXTO);
  tft.fillCircle(80, 105, 6, COR_FUNDO);

  delay(800);

  fullRedraw();
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

  const char* opcoes[] = {"COM", "JOG", "DOR", "CUR"};

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

    menuInitialized = true;
    lastMenuOpcao = -1;
  }

  if (lastMenuOpcao != menuOpcao) {
    if (lastMenuOpcao >= 0) {
      drawMenuItem(lastMenuOpcao, false);
    }

    drawMenuItem(menuOpcao, true);
    lastMenuOpcao = menuOpcao;
  }
}

void handleAlimentar();
void handleDormir();
void handleCurar();
void game_reset();
void ship_game_reset();

void menu_loop() {
  drawMenu();

  unsigned long currentMillis = millis();

  if (digitalRead(pinoBotaoNavegar) == LOW && currentMillis - lastInputTime > inputCooldown) {
    lastInputTime = currentMillis;
    menuOpcao = (menuOpcao + 1) % totalOpcoes;
    play_tone(440, 50);
  }

  if (digitalRead(pinoBotaoSelect) == LOW && currentMillis - lastInputTime > inputCooldown) {
    lastInputTime = currentMillis;
    play_tone(880, 100);

    menuInitialized = false;

    switch (menuOpcao) {
      case 0:
        handleAlimentar();
        currentMode = MODE_TAMAGOTCHI;
        break;

      case 1:
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

void drawGameMenuItem(int index, bool selected)
{
    int x = 20 + (index * 80);
    int y = 30;
    int w = 50, h = 50;

    tft.fillRect(x - 3, y - 3, w + 6, h + 20, COR_FUNDO);

    if (selected)
    {
        tft.drawRect(x - 2, y - 2, w + 4, h + 4, ST7735_YELLOW);
    }
    else
    {
        tft.drawRect(x, y, w, h, COR_TEXTO);
    }

    if (index == 0)
    {
        // icone: quadrado pulando
        tft.fillRect(x + 19, y + 30, 12, 12, ST7735_GREEN);
        tft.drawLine(x + 25, y + 10, x + 25, y + 27, COR_TEXTO);
        tft.drawLine(x + 21, y + 14, x + 25, y + 10, COR_TEXTO);
        tft.drawLine(x + 29, y + 14, x + 25, y + 10, COR_TEXTO);
    }
    else
    {
        // icone: nave
        tft.fillTriangle(x + 25, y + 10, x + 12, y + 37, x + 37, y + 37, ST7735_CYAN);
        tft.fillRect(x + 22, y + 37, 6, 5, ST7735_CYAN);
    }

    const char *nomes[] = {"PULO", "NAVE"};
    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(x + 13, y + h + 4);
    tft.print(nomes[index]);
}

void drawGameSelectMenu()
{
    if (!gameMenuInitialized)
    {
        tft.fillScreen(COR_FUNDO);

        tft.setTextSize(1);
        tft.setTextColor(ST7735_CYAN, COR_FUNDO);
        tft.setCursor(41, 7);
        tft.print("ESCOLHA O JOGO");

        for (int i = 0; i < totalGames; i++)
        {
            drawGameMenuItem(i, false);
        }

        gameMenuInitialized = true;
        lastGameMenuOpcao = -1;
    }

    if (lastGameMenuOpcao != gameMenuOpcao)
    {
        if (lastGameMenuOpcao >= 0)
        {
            drawGameMenuItem(lastGameMenuOpcao, false);
        }

        drawGameMenuItem(gameMenuOpcao, true);
        lastGameMenuOpcao = gameMenuOpcao;
    }
}

void game_select_loop()
{
    drawGameSelectMenu();

    unsigned long currentMillis = millis();

    if (digitalRead(pinoBotaoNavegar) == LOW && currentMillis - lastInputTime > inputCooldown)
    {
        lastInputTime = currentMillis;
        gameMenuOpcao = (gameMenuOpcao + 1) % totalGames;
        play_tone(440, 50);
    }

    if (digitalRead(pinoBotaoSelect) == LOW && currentMillis - lastInputTime > inputCooldown)
    {
        lastInputTime = currentMillis;
        play_tone(880, 100);

        gameMenuInitialized = false;

        if (gameMenuOpcao == 0)
        {
            currentGame = GAME_JUMP;
            game_reset();
        }
        else
        {
            currentGame = GAME_SHIP;
            ship_game_reset();
        }

        currentMode = MODE_GAME;
    }
}

// ================= JOGO =================

void game_reset() {
  play_tone(600, 100);

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
}

void game_over() {
  play_tone(200, 500);

  tft.fillScreen(COR_FUNDO);

  tft.setTextSize(2);
  tft.setTextColor(ST7735_RED, COR_FUNDO);
  tft.setCursor(25, 35);
  tft.print("GAME OVER");

  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setCursor(50, 65);
  tft.print("Score: ");
  tft.print(score);

  if (score > high_score) {
    high_score = score;
    saveData();

    tft.setCursor(45, 85);
    tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
    tft.print("NOVO RECORDE!");
  }

  delay(2500);

  currentMode = MODE_TAMAGOTCHI;
  fullRedraw();
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

void game_loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastGameUpdate = 0;

  if (currentMillis - lastGameUpdate < 25) {
    return;
  }

  lastGameUpdate = currentMillis;

  if (digitalRead(pinoBotaoNavegar) == LOW && !player_is_jumping) {
    player_vel_y = JUMP_FORCE;
    player_is_jumping = true;
    tone(pinoBuzzer, 800, 50);
  }

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

// ================= AÇÕES =================

// ================= JOGO 2: NAVE =================

void ship_game_reset()
{
    play_tone(600, 100);

    ship_score = 0;
    last_ship_score = -1;

    ship_x = SCREEN_WIDTH / 2 - SHIP_WIDTH / 2;
    last_ship_x = ship_x;

    ship_enemy_speed = 2;
    enemySpawnInterval = 1100;

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].active = false;
        enemies[i].wasDrawn = false;
    }

    for (int i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].active = false;
        bullets[i].wasDrawn = false;
    }

    lastEnemySpawn = millis();
    lastAutoFire = millis();

    tft.fillScreen(COR_FUNDO);
}

void ship_game_over()
{
    play_tone(200, 500);

    tft.fillScreen(COR_FUNDO);

    tft.setTextSize(1);
    tft.setTextColor(ST7735_RED, COR_FUNDO);
    tft.setTextSize(2);
    tft.setCursor(26, 32);
    tft.print("GAME OVER");

    tft.setTextSize(1);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.setCursor(52, 57);
    tft.print("Score: ");
    tft.print(ship_score);

    if (ship_score > high_score_ship)
    {
        high_score_ship = ship_score;
        saveData();

        tft.setCursor(41, 77);
        tft.setTextColor(ST7735_YELLOW, COR_FUNDO);
        tft.print("NOVO RECORDE!");
    }

    delay(2500);

    currentMode = MODE_TAMAGOTCHI;
    fullRedraw();
}

void updateShipDisplay()
{
    // nave
    if (last_ship_x != ship_x)
    {
        tft.fillRect(last_ship_x, SHIP_Y, SHIP_WIDTH, SHIP_HEIGHT, COR_FUNDO);
    }

    tft.fillTriangle(ship_x + SHIP_WIDTH / 2, SHIP_Y,
                     ship_x, SHIP_Y + SHIP_HEIGHT - 1,
                     ship_x + SHIP_WIDTH - 1, SHIP_Y + SHIP_HEIGHT - 1,
                     ST7735_CYAN);

    last_ship_x = ship_x;

    // balas
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (bullets[i].wasDrawn)
        {
            tft.fillRect(bullets[i].lastX, bullets[i].lastY, BULLET_WIDTH, BULLET_HEIGHT, COR_FUNDO);
        }

        if (bullets[i].active)
        {
            tft.fillRect(bullets[i].x, bullets[i].y, BULLET_WIDTH, BULLET_HEIGHT, ST7735_YELLOW);
            bullets[i].lastX = bullets[i].x;
            bullets[i].lastY = bullets[i].y;
            bullets[i].wasDrawn = true;
        }
        else
        {
            bullets[i].wasDrawn = false;
        }
    }

    // inimigos
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].wasDrawn)
        {
            tft.fillRect(enemies[i].lastX, enemies[i].lastY, ENEMY_WIDTH, ENEMY_HEIGHT, COR_FUNDO);
        }

        if (enemies[i].active)
        {
            tft.fillRect(enemies[i].x, enemies[i].y, ENEMY_WIDTH, ENEMY_HEIGHT, ST7735_RED);
            enemies[i].lastX = enemies[i].x;
            enemies[i].lastY = enemies[i].y;
            enemies[i].wasDrawn = true;
        }
        else
        {
            enemies[i].wasDrawn = false;
        }
    }

    // score
    if (ship_score != last_ship_score)
    {
        tft.fillRect(4, 4, 75, 11, COR_FUNDO);
        tft.setTextSize(1);
        tft.setTextColor(COR_TEXTO, COR_FUNDO);
        tft.setCursor(4, 4);
        tft.print("Score: ");
        tft.print(ship_score);
        last_ship_score = ship_score;
    }
}

void ship_game_loop()
{
    unsigned long currentMillis = millis();
    static unsigned long lastShipUpdate = 0;

    if (currentMillis - lastShipUpdate < 30)
    {
        return;
    }

    lastShipUpdate = currentMillis;

    // movimento da nave (esquerda/direita)
    if (digitalRead(pinoBotaoNavegar) == LOW)
    {
        ship_x -= SHIP_SPEED;
    }
    if (digitalRead(pinoBotaoSelect) == LOW)
    {
        ship_x += SHIP_SPEED;
    }

    if (ship_x < 0)
        ship_x = 0;
    if (ship_x > SCREEN_WIDTH - SHIP_WIDTH)
        ship_x = SCREEN_WIDTH - SHIP_WIDTH;

    // tiro automático
    if (currentMillis - lastAutoFire >= autoFireInterval)
    {
        lastAutoFire = currentMillis;

        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (!bullets[i].active)
            {
                bullets[i].active = true;
                bullets[i].x = ship_x + SHIP_WIDTH / 2 - BULLET_WIDTH / 2;
                bullets[i].y = SHIP_Y - BULLET_HEIGHT;
                tone(pinoBuzzer, 900, 30);
                break;
            }
        }
    }

    // move balas
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (bullets[i].active)
        {
            bullets[i].y -= 5;

            if (bullets[i].y < 18)
            {
                bullets[i].active = false;
            }
        }
    }

    // spawn de inimigos
    if (currentMillis - lastEnemySpawn >= enemySpawnInterval)
    {
        lastEnemySpawn = currentMillis;

        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (!enemies[i].active)
            {
                enemies[i].active = true;
                enemies[i].x = random(0, SCREEN_WIDTH - ENEMY_WIDTH);
                enemies[i].y = 18;
                break;
            }
        }

        if (enemySpawnInterval > 500)
        {
            enemySpawnInterval -= 20;
        }
    }

    // move inimigos e checa colisão com a nave
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active)
            continue;

        enemies[i].y += ship_enemy_speed;

        if (enemies[i].y > SCREEN_HEIGHT)
        {
            enemies[i].active = false;
            continue;
        }

        if (enemies[i].y + ENEMY_HEIGHT > SHIP_Y &&
            enemies[i].x < ship_x + SHIP_WIDTH &&
            enemies[i].x + ENEMY_WIDTH > ship_x)
        {
            ship_game_over();
            return;
        }
    }

    // colisão bala x inimigo
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
            continue;

        for (int j = 0; j < MAX_ENEMIES; j++)
        {
            if (!enemies[j].active)
                continue;

            if (bullets[i].x < enemies[j].x + ENEMY_WIDTH &&
                bullets[i].x + BULLET_WIDTH > enemies[j].x &&
                bullets[i].y < enemies[j].y + ENEMY_HEIGHT &&
                bullets[i].y + BULLET_HEIGHT > enemies[j].y)
            {
                bullets[i].active = false;
                enemies[j].active = false;
                ship_score++;

                tone(pinoBuzzer, 1200, 40);

                if (ship_score % 5 == 0 && ship_enemy_speed < 4)
                {
                    ship_enemy_speed++;
                }
            }
        }
    }

    updateShipDisplay();
}

void handleCurar() {
  if (energia < 50 || fome < 50 || felicidade < 50) {
    energia = 100;
    fome = 100;
    felicidade = 100;

    drawHappyFace();

    play_tone(1000, 100);
    play_tone(1500, 200);
  } else {
    play_tone(200, 100);
    fullRedraw();
  }
}

void handleAlimentar()
{
    fome = 100;

    felicidade += 5;

    if(felicidade > 100)
        felicidade = 100;

    drawEatingFace();
}

void handleDormir() {
  estaDormindo = !estaDormindo;

  if (estaDormindo) {
    energia += 20;
    if (energia > 100) energia = 100;
  }

  fullRedraw();
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
  tft.setCursor(32, 95);
  tft.print("ST7735 128x160");

  play_tone(500, 100);
  play_tone(800, 200);

  delay(1500);

  fullRedraw();

  Serial.println("Tamagotchi ESP32 ST7735 iniciado!");
}

// ================= LOOP =================

void loop() {
  unsigned long currentMillis = millis();

  // Relógio interno
  if (currentMillis - previousMillisClock >= 60000) {
    previousMillisClock = currentMillis;

    gameMinute++;

    if (gameMinute >= 60) {
      gameMinute = 0;
      gameHour++;

      if (gameHour >= 24) {
        gameHour = 0;
      }
    }
  }

  // Decaimento dos status
  if (currentMillis - previousMillisStats >= intervalStats) {
    previousMillisStats = currentMillis;

    if (!estaDormindo) {
      if (energia > 0) energia--;
      if (felicidade > 0) felicidade--;
      if (fome > 0) fome--;
    } else {
      if (energia < 100) energia++;
    }
  }

  // Salvar dados
  if (currentMillis - previousMillisSave >= saveInterval) {
    previousMillisSave = currentMillis;
    saveData();
  }

  switch (currentMode) {
    case MODE_TAMAGOTCHI:
      if (digitalRead(pinoBotaoNavegar) == LOW && currentMillis - lastInputTime > inputCooldown) {
        lastInputTime = currentMillis;
        currentMode = MODE_MENU;
        menuInitialized = false;
        play_tone(400, 50);
        break;
      }

      if (!estaDormindo) {
        if (fome < 20 || felicidade < 20 || energia < 10) {
          drawSickFace();
        } else if (fome < 40) {
          drawSadFace();
        } else {
          if (currentMillis - lastBlinkTime > 3000) {
            lastBlinkTime = currentMillis;
            blink();
          }

          updateInterface();
        }
      } else {
        if (currentMillis - lastRedrawTime > 1000) {
          lastRedrawTime = currentMillis;
          drawSleeping();
        }
      }
      break;

    case MODE_GAME:
      if (currentGame == GAME_SHIP) {
        ship_game_loop();
      } else {
        game_loop();
      }
      break;

    case MODE_GAME_SELECT:
      game_select_loop();
      break;

    case MODE_MENU:
      menu_loop();
      break;
  }

  delay(10);
}
