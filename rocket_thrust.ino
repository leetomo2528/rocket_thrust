/*
 * rocket_thrust.ino — KNSB 고체연료 추력 측정
 * 보드: Arduino Uno (+ HC-05 블루투스)
 * 센서: 10kg 로드셀 + HX711 (DOUT=D2, SCK=D3)
 * BT:   HC-05 SoftwareSerial (RX=D10, TX=D11)
 * 측정: START 명령 후 20초 고정, 100Hz 샘플링, CSV 시리얼+BT 출력
 *
 * 명령어 (CR/LF 없이 그대로 전송):
 *   TARE        영점 잡기 (측정 전 1회)
 *   START       20초 측정 시작
 *   STOP        측정 강제 중단
 *   CAL <val>   scale 보정값 설정 (단위: count/kg, 기본 2280.f)
 *   STATUS      현재 상태/영점 출력
 */

#include <HX711.h>
#include <SoftwareSerial.h>

// ===== 핀 배정 =====
#define HX_DOUT    2
#define HX_SCK     3
#define BT_RX     10   // HC-05 TX → Uno D10
#define BT_TX     11   // HC-05 RX ← Uno D11 (5V → 3.3V 분압 권장)
#define LED_PIN   13

// ===== 측정 파라미터 =====
#define DURATION_MS    20000UL   // 20초 고정 측정
#define SAMPLE_MS      10        // 100Hz (10ms)
#define GRAVITY        9.80665f  // kg→N 변환용 (출력은 kg 단위, N 변환은 PC 측에서)
#define DEFAULT_SCALE  2280.0f   // 추정 scale (10kg 로드셀, 알려진 추로 보정 필요)

HX711 scale;
SoftwareSerial bt(BT_RX, BT_TX);

enum State { IDLE, RUNNING, DONE };
State state = IDLE;

unsigned long runStart = 0;
unsigned long lastSample = 0;
float calibScale = DEFAULT_SCALE;
unsigned long sampleCount = 0;

// ===== 버퍼 (BT 전송률 한계 대비) =====
const size_t BUF_SZ = 64;
char rxBuf[BUF_SZ];
size_t rxLen = 0;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  bt.begin(9600);  // HC-05 기본 9600bps

  if (!scale.begin(HX_DOUT, HX_SCK)) {
    Serial.println(F("[ERR] HX711 not found"));
    bt.println(F("[ERR] HX711 not found"));
    while (1) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(200); }
  }
  scale.set_scale(calibScale);
  scale.tare();   // 전원 인가 후 5~10초 후에 다시 TARE 권장
  Serial.println(F("[OK] HX711 ready, tared"));
  bt.println(F("[OK] HX711 ready, tared"));
  Serial.println(F("[INFO] Commands: TARE|START|STOP|CAL<val>|STATUS"));
  printPrompt();
}

void loop() {
  // 명령 처리
  readBT();

  // 샘플링
  if (state == RUNNING) {
    unsigned long now = millis();
    if (now - lastSample >= SAMPLE_MS) {
      lastSample = now;

      // HX711은 ~80Hz 최대. 100Hz 요청해도 실제 ~80Hz로 샘플.
      // read() 블로킹 (약 100us~10ms depending on DOUT)
      float kg = scale.get_units(1);   // 1회 평균
      unsigned long t = now - runStart;

      // CSV: time_ms,thrust_kg
      Serial.print(t); Serial.print(','); Serial.println(kg, 5);
      bt.print(t); bt.print(','); bt.println(kg, 5);
      sampleCount++;
    }

    if (millis() - runStart >= DURATION_MS) {
      state = DONE;
      digitalWrite(LED_PIN, HIGH);  // 완료 LED 점등
      Serial.print(F("[DONE] 20s, samples=")); Serial.println(sampleCount);
      bt.print(F("[DONE] 20s, samples=")); bt.println(sampleCount);
      printPrompt();
    }
  }
}

// ===== BT/Serial 명령 파싱 =====
void readBT() {
  while (bt.available()) {
    char c = bt.read();
    if (c == '\r' || c == '\n') {
      if (rxLen > 0) { rxBuf[rxLen] = 0; handleCmd(rxBuf); rxLen = 0; }
      continue;
    }
    if (rxLen < BUF_SZ - 1) rxBuf[rxLen++] = c;
    else { rxBuf[rxLen] = 0; handleCmd(rxBuf); rxLen = 0; }
  }
  // USB 시리얼도 동일하게
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (rxLen > 0) { rxBuf[rxLen] = 0; handleCmd(rxBuf); rxLen = 0; }
      continue;
    }
    if (rxLen < BUF_SZ - 1) rxBuf[rxLen++] = c;
    else { rxBuf[rxLen] = 0; handleCmd(rxBuf); rxLen = 0; }
  }
}

void handleCmd(const char* cmd) {
  String s = String(cmd);
  s.trim();
  s.toUpperCase();

  if (s == "TARE") {
    scale.tare();
    sampleCount = 0;
    Serial.println(F("[OK] Tare done"));
    bt.println(F("[OK] Tare done"));
  }
  else if (s.startsWith("CAL ")) {
    float v = s.substring(4).toFloat();
    if (v > 0) {
      calibScale = v;
      scale.set_scale(calibScale);
      Serial.print(F("[OK] Scale=")); Serial.println(calibScale, 4);
      bt.print(F("[OK] Scale=")); bt.println(calibScale, 4);
    } else { Serial.println(F("[ERR] CAL value invalid")); bt.println(F("[ERR] CAL value invalid")); }
  }
  else if (s == "START") {
    if (state == RUNNING) {
      Serial.println(F("[WARN] Already running"));
      bt.println(F("[WARN] Already running"));
    } else {
      state = RUNNING;
      runStart = millis();
      lastSample = runStart - SAMPLE_MS;  // 즉시 첫 샘플
      sampleCount = 0;
      digitalWrite(LED_PIN, LOW);
      Serial.println(F("[OK] START 20s thrust capture @100Hz"));
      bt.println(F("[OK] START 20s thrust capture @100Hz"));
      Serial.println(F("time_ms,thrust_kg"));
      bt.println(F("time_ms,thrust_kg"));
    }
  }
  else if (s == "STOP") {
    state = IDLE;
    digitalWrite(LED_PIN, HIGH);
    Serial.print(F("[STOP] samples=")); Serial.println(sampleCount);
    bt.print(F("[STOP] samples=")); bt.println(sampleCount);
    printPrompt();
  }
  else if (s == "STATUS") {
    Serial.print(F("[STATUS] state="));
    bt.print(F("[STATUS] state="));
    const char* st = (state==IDLE)?"IDLE":(state==RUNNING)?"RUNNING":"DONE";
    Serial.print(st); Serial.print(F(" scale=")); Serial.println(calibScale, 4);
    bt.print(st); bt.print(F(" scale=")); bt.println(calibScale, 4);
    if (state == RUNNING) {
      unsigned long remain = (DURATION_MS - (millis() - runStart)) / 1000;
      Serial.print(F("[STATUS] remain_s=")); Serial.println(remain);
      bt.print(F("[STATUS] remain_s=")); bt.println(remain);
    }
  }
  else {
    Serial.print(F("[ERR] Unknown: ")); Serial.println(cmd);
    bt.print(F("[ERR] Unknown: ")); bt.println(cmd);
  }
}

void printPrompt() {
  Serial.println(F("rocket_thrust>"));
  bt.println(F("rocket_thrust>"));
}