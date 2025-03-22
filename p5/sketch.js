// 전역 변수 선언
let redSlider, yellowSlider, greenSlider;
let RED_DURATION = 2000, YELLOW_DURATION = 500, GREEN_DURATION = 2000; // 각 신호의 초기 주기
let brightness = 255;
let modeText = "기본모드";
let port, connectBtn;
let redState = false, yellowState = false, greenState = false;
let writer = null;
let reader;
let audioFiles = {}; // 음악 파일 저장 객체
let currentAudio; // 현재 재생 중인 오디오
let volumeSlider; // 볼륨 조절 슬라이더
let songTitle = ""; // 노래 제목을 저장할 변수

function preload() {
  // mp3 파일 미리 로드
  soundFormats('mp3');
  audioFiles = { 
    0: loadSound('sound/How_Do_I_Say_Goodbye-Dean_Lewis.mp3'), // 기본 모드 음악
    1: loadSound('sound/Someone_You_Loved-Lewis_Capaldi.mp3'),   // 1번 모드 음악
    2: loadSound('sound/나의_바람-비투비.mp3'),   // 2번 모드 음악
    3: loadSound('sound/Try_eveything-주토피아OST.mp3')    // 3번 모드 음악
  };
}

function setup() { // 초기 상태 설정. 한 번만 실행된다.
  console.log("setup() 실행됨"); // setup() 실행되는지 확인 --- 잘 됨
  createCanvas(800, 600); // 캔버스 생성
  background(200); // 배경색 설정
  port = createSerial(); // WebSerial 객체 생성

  // 연결 버튼 생성
  connectBtn = createButton("Connect to Arduino");
  connectBtn.position(365, 80);
  connectBtn.mousePressed(connectBtnClick);

  // 슬라이더 생성
  redSlider = createSlider(500, 5000, RED_DURATION, 100); // 주기를 0.5초에서 5초 사이로 설정
  redSlider.position(140, 550);
  yellowSlider = createSlider(500, 5000, YELLOW_DURATION, 100);
  yellowSlider.position(340, 550);
  greenSlider = createSlider(500, 5000, GREEN_DURATION, 100);
  greenSlider.position(540, 550);
  
  // 볼륨 조절 슬라이더 생성
  volumeSlider = createSlider(0, 1, 0.5, 0.01); // 0 ~ 1 사이 값, 초기값 0.5
  volumeSlider.position(30, 170);
  volumeSlider.style('transform', 'rotate(-90deg)'); // 세로로 표시
  volumeSlider.input(updateVolume);

  // 슬라이더 값이 변경될 때마다 sendSliderValues() 함수를 자동으로 호출함
  redSlider.input(sendSliderValues); 
  yellowSlider.input(sendSliderValues);
  greenSlider.input(sendSliderValues);
}

function draw() {
  background(220); // 배경색 설정
  console.log("draw() 실행됨");
  brightnessupdate(); // 밝기 텍스트 업데이트
  updateModeText(); // 모드 텍스트 업데이트
  drawTrafficLight(); // 신호등 UI 업데이트

  textSize(16);
  fill(0);
  text("Red Duration", 200, 530); // 빨간 신호 주기 텍스트
  text("Yellow Duration", 400, 530); // 노란 신호 주기 텍스트
  text("Green Duration", 600, 530); // 초록 신호 주기 텍스트
  text("Volume", 88, 80); // 볼륨 텍스트

  textSize(20);
  textAlign(CENTER, CENTER);
  fill(0);
  text(songTitle, width / 2, height / 2); // 노래 제목 표시
}

// 슬라이더 값 변경 → 아두이노로 전송
async function sendSliderValues() {
  console.log("sendSliderValues() 실행됨");

  if (!port || !port.writable) {
    console.log("포트가 열려있지 않음");
    return;
  }

  try {
    if (!writer) {
      writer = port.writable.getWriter();  // writer가 없을 때만 getWriter() 호출
    }

    let red = redSlider.value(); // 빨간 신호 주기 값
    let yellow = yellowSlider.value(); // 노란 신호 주기 값
    let green = greenSlider.value(); // 초록 신호 주기 값

    console.log("슬라이더 변경됨:", red, yellow, green);

    // 개행 없이 한 줄씩 보냄
    await writer.write(new TextEncoder().encode(`RED_DURATION ${red}\n`));
    await writer.write(new TextEncoder().encode(`YELLOW_DURATION ${yellow}\n`));
    await writer.write(new TextEncoder().encode(`GREEN_DURATION ${green}\n`));
    console.log("주기 변경값이 아두이노로 잘 전송됨");

  } catch (error) {
    console.error("데이터 전송 오류:", error);
  }
}

// 볼륨 조절 함수
function updateVolume() {
  let volume = volumeSlider.value(); // 볼륨 값
  if (currentAudio) {
    currentAudio.setVolume(volume); // 현재 재생 중인 오디오의 볼륨 설정
  }
}

