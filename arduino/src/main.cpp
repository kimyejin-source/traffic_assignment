#include <TaskScheduler.h> // TaskScheduler 라이브러리 포함
#include <Arduino.h> // Arduino 라이브러리 포함
#include <PinChangeInterrupt.h> // PinChangeInterrupt 라이브러리 포함
#define DEBOUNCE_DELAY 50  // 디바운스 지연 시간 (밀리초)

// 신호등 주기 (밀리초)
int RED_TIME = 2000; // 빨간불 시간
int YELLOW_TIME = 500; // 노란불 시간
int GREEN_TIME = 2000; // 초록불 시간
const int GREEN_BLINK = 1000; // 초록불 깜빡임 시간
const int GREEN_BLINK_INTERVAL = 166; // 초록불 깜빡임 간격

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
bool redState = false; // 빨간불 상태
bool yellowState = false; // 노란불 상태
bool greenState = false; // 초록불 상태
volatile unsigned long lastPressTime1 = 0; // 1번 버튼 마지막 눌린 시간
volatile unsigned long lastPressTime2 = 0; // 2번 버튼 마지막 눌린 시간
volatile unsigned long lastPressTime3 = 0; // 3번 버튼 마지막 눌린 시간

// 함수 선언
bool redOE(); // 빨간불 OnEnable 함수
void redOD(); // 빨간불 OnDisable 함수
bool yellowOE(); // 노란불 OnEnable 함수
void yellowOD(); // 노란불 OnDisable 함수
void yellowOD_2(); // 노란불 OnDisable 함수 (다른 용도)
bool greenOE(); // 초록불 OnEnable 함수
void greenOD(); // 초록불 OnDisable 함수
bool greenBlinkOE(); // 초록불 깜빡임 OnEnable 함수
void greenBlinkCB(); // 초록불 깜빡임 콜백 함수
void greenBlinkOD(); // 초록불 깜빡임 OnDisable 함수
void allBlinkCB(); // 모든 불 깜빡임 콜백 함수
void allBlinkOD(); // 모든 불 깜빡임 OnDisable 함수
void buttonISR1(); // 1번 버튼 인터럽트 서비스 루틴
void buttonISR2(); // 2번 버튼 인터럽트 서비스 루틴
void buttonISR3(); // 3번 버튼 인터럽트 서비스 루틴
void parseSerialData(String data); // 시리얼 데이터 파싱 함수

// 스케줄러 객체 생성
Scheduler ts;

// Task 객체 생성
Task red(RED_TIME, TASK_ONCE, NULL, &ts, false, redOE, redOD); // 빨간불 Task
Task yellow(YELLOW_TIME, TASK_ONCE, NULL, &ts, false, yellowOE, yellowOD); // 노란불 Task
Task green(GREEN_TIME, TASK_ONCE, NULL, &ts, false, greenOE, greenOD); // 초록불 Task
Task greenBlink(GREEN_BLINK_INTERVAL, GREEN_BLINK / (GREEN_BLINK_INTERVAL), greenBlinkCB, &ts, false, greenBlinkOE, greenBlinkOD); // 초록불 깜빡임 Task
Task yellow_2(YELLOW_TIME, TASK_ONCE, NULL, &ts, false, yellowOE, yellowOD_2); // 노란불 Task (다른 용도)
Task allBlink(GREEN_BLINK_INTERVAL, TASK_FOREVER, allBlinkCB, &ts, false, NULL, allBlinkOD); // 모든 불 깜빡임 Task
Task ledControl(100, TASK_FOREVER, []() { // LED 제어 Task
  // LED 밝기 제어
  analogWrite(11, redState ? ledBrightness : 0); // 빨간불 밝기 제어
  analogWrite(10, yellowState ? ledBrightness : 0); // 노란불 밝기 제어
  analogWrite(9, greenState ? ledBrightness : 0); // 초록불 밝기 제어

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
  pinMode(11, OUTPUT); // 빨간불 핀 설정
  pinMode(10, OUTPUT); // 노란불 핀 설정
  pinMode(9, OUTPUT); // 초록불 핀 설정
  pinMode(BUTTON1_PIN, INPUT_PULLUP); // 1번 버튼 핀 설정
  pinMode(BUTTON2_PIN, INPUT_PULLUP); // 2번 버튼 핀 설정
  pinMode(BUTTON3_PIN, INPUT_PULLUP); // 3번 버튼 핀 설정

  // 인터럽트 설정
  attachPCINT(digitalPinToPCINT(BUTTON1_PIN), buttonISR1, FALLING); // 1번 버튼 인터럽트 설정
  attachPCINT(digitalPinToPCINT(BUTTON2_PIN), buttonISR2, FALLING); // 2번 버튼 인터럽트 설정
  attachPCINT(digitalPinToPCINT(BUTTON3_PIN), buttonISR3, FALLING); // 3번 버튼 인터럽트 설정

  red.restartDelayed(); // 빨간불 Task 지연 시작
}

void loop() {
  potValue = analogRead(POT_PIN); // 가변저항 값 읽기
  ledBrightness = map(potValue, 0, 1023, 0, 255); // 가변저항 값을 LED 밝기로 매핑

  // 시리얼 데이터 읽기
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n'); // 개행 문자 기준으로 시리얼 데이터 읽기
    parseSerialData(data); // 시리얼 데이터 파싱
  }

  ts.execute(); // 스케줄러 실행
}

