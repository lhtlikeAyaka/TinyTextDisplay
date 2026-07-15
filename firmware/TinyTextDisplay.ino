#include <Arduino.h>
#include <SPI.h>
#include "BluetoothSerial.h" 
#include "font.h"

#define TFT_CS   5
#define TFT_DC   17
#define TFT_RST  16

#define TFT_WIDTH  284
#define TFT_HEIGHT 76

#define X_OFFSET 18
#define Y_OFFSET 82

//字号高度定义
#define BUFFER_HEIGHT 32           
//字体间距参数
#define ASCII_STEP  24   
#define CJK_STEP    38   
//滚动与蓝牙配置
// ==========================================
char lyrics[512] = "蓝牙已就绪，等待发送歌词... 🎵"; 

const int frameDelayMs = 1;        // 1ms 延迟
const int scrollStep = 6;          // 每次移动 6 像素
const unsigned long pauseTime = 900; // 首尾静止停顿时间

BluetoothSerial SerialBT;          
String rxBuffer = "";              
// 状态机定义
enum ScrollState {
  STATE_STATIC,       
  STATE_PAUSE_START,  
  STATE_SCROLLING,    
  STATE_PAUSE_END     
};

ScrollState scrollState = STATE_STATIC;
int scrollX = 0;
int lyricsWidth = 0;
unsigned long stateTimer = 0;
unsigned long lastUpdate = 0;

bool staticDrawn = false; 

// 32像素高的局部行缓冲区
uint16_t lineBuffer[TFT_WIDTH * BUFFER_HEIGHT];


// 自动确认蓝牙配对

void btConfirmCallback(uint32_t numVal) {
  SerialBT.confirmReply(true); 
}


// LCD 驱动

void lcd_cmd(uint8_t cmd)
{
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(TFT_CS, HIGH);
}

void lcd_data(uint8_t data)
{
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(data);
  digitalWrite(TFT_CS, HIGH);
}

void resetLCD()
{
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);
}

void initLCD()
{
  resetLCD();
  lcd_cmd(0x01);
  delay(150);
  lcd_cmd(0x11);
  delay(120);
  lcd_cmd(0x3A);
  lcd_data(0x55);
  lcd_cmd(0x36);
  lcd_data(0x60);
  lcd_cmd(0x20);

  lcd_cmd(0xB2);
  lcd_data(0x0C);
  lcd_data(0x0C);
  lcd_data(0x60);
  lcd_data(0x33);
  lcd_data(0x33);

  lcd_cmd(0xB7);
  lcd_data(0x35);
  lcd_cmd(0xBB);
  lcd_data(0x19);
  lcd_cmd(0xC0);
  lcd_data(0x2C);
  lcd_cmd(0xC2);
  lcd_data(0x01);
  lcd_cmd(0xC3);
  lcd_data(0x12);
  lcd_cmd(0xC4);
  lcd_data(0x20);
  lcd_cmd(0xC6);
  lcd_data(0x0F);
  lcd_cmd(0x29);
}

void setWindow(int x, int y, int w, int h)
{
  uint16_t xs = x + X_OFFSET;
  uint16_t xe = xs + w - 1;
  uint16_t ys = y + Y_OFFSET;
  uint16_t ye = ys + h - 1;

  lcd_cmd(0x2A);
  lcd_data(xs >> 8);
  lcd_data(xs);
  lcd_data(xe >> 8);
  lcd_data(xe);

  lcd_cmd(0x2B);
  lcd_data(ys >> 8);
  lcd_data(ys);
  lcd_data(ye >> 8);
  lcd_data(ye);

  lcd_cmd(0x2C);
}

void fillColor(uint16_t color)
{
  setWindow(0, 0, TFT_WIDTH, TFT_HEIGHT);
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  for (long i = 0; i < (long)TFT_WIDTH * TFT_HEIGHT; i++)
  {
    SPI.transfer(color >> 8);
    SPI.transfer(color & 0xFF);
  }
  digitalWrite(TFT_CS, HIGH);
}


// 硬件缓冲操作

void clearLineBuffer(uint16_t color)
{
  uint16_t swappedColor = (color >> 8) | (color << 8);
  for (int i = 0; i < TFT_WIDTH * BUFFER_HEIGHT; i++) {
    lineBuffer[i] = swappedColor;
  }
}

void drawPixelToBuffer(int x, int y, uint16_t color)
{
  if (x >= 0 && x < TFT_WIDTH && y >= 0 && y < BUFFER_HEIGHT) {
    uint16_t swappedColor = (color >> 8) | (color << 8);
    lineBuffer[y * TFT_WIDTH + x] = swappedColor;
  }
}

void pushLineBuffer(int y_screen)
{
  setWindow(0, y_screen, TFT_WIDTH, BUFFER_HEIGHT);
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  SPI.writeBytes((uint8_t*)lineBuffer, TFT_WIDTH * BUFFER_HEIGHT * 2);
  digitalWrite(TFT_CS, HIGH);
}


// 2倍插值字库放大渲染

const uint8_t* getFont(uint16_t code)
{
  for (int i = 0; i < font16_count; i++) {
    if (pgm_read_word(&font16[i].code) == code) {
      return font16[i].data;
    }
  }
  return NULL;
}