// 아두이노에서 받은 데이터를 처리(LED의 켜짐 유무와 밝기, 모드 업데이트)
function parseSerialData(data) {
  let tokens = data.split(" "); // 데이터를 공백으로 분리
  console.log("Parsed tokens:", tokens);

  // LED 상태 초기화
  redState = false;
  yellowState = false;
  greenState = false;

  tokens.forEach((token, index) => {
    if (token === "RED") redState = true;
    if (token === "YELLOW") yellowState = true;
    if (token === "GREEN") greenState = true;

    if(token === "RED_TIME") RED_DURATION = parseInt(tokens[index + 1]);
    if(token === "YELLOW_TIME") YELLOW_DURATION = parseInt(tokens[index + 1]);
    if(token === "GREEN_TIME") GREEN_DURATION = parseInt(tokens[index + 1]);

    if (token === "BRIGHTNESS") {
      let newBrightness = parseInt(tokens[index + 1]);
      if (!isNaN(newBrightness)) brightness = newBrightness;
    }

    if (token.includes("MODE")) {
      if (token === "MODE0") {
        modeText = "기본모드"; 
        changeMusic(0);
        songTitle = "How Do I Say Goodbye - Dean Lewis"; // 기본모드 음악 제목
      }
      else if (token === "MODE1") {
        modeText = "1번모드"; 
        changeMusic(1);
        songTitle = "Someone You Loved - Lewis Capaldi"; // 1번모드 음악 제목
      }
      else if (token === "MODE2") {
        modeText = "2번모드"; 
        changeMusic(2);
        songTitle = "나의 바람 - 비투비"; // 2번모드 음악 제목
      }
      else if (token === "MODE3") {  
        modeText = "3번모드"; 
        songTitle = "Try Everything - 주토피아 OST"; // 3번모드 음악 제목
        changeMusic(3);
      }
    }
  });
}

function changeMusic(newMode) {
  if (currentAudio.isPlaying()) {
    currentAudio.stop(); // 기존 음악 정지
  }
  currentAudio = audioFiles[newMode];
  currentAudio.loop(); // 새로운 음악 재생
}

// 신호등 UI
function drawTrafficLight() { // 각 LED의 켜져있으면 밝기대로 켜고, 꺼져있으면 어둡게 표시
  fill(redState ? color(255, 0, 0, brightness) : color(50));
  ellipse(200, 200, 50, 50);

  fill(yellowState ? color(255, 255, 0, brightness) : color(50));
  ellipse(400, 200, 50, 50);

  fill(greenState ? color(0, 255, 0, brightness) : color(50));
  ellipse(600, 200, 50, 50);
}

// 밝기 텍스트 표시
function brightnessupdate(){
  textSize(16);
  fill(0);
  text("Brightness", 700, 80); // 밝기 텍스트
  textSize(16);
  fill(0);
  text(brightness, 700, 110); // 밝기 값
}

// 모드 텍스트 표시
function updateModeText() {
  fill(0);
  textSize(32);
  textAlign(CENTER);
  text(modeText, width / 2, 50); // 모드 텍스트
}

// 버튼 클릭 시 포트 연결
async function connectBtnClick() {
  try {
    port = await navigator.serial.requestPort(); // 포트 요청
    await port.open({ baudRate: 9600 }); // 포트 열기

    connectBtn.html("Connected"); // 버튼 텍스트 변경
    currentAudio = audioFiles[0]; // 기본 모드 음악 설정
    currentAudio.loop(); // 기본 모드 음악 반복 재생 시작
    songTitle = 'How Do I Say Goodbye - Dean Lewis'; // 기본 모드의 노래 제목 표시
    console.log("Serial Connected!");

    writer = port.writable.getWriter(); // 쓰기 스트림 가져오기
    reader = port.readable.getReader(); // 읽기 스트림 가져오기

    // 아두이노에서 데이터 읽기 시작
    readFromArduino(); 

  } catch (error) {
    console.error("Connection failed:", error); // 연결 실패 시 에러 출력
  }
}

async function readFromArduino() {
  console.log("아두이노에서 데이터 읽어오고 있음");
  
  let buffer = ""; // 데이터 버퍼

  while (port.readable) {
    try {
      const { value, done } = await reader.read();
      if (done) break; // 데이터 읽기 종료 시 루프 탈출
      
      buffer += new TextDecoder().decode(value); // 수신된 데이터를 문자열로 변환하여 버퍼에 추가

      let lines = buffer.split("\n"); // 줄바꿈 기준으로 데이터 분리
      buffer = lines.pop(); // 마지막 줄이 완전하지 않으면 버퍼에 남겨둠

      for (let line of lines) {
        line = line.trim(); // 공백 제거
        if (line.length > 0) {
          console.log("Received data:", line);
          parseSerialData(line); // 한 줄씩 파싱
        }
      }
    } catch (error) {
      console.error("읽기 오류:", error);
      break;
    }
  }
}