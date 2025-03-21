#include <TaskScheduler.h>
#include <Arduino.h>
#include <PinChangeInterrupt.h>
#define DEBOUNCE_DELAY 50  // 디바운스 지연 시간 (밀리초)

// 신호등 주기 (밀리초)
int RED_TIME = 2000;
int YELLOW_TIME = 500;
int GREEN_TIME = 2000;
const int GREEN_BLINK = 1000;
const int GREEN_BLINK_INTERVAL = 166;

// 핀 번호 설정
const int BUTTON1_PIN = 2; // 1번 버튼 핀
const int BUTTON2_PIN = 3; // 2번 버튼 핀
const int BUTTON3_PIN = 4; // 3번 버튼 핀
const int POT_PIN = A0;    // 가변저항 핀

// 상태 변수
bool basicFunctionOn = true; // 기본 기능의 동작 활성화 상태 변수
bool firstFunctionOn = false; // 1번 스위치 동작 활성화 상태 변수
bool secondFunctionOn = false; // 2번 스위치 동작 활성화 상태 변수
int potValue = 0;          // 가변저항 값
int ledBrightness = 0;     // LED 밝기
bool redState = false;
bool yellowState = false;
bool greenState = false;
volatile unsigned long lastPressTime1 = 0;
volatile unsigned long lastPressTime2 = 0;
volatile unsigned long lastPressTime3 = 0;

// 함수 선언
bool redOE();
void redOD();
bool yellowOE();
void yellowOD();
void yellowOD_2();
bool greenOE();
void greenOD();
bool greenBlinkOE();
void greenBlinkCB();
void greenBlinkOD();
void allBlinkCB();
void allBlinkOD();
void buttonISR1(); // ISR 함수 선언
void buttonISR2(); // ISR 함수 선언
void buttonISR3(); // ISR 함수 선언
void parseSerialData(String data); // p5에서 변경된 주기를 반영하는 함수

// 스케줄러 객체 생성
Scheduler ts;

// Task 객체 생성
Task red(RED_TIME, TASK_ONCE, NULL, &ts, false, redOE, redOD);
Task yellow(YELLOW_TIME, TASK_ONCE, NULL, &ts, false, yellowOE, yellowOD);
Task green(GREEN_TIME, TASK_ONCE, NULL, &ts, false, greenOE, greenOD);
Task greenBlink(GREEN_BLINK_INTERVAL, GREEN_BLINK / (GREEN_BLINK_INTERVAL), greenBlinkCB, &ts, false, greenBlinkOE, greenBlinkOD);
Task yellow_2(YELLOW_TIME, TASK_ONCE, NULL, &ts, false, yellowOE, yellowOD_2);
Task allBlink(GREEN_BLINK_INTERVAL, TASK_FOREVER, allBlinkCB, &ts, false, NULL, allBlinkOD);
Task ledControl(100, TASK_FOREVER, []() {
  // LED 밝기 제어
  analogWrite(11, redState ? ledBrightness : 0);
  analogWrite(10, yellowState ? ledBrightness : 0);
  analogWrite(9, greenState ? ledBrightness : 0);

  // 시리얼로 상태 전송
  Serial.print("RED_TIME ");
  Serial.print(RED_TIME);
  Serial.print(" "); // 띄어쓰기 추가
  Serial.print("YELLOW_TIME ");
  Serial.print(YELLOW_TIME);
  Serial.print(" "); // 띄어쓰기 추가
  Serial.print("GREEN_TIME ");
  Serial.print(GREEN_TIME);
  Serial.print(" ");
  Serial.print(redState ? "RED " : "");
  Serial.print(yellowState ? "YELLOW " : "");
  Serial.print(greenState ? "GREEN " : "");
  Serial.print("BRIGHTNESS ");
  Serial.println(ledBrightness); // 가변저항 값 출력
}, &ts, true);

void setup() {  
  Serial.begin(9600); // 시리얼 통신 초기화
  pinMode(11, OUTPUT); // R 핀 설정
  pinMode(10, OUTPUT); // Y 핀 설정
  pinMode(9, OUTPUT); // G 핀 설정
  pinMode(BUTTON1_PIN, INPUT_PULLUP); // 1번 버튼 핀 설정
  pinMode(BUTTON2_PIN, INPUT_PULLUP); // 2번 버튼 핀 설정
  pinMode(BUTTON3_PIN, INPUT_PULLUP); // 3번 버튼 핀 설정

  // 인터럽트 설정
  attachPCINT(digitalPinToPCINT(BUTTON1_PIN), buttonISR1, FALLING);
  attachPCINT(digitalPinToPCINT(BUTTON2_PIN), buttonISR2, FALLING);
  attachPCINT(digitalPinToPCINT(BUTTON3_PIN), buttonISR3, FALLING);

  red.restartDelayed(); // 바로 기본기능 시작함
}

