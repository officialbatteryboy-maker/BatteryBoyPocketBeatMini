#include <Arduino.h>
#include <TFT_eSPI.h>

#define SCREEN_W 320
#define SCREEN_H 240

TFT_eSPI tft = TFT_eSPI();

enum Tab
{
  TabHome,
  TabLibrary,
  TabBattle
};

struct Song
{
  const char* title;
  const char* artist;
  const char* album;
  const char* affinity;
  uint32_t colorA;
  uint32_t colorB;
};

const Song songs[] = {
  {"Midnight City", "M83", "Hurry Up, We're Dreaming", "Pulse", TFT_PURPLE, TFT_MAGENTA},
  {"Cherry-coloured Funk", "Cocteau Twins", "Heaven or Las Vegas", "Nocturne", TFT_BLUE, TFT_DARKPURPLE},
  {"Digital Love", "Daft Punk", "Discovery", "Groove", TFT_CYAN, TFT_NAVY},
  {"Resonance", "HOME", "Odyssey", "Pulse", TFT_YELLOW, TFT_MAGENTA},
  {"A Walk", "Tycho", "Dive", "Echo", TFT_GREEN, TFT_BLUE},
};

struct AppState
{
  Tab tab = TabHome;
  int currentSong = 0;
  bool playing = false;
  int progress = 38;
  int turn = 1;
  bool bluetoothOpen = false;
  bool levelUpOpen = false;
};

AppState app;

uint32_t mixColor(uint32_t a, uint32_t b, float t)
{
  uint16_t r1 = (a >> 11) & 0x1F;
  uint16_t g1 = (a >> 5) & 0x3F;
  uint16_t b1 = a & 0x1F;

  uint16_t r2 = (b >> 11) & 0x1F;
  uint16_t g2 = (b >> 5) & 0x3F;
  uint16_t b2 = b & 0x1F;

  uint16_t r = (uint16_t)(r1 * (1.0f - t) + r2 * t);
  uint16_t g = (uint16_t)(g1 * (1.0f - t) + g2 * t);
  uint16_t b = (uint16_t)(b1 * (1.0f - t) + b2 * t);

  return (r << 11) | (g << 5) | b;
}

void drawRoundedRectSoft(int x, int y, int w, int h, int r, uint32_t color)
{
  tft.fillRoundRect(x, y, w, h, r, color);
}

void drawAlbumArt(int x, int y, int w, int h, const Song& song)
{
  uint32_t c1 = song.colorA;
  uint32_t c2 = song.colorB;

  tft.fillRoundRect(x, y, w, h, 6, mixColor(c1, c2, 0.65f));
  tft.fillCircle(x + w - 18, y + 18, 20, TFT_YELLOW);
  tft.fillRect(x + 10, y + 26, w - 20, h - 34, TFT_DARKPURPLE);
  tft.setTextColor(TFT_WHITE, mixColor(c1, c2, 0.65f));
  tft.setTextDatum(MC_DATUM);
  tft.drawString(song.artist, x + w / 2, y + h / 2, 2);
}

