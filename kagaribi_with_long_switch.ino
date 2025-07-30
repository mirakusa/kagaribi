#include "FastLED.h"

// ピン定義
#define DATA_PIN_1    4
#define DATA_PIN_2    5
#define DATA_PIN_3    6
#define SWITCH_PIN    7

#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS 15
#define lighting_PER_SECOND 10

CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];

CRGBPalette16 gPal;

int BRIGHTNESS = 255;
int scaleVol = 240;       
int SPARKING = 120;
int COOLING = 25;
bool gReverseDirection = false;

// トグル制御用
bool ledOn = true;
bool lastButtonState = HIGH;

// 長押し検出用
unsigned long buttonPressStartTime = 0;
bool buttonPressDetected = false;
const unsigned long LONG_PRESS_TIME = 500; // 0.5秒

// フェード制御用
uint8_t currentBrightness = 220;  // 現在の明るさ
uint8_t targetBrightness = 220;   // 目標の明るさ
bool isFading = false;            // フェード中かどうか

struct FlameData {
  uint8_t heat[NUM_LEDS];
  uint8_t flameHeight;
  uint8_t targetHeight;
  uint8_t heightTimer;
  uint8_t phaseOffset;
};

FlameData flame1, flame2, flame3;

void setup() {
  delay(1000);
  
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  FastLED.addLeds<LED_TYPE, DATA_PIN_1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN_2, COLOR_ORDER>(leds2, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN_3, COLOR_ORDER>(leds3, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  FastLED.setBrightness(BRIGHTNESS);
  
  flame1.flameHeight = 12;
  flame1.targetHeight = 12;
  flame1.heightTimer = 0;
  flame1.phaseOffset = 0;
  
  flame2.flameHeight = 10;
  flame2.targetHeight = 14;
  flame2.heightTimer = 17;
  flame2.phaseOffset = 85;
  
  flame3.flameHeight = 14;
  flame3.targetHeight = 10;
  flame3.heightTimer = 33;
  flame3.phaseOffset = 170;
}

void processFlame(FlameData &flame, CRGB leds[]) {
  flame.heightTimer++;
  // タイマーの上限を短縮（50から30に）して、高さの変更頻度を増加
  if (flame.heightTimer > 30 + flame.phaseOffset / 8) {
    flame.heightTimer = 0;
    // 高さの範囲を拡大（8〜18から5〜20に）
    flame.targetHeight = random8(5, 20);
  }
  
  // 高さ変化の速度を向上（30%から50%に）
  if (flame.flameHeight < flame.targetHeight) {
    if (random8(100) < 50) flame.flameHeight++;
  } else if (flame.flameHeight > flame.targetHeight) {
    if (random8(100) < 50) flame.flameHeight--;
  }
  
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i >= flame.flameHeight) {
      flame.heat[i] = qsub8(flame.heat[i], random8(40, 80));
    } else {
      flame.heat[i] = qsub8(flame.heat[i], random8(0, COOLING));
    }
  }
  
  for (int k = NUM_LEDS - 1; k >= 2; k--) {
    if (k < flame.flameHeight) {
      flame.heat[k] = (flame.heat[k - 1] + flame.heat[k - 2] * 2) / 3;
      if (random8(100 + flame.phaseOffset) < 10) {
        flame.heat[k] = qadd8(flame.heat[k], random8(10, 30));
      }
    }
  }
  
  if (random8() < SPARKING + flame.phaseOffset / 10) {
    int pos = random8(3);
    flame.heat[pos] = qadd8(flame.heat[pos], random8(160, 200));
  }
  
  for (int i = 0; i < 3; i++) {
    if (flame.heat[i] < 80) {
      flame.heat[i] = 80 + random8(20);
    }
  }
  
  for (int j = 0; j < NUM_LEDS; j++) {
    uint8_t colorindex = scale8(flame.heat[j], scaleVol);
    if (random8(100) < 5) {
      colorindex = qadd8(colorindex, random8(10));
    }
    
    CRGB color = ColorFromPalette(gPal, colorindex);
    
    if (j > flame.flameHeight) {
      color.fadeToBlackBy(200);
    } else if (j == flame.flameHeight) {
      if (random8(100) < 50) {
        color.fadeToBlackBy(random8(0, 128));
      }
    }
    
    leds[j] = color;
  }
  
  if (random8(100 + flame.phaseOffset / 2) < 20 && flame.flameHeight < NUM_LEDS - 1) {
    leds[flame.flameHeight] = CRGB(255, 40, 0);
  }
}

void updateFade() {
  if (currentBrightness != targetBrightness) {
    isFading = true;
    
    // フェード速度（値を変更することで速度調整可能）
    int fadeStep = 10;
    
    if (currentBrightness < targetBrightness) {
      currentBrightness = min(currentBrightness + fadeStep, targetBrightness);
    } else {
      currentBrightness = max(currentBrightness - fadeStep, targetBrightness);
    }
    
    FastLED.setBrightness(currentBrightness);
    
    if (currentBrightness == targetBrightness) {
      isFading = false;
    }
  }
}

void loop() {
  // スイッチチェック
  int buttonState = digitalRead(SWITCH_PIN);
  
  // ボタンが押された瞬間を検出
  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressStartTime = millis();
    buttonPressDetected = false;
  }
  
  // ボタンが押されている間
  if (buttonState == LOW && !buttonPressDetected && !isFading) {
    // 長押し時間をチェック
    if ((millis() - buttonPressStartTime) >= LONG_PRESS_TIME) {
      buttonPressDetected = true;
      ledOn = !ledOn;
      digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
      
      // フェードの目標値を設定
      if (ledOn) {
        targetBrightness = BRIGHTNESS;  // フェードイン
      } else {
        targetBrightness = 0;  // フェードアウト
      }
    }
  }
  
  // ボタンが離された時
  if (buttonState == HIGH && lastButtonState == LOW) {
    buttonPressDetected = false;
  }
  
  lastButtonState = buttonState;
  
  // フェード処理
  updateFade();
  
  // 完全に消灯している場合は炎の処理をスキップ
  if (currentBrightness == 0) {
    fill_solid(leds1, NUM_LEDS, CRGB::Black);
    fill_solid(leds2, NUM_LEDS, CRGB::Black);
    fill_solid(leds3, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(20);
    return;
  }
  
  // 炎エフェクト
  gPal = CRGBPalette16(
    CRGB::Black,
    CRGB(20, 0, 0),
    CRGB(40, 0, 0),
    CRGB(80, 0, 0),
    CRGB(120, 0, 0),
    CRGB(180, 0, 0),
    CRGB(255, 0, 0),
    CRGB(255, 20, 0),
    CRGB(255, 40, 0),
    CRGB(255, 60, 0),
    CRGB(200, 20, 0),
    CRGB(160, 0, 0),
    CRGB(100, 0, 0),
    CRGB(60, 0, 0),
    CRGB(30, 0, 0),
    CRGB::Black
  );
  
  processFlame(flame1, leds1);
  processFlame(flame2, leds2);
  processFlame(flame3, leds3);
  
  random16_add_entropy(random());
  FastLED.show();
  FastLED.delay(1000 / lighting_PER_SECOND);
}
