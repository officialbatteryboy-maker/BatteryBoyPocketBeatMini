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

// Common ESP32-2432S028 CYD pin map. Adjust for other revisions.
static constexpr int TOUCH_CS_PIN = 33;
static constexpr int TOUCH_IRQ_PIN = 36;
static constexpr int SD_CS_PIN = 5;
static constexpr int SD_SCK_PIN = 18;
static constexpr int SD_MISO_PIN = 19;
static constexpr int SD_MOSI_PIN = 23;
static constexpr size_t MAX_TRACKS = 100;
// Set this to the exact advertised name of your Bluetooth speaker before pairing.
static const char *BT_SPEAKER_NAME = "YOUR SPEAKER NAME";
static constexpr size_t PCM_STREAM_BYTES = 16 * 1024;

TFT_eSPI tft;
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
SPIClass sdSPI(VSPI);
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
    // Drop samples rather than blocking the MP3 decoder if Bluetooth falls behind.
    return xStreamBufferSend(pcmStream, sample, sizeof(int16_t) * 2, 0) == sizeof(int16_t) * 2;
  }
};
AudioOutputBluetooth *audioOut = nullptr;
bool bluetoothSourceStarted = false;
bool btConnected = false;

enum Screen : uint8_t { HOME, LIBRARY, COLLECTION, SETTINGS };
Screen screenNow = HOME;

struct Song {
  String path;
  String filename;
  String title;
  String artist;
  String album;
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
  // Use a separate SPI bus for SD so SD reads cannot seize the TFT/touch bus.
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSPI, 16000000)) {
    statusText = "SD NOT READY - UI STILL ACTIVE";
    return;
  }
  sdReady = true;
  scanDirectory("/MUSIC");
  scanDirectory("/");
  if (songs.empty()) statusText = "SD OK - NO MP3 FILES FOUND";
  else statusText = String("SD READY - ") + songs.size() + " TRACKS";
}

void drawTopBar() {
  tft.fillRect(0, 0, 320, 25, C_DARK);
  tft.drawFastHLine(0, 24, 320, C_PURPLE);
  tft.setTextColor(C_MINT, C_DARK);
  tft.drawString("POCKETBEAT", 8, 7, 2);
  tft.setTextColor(C_MUTED, C_DARK);
  tft.drawRightString(String(sdReady ? "SD OK" : "NO SD") + " / " + (btConnected ? "BT OK" : "BT"), 311, 8, 1);
}

void drawNav() {
  tft.fillRect(0, 204, 320, 36, 0x1084);
  tft.drawFastHLine(0, 204, 320, C_PURPLE);
  const char *labels[] = {"HOME", "LIBRARY", "DEX", "SETUP"};
  int xs[] = {0, 80, 160, 240};
  for (int i = 0; i < 4; i++) {
    uint16_t col = (int)screenNow == i ? C_YELLOW : C_MUTED;
    tft.setTextColor(col, 0x1084);
    tft.drawCentreString(labels[i], xs[i] + 40, 218, 1);
    if ((int)screenNow == i) tft.fillRect(xs[i] + 17, 236, 46, 3, C_YELLOW);
  }
}

void drawArt(int x, int y, int size, int seed) {
  uint16_t c1 = seed % 3 == 0 ? C_PINK : (seed % 3 == 1 ? C_PURPLE : C_MINT);
  uint16_t c2 = seed % 2 ? 0x512D : 0xF2A6;
  tft.fillRect(x, y, size, size, c1);
  tft.fillCircle(x + size * 3 / 4, y + size / 4, size / 5, C_YELLOW);
  for (int i = 0; i < 4; i++) {
    int px = x + (i * 13 + seed * 7) % size;
    int py = y + size / 2 + (i * 7) % (size / 2);
    tft.drawLine(px, py, px + size / 5, y + size - 2, c2);
  }
  tft.drawRect(x, y, size, size, C_WHITE);
  tft.setTextColor(C_WHITE, c1);
  tft.drawCentreString(seed >= 0 && seed < (int)songs.size() && songs[seed].artist.length()
      ? songs[seed].artist.substring(0, 1) : "B", x + size / 2, y + size / 2 - 8, 4);
}

