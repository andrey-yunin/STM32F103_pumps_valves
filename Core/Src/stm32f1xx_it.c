/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h" // Для can_rx_queueHandle
#include "app_config.h" // Чтобы видеть CanFrame_t
#include "pumps_valves_gpio.h"
#include "task_can_handler.h"



/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void EnterSafeFaultState(void)
{
	/*
	 * Fault handlers must not depend on FreeRTOS, CAN queues or dynamic memory.
	 * Stop interrupts first, then force all outputs to the domain safe state.
	 * The active IWDG is intentionally not refreshed here, so a hardware reset
	 * follows after the watchdog timeout.
	 */
	__disable_irq();
	PumpsValves_AllOff();
	__DSB();
	__ISB();
}


/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern CAN_HandleTypeDef hcan;
extern TIM_HandleTypeDef htim4;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
	EnterSafeFaultState();

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
	EnterSafeFaultState();

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
	EnterSafeFaultState();

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
	EnterSafeFaultState();

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
	EnterSafeFaultState();

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles USB low priority or CAN RX0 interrupts.
  */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 0 */

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan);
  /* USER CODE BEGIN USB_LP_CAN1_RX0_IRQn 1 */

  /* USER CODE END USB_LP_CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles CAN RX1 interrupt.
  */
void CAN1_RX1_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_RX1_IRQn 0 */

  /* USER CODE END CAN1_RX1_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan);
  /* USER CODE BEGIN CAN1_RX1_IRQn 1 */

  /* USER CODE END CAN1_RX1_IRQn 1 */
}

/**
  * @brief This function handles TIM4 global interrupt.
  */
void TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM4_IRQn 0 */

  /* USER CODE END TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM4_IRQn 1 */

  /* USER CODE END TIM4_IRQn 1 */
}

/* USER CODE BEGIN 1 */
// Callback вызывается HAL, когда в CAN FIFO0 появился новый принятый фрейм.
//
// В ISR нельзя долго работать и нельзя выполнять прикладную логику.
// Поэтому здесь только быстро забираем фрейм из аппаратного FIFO,
// кладем его в RTOS-очередь и будим задачу CAN handler.
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CanRxFrame_t rx_frame;

	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_frame.header, rx_frame.data) == HAL_OK)
		{
		// Если очередь заполнена, фрейм теряется на транспортном уровне.
		// NACK отсюда отправлять нельзя: мы в ISR, плюс команда еще не разобрана.
		// Дирижер увидит timeout ACK и потом сможет запросить GET_STATUS.
		if (osMessageQueuePut(can_rx_queueHandle, &rx_frame, 0, 0) == osOK)
			{
			osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_RX);
			}
		else
			{
			CAN_Diagnostics_RecordRxQueueOverflow();
			}
		}
}

// IRQ для CAN status/error line.
//
// Этот обработчик нужен для CAN_IT_ERROR_WARNING, CAN_IT_ERROR_PASSIVE,
// CAN_IT_BUSOFF, CAN_IT_LAST_ERROR_CODE и CAN_IT_ERROR.
// Без него HAL_CAN_ErrorCallback() может не вызываться.
void CAN1_SCE_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan);
}

// Callback CAN-ошибок.
//
// Здесь мы не лечим шину и не перезапускаем CAN вручную.
// restart-ms на стороне SocketCAN и auto-recovery bxCAN живут отдельно.
// Наша задача: зафиксировать состояние, чтобы Дирижер видел деградацию.
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	CAN_Diagnostics_RecordCanError(HAL_CAN_GetError(hcan), hcan->Instance->ESR);
}

/* USER CODE END 1 */
