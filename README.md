# DC_Tutorial

STM32 NUCLEO-F446ZE 보드로 **엔코더 DC 모터를 단계별로 제어**해 보는 튜토리얼입니다.
엔코더 읽기 → 오픈 루프 PWM 구동 → PI 속도 제어 → PID 속도 제어 순서로, 각 단계가 앞 단계의 코드를 그대로 이어받아 기능을 하나씩 더합니다.

| 항목 | 내용 |
| --- | --- |
| MCU | STM32F446ZETx (NUCLEO-F446ZE) |
| 모터 드라이버 | Cytron MDD10A (2채널, PWM + DIR 방식) |
| 개발 환경 | STM32CubeIDE / STM32CubeMX |
| 펌웨어 패키지 | STM32Cube FW_F4 V1.28.3 (HAL) |
| 시스템 클럭 | 168 MHz (APB1 타이머 84 MHz, APB2 타이머 168 MHz) |

---

## 전체 폴더 구조

```
DC_Tutorial/
├── README.md                     # 이 문서
├── f446xx_reference_manual.pdf   # STM32F446xx 레퍼런스 매뉴얼 (레지스터 참고용)
│
├── STEP0_Encoder/                # [0단계] 엔코더 카운트 읽기
├── STEP1_OpenLoop/               # [1단계] PWM + 방향 핀으로 모터 오픈 루프 구동
├── STEP2_PI_Control/             # [2단계] 타이머 인터럽트 기반 PI 속도 제어
└── STEP3_PID_Control/            # [3단계] D항을 추가한 PID 속도 제어
```

```
STEP0 엔코더 ──► STEP1 오픈 루프 ──► STEP2 PI 제어 ──► STEP3 PID 제어
 (측정)            (구동)              (측정+구동+피드백)    (미분항 추가)
```

---

## 각 STEP 프로젝트의 내부 구조

네 개의 STEP 폴더는 모두 **독립적인 STM32CubeIDE 프로젝트**이며 내부 구조가 같습니다.
각 폴더를 STM32CubeIDE에서 따로 열어 빌드할 수 있습니다.

```
STEPx_Name/
├── STEPx_Name.ioc               # CubeMX 설정 파일 (핀, 타이머, 클럭 설정) ← 하드웨어 설정의 원본
├── STEPx_Name Debug.launch      # STM32CubeIDE 디버그 실행 설정
├── STM32F446ZETX_FLASH.ld       # 링커 스크립트 (Flash 실행용)
├── STM32F446ZETX_RAM.ld         # 링커 스크립트 (RAM 실행용)
├── .project / .cproject         # Eclipse(STM32CubeIDE) 프로젝트 파일
├── .mxproject                   # CubeMX 코드 생성 정보
│
├── Core/
│   ├── Inc/                     # 헤더 파일
│   │   ├── main.h               #   핀 이름 매크로 (MOTOR1_DIR_Pin 등)
│   │   ├── tim.h / gpio.h / usart.h / usb_otg.h
│   │   ├── stm32f4xx_hal_conf.h #   HAL 모듈 활성화 설정
│   │   └── stm32f4xx_it.h
│   ├── Src/
│   │   ├── main.c               # ★ 튜토리얼의 핵심 코드가 작성된 파일
│   │   ├── tim.c                #   타이머(엔코더, PWM, 주기 인터럽트) 초기화
│   │   ├── gpio.c               #   GPIO 초기화
│   │   ├── usart.c / usb_otg.c  #   보드 기본 주변장치 초기화
│   │   ├── stm32f4xx_it.c       #   인터럽트 핸들러
│   │   ├── stm32f4xx_hal_msp.c  #   HAL 저수준 초기화
│   │   └── system_stm32f4xx.c, syscalls.c, sysmem.c
│   └── Startup/
│       └── startup_stm32f446zetx.s   # 벡터 테이블 및 리셋 핸들러
│
└── Drivers/                     # CubeMX가 생성한 HAL 드라이버 / CMSIS (수정하지 않음)
```