void parseSerialData(String data) {
  data.trim(); // 데이터 앞뒤 공백 제거

  if (data.length() == 0) return; // 빈 데이터 방지

  int spaceIndex = data.indexOf(' '); // 공백 위치 찾기
  if (spaceIndex == -1) return; // 공백이 없으면 처리 안 함

  String command = data.substring(0, spaceIndex); // 명령어 추출
  int value = data.substring(spaceIndex + 1).toInt(); // 값 추출 및 정수 변환

  if (command == "RED_DURATION") {
    RED_TIME = value; // 빨간불 시간 설정
    red.setInterval(RED_TIME); // 빨간불 Task 간격 설정
    
  } else if (command == "YELLOW_DURATION") {
    YELLOW_TIME = value; // 노란불 시간 설정
    yellow.setInterval(YELLOW_TIME); // 노란불 Task 간격 설정
  } else if (command == "GREEN_DURATION") {
    GREEN_TIME = value; // 초록불 시간 설정
    green.setInterval(GREEN_TIME); // 초록불 Task 간격 설정
  }
}

void buttonISR1() {
  unsigned long currentTime = millis(); // 현재 시간 읽기
  if (currentTime - lastPressTime1 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime1 = currentTime; // 마지막 눌린 시간 갱신

  if(!firstFunctionOn) { // 1번 기능이 꺼져있으면
    Serial.println("MODE1"); // 디버깅 메시지 출력
    red.abort(); // 빨간불 Task 정지
    yellow.abort(); // 노란불 Task 정지
    green.abort(); // 초록불 Task 정지
    greenBlink.abort(); // 초록불 깜빡임 Task 정지
    yellow_2.abort(); // 노란불 Task 정지 (다른 용도)
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    redState = true; // 빨간불 켜기
    yellowState = false; // 노란불 끄기
    greenState = false; // 초록불 끄기
    basicFunctionOn = false; // 기본동작 비활성화
  }
  else { // 1번 기능이 켜져있으면
    Serial.println("MODE0"); // 디버깅 메시지 출력
    red.abort(); // 빨간불 Task 정지
    yellow.abort(); // 노란불 Task 정지
    green.abort(); // 초록불 Task 정지
    greenBlink.abort(); // 초록불 깜빡임 Task 정지
    yellow_2.abort(); // 노란불 Task 정지 (다른 용도)
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    redState = false; // 빨간불 끄기
    red.restartDelayed(); // 빨간불 Task 지연 시작
    basicFunctionOn = true; // 기본동작 활성화
  }
  firstFunctionOn = !firstFunctionOn; // 1번 동작 상태 변화 명시
}

void buttonISR2() {
  unsigned long currentTime = millis(); // 현재 시간 읽기
  if (currentTime - lastPressTime2 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime2 = currentTime; // 마지막 눌린 시간 갱신

  if(!secondFunctionOn) { // 2번 기능이 꺼져있으면
    Serial.println("MODE2"); // 디버깅 메시지 출력
    red.abort(); // 빨간불 Task 정지
    yellow.abort(); // 노란불 Task 정지
    green.abort(); // 초록불 Task 정지
    greenBlink.abort(); // 초록불 깜빡임 Task 정지
    yellow_2.abort(); // 노란불 Task 정지 (다른 용도)
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    allBlink.restart(); // 모든 불 깜빡임 Task 재시작
    basicFunctionOn = false; // 기본동작 비활성화
  } else { // 2번 기능이 켜져있으면
    Serial.println("MODE0"); // 디버깅 메시지 출력
    red.abort(); // 빨간불 Task 정지
    yellow.abort(); // 노란불 Task 정지
    green.abort(); // 초록불 Task 정지
    greenBlink.abort(); // 초록불 깜빡임 Task 정지
    yellow_2.abort(); // 노란불 Task 정지 (다른 용도)
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    redState = false; // 빨간불 끄기
    yellowState = false; // 노란불 끄기
    greenState = false; // 초록불 끄기
    red.restartDelayed(); // 빨간불 Task 지연 시작
    basicFunctionOn = true; // 기본동작 활성화
  }
  secondFunctionOn = !secondFunctionOn; // 2번 동작 상태 변화 명시
}

void buttonISR3() {
  unsigned long currentTime = millis(); // 현재 시간 읽기
  if (currentTime - lastPressTime3 < DEBOUNCE_DELAY) return; // 디바운스 처리
  lastPressTime3 = currentTime; // 마지막 눌린 시간 갱신

  if (basicFunctionOn) { // 기본기능 중이면
    Serial.println("MODE3"); // 디버깅 메시지 출력
    red.abort(); // 빨간불 Task 정지
    yellow.abort(); // 노란불 Task 정지
    green.abort(); // 초록불 Task 정지
    greenBlink.abort(); // 초록불 깜빡임 Task 정지
    yellow_2.abort(); // 노란불 Task 정지 (다른 용도)
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    redState = false; // 빨간불 끄기
    yellowState = false; // 노란불 끄기
    greenState = false; // 초록불 끄기
  }
  else { // 기본기능 꺼져있으면(1번 켜져있거나 2번 켜져있는 경우도 포함함)
    Serial.println("MODE0"); // 디버깅 메시지 출력
    digitalWrite(11, LOW); // 빨간불 끄기
    allBlink.abort(); // 모든 불 깜빡임 Task 정지
    red.restartDelayed(); // 빨간불 Task 지연 시작
  }
  basicFunctionOn = !basicFunctionOn; // 기본동작 상태 변화 명시  
}

bool redOE() { 
  redState = true; // 빨간불 켜기
  return true;
}

void redOD() {
  redState = false; // 빨간불 끄기
  yellow.restartDelayed(); // 노란불 Task 지연 시작
}

bool yellowOE() {
  yellowState = true; // 노란불 켜기
  return true;
}

void yellowOD() {
  yellowState = false; // 노란불 끄기
  green.restartDelayed(); // 초록불 Task 지연 시작
}

void yellowOD_2() {
  yellowState = false; // 노란불 끄기
  red.restartDelayed(); // 빨간불 Task 지연 시작
}

bool greenOE() {
  greenState = true; // 초록불 켜기
  return true;
}

void greenOD() {
  greenState = false; // 초록불 끄기
  greenBlink.restartDelayed(); // 초록불 깜빡임 Task 지연 시작
}

bool greenBlinkOE() {
  greenState = false; // 초록불 끄기
  return true;
}

void greenBlinkCB()  {
  greenState = !greenState; // 초록불 상태 반전
}

void greenBlinkOD() {
  greenState = false; // 초록불 끄기
  yellow_2.restartDelayed(); // 노란불 Task 지연 시작 (다른 용도)
}

bool allState; // 모든 불 상태
void allBlinkCB() {
  allState = !allState; // 모든 불 상태 반전
  redState = allState; // 빨간불 상태 설정
  yellowState = allState; // 노란불 상태 설정
  greenState = allState; // 초록불 상태 설정
}

void allBlinkOD() {
  redState = false; // 빨간불 끄기
  yellowState = false; // 노란불 끄기
  greenState = false; // 초록불 끄기
}