void drawStatusBar()
{
  tft.fillRect(0, 0, SCREEN_W, 24, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("SONGDEX", 12, 12, 2);
  tft.drawString("CYD • 001", 120, 12, 2);
  tft.fillRect(250, 7, 52, 12, TFT_DARKPURPLE);
  tft.fillRect(250, 7, 40, 12, TFT_GREEN);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("82%", 240, 12, 2);
}

void drawTabBar()
{
  tft.fillRect(0, 204, SCREEN_W, 36, TFT_NAVY);
  tft.drawFastHLine(0, 203, SCREEN_W, TFT_DARKGREY);

  const int tabWidth = SCREEN_W / 3;
  static const char* labels[] = {"PLAYER", "SONGDEX", "BATTLE"};

  for (int i = 0; i < 3; ++i)
  {
    int cx = i * tabWidth + tabWidth / 2;
    int active = (app.tab == i) ? 1 : 0;
    tft.setTextColor(active ? TFT_YELLOW : TFT_LIGHTGREY, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(labels[i], cx, 220, 2);
  }
}

void drawHomeScreen()
{
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("NOW SPINNING", 16, 32, 2);

  const Song& song = songs[app.currentSong];
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(song.title, 16, 48, 4);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(song.artist, 18, 90, 2);

  tft.fillRoundRect(16, 116, 148, 12, 4, TFT_DARKGREY);
  tft.fillRoundRect(16, 116, 100, 12, 4, TFT_YELLOW);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("LEVEL CHAIN 2/5", 16, 134, 2);

  tft.fillRoundRect(16, 154, 160, 40, 8, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("SURPRISE ME!", 30, 170, 3);

  drawAlbumArt(205, 38, 95, 95, song);
  tft.fillCircle(260, 150, 38, TFT_DARKPURPLE);
  tft.fillCircle(260, 150, 18, TFT_PINK);

  tft.fillRoundRect(16, 196, 288, 32, 8, TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("1:32", 26, 208, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(song.album, 290, 208, 2);

  tft.drawFastVLine(110, 196, 32, TFT_LIGHTGREY);
  tft.fillRoundRect(120, 206, 150, 6, 3, TFT_LIGHTGREY);
  tft.fillRoundRect(120, 206, app.progress * 1.5f, 6, 3, TFT_PINK);
}

void drawLibraryScreen()
{
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SD COLLECTION", 16, 32, 2);
  tft.drawString("Your Songdex", 16, 50, 4);

  for (int i = 0; i < min(4, (int)(sizeof(songs) / sizeof(songs[0]))); ++i)
  {
    int y = 78 + i * 36;
    tft.fillRoundRect(16, y, 288, 30, 6, TFT_DARKGREY);
    drawAlbumArt(22, y + 4, 22, 22, songs[(app.currentSong + i) % 5]);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(songs[(app.currentSong + i) % 5].title, 52, y + 8, 2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    tft.drawString(songs[(app.currentSong + i) % 5].artist, 52, y + 20, 2);
  }

  tft.fillRoundRect(16, 196, 288, 28, 6, TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(songs[app.currentSong].album, 24, 204, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("4:03", 295, 204, 2);
}

void drawBattleScreen()
{
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("WIRELESS ARENA", 16, 32, 2);
  tft.drawString("Song Battle", 16, 50, 4);

  tft.fillRoundRect(16, 84, 288, 64, 8, TFT_DARKPURPLE);
  tft.setTextColor(TFT_YELLOW, TFT_DARKPURPLE);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("YOUR CHAMPION", 24, 96, 2);
  tft.drawString(songs[app.currentSong].title, 24, 120, 3);
  tft.drawString(songs[app.currentSong].artist, 24, 144, 2);

  tft.fillRoundRect(220, 100, 70, 30, 8, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("POWER", 255, 110, 2);
  tft.drawString("82", 255, 128, 3);

  tft.fillRoundRect(16, 160, 288, 54, 8, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("TRAINERS NEARBY", 24, 176, 2);
  tft.drawString("MOSSY-07", 24, 196, 2);
  tft.drawString("NOVA-CYD", 150, 196, 2);
  tft.drawString("BEATBOY", 260, 196, 2);
}

void render()
{
  switch (app.tab)
  {
    case TabHome:
      drawHomeScreen();
      break;
    case TabLibrary:
      drawLibraryScreen();
      break;
    case TabBattle:
      drawBattleScreen();
      break;
  }

  drawTabBar();
}

void setup()
{
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&FreeMonoBold9pt7b);

  app.currentSong = 0;
  app.progress = 38;
  app.playing = true;
}

void loop()
{
  static unsigned long lastTick = 0;

  if (millis() - lastTick > 500)
  {
    lastTick = millis();
    if (app.playing)
    {
      app.progress += 1;
      if (app.progress > 100)
      {
        app.progress = 0;
        app.currentSong = (app.currentSong + 1) % (sizeof(songs) / sizeof(songs[0]));
      }
    }
  }

  if (Serial.available())
  {
    char ch = Serial.read();
    switch (ch)
    {
      case '1':
        app.tab = TabHome;
        break;
      case '2':
        app.tab = TabLibrary;
        break;
      case '3':
        app.tab = TabBattle;
        break;
      case ' ':
        app.playing = !app.playing;
        break;
      case 'n':
        app.currentSong = (app.currentSong + 1) % (sizeof(songs) / sizeof(songs[0]));
        break;
      case 'p':
        app.currentSong = (app.currentSong - 1 + (sizeof(songs) / sizeof(songs[0]))) % (sizeof(songs) / sizeof(songs[0]));
        break;
    }
  }

  render();
  delay(30);
}