void drawButton(int x, int y, int w, int h, const char *label, uint16_t fill, uint16_t ink = C_WHITE) {
  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.drawRoundRect(x, y, w, h, 5, C_PURPLE);
  tft.setTextColor(ink, fill);
  tft.drawCentreString(label, x + w / 2, y + h / 2 - 4, 2);
}

void drawHome() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawString("LOCAL MUSIC MONSTER", 10, 32, 1);
  if (currentSong >= 0 && currentSong < (int)songs.size()) {
    Song &s = songs[currentSong];
    drawArt(218, 43, 88, currentSong);
    tft.setTextColor(C_WHITE, C_BG);
    tft.drawString(s.title.substring(0, 18), 10, 52, 2);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString(s.artist.substring(0, 22), 10, 77, 1);
    tft.drawString(s.album.substring(0, 22), 10, 91, 1);
    tft.setTextColor(C_YELLOW, C_BG);
    tft.drawString(String("LV.") + (1 + currentSong % 20) + "  /  MUSIC DEX", 10, 113, 1);
  } else {
    tft.setTextColor(C_WHITE, C_BG);
    tft.drawString("YOUR MUSIC, YOUR WORLD", 10, 54, 2);
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString(sdReady ? "Open LIBRARY to pick a track" : "Insert FAT32 microSD card", 10, 80, 1);
    drawArt(226, 46, 72, 0);
  }
  drawButton(10, 133, 90, 30, "<<", C_PANEL2);
  drawButton(106, 133, 90, 30, uiPlaying ? "PAUSE" : "PLAY", C_PINK, C_BG);
  drawButton(202, 133, 40, 30, ">>", C_PANEL2);
  drawButton(248, 133, 62, 30, "SPIN!", C_YELLOW, C_BG);
  tft.setTextColor(C_MUTED, C_BG);
  String msg = statusText;
  if (msg.length() > 44) msg = msg.substring(0, 44);
  tft.drawString(msg, 10, 176, 1);
}

void drawLibrary() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawString("MUSIC LIBRARY", 10, 31, 2);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawRightString(String(songs.size()) + " TRACKS", 310, 35, 1);
  if (!sdReady) {
    tft.setTextColor(C_PINK, C_BG);
    tft.drawString("SD unavailable. Check FAT32 card.", 10, 65, 1);
    return;
  }
  if (songs.empty()) {
    tft.setTextColor(C_MUTED, C_BG);
    tft.drawString("Put MP3 files in /MUSIC or card root.", 10, 65, 1);
    return;
  }
  for (int row = 0; row < 5; row++) {
    int i = listOffset + row;
    int y = 52 + row * 29;
    if (i >= (int)songs.size()) break;
    uint16_t bg = i == currentSong ? C_PANEL2 : C_PANEL;
    tft.fillRoundRect(7, y, 306, 25, 4, bg);
    tft.setTextColor(i == currentSong ? C_YELLOW : C_WHITE, bg);
    String title = songs[i].title;
    if (title.length() > 22) title = title.substring(0, 22);
    tft.drawString(String(i + 1) + ". " + title, 13, y + 3, 1);
    tft.setTextColor(C_MUTED, bg);
    String artist = songs[i].artist;
    if (artist.length() > 27) artist = artist.substring(0, 27);
    tft.drawString(artist, 13, y + 14, 1);
  }
  drawButton(7, 199, 55, 1, "", C_BG);
}

void drawCollection() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.drawString("MUSIC DEX / TRAINING", 10, 33, 2);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawString("Tracks indexed", 10, 66, 2);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawNumber((int)songs.size(), 246, 66, 4);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("Concept port: each local song is a creature.", 10, 100, 1);
  tft.drawString("XP, evolutions and saved stats are not", 10, 116, 1);
  tft.drawString("enabled in this first hardware build.", 10, 130, 1);
  if (currentSong >= 0 && currentSong < (int)songs.size()) {
    drawArt(10, 146, 43, currentSong);
    tft.setTextColor(C_WHITE, C_BG);
    tft.drawString(songs[currentSong].title.substring(0, 25), 63, 157, 2);
  }
}

