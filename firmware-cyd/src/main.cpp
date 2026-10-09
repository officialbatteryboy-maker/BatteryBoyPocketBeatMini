#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <BluetoothA2DPSource.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioOutput.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <vector>

static constexpr int TOUCH_CS_PIN = 33;
static constexpr int TOUCH_IRQ_PIN = 36;
static constexpr int SD_CS_PIN = 5;
static constexpr int SD_SCK_PIN = 18;
static constexpr int SD_MISO_PIN = 19;
static constexpr int SD_MOSI_PIN = 23;
static constexpr size_t MAX_TRACKS = 100;
static const char *BT_SPEAKER_NAME = "YOUR SPEAKER NAME";
static constexpr size_t PCM_STREAM_BYTES = 16 * 1024;

TFT_eSPI tft;
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
SPIClass sdSPI(HSPI);
BluetoothA2DPSource a2dpSource;
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *audioFile = nullptr;
StreamBufferHandle_t pcmStream = nullptr;
class AudioOutputBluetooth : public AudioOutput {
public:
  bool begin() override { return true; }
  bool stop() override { return true; }
  void flush() override {}
  bool ConsumeSample(int16_t sample[2]) override {
    if (!pcmStream) return false;
    return xStreamBufferSend(pcmStream, sample, sizeof(int16_t) * 2, 0) == sizeof(int16_t) * 2;
  }
};
AudioOutputBluetooth *audioOut = nullptr;
bool bluetoothSourceStarted = false;
bool btConnected = false;

enum Screen : uint8_t { HOME, LIBRARY, BATTLE, SETTINGS };
Screen screenNow = HOME;

struct Song {
  String path;
  String filename;
  String title;
  String artist;
  String album;
  String affinity;
  uint32_t size = 0;
};

std::vector<Song> songs;
int currentSong = -1;
int listOffset = 0;
bool sdReady = false;
bool uiPlaying = false;
String statusText = "BOOTING POCKETBEAT...";
String btStatus = "BT OFF";
uint32_t lastTouchMs = 0;
uint32_t lastDrawMs = 0;
bool needsDraw = true;

const uint16_t C_BG = 0x100A;
const uint16_t C_PANEL = 0x210F;
const uint16_t C_PANEL2 = 0x3118;
const uint16_t C_PURPLE = 0xA2B5;
const uint16_t C_PINK = 0xF1D1;
const uint16_t C_MINT = 0x7FBA;
const uint16_t C_YELLOW = 0xFE6D;
const uint16_t C_WHITE = 0xFFDF;
const uint16_t C_MUTED = 0xB2B4;
const uint16_t C_DARK = 0x18C5;
const uint16_t C_LAVENDER = 0xB9B5;
const uint16_t C_BLACK = 0x0000;

String basenameOf(const String &path) {
  int slash = path.lastIndexOf('/');
  String name = slash >= 0 ? path.substring(slash + 1) : path;
  int dot = name.lastIndexOf('.');
  if (dot > 0) name = name.substring(0, dot);
  name.replace('_', ' ');
  name.replace('-', ' ');
  return name;
}

String cleanTag(const uint8_t *buf, size_t n) {
  String out;
  for (size_t i = 0; i < n; ++i) {
    char c = (char)buf[i];
    if (c == 0 || c == '\r' || c == '\n') break;
    if ((uint8_t)c >= 32) out += c;
  }
  out.trim();
  return out;
}

void readId3v1(Song &song) {
  File f = SD.open(song.path, FILE_READ);
  if (!f) return;
  if (f.size() >= 128 && f.seek(f.size() - 128)) {
    uint8_t tag[128];
    if (f.read(tag, sizeof(tag)) == sizeof(tag) &&
        tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G') {
      String title = cleanTag(tag + 3, 30);
      String artist = cleanTag(tag + 33, 30);
      String album = cleanTag(tag + 63, 30);
      if (title.length()) song.title = title;
      if (artist.length()) song.artist = artist;
      if (album.length()) song.album = album;
    }
  }
  f.close();
}

bool isMp3(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

void addTrack(const String &path, const String &name, uint32_t size) {
  if (songs.size() >= MAX_TRACKS || !isMp3(name)) return;
  Song s;
  s.path = path;
  s.filename = name;
  s.title = basenameOf(name);
  s.artist = "UNKNOWN ARTIST";
  s.album = "LOCAL FILE";
  s.affinity = "Pulse";
  s.size = size;
  readId3v1(s);
  songs.push_back(s);
}

void scanDirectory(const char *dirPath) {
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  for (File entry = dir.openNextFile(); entry && songs.size() < MAX_TRACKS; entry = dir.openNextFile()) {
    if (!entry.isDirectory()) {
      String name = String(entry.name());
      String path = String(dirPath);
      if (path != "/") path += "/";
      path += name;
      addTrack(path, name, (uint32_t)entry.size());
    }
    entry.close();
  }
  dir.close();
}

void scanCardOnce() {
  songs.clear();
  sdReady = false;
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSPI, 8000000)) {
    statusText = "SD NOT READY - UI STILL ACTIVE";
    return;
  }
  sdReady = true;
  scanDirectory("/MUSIC");
  scanDirectory("/");
  if (songs.empty()) statusText = "SD OK - NO MP3 FILES FOUND";
  else statusText = String("SD READY - ") + songs.size() + " TRACKS";
  if (songs.empty()) currentSong = -1;
  else currentSong = 0;
}

static uint16_t blendColor(uint16_t a, uint16_t b, float t) {
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

void drawRoundedFrame(int x, int y, int w, int h, int r, uint16_t fill, uint16_t stroke = C_PURPLE) {
  tft.fillRoundRect(x, y, w, h, r, fill);
  tft.drawRoundRect(x, y, w, h, r, stroke);
}

void drawBigButton(int x, int y, int w, int h, const char *txt, uint16_t bg, uint16_t ink = C_BLACK) {
  tft.fillRoundRect(x, y, w, h, 7, bg);
  tft.drawRoundRect(x, y, w, h, 7, C_WHITE);
  tft.setTextColor(ink, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(txt, x + w / 2, y + h / 2 + 1, 2);
}

void drawMiniArt(int x, int y, int s, uint32_t c1, uint32_t c2, const String &initial) {
  tft.fillRoundRect(x, y, s, s, 4, c1);
  tft.fillCircle(x + s * 3 / 4, y + s / 4, s / 4, TFT_YELLOW);
  tft.fillRect(x + 6, y + s / 2, s - 12, s / 3, c2);
  tft.setTextColor(TFT_WHITE, c1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(initial.substring(0, 1), x + s / 2, y + s / 2 - 2, 3);
}

void drawStatusBar() {
  tft.fillRect(0, 0, 320, 24, 0x18C5);
  tft.setTextColor(C_MINT, 0x18C5);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("SONGDEX", 12, 8, 2);
  tft.setTextColor(C_LAVENDER, 0x18C5);
  tft.drawString("CYD • 001", 112, 8, 2);

  tft.fillRoundRect(246, 8, 36, 9, 3, C_DARK);
  tft.fillRoundRect(252, 9, 24, 7, 2, C_MINT);
  tft.setTextColor(C_WHITE, 0x18C5);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("82%", 306, 8, 2);
}

void drawHomeScreen() {
  tft.fillRect(0, 25, 320, 179, C_BG);

  tft.setTextColor(C_MINT, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("NOW SPINNING", 16, 34, 2);

  if (currentSong >= 0 && currentSong < (int)songs.size()) {
    const Song &song = songs[currentSong];
    tft.setTextColor(C_WHITE, C_BG);
    tft.drawString(song.title.substring(0, 16), 16, 54, 4);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString(song.artist.substring(0, 22), 18, 92, 2);
    tft.setTextColor(C_LAVENDER, C_BG);
    tft.drawString(song.affinity, 18, 112, 2);

    tft.fillRect(18, 133, 120, 10, C_DARK);
    tft.fillRect(18, 133, 70, 10, C_YELLOW);
    tft.setTextColor(C_YELLOW, C_BG);
    tft.drawString("LEVEL CHAIN 2/5", 18, 146, 2);

    tft.fillRoundRect(18, 166, 152, 34, 8, C_YELLOW);
    tft.setTextColor(C_BLACK, C_YELLOW);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SURPRISE ME!", 94, 183, 2);

    uint16_t c1 = 0xF81F; uint16_t c2 = 0xA2B5;
    if (currentSong % 3 == 0) { c1 = 0xF81F; c2 = 0x5A3D; }
    else if (currentSong % 3 == 1) { c1 = 0xF800; c2 = 0xA2B5; }
    else { c1 = 0x07E0; c2 = 0x001F; }

    tft.fillRoundRect(210, 38, 92, 92, 10, C_DARK);
    tft.fillCircle(260, 90, 34, blendColor(c1, c2, 0.7f));
    tft.fillRect(220, 110, 72, 30, 0x312A);
    tft.setTextColor(C_WHITE, blendColor(c1, c2, 0.7f));
    tft.setTextDatum(MC_DATUM);
    tft.drawString(song.artist.substring(0, 1), 260, 89, 4);

    tft.fillRoundRect(105, 205, 115, 10, 5, C_DARK);
    tft.fillRoundRect(105, 205, (progress * 115) / 100, 10, 5, C_PINK);
    tft.setTextColor(C_MUTED, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("1:32", 20, 204, 2);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(song.length, 300, 204, 2);
  } else {
    tft.setTextColor(C_WHITE, C_BG);
    tft.drawString("DROP A TRACK", 16, 54, 4);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString("Insert SD and open library", 18, 94, 2);
    tft.fillRoundRect(216, 42, 84, 84, 10, C_PANEL2);
    tft.setTextColor(C_WHITE, C_PANEL2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("♫", 258, 80, 4);
  }

  tft.fillRoundRect(18, 210, 58, 34, 7, C_PANEL2);
  tft.fillRoundRect(92, 210, 58, 34, 7, C_PINK);
  tft.fillRoundRect(166, 210, 58, 34, 7, C_PANEL2);
  tft.fillRoundRect(240, 210, 60, 34, 7, C_YELLOW);

  tft.setTextColor(C_WHITE, C_PANEL2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("<<", 47, 228, 2);
  tft.setTextColor(C_BLACK, C_PINK);
  tft.drawString(uiPlaying ? "PAUSE" : "PLAY", 121, 228, 2);
  tft.setTextColor(C_WHITE, C_PANEL2);
  tft.drawString(">>", 195, 228, 2);
  tft.setTextColor(C_BLACK, C_YELLOW);
  tft.drawString("SPIN", 270, 228, 2);

  tft.setTextColor(C_MUTED, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(statusText.substring(0, 28), 18, 154, 1);
}

void drawLibraryScreen() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SD COLLECTION", 16, 34, 2);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawRightString(String(songs.size()) + " TRACKS", 302, 38, 2);

  tft.fillRoundRect(16, 66, 288, 28, 6, 0x2A1E);
  tft.setTextColor(C_MUTED, 0x2A1E);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Search your collection...", 30, 79, 2);

  for (int row = 0; row < 5; ++row) {
    int i = listOffset + row;
    if (i >= (int)songs.size()) break;
    int y = 106 + row * 22;
    bool active = i == currentSong;
    tft.fillRoundRect(16, y, 288, 18, 4, active ? 0x3A2D : C_PANEL);
    tft.setTextColor(active ? C_YELLOW : C_WHITE, active ? 0x3A2D : C_PANEL);
    tft.setTextDatum(TL_DATUM);
    String text = songs[i].title;
    if (text.length() > 18) text = text.substring(0, 18);
    tft.drawString(String(i + 1) + ". " + text, 24, y + 4, 2);

    tft.setTextColor(C_MUTED, active ? 0x3A2D : C_PANEL);
    tft.drawRightString(songs[i].artist.substring(0, 12), 292, y + 5, 1);
  }

  tft.fillRoundRect(16, 206, 288, 24, 6, C_PANEL2);
  tft.setTextColor(C_WHITE, C_PANEL2);
  tft.setTextDatum(TL_DATUM);
  String albumText = currentSong >= 0 && currentSong < (int)songs.size() ? songs[currentSong].album : "NO TRACK";
  tft.drawString(albumText.substring(0, 18), 22, 212, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("4:03", 296, 213, 2);
}

void drawBattleScreen() {
  tft.fillRect(0, 25, 320, 179, C_BG);

  tft.setTextColor(C_WHITE, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("WIRELESS ARENA", 16, 34, 2);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawRightString("ONLINE", 300, 38, 2);

  tft.fillRoundRect(16, 66, 288, 54, 8, 0x2A1E);
  tft.fillRoundRect(25, 74, 40, 40, 6, C_YELLOW);
  tft.setTextColor(C_BLACK, C_YELLOW);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("M", 45, 94, 2);
  tft.setTextColor(C_WHITE, 0x2A1E);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("YOUR CHAMPION", 90, 76, 2);
  tft.drawString(songs[currentSong >= 0 ? currentSong : 0].title.substring(0, 16), 90, 96, 2);
  tft.setTextColor(C_MINT, 0x2A1E);
  tft.drawString("POWER 82", 90, 116, 2);

  tft.setTextColor(C_MUTED, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("TRAINERS NEARBY", 16, 130, 2);

  const char *names[] = {"MOSSY-07", "NOVA-CYD", "BEATBOY"};
  for (int i = 0; i < 3; ++i) {
    int x = 16 + i * 96;
    int y = 148;
    tft.fillRoundRect(x, y, 88, 38, 6, i == 0 ? 0x3A2D : C_PANEL);
    tft.fillRoundRect(x + 6, y + 6, 20, 20, 4, i == 0 ? C_PINK : C_MINT);
    tft.setTextColor(C_WHITE, i == 0 ? 0x3A2D : C_PANEL);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(names[i], x + 32, y + 8, 1);
    tft.setTextColor(C_MUTED, i == 0 ? 0x3A2D : C_PANEL);
    tft.drawString("LV.18", x + 32, y + 20, 1);
  }
}

void drawSettingsScreen() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("DEVICE SETTINGS", 16, 34, 2);

  drawBigButton(18, 66, 130, 32, btConnected ? "BT READY" : "CONNECT", C_PURPLE);
  drawBigButton(172, 66, 130, 32, "STOP", C_PINK, C_WHITE);

  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("BT_SPEAKER_NAME in src/main.cpp", 18, 110, 1);
  tft.drawString("Touch diagnostics enabled", 18, 130, 1);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.drawString(sdReady ? "SD: MOUNTED" : "SD: NOT MOUNTED", 18, 160, 2);
}

void drawNavBar() {
  tft.fillRect(0, 204, 320, 36, 0x1084);
  tft.drawFastHLine(0, 204, 320, C_PURPLE);

  const char *labels[] = {"PLAYER", "SONGDEX", "BATTLE", "SETUP"};
  int xs[] = {0, 80, 160, 240};
  for (int i = 0; i < 4; ++i) {
    uint16_t col = (int)screenNow == i ? C_YELLOW : C_MUTED;
    tft.setTextColor(col, 0x1084);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(labels[i], xs[i] + 40, 220, 1);
    if ((int)screenNow == i) tft.fillRect(xs[i] + 17, 234, 46, 3, C_YELLOW);
  }
}

void render() {
  tft.fillScreen(C_BG);
  drawStatusBar();
  switch (screenNow) {
    case HOME:
      drawHomeScreen();
      break;
    case LIBRARY:
      drawLibraryScreen();
      break;
    case BATTLE:
      drawBattleScreen();
      break;
    case SETTINGS:
      drawSettingsScreen();
      break;
  }
  drawNavBar();
  needsDraw = false;
  lastDrawMs = millis();
}

int32_t bluetoothPcmCallback(uint8_t *data, int32_t byteCount) {
  if (!data || byteCount <= 0) return 0;
  memset(data, 0, byteCount);
  if (pcmStream) {
    xStreamBufferReceive(pcmStream, data, byteCount, 0);
  }
  return byteCount;
}

void stopPlayback() {
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (audioFile) { audioFile->close(); delete audioFile; audioFile = nullptr; }
  uiPlaying = false;
  if (pcmStream) xStreamBufferReset(pcmStream);
  statusText = "PLAYBACK STOPPED";
  needsDraw = true;
}

void startBluetooth() {
  if (bluetoothSourceStarted) return;
  if (String(BT_SPEAKER_NAME) == "YOUR SPEAKER NAME") {
    btStatus = "EDIT BT_SPEAKER_NAME FIRST";
    statusText = btStatus;
    needsDraw = true;
    return;
  }
  if (!pcmStream) pcmStream = xStreamBufferCreate(PCM_STREAM_BYTES, 4);
  if (!pcmStream) {
    btStatus = "PCM BUFFER FAILED";
    statusText = btStatus;
    needsDraw = true;
    return;
  }
  a2dpSource.start_raw(BT_SPEAKER_NAME, bluetoothPcmCallback);
  bluetoothSourceStarted = true;
  btStatus = String("CONNECTING TO ") + BT_SPEAKER_NAME;
  statusText = btStatus;
  needsDraw = true;
}

void startPlayback() {
  if (currentSong < 0 || currentSong >= (int)songs.size()) {
    if (songs.empty()) { statusText = "NO TRACKS - CHECK SD"; needsDraw = true; return; }
    currentSong = 0;
  }
  startBluetooth();
  if (!pcmStream) return;
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (audioFile) { audioFile->close(); delete audioFile; audioFile = nullptr; }
  if (audioOut) { delete audioOut; audioOut = nullptr; }
  xStreamBufferReset(pcmStream);

  audioFile = new AudioFileSourceSD(songs[currentSong].path.c_str());
  audioOut = new AudioOutputBluetooth();
  mp3 = new AudioGeneratorMP3();
  if (!audioFile || !audioOut || !mp3 || !mp3->begin(audioFile, audioOut)) {
    stopPlayback();
    statusText = "MP3 START FAILED - CHECK FILE";
    needsDraw = true;
    return;
  }
  uiPlaying = true;
  statusText = String("PLAYING: ") + songs[currentSong].title;
  needsDraw = true;
}

void selectSong(int index, bool surprise = false) {
  if (songs.empty()) {
    statusText = "NO TRACKS - CHECK SD CARD";
    needsDraw = true;
    return;
  }
  if (index < 0) index = (int)songs.size() - 1;
  if (index >= (int)songs.size()) index = 0;
  if (uiPlaying) stopPlayback();
  currentSong = index;
  statusText = surprise ? "SURPRISE SPIN - READY TO PLAY" : "TRACK SELECTED";
  screenNow = HOME;
  needsDraw = true;
}

bool hit(int x, int y, int x1, int y1, int x2, int y2) {
  return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

void handleTouch(int x, int y, int rawX, int rawY) {
  if (y >= 204) {
    if (x < 80) screenNow = HOME;
    else if (x < 160) screenNow = LIBRARY;
    else if (x < 240) screenNow = BATTLE;
    else screenNow = SETTINGS;
    needsDraw = true;
    return;
  }

  if (screenNow == HOME) {
    if (hit(x, y, 18, 210, 76, 244)) {
      selectSong(currentSong - 1);
      return;
    }
    if (hit(x, y, 92, 210, 150, 244)) {
      if (uiPlaying) stopPlayback(); else startPlayback();
      return;
    }
    if (hit(x, y, 166, 210, 224, 244)) {
      selectSong(currentSong + 1);
      return;
    }
    if (hit(x, y, 240, 210, 300, 244)) {
      if (!songs.empty()) selectSong(random((int)songs.size()), true);
    }
    return;
  }

  if (screenNow == LIBRARY) {
    if (y >= 106 && y < 106 + 5 * 22 + 2) {
      int row = (y - 106) / 22;
      int idx = listOffset + row;
      if (idx >= 0 && idx < (int)songs.size()) selectSong(idx);
    }
    return;
  }

  if (screenNow == BATTLE) {
    if (y >= 66 && y <= 120) {
      if (x >= 16 && x <= 304) {
        statusText = "BATTLE READY";
        needsDraw = true;
      }
    }
    return;
  }

  if (screenNow == SETTINGS) {
    if (hit(x, y, 18, 66, 148, 98)) {
      startBluetooth();
      return;
    }
    if (hit(x, y, 172, 66, 302, 98)) {
      stopPlayback();
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("[BOOT] PocketBeat CYD starting");

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(50);

  tft.init();
  tft.setRotation(1);
  tft.setTextWrap(false);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawCentreString("SONGDEX", 160, 90, 4);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawCentreString("LOADING...", 160, 128, 2);

  SPI.begin(14, 12, 13, TOUCH_CS_PIN);
  touch.begin();
  touch.setRotation(1);
  randomSeed(esp_random());

  scanCardOnce();
  render();
  Serial.println("[BOOT] Setup complete");
}

void loop() {
  if (bluetoothSourceStarted) btConnected = a2dpSource.is_connected();
  if (mp3 && uiPlaying) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        uiPlaying = false;
        statusText = "TRACK FINISHED";
        needsDraw = true;
      }
    } else {
      uiPlaying = false;
    }
  }

  if (touch.touched() && millis() - lastTouchMs > 180) {
    TS_Point p = touch.getPoint();
    int x = map(p.x, 250, 3800, 320, 0);
    int y = map(p.y, 250, 3800, 240, 0);
    x = constrain(x, 0, 319);
    y = constrain(y, 0, 239);
    handleTouch(x, y, p.x, p.y);
    lastTouchMs = millis();
  }

  if (needsDraw && millis() - lastDrawMs > 25) render();
  delay(3);
}