void loop() {
  potValue = analogRead(POT_PIN);
  ledBrightness = map(potValue, 0, 1023, 0, 255);

  // 시리얼 데이터 읽기
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n'); // 더 안정적으로 개행 문자 기준 읽기
    parseSerialData(data);
  }

  ts.execute();
}

void parseSerialData(String data) {
  data.trim();

  if (data.length() == 0) return; // 빈 데이터 방지

  int spaceIndex = data.indexOf(' '); // 공백 위치 찾기
  if (spaceIndex == -1) return; // 공백이 없으면 처리 안 함

  String command = data.substring(0, spaceIndex);
  int value = data.substring(spaceIndex + 1).toInt();

  if (command == "RED_DURATION") {
    RED_TIME = value;
    red.setInterval(RED_TIME);
    
  } else if (command == "YELLOW_DURATION") {
    YELLOW_TIME = value;
    yellow.setInterval(YELLOW_TIME);
  } else if (command == "GREEN_DURATION") {
    GREEN_TIME = value;
    green.setInterval(GREEN_TIME);

  }
}

void buttonISR1() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime1 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime1 = currentTime;

  if(!firstFunctionOn) { // 1번 기능이 꺼져있으면
    Serial.println("MODE1"); // 디버깅 메시지 출력
    red.abort(); // OnDisable 함수 호출 않고 Task 정지
    yellow.abort();
    green.abort();
    greenBlink.abort();
    yellow_2.abort();
    allBlink.abort();
    redState = true; //1번 기능 동작시키기
    yellowState = false;
    greenState = false;
    basicFunctionOn = false; // 기본동작 비활성화
  }
  else { // 1번 기능이 켜져있으면
    Serial.println("MODE0");
    red.abort(); 
    yellow.abort();
    green.abort();
    greenBlink.abort();
    yellow_2.abort();
    allBlink.abort();
    redState = false; // 1번 기능 중지시키고
    red.restartDelayed(); // 기본동작 재시작
    basicFunctionOn = true; // 기본동작 활성화
  }
  firstFunctionOn = !firstFunctionOn; // 1번 동작 상태 변화 명시
}

void buttonISR2() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime2 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime2 = currentTime;

  if(!secondFunctionOn) { // 2번 기능이 꺼져있으면
    Serial.println("MODE2"); // 디버깅 메시지 출력
    red.abort(); // OnDisable 함수 호출 않고 Task 정지
    yellow.abort();
    green.abort();
    greenBlink.abort();
    yellow_2.abort();
    allBlink.abort();
    allBlink.restart(); // 2번 기능 동작시키기
    basicFunctionOn = false; // 기본동작 비활성화
  } else { // 2번 기능이 켜져있으면
    Serial.println("MODE0");
    red.abort(); 
    yellow.abort();
    green.abort();
    greenBlink.abort();
    yellow_2.abort();
    allBlink.abort(); // 2번 기능 중지시키고
    redState = false; // 모든 LED 끄기
    yellowState = false;
    greenState = false;
    red.restartDelayed(); // 기본동작 재시작
    basicFunctionOn = true; // 기본동작 활성화 명시
  }
  secondFunctionOn = !secondFunctionOn; // 2번 동작 상태 변화 명시
}

void buttonISR3() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime3 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime3 = currentTime;

  if (basicFunctionOn) { // 기본기능 중이면
    Serial.println("MODE3"); // 디버깅 메시지 출력
    red.abort(); // OnDisable 함수 호출 않고 Task 정지
    yellow.abort();
    green.abort();
    greenBlink.abort();
    yellow_2.abort();
    allBlink.abort();
    redState = false;
    yellowState = false;
    greenState = false;
  }
  else { // 기본기능 꺼져있으면(1번 켜져있거나 2번 켜져있는 경우도 포함함)
    Serial.println("MODE0"); // 디버깅 메시지 출력
    digitalWrite(11, LOW);
    allBlink.abort();
    red.restartDelayed(); // 기본기능 재시작
  }
  basicFunctionOn = !basicFunctionOn; // 기본동작 상태 변화 명시  
}

bool redOE() { 
  redState = true;
  return true;
}

void redOD() {
  redState = false;
  yellow.restartDelayed();
}

bool yellowOE() {
  yellowState = true;
  return true;
}

void yellowOD() {
  yellowState = false;
  green.restartDelayed();
}

void yellowOD_2() {
  yellowState = false;
  red.restartDelayed();
}

bool greenOE() {
  greenState = true;
  return true;
}

void greenOD() {
  greenState = false;
  greenBlink.restartDelayed();
}

bool greenBlinkOE() {
  greenState = false;
  return true;
}

void greenBlinkCB()  {
  greenState = !greenState;
}

void greenBlinkOD() {
  greenState = false;
  yellow_2.restartDelayed();
}

bool allState;
void allBlinkCB() {
  allState = !allState;
  redState = allState;
  yellowState = allState;
  greenState = allState;
}

void allBlinkOD() {
  redState = false;
  yellowState = false;
  greenState = false;
}