void drawChinese32ToBuffer(int x, int y, uint16_t code, uint16_t color)
{
  const uint8_t *font = getFont(code);
  if (font == NULL) return;

  for (int row = 0; row < 16; row++) {
    for (int col = 0; col < 16; col++) {
      int index = row * 2 + col / 8;
      int bit = 7 - (col % 8);
      
      if (font[index] & (1 << bit)) {
        int px = x + col * 2;
        int py = y + row * 2;
        drawPixelToBuffer(px,     py,     color);
        drawPixelToBuffer(px + 1, py,     color);
        drawPixelToBuffer(px,     py + 1, color);
        drawPixelToBuffer(px + 1, py + 1, color);
      }
    }
  }
}


// UTF8解析

uint16_t utf8ToUnicode(const char *s, int &len)
{
  uint8_t c = s[0];
  if (c < 0x80) {
    len = 1;
    return c;
  }
  if ((c & 0xE0) == 0xC0) {
    len = 2;
    return ((c & 0x1F) << 6) | (s[1] & 0x3F);
  }
  if ((c & 0xF0) == 0xE0) {
    len = 3;
    return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
  }
  len = 1;
  return 0;
}

void drawTextUTF8ToBuffer(int x, int y, const char *text, uint16_t color)
{
  while (*text) {
    int len;
    uint16_t code = utf8ToUnicode(text, len);
    if (code < 128) {
      drawChinese32ToBuffer(x, y, code, color);
      x += ASCII_STEP; 
    } else {
      drawChinese32ToBuffer(x, y, code, color);
      x += CJK_STEP;   
    }
    text += len;
  }
}

int getTextWidth(const char *text)
{
  int width = 0;
  while (*text) {
    int len;
    uint16_t code = utf8ToUnicode(text, len);
    if (code < 128) {
      width += ASCII_STEP; 
    } else {
      width += CJK_STEP;   
    }
    text += len;
  }
  return width;
}


// 核心逻辑：统一字符解析器

void parseIncomingChar(char c)
{
  if (c == '\n' || c == '\r') {
    if (rxBuffer.length() > 0) {
      strncpy(lyrics, rxBuffer.c_str(), sizeof(lyrics) - 1);
      lyrics[sizeof(lyrics) - 1] = '\0'; 
      
      lyricsWidth = getTextWidth(lyrics);
      
      staticDrawn = false; 
      if (lyricsWidth <= TFT_WIDTH) {
        scrollState = STATE_STATIC;
        scrollX = (TFT_WIDTH - lyricsWidth) / 2; 
      } else {
        scrollState = STATE_PAUSE_START;
        scrollX = 0;                             
        stateTimer = millis();                   
      }
      
      rxBuffer = ""; 
    }
  } else {
    if (rxBuffer.length() < sizeof(lyrics) - 1) {
      rxBuffer += c;
    }
  }
}

// 核心逻辑：同时轮询 蓝牙 和 USB硬件串口
void checkIncomingData()
{
  // 1. 读蓝牙
  while (SerialBT.available()) {
    char c = SerialBT.read();
    parseIncomingChar(c);
  }
  
  // 2. 读 USB 硬件串口
  while (Serial.available()) {
    char c = Serial.read();
    parseIncomingChar(c);
  }
}


// 主程序

void setup()
{
  // 开启硬件串口（USB通道），波特率设为 115200
  Serial.begin(115200);

  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);

  digitalWrite(TFT_CS, HIGH);

  SPI.begin(18, -1, 23, 5);
  SPI.beginTransaction(
    SPISettings(24000000, MSBFIRST, SPI_MODE0)
  );

  initLCD();
  fillColor(0x0000); 

  // 初始化蓝牙并设置自动确认回调
  SerialBT.begin("开发板");
  SerialBT.onConfirmRequest(btConfirmCallback); 

  // 初始化首次显示的歌词宽度与状态
  lyricsWidth = getTextWidth(lyrics);
  if (lyricsWidth <= TFT_WIDTH) {
    scrollState = STATE_STATIC;
    scrollX = (TFT_WIDTH - lyricsWidth) / 2;
  } else {
    scrollState = STATE_PAUSE_START;
    scrollX = 0;                             
    stateTimer = millis();                   
  }
}

void loop()
{
  // 轮询接收数据
  checkIncomingData();

  unsigned long now = millis();
  bool needRefresh = false;
  const int y_center = 22;

  if (scrollState == STATE_STATIC) {
    if (!staticDrawn) {
      clearLineBuffer(0x0000);
      drawTextUTF8ToBuffer(scrollX, 0, lyrics, 0xFFFF);
      pushLineBuffer(y_center);
      staticDrawn = true;
    }
  }
  else {
    switch (scrollState) {
      case STATE_PAUSE_START:
        if (now - stateTimer >= pauseTime) {
          scrollState = STATE_SCROLLING;
          lastUpdate = now;
        }
        needRefresh = true;
        break;

      case STATE_SCROLLING:
        if (now - lastUpdate >= frameDelayMs) {
          lastUpdate = now;
          scrollX -= scrollStep;

          if (scrollX <= TFT_WIDTH - lyricsWidth - 20) {
            scrollState = STATE_PAUSE_END;
            stateTimer = now;
          }
          needRefresh = true;
        }
        break;

      case STATE_PAUSE_END:
        if (now - stateTimer >= pauseTime) {
          scrollState = STATE_PAUSE_START;
          scrollX = 0;
          stateTimer = now;
        }
        needRefresh = true;
        break;
        
      default:
        break;
    }

    if (needRefresh) {
      clearLineBuffer(0x0000);
      drawTextUTF8ToBuffer(scrollX, 0, lyrics, 0xFFFF);
      pushLineBuffer(y_center); 
    }
  }
}
