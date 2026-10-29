/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	float Kp;			// 비례 이득
	float Ki;			// 적분 이득
	float dt;			// 제어 주기
	float error_sum;	// 적분 항목을 위한 최대 오차 누적값
	float max_output;	// 제어 출력 최대 한계치
	float min_output;	// 제어 출력 최소 한계치
} PI_Controller;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
PI_Controller motor1_pi;
volatile float target_rpm = 3.0f;
float current_rpm = 0.0f;
uint32_t current_count = 0;
uint32_t last_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void SetMotor1SpeedPwm(int16_t pulse_value);
void PI_Init(PI_Controller *pi, float kp, float ki, float dt, float max_out, float min_out);
float PI_Update(PI_Controller *pi, float target, float current);
void PI_reset(PI_Controller *pi);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_TIM9_Init();
  MX_TIM14_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  PI_Init(&motor1_pi, 0.5f, 2.0f, 0.1f, 599.0f, -599.0f);

  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1); // 모터 1
  HAL_TIM_Base_Start_IT(&htim6); // 100ms 주기 타이머 인터럽트 시작
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void SetMotor1SpeedPwm(int16_t pulse_value)
{
	if (pulse_value > 599) pulse_value = 599;
	if (pulse_value < -599) pulse_value = -599;

	if (pulse_value >= 0)
	{
		HAL_GPIO_WritePin(MOTOR1_DIR_GPIO_Port, MOTOR1_DIR_Pin, GPIO_PIN_SET);   // 정방향
		__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, pulse_value);
	}
	else
	{
		HAL_GPIO_WritePin(MOTOR1_DIR_GPIO_Port, MOTOR1_DIR_Pin, GPIO_PIN_RESET);   // 역방향
		__HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, -pulse_value);
	}
}

void PI_Init(PI_Controller *pi, float kp, float ki, float dt, float max_out, float min_out)
{
	pi->Kp = kp;
	pi->Ki = ki;
	pi->dt = dt;
	pi->error_sum = 0.0;
	pi->max_output = max_out;
	pi->min_output = min_out;
}

float PI_Update(PI_Controller *pi, float target, float current)
{
	float error = target - current;

	float p_term = pi->Kp * error;

	pi->error_sum += error * pi->dt;
	float i_term = pi->Ki * pi->error_sum;

	float output = p_term + i_term;

	if (output > pi->max_output)
	{
		output = pi->max_output;

		pi->error_sum -= error * pi->dt;
	}
	else if (output < pi -> min_output)
	{
		output = pi->min_output;
		pi->error_sum -= error * pi->dt;
	}

	return output;
}

void PI_reset(PI_Controller *pi)
{
	pi->error_sum = 0.0;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{ // 100ms 주기가 도래했을 때
	if (htim->Instance == TIM6)
	{
		// 1. 엔코더 레지스터에서 현재 카운트 정밀 수집
		current_count = TIM2->CNT;

		// 2. 2의 보수 연산을 통한 변위(Delta) 추출 (오버플로우 자동 처리)
		int32_t delta_count = (int32_t)(current_count - last_count);
		last_count = current_count;

		// 3. 델타 카운트를 현재 출력축 RPM으로 변환
		// 공식: (delta / (CPR * 기어비)) * (60초 / 0.1초)
		// 예시 수치: 26 CPR, 100:1 기어 기준
		float cpr = 26.0f;
		float gear_ratio = 100.0f;
		current_rpm = ((float)delta_count / (cpr * gear_ratio)) * 600.0f;

		// 4. PI 제어기 업데이트 수행 ➡️ 출력값은 듀티비 정수 펄스로 환산됨 (-599 ~ 599)
		float control_output = PI_Update(&motor1_pi, target_rpm, current_rpm);

		// 5. 앞서 구현한 오픈 루프 함수에 제어 출력 대입하여 모터 전압 인가
		SetMotor1SpeedPwm((int16_t)control_output);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