> 직접 작성한 코드는 대부분 `Core/Src/main.c`의 `/* USER CODE BEGIN ... */` ~ `/* USER CODE END ... */` 구간에 있습니다.
> 이 구간 안의 코드는 `.ioc`로 코드를 다시 생성해도 지워지지 않습니다.

---

## 공통 하드웨어 연결

모든 STEP이 같은 핀 배치를 사용합니다. (`.ioc` 기준)

```
                PWM1, DIR1, GND               M1A, M1B
NUCLEO-F446ZE ──────────────────► MDD10A ──────────────► DC 모터 1 (엔코더 내장)
      ▲                             ▲                         │
      │                        모터 전원 (POWER +/-)           │
      └──────────── 엔코더 A/B상 → PA0, PA1 ───────────────────┘
```

| 신호 | 핀 | 주변장치 | MDD10A 연결 | 설명 |
| --- | --- | --- | --- | --- |
| `MOTOR1_ENC_A` | PA0 | TIM2_CH1 | (모터 1 엔코더 A상) | 모터 1 엔코더 A상 |
| `MOTOR1_ENC_B` | PA1 | TIM2_CH2 | (모터 1 엔코더 B상) | 모터 1 엔코더 B상 |
| `MOTOR1_PWM` | PF9 | TIM14_CH1 | PWM1 | 모터 1 속도 PWM (20 kHz, ARR = 599) |
| `MOTOR1_DIR` | PG1 | GPIO 출력 | DIR1 | 모터 1 회전 방향 |
| `MOTOR2_PWM` | PE6 | TIM9_CH2 | PWM2 | 모터 2 속도 PWM (20 kHz, ARR = 1199) |
| `MOTOR2_DIR` | PG15 | GPIO 출력 | DIR2 | 모터 2 회전 방향 |
| GND | GND | — | GND | 보드와 드라이버의 접지 공통 연결 |

**MDD10A 사용 시 참고**
- MDD10A는 채널마다 **PWM(속도) + DIR(방향)** 두 신호로 제어합니다. 코드의 `SetMotor1SpeedPwm()`이 이 방식에 맞춰 부호로 DIR 핀을, 크기로 PWM 듀티를 정합니다.
- PWM 주파수는 MDD10A가 지원하는 최대치인 20 kHz로 맞췄습니다.
- 3.3 V 로직 입력을 지원하므로 NUCLEO 핀을 바로 연결할 수 있습니다.
- 모터 전원(5–30 V)은 드라이버의 POWER 단자에 따로 공급하고, **NUCLEO의 GND와 드라이버의 GND는 반드시 연결**해야 합니다.
- 실제 회전 방향이 코드와 반대라면 모터의 M1A와 M1B 선을 바꿔 연결하세요.

타이머 설정 요약:

| 타이머 | 용도 | 설정 | 사용 STEP |
| --- | --- | --- | --- |
| TIM2 | 엔코더 모드 (32비트 카운터) | Encoder Mode TI1, Period = 0xFFFFFFFF | 0, 2, 3 |
| TIM14 | 모터 1 PWM | 84 MHz / (6+1) / (599+1) = 20 kHz | 1, 2, 3 |
| TIM9 | 모터 2 PWM | 168 MHz / (6+1) / (1199+1) = 20 kHz | 1 |
| TIM6 | 제어 주기 인터럽트 | 84 MHz / (8399+1) = 10 kHz 기준 | 2, 3 |

---

## 단계별 설명

### 📁 STEP0_Encoder — 엔코더 읽기

모터에 달린 쿼드러처 엔코더 신호를 TIM2의 **엔코더 모드**로 받아 회전량을 카운트합니다.

- `HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL)`로 엔코더 타이머 시작
- 메인 루프에서 `TIM2->CNT` 값을 `current_count`에 계속 저장
- 디버거의 Live Expressions로 `current_count`를 보며 모터를 손으로 돌려 카운트 변화를 확인합니다.

