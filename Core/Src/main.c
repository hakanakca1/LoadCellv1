/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>  // sprintf fonksiyonu için gerekli
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
long hx711_ham_veri = 0;
long hx711_dara_degeri = 0;   // Ofset değerimiz
long hx711_net_veri = 0;    // Dara düşülmüş sonuç
volatile uint8_t saniye_doldu = 0;

// Kalibrasyon için eklenenler:
float referans_gram = 500.0;      // Load cell üzerine koyduğun ağırlığın gerçek gramı (Örn: 500g)
long hx711_referans_veri = 112000; // O ağırlığı koyduğunda okuduğun hx711_net_veri değeri
float kalibrasyon_faktoru = 0;    // Hesaplanan katsayı
float gercek_gram = 0;            // Sonuç
int buton_durumu_izle=0;

volatile uint8_t timer_10ms_bayrak = 0;
long hx711_toplam = 0;
int okuma_sayaci = 0;
float son_agirlik_ortalamasi = 0;

char lcd_buffer[20];       // 'lcd_buffer' hatasını çözer
uint32_t pulse_sayisi = 0; // 'pulse_sayisi' hatasını çözer
uint32_t son_sayac = 0;    // Pulse hesaplama için gerekli

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

long HX711_Basit_Oku(void)
{
	long count = 0;

	// 1. Sensörün hazır olmasını bekle (DT pini LOW olmalı)
	// Eğer sensör bağlı değilse burada takılır. Test için seri port veya LED kullanacağız.
	while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);

	// 2. 24 bit veriyi kaydırarak oku
	for(int i=0; i<24; i++)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // SCK HIGH
		count = count << 1;                                   // Veriyi sola kaydır
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); // SCK LOW

		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
		{
			count++;
		}
	}

	// 3. 25. Pulse (Kanal A, Gain 128 ayarı için)
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

	// 4. Negatif sayı düzeltmesi (2's Complement)
	if(count & 0x800000) count |= 0xFF000000;

	return count;
}

void HX711_Dara_Al(void)
{
	long toplam = 0;
	// 10 örnek alarak gürültüyü engelleyelim
	for(int i=0; i<10; i++)
	{
		toplam += HX711_Basit_Oku();
		HAL_Delay(10);
	}
	hx711_dara_degeri = toplam / 10;
}

#define LCD_ADDR 0x4E // Eğer ekranın adresi 0x27 ise burayı 0x4E yap, 0x3F ise 0x7E yap.

extern I2C_HandleTypeDef hi2c1;  // I2C handler

void lcd_send_cmd(char cmd) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;  // en=1, rs=0
    data_t[1] = data_u | 0x08;  // en=0, rs=0
    data_t[2] = data_l | 0x0C;  // en=1, rs=0
    data_t[3] = data_l | 0x08;  // en=0, rs=0
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_send_data(char data) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;  // en=1, rs=1
    data_t[1] = data_u | 0x09;  // en=0, rs=1
    data_t[2] = data_l | 0x0D;  // en=1, rs=1
    data_t[3] = data_l | 0x09;  // en=0, rs=1
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_init(void) {
    HAL_Delay(50);
    lcd_send_cmd(0x30);
    HAL_Delay(5);
    lcd_send_cmd(0x30);
    HAL_Delay(1);
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x20);  // 4-bit mode
    HAL_Delay(10);
    lcd_send_cmd(0x28);  // 2 line, 5*8 font
    HAL_Delay(1);
    lcd_send_cmd(0x08);  // display off
    HAL_Delay(1);
    lcd_send_cmd(0x01);  // clear display
    HAL_Delay(1);
    lcd_send_cmd(0x06);  // entry mode set
    HAL_Delay(1);
    lcd_send_cmd(0x0C);  // display on, cursor off
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

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
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  lcd_init();
  lcd_put_cur(0, 0);
  lcd_send_string("Sistem Aciliyor");
  HAL_Delay(1000);
  lcd_send_cmd(0x01); // Ekranı temizle

  HAL_TIM_Base_Start_IT(&htim6); // Kesmeli modda başlat (IT takısı önemli)
  //HAL_TIM_Base_Start(&htim2); // TIM2 donanım sayacını başlat
	// Program başlarken sensörün oturması için kısa bir bekleme ve İLK DARA
	HAL_Delay(500);
	HX711_Dara_Al();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	while (1)
	{
		// --- 1. BUTON KONTROLÜ (Dara Yenileme) ---
		if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
		{
			buton_durumu_izle=1;
			HAL_Delay(50); // Ark (Debounce) önleme
			if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
			{
				HX711_Dara_Al(); // Butona basılınca darayı tekrar al
				hx711_toplam = 0; // Toplamı sıfırla ki eski veri karışmasın
				okuma_sayaci = 0;
				// Butondan elini çekene kadar bekle ki sürekli dara almasın
				while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
			}
		}
		else
		{
			buton_durumu_izle=0;
		}


		// Her 10ms'de bir buraya girer
		if (timer_10ms_bayrak == 1)
		{
			timer_10ms_bayrak = 0;

			// 1. Her 10ms'de bir hızlıca oku ve toplama ekle
			hx711_ham_veri = HX711_Basit_Oku();
			hx711_toplam += (hx711_ham_veri - hx711_dara_degeri);
			okuma_sayaci++;
			HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

			// 2. 100 okuma tamamlandı mı? (Yani 1 saniye geçti mi?)
			if (okuma_sayaci >= 5)
			{
				kalibrasyon_faktoru = (float)referans_gram / (float)hx711_referans_veri;
				// Ortalamayı hesapla
				long ortalama_raw = hx711_toplam / 5;
				son_agirlik_ortalamasi = (float)ortalama_raw * kalibrasyon_faktoru;

				// LCD'ye 'gercek_gram' yerine 'son_agirlik_ortalamasi' yazdır
				    lcd_send_cmd(0x01);
				    lcd_put_cur(0, 0);
				    sprintf(lcd_buffer, "Agirlik: %.1f g", son_agirlik_ortalamasi); // son_agirlik_ortalamasi'nı kullanın				    lcd_send_string(lcd_buffer);

				    // Alt satır
				    lcd_put_cur(1, 0);
				    sprintf(lcd_buffer, "Pulse: %lu", pulse_sayisi);
				    lcd_send_string(lcd_buffer);

				// Değişkenleri sıfırla (Bir sonraki 100'lük paket için)
				hx711_toplam = 0;
				okuma_sayaci = 0;

				// Çalıştığını anlamak için LED'i saniyede bir kez tersle

			}
		}

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM6)
	{
		timer_10ms_bayrak = 1; // Bayrağı kaldır ki 'while' döngüsü çalışsın
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
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
