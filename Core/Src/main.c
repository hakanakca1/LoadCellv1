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
#include "lcd_i2c.h" // Yeni kütüphaneyi ekledik

#include "ssd1306.h"       // lcd_i2c.h yerine bunu ekledik

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

volatile uint8_t timer_200ms_bayrak = 0;
long hx711_toplam = 0;
int okuma_sayaci = 0;
float son_agirlik_ortalamasi = 0;

char lcd_buffer[20];       // 'lcd_buffer' hatasını çözer
uint32_t pulse_sayisi = 0; // 'pulse_sayisi' hatasını çözer
uint32_t son_sayac = 0;    // Pulse hesaplama için gerekli

#define PI 3.14159265
#define DISK_CAPI 4.0        // cm
float disk_cevre = PI * DISK_CAPI; // Yaklaşık 12.566 cm
float anlik_hiz = 0;         // cm/sn
double toplam_uzunluk = 0;   // cm (Hassasiyet için double)

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
	// GPIO_PIN_4 yerine LoadCell_DT_Pin, GPIOA yerine LoadCell_DT_GPIO_Port kullanıyoruz
	while(HAL_GPIO_ReadPin(LoadCell_DT_GPIO_Port, LoadCell_DT_Pin) == GPIO_PIN_SET);

	for(int i=0; i<24; i++)
	{
		HAL_GPIO_WritePin(LoadCell_SCK_GPIO_Port, LoadCell_SCK_Pin, GPIO_PIN_SET);   // SCK HIGH
		count = count << 1;
		HAL_GPIO_WritePin(LoadCell_SCK_GPIO_Port, LoadCell_SCK_Pin, GPIO_PIN_RESET); // SCK LOW

		if(HAL_GPIO_ReadPin(LoadCell_DT_GPIO_Port, LoadCell_DT_Pin) == GPIO_PIN_SET)
		{
			count++;
		}
	}

	// 25. Pulse
	HAL_GPIO_WritePin(LoadCell_SCK_GPIO_Port, LoadCell_SCK_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LoadCell_SCK_GPIO_Port, LoadCell_SCK_Pin, GPIO_PIN_RESET);

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
	MX_TIM4_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */

	/*
	lcd_init();
	lcd_clear(); // Ekranın tamamını temizler ve imleci (0,0) konumuna getirir.
	lcd_put_cur(0, 0);
	lcd_send_string("Sistem Aciliyor");
	HAL_Delay(1000);
	lcd_send_cmd(0x01); // Ekranı temizle
	 */

	/* --- BAŞLANGIÇ EKRANI --- */
	ssd1306_Init();
	ssd1306_Fill(Black);

	// "System" kelimesini ortalayalım (X=40 civarı, Y=2 üst satır)
	ssd1306_SetCursor(40, 8);
	ssd1306_WriteString("System", Font_7x10, White);

	// "Starting" kelimesini ortalayalım (X=32 civarı, Y=18 alt satır)
	ssd1306_SetCursor(32, 24);
	ssd1306_WriteString("Starting", Font_7x10, White);

	ssd1306_UpdateScreen();
	HAL_Delay(500); // Yazının okunması için 2 saniye bekle

	HAL_TIM_Base_Start_IT(&htim6); // Kesmeli modda başlat (IT takısı önemli)
	HAL_TIM_Base_Start(&htim2); // TIM2 donanım sayacını başlat
	HAL_TIM_Base_Start(&htim4);
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


		// Her 200ms'de bir buraya girer
		if (timer_200ms_bayrak == 1)
		{
			timer_200ms_bayrak = 0;
			HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

			// 1. Her 200ms'de bir hızlıca oku ve toplama ekle
			hx711_ham_veri = HX711_Basit_Oku();
			hx711_toplam += (hx711_ham_veri - hx711_dara_degeri);
			okuma_sayaci++;


			// 2. 5 okuma tamamlandı mı? (Yani 1 saniye geçti mi?)

			if (okuma_sayaci >= 5) // 1 Saniye dolduğunda (200ms * 5)
			{
			    /* --- 1. GRUP: VERİ TOPLAMA (PULSE VE SAYAÇ) --- */
			    uint32_t su_anki_sayac = __HAL_TIM_GET_COUNTER(&htim2);
			    uint32_t fark;

			    // Taşma kontrolü ile fark hesaplama
			    if (su_anki_sayac >= son_sayac) {
			        fark = su_anki_sayac - son_sayac;
			    } else {
			        fark = (4294967295 - son_sayac) + su_anki_sayac;
			    }

			    /* --- 2. GRUP: MEKANİK HESAPLAMALAR (HIZ VE MESAFE) --- */
			    pulse_sayisi = fark;             // Saniyedeki tur (Hz)
			    anlik_hiz = (float)fark * disk_cevre; // cm/sn
			    // Toplam uzunluğu metreye çevirerek biriktir (cm * 0.01 = m)
			    toplam_uzunluk += (double)fark * disk_cevre * 0.01;

			    son_sayac = su_anki_sayac; // Bir sonraki saniye için kaydet

			    /* --- 3. GRUP: AĞIRLIK HESAPLAMALARI (HX711) --- */
			    kalibrasyon_faktoru = (float)referans_gram / (float)hx711_referans_veri;
			    long ortalama_raw = hx711_toplam / 5;
			    son_agirlik_ortalamasi = (float)ortalama_raw * kalibrasyon_faktoru;


			    /* --- 4. GRUP: EKRAN GÜNCELLEME (LCD) --- */
			    /*
			    // lcd_clear() kullanmıyoruz, sadece imleci başa alıyoruz (Daha stabil görüntü)
			    lcd_put_cur(0, 0);
			    //sprintf(lcd_buffer, "%5.1fg %5.1fcm/s", son_agirlik_ortalamasi, anlik_hiz);
			    sprintf(lcd_buffer, "%5.1fg  %4luHz ", son_agirlik_ortalamasi, (unsigned long)pulse_sayisi);
			    lcd_send_string(lcd_buffer);

			    lcd_put_cur(1, 0);
			    //sprintf(lcd_buffer, "B: %8.2f m  ", toplam_uzunluk);
			    sprintf(lcd_buffer, "%4.1fcm/s %6.2fm", anlik_hiz, toplam_uzunluk);
			    lcd_send_string(lcd_buffer);
			    */

			    /* --- 4. GRUP: EKRAN GÜNCELLEME (OLED) --- */
			    ssd1306_Fill(Black); // Ekranı (bufferı) temizle

			    // İlk satır: Gram ve Hz
			    /* --- 1. SATIR: Ağırlık ve Frekans (Sağa Yasla) --- */
			    sprintf(lcd_buffer, "%5.1fg", son_agirlik_ortalamasi);
			    int agirlik_genisligi = strlen(lcd_buffer) * 10;
			    int agirlik_x_baslangic = 128 - agirlik_genisligi - 2;
			    if(agirlik_x_baslangic < 0) agirlik_x_baslangic = 0;
			    ssd1306_SetCursor(agirlik_x_baslangic, 4);
			    ssd1306_WriteString(lcd_buffer, Font_7x10, White);

			    /* --- Hız İçin Sağa Yaslama (Tam Sayı) --- */
			    sprintf(lcd_buffer, "%luHz %ucm/s", (unsigned long)pulse_sayisi, (unsigned int)anlik_hiz);
			    int hizi_genisligi = strlen(lcd_buffer) * 10;
			    int hizi_x_baslangic = 128 - hizi_genisligi - 2;
			    if(hizi_x_baslangic < 0) hizi_x_baslangic = 0;
			    ssd1306_SetCursor(hizi_x_baslangic, 14);
			    ssd1306_WriteString(lcd_buffer, Font_7x10, White);

			    /* --- Metre İçin Sağa Yaslama --- */
			    sprintf(lcd_buffer, "%6.1fm", toplam_uzunluk);
			    int metre_genisligi = strlen(lcd_buffer) * 10;
			    ssd1306_SetCursor(128 - metre_genisligi, 24);
			    ssd1306_WriteString(lcd_buffer, Font_7x10, White);



			    //ssd1306_WriteString("ABC abc", Font_7x10, White);
			    // Tamponu ekrana gönder
			    ssd1306_UpdateScreen();

			    /* --- 5. GRUP: SIFIRLAMA VE HAZIRLIK --- */

			    hx711_toplam = 0;
			    okuma_sayaci = 0;
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
		timer_200ms_bayrak = 1; // Bayrağı kaldır ki 'while' döngüsü çalışsın
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