**배우는 것:** 타이머 엔코더 모드, 카운터 레지스터 직접 읽기

---

### 📁 STEP1_OpenLoop — 오픈 루프 PWM 구동

피드백 없이 PWM 듀티와 방향 핀만으로 모터를 돌립니다.

- `HAL_TIM_PWM_Start()`로 TIM14(모터 1), TIM9(모터 2) PWM 출력 시작
- `SetMotor1SpeedPwm(int16_t pulse_value)`
  - 입력을 `-599 ~ 599`로 제한
  - 부호에 따라 `MOTOR1_DIR` 핀으로 방향을 정하고, `__HAL_TIM_SET_COMPARE()`로 듀티 설정
- 디버거에서 전역 변수 `pulse_in` 값을 바꿔 가며 속도와 방향을 확인합니다.

**배우는 것:** PWM 생성, 듀티비와 CCR 값의 관계, 방향 제어

---

### 📁 STEP2_PI_Control — PI 속도 제어

STEP0(측정)과 STEP1(구동)을 합치고, TIM6 주기 인터럽트 안에서 **PI 제어기**로 목표 RPM을 따라가게 합니다.

- TIM6 주기: Prescaler 8399, Period 999 → **100 ms**마다 `HAL_TIM_PeriodElapsedCallback()` 호출
- 콜백 내부 처리 순서
  1. `TIM2->CNT`로 현재 카운트 읽기
  2. `(int32_t)(current - last)`로 변위 계산 (카운터 오버플로우 자동 처리)
  3. RPM 환산: `delta / (CPR × 기어비) × 600` (CPR 26, 기어비 100:1 기준)
  4. `PI_Update()`로 제어 출력 계산
  5. `SetMotor1SpeedPwm()`로 PWM 출력
- `PI_Controller` 구조체: `Kp`, `Ki`, `dt`, 오차 누적값, 출력 상·하한
- 출력이 포화되면 이번 주기의 적분 누적을 되돌리는 **Anti-windup** 적용
- 초기값: `Kp = 0.5`, `Ki = 2.0`, `dt = 0.1`, 출력 범위 `±599`, 목표 `target_rpm = 3.0`

**배우는 것:** 주기 인터럽트 기반 제어 루프, 엔코더 → RPM 변환, PI 제어, Anti-windup

---

### 📁 STEP3_PID_Control — PID 속도 제어

STEP2에 **미분(D)항**을 추가하고 제어 주기를 줄입니다.

- TIM6 주기: Prescaler 8399, Period 99 → **10 ms**
- RPM 환산 계수: `× 6000` (60 s / 0.01 s)
- `PID_Controller` 구조체에 `Kd`, `prev_error` 추가
- `PID_Update()`: `P + I + D` 출력, D항은 `Kd × (error - prev_error) / dt`
- 초기값: `Kp = 0.5`, `Ki = 2.0`, `Kd = 0.3`, `dt = 0.01`, 출력 범위 `±599`

**배우는 것:** 미분항의 역할, 제어 주기와 RPM 분해능의 관계

---

## 사용 방법

1. 저장소를 클론합니다.
2. STM32CubeIDE에서 `File > Import > Existing Projects into Workspace`로 원하는 `STEPx_...` 폴더를 불러옵니다.
3. 빌드 후 `STEPx_... Debug.launch`로 보드에 다운로드하고 디버깅합니다.
4. Live Expressions에 `current_count`, `pulse_in`, `current_rpm`, `target_rpm` 등을 등록해 값을 관찰하거나 바꿔 봅니다.

## 참고 자료

- [`f446xx_reference_manual.pdf`](f446xx_reference_manual.pdf) — STM32F446xx Reference Manual (RM0390). 타이머 엔코더 모드와 PWM 레지스터 설명을 참고하세요.