void drawSettings() {
  tft.fillRect(0, 25, 320, 179, C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawString("DEVICE SETTINGS", 10, 33, 2);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawString("BLUETOOTH AUDIO", 10, 58, 1);
  drawButton(10, 72, 142, 30, btConnected ? "BT CONNECTED" : "CONNECT SPEAKER", C_PURPLE);
  drawButton(164, 72, 142, 30, "STOP AUDIO", C_PINK, C_BG);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString(btStatus.substring(0, 36), 10, 109, 1);
  tft.drawString("Set BT_SPEAKER_NAME in src/main.cpp", 10, 124, 1);
  tft.drawString("TOUCH DIAGNOSTICS", 10, 145, 1);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("No boot-time calibration loop is used.", 10, 160, 1);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.drawString(sdReady ? "SD: MOUNTED" : "SD: NOT MOUNTED", 10, 177, 1);
}

void render() {
  tft.fillScreen(C_BG);
  drawTopBar();
  switch (screenNow) {
    case HOME: drawHome(); break;
    case LIBRARY: drawLibrary(); break;
    case COLLECTION: drawCollection(); break;
    case SETTINGS: drawSettings(); break;
  }
  drawNav();
  needsDraw = false;
  lastDrawMs = millis();
}

int32_t bluetoothPcmCallback(uint8_t *data, int32_t byteCount) {
  if (!data || byteCount <= 0) return 0;
  memset(data, 0, byteCount);
  if (pcmStream) {
    // Return silence for any bytes not yet supplied by the MP3 decoder.
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
  if (!pcmStream) { btStatus = "PCM BUFFER FAILED"; statusText = btStatus; needsDraw = true; return; }
  // The A2DP source discovers/connects to the named speaker; first connection may take several seconds.
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
    int tab = x / 80;
    screenNow = tab == 0 ? HOME : tab == 1 ? LIBRARY : tab == 2 ? COLLECTION : SETTINGS;
    needsDraw = true;
    return;
  }
  if (screenNow == HOME) {
    if (y >= 130 && y <= 168) {
      if (x < 102) selectSong(currentSong - 1);
      else if (x < 199) {
        if (uiPlaying) stopPlayback(); else startPlayback();
      } else if (x < 246) selectSong(currentSong + 1);
      else {
        if (!songs.empty()) selectSong(random((int)songs.size()), true);
        else { statusText = "NO TRACKS TO SURPRISE YOU WITH"; needsDraw = true; }
      }
    } else if (y < 25) {
      screenNow = SETTINGS;
      statusText = String("TOUCH RAW ") + rawX + "," + rawY;
      needsDraw = true;
    }
    return;
  }
  if (screenNow == LIBRARY) {
    if (y >= 50 && y < 195) {
      int row = (y - 52) / 29;
      int idx = listOffset + row;
      if (row >= 0 && row < 5 && idx < (int)songs.size()) selectSong(idx);
    }
    return;
  }
  if (screenNow == SETTINGS) {
    if (y >= 68 && y <= 105) {
      if (x < 158) startBluetooth();
      else stopPlayback();
      return;
    }
    statusText = String("TOUCH RAW ") + rawX + "," + rawY;
    needsDraw = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(80);
  tft.init();
  tft.setRotation(1); // landscape 320x240
  tft.setTextWrap(false);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawCentreString("POCKETBEAT MINI", 160, 90, 4);
  tft.setTextColor(C_MINT, C_BG);
  tft.drawCentreString("STARTING...", 160, 128, 2);

  SPI.begin(14, 12, 13, TOUCH_CS_PIN);
  touch.begin();
  touch.setRotation(1);
  randomSeed(esp_random());

  // Deliberately no infinite retry or calibration loop here.
  scanCardOnce();
  render();
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
    // Approximate defaults for common CYD. Change these if touch axes are offset.
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
