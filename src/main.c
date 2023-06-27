#include "main.h"

#ifdef STM32F10X_MD_VL
#include <stm32f10x_rcc.h>
#include <stm32f10x_iwdg.h>
#include <stm32f10x.h>
#include <drivers/f1/gpio_conf_stm32f1x.h>
#endif

#ifdef STM32L471xx
#include <stm32l4xx_hal_cortex.h>
#include <stm32l4xx.h>
#include <stm32l4xx_ll_iwdg.h>
#include <stm32l4xx_ll_rcc.h>
#include <stm32l4xx_ll_gpio.h>
#include "cmsis/stm32l4xx/system_stm32l4xx.h"

#include "gsm/sim800c.h"
#include "gsm/sim800c_engineering.h"
#include "gsm/sim800c_poolers.h"
#include "gsm/sim800c_gprs.h"
#include "http_client/http_client.h"

#include "nvm.h"

#include "aprsis.h"
#include "api/api.h"
#include "drivers/l4/pwm_input_stm32l4x.h"
#include "drivers/l4/spi_speed_stm32l4x.h"
#include "drivers/max31865.h"
#endif

#include <delay.h>
#include <LedConfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packet_tx_handler.h"

#include "station_config.h"
#include <configuration_nvm/config_data_externs.h>
#include <configuration_nvm/configuration_handler.h>

#include "diag/Trace.h"
#include "antilib_adc.h"
#include "afsk_pr.h"
#include "TimerConfig.h"
#include "PathConfig.h"
#include "LedConfig.h"
#include "io.h"
#include "float_to_string.h"
#include "pwr_save.h"
#include <wx_pwr_switch.h>
#include "io_default_vbat_scaling.h"

#include "it_handlers.h"

#include "aprs/digi.h"
#include "aprs/telemetry.h"
#include "aprs/dac.h"
#include "aprs/beacon.h"

#include "ve_direct_protocol/parser.h"

#include "rte_wx.h"
#include "rte_pv.h"
#include "rte_main.h"
#include "rte_rtu.h"

#include <wx_handler.h>
#include "drivers/dallas.h"
#include "drivers/i2c.h"
#include "drivers/spi.h"
#include "drivers/analog_anemometer.h"
#include "dust_sensor/sds011.h"
#include "aprs/wx.h"

#include "../system/include/modbus_rtu/rtu_serial_io.h"

#include "../system/include/davis_vantage/davis.h"
#include "../system/include/davis_vantage/davis_parsers.h"

#include "drivers/ms5611.h"
#include <drivers/bme280.h>

#include "umb_master/umb_master.h"
#include "umb_master/umb_channel_pool.h"
#include "umb_master/umb_0x26_status.h"

#include "drivers/dallas.h"

#include <kiss_communication/kiss_communication.h>
#include <etc/kiss_configuation.h>

#define SOH 0x01

//#include "variant.h"

//#define SERIAL_TX_TEST_MODE

// Niebieska dioda -> DCD
// Zielona dioda -> anemometr albo TX

// backup registers (ParaTNC)
// 0 ->
// 2 -> boot and hard fault count
// 3 -> controller configuration status
// 4 ->
// 5 ->
// 6 ->

// backup registers (ParaMETEO)
// 0 -> powersave status
// 1 -> last sleep rtc time
// 2 -> last wakeup rtc time
// 3 -> controller configuration status
// 4 -> wakeup events MSB, sleep events LSB
// 5 -> monitor


#define CONFIG_FIRST_RESTORED 			(1)
#define CONFIG_FIRST_FAIL_RESTORING	  	(1 << 1)
#define CONFIG_FIRST_CRC_OK				(1 << 2)

#define CONFIG_SECOND_RESTORED 				(1 << 3)
#define CONFIG_SECOND_FAIL_RESTORING	  	(1 << 4)
#define CONFIG_SECOND_CRC_OK				(1 << 5)

/**
 * A foreword about '#define' mess. This software is indented to run on at least two
 * different hardware platforms. First which is ParaTNC basing on STM32F100 and second
 * ParaMETEO using STM32L476. In future more platforms may appear. Like ParaTNC2 which
 * will be a ParaMETEO without battery charging and in form factor similar to ParaTNC.
 *
 * To obtain such compatibility a lot of #defines and different makefiles has to be used.
 * Some parts of the code are 'included' per target CPU basis, as are independent from
 * target platform either directly (like handling serial port or GPIO configuration), or
 * as a result of an assumption that all target plaforms with STML476 will have GSM modem.
 *
 * Some platforms had or may have in the future few hadware revisions. ParaTNC had
 * revisions A, B and C. Currently A and B are abandoned an assumption is that all ParaTNC
 * builds applies to C. A choose of hardware revision is done in file 'station_config_target_hw.h'
 * which is currently empty
 */

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wempty-body"

// used configuration structures
const config_data_mode_t * main_config_data_mode = 0;
const config_data_basic_t * main_config_data_basic = 0;
const config_data_wx_sources_t * main_config_data_wx_sources = 0;
const config_data_umb_t * main_config_data_umb = 0;
const config_data_rtu_t * main_config_data_rtu = 0;
#ifdef STM32L471xx
const config_data_gsm_t * main_config_data_gsm = 0;
#endif

//! global variable incremented by the SysTick handler to measure time in miliseconds
volatile uint32_t master_time = 0;

//! current timestamp from RTC in NVM format
uint32_t main_nvm_timestamp = 0;

//! this global variable stores numbers of ticks of idling CPU
uint32_t main_idle_cpu_ticks = 0;

//! current cpu idle ticks
uint32_t main_current_cpu_idle_ticks = 0;

//! approx cpu load in percents
int8_t main_cpu_load = 0;

//! global variable used as a timer to trigger meteo sensors mesurements
int32_t main_wx_sensors_pool_timer = 65500;

//! global variable used as a timer to trigger packet sending
int32_t main_one_minute_pool_timer = 45000;

//! one second pool interval
int32_t main_one_second_pool_timer = 1000;

//! two second pool interval
int32_t main_two_second_pool_timer = 2000;

//! ten second pool interval
int32_t main_ten_second_pool_timer = 10000;

//! serial context for UART used to KISS
srl_context_t main_kiss_srl_ctx;

//! serial context for UART used for comm with wx sensors
srl_context_t main_wx_srl_ctx;

#if defined(STM32L471xx)
//! serial context for communication with GSM module
srl_context_t main_gsm_srl_ctx;
#endif

//! operation mode of USART1 (RS232 on RJ45 socket)
main_usart_mode_t main_usart1_kiss_mode = USART_MODE_UNDEF;

//! operation mode of USART2 (RS485)
main_usart_mode_t main_usart2_wx_mode = USART_MODE_UNDEF;

//!
configuration_button_function_t main_button_one_left;

//!
configuration_button_function_t main_button_two_right;

//! a pointer to KISS context
srl_context_t* main_kiss_srl_ctx_ptr;

//! a pointer to wx comms context
srl_context_t* main_wx_srl_ctx_ptr;

//! a pointer to gsm context
srl_context_t* main_gsm_srl_ctx_ptr;

//! target USART1 (kiss) baudrate
uint32_t main_target_kiss_baudrate;

//! target USART2 (wx) baudrate
uint32_t main_target_wx_baudrate;

//! controls if the KISS modem is enabled
uint8_t main_kiss_enabled = 1;

//! controls if DAVIS serialprotocol client is enabled by the configuration
uint8_t main_davis_serial_enabled = 0;

uint8_t main_modbus_rtu_master_enabled = 0;

uint8_t main_reset_config_to_default = 0;

//! global variables represending the AX25/APRS stack
AX25Ctx main_ax25;
Afsk main_afsk;


AX25Call main_own_path[3];
uint8_t main_own_path_ln = 0;
uint8_t main_own_aprs_msg_len;
char main_own_aprs_msg[OWN_APRS_MSG_LN];

char main_string_latitude[9];
char main_string_longitude[9];
char main_callsign_with_ssid[10];

uint8_t main_small_buffer[KISS_CONFIG_DIAGNOSTIC_BUFFER_LN];

char main_symbol_f = '/';
char main_symbol_s = '#';

//! global variable used to store return value from various functions
volatile uint8_t retval = 100;

uint16_t buffer_len = 0;

//! return value from UMB related functions
umb_retval_t main_umb_retval = UMB_UNINITIALIZED;

//! result of CRC calculation
uint32_t main_crc_result = 0;

#if defined(STM32L471xx)
LL_GPIO_InitTypeDef GPIO_InitTypeDef;

gsm_sim800_state_t main_gsm_state;

uint32_t rte_main_rx_total = 0;
uint32_t rte_main_tx_total = 0;

volatile int i = 0;
#endif

#if defined(PARAMETEO)
uint8_t main_woken_up = 0;
#endif

char after_tx_lock;

unsigned short rx10m = 0, tx10m = 0, digi10m = 0, digidrop10m = 0, kiss10m = 0;

static void message_callback(struct AX25Msg *msg) {

}

const char * post_content = "{\
  \"main_config_data_basic_callsign\": \"SP8EBC\",\
  \"main_config_data_basic_ssid\": 8,\
  \"master_time\": 12345,\
  \"main_cpu_load\": 50,\
  \"rx10m\": 30,\
  \"tx10m\": 20,\
  \"digi10m\": 50,\
  \"digidrop10m\": 10,\
  \"kiss10m\": 5,\
  \"rte_main_rx_total\": 11,\
  \"rte_main_tx_total\": 12,\
  \"rte_main_average_battery_voltage\": 123,\
  \"rte_main_wakeup_count\": 0,\
  \"rte_main_going_sleep_count\": 2,\
  \"rte_main_last_sleep_master_time\": 9}";

//#define SERIAL_TX_TEST_MODE

int main(int argc, char* argv[]){

  int32_t ln = 0;

  it_handlers_inhibit_radiomodem_dcd_led = 1;

  memset(main_own_aprs_msg, 0x00, OWN_APRS_MSG_LN);

#if defined(STM32F10X_MD_VL)
  RCC->APB1ENR |= (RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM7EN | RCC_APB1ENR_TIM4EN);
  RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_TIM1EN);
  RCC->AHBENR |= RCC_AHBENR_CRCEN;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  // choosing the signal source for the SysTick timer.
  SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);

  // Configuring the SysTick timer to generate interrupt 100x per second (one interrupt = 10ms)
  SysTick_Config(SystemCoreClock / SYSTICK_TICKS_PER_SECONDS);

  // setting an Systick interrupt priority
  NVIC_SetPriority(SysTick_IRQn, 5);

  // enable access to BKP registers
  RCC->APB1ENR |= (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN);
  PWR->CR |= PWR_CR_DBP;

  // read current number of boot cycles
  rte_main_boot_cycles = (uint8_t)(BKP->DR2 & 0xFF);

  // read current number of hard faults
  rte_main_hard_faults = (uint8_t)((BKP->DR2 & 0xFF00) >> 8);

  // increase boot cycles count
  rte_main_boot_cycles++;

  // erasing old value from backup registers
  BKP->DR2 &= (0xFFFF ^ 0xFF);

  // storing increased value
  BKP->DR2 |= rte_main_boot_cycles;

  BKP->DR3 = 0;
  BKP->DR4 = 0;
  BKP->DR5 = 0;
  BKP->DR6 = 0;
#endif

#if defined(STM32L471xx)
  system_clock_update_l4();

  if (system_clock_configure_l4() != 0) {
	  HAL_NVIC_SystemReset();

  }

  // enable access to PWR control registers
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;

  system_clock_update_l4();

  system_clock_configure_rtc_l4();

  RCC->APB1ENR1 |= (RCC_APB1ENR1_SPI2EN | RCC_APB1ENR1_TIM2EN | RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM4EN | RCC_APB1ENR1_TIM5EN | RCC_APB1ENR1_TIM7EN | RCC_APB1ENR1_USART2EN | RCC_APB1ENR1_USART3EN | RCC_APB1ENR1_DAC1EN | RCC_APB1ENR1_I2C1EN | RCC_APB1ENR1_USART3EN);
  RCC->APB2ENR |= (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_TIM8EN); // RCC_APB1ENR1_USART3EN
  RCC->AHB1ENR |= (RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_DMA1EN);
  RCC->AHB2ENR |= (RCC_AHB2ENR_ADCEN | RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIODEN);
  RCC->BDCR |= RCC_BDCR_RTCEN;

  /* Set Interrupt Group Priority */
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  // set systick frequency
  HAL_SYSTICK_Config(SystemCoreClock / (1000U / (uint32_t)10));

  // set systick interrupt priority
  HAL_NVIC_SetPriority(SysTick_IRQn, 5, 0U);
#endif

  rte_main_reboot_req = 0;

  // initializing variables & arrays in rte_wx
  rte_wx_init();
  rte_rtu_init();

  // calculate CRC over configuration blocks
  main_crc_result = configuration_handler_check_crc();

  // restore config to default if requested
  if (main_reset_config_to_default == 1) {
	  main_crc_result = 0;

	  configuration_set_register(0);

#if defined(STM32L471xx)
	  nvm_erase_all();

//	  nvm_test_prefill();
#endif
  }

  // if first section has wrong CRC and it hasn't been restored before
  if ((main_crc_result & 0x01) == 0 && (configuration_get_register() & CONFIG_FIRST_FAIL_RESTORING) == 0) {
	  // restore default configuration
	  if (configuration_handler_restore_default_first() == 0) {

		  // if configuration has been restored successfully
		  configuration_set_bits_register(CONFIG_FIRST_RESTORED);

		  // set also CRC flag because if restoring is successfull the region has good CRC
		  configuration_set_bits_register(CONFIG_FIRST_CRC_OK);

	  }
	  else {
		  // if not store the flag in the backup register to block
		  // reinitializing once again in the consecutive restart
		  configuration_set_bits_register(CONFIG_FIRST_FAIL_RESTORING);

		  configuration_clear_bits_register(CONFIG_FIRST_CRC_OK);
	  }


  }
  else {
	  // if the combined confition is not met check failed restoring flag
	  if ((configuration_get_register() & CONFIG_FIRST_FAIL_RESTORING) == 0) {
		  // a CRC checksum is ok, so first configuration section can be used further
		  configuration_set_bits_register(CONFIG_FIRST_CRC_OK);
	  }
	  else {
		  ;
	  }
  }

  // if second section has wrong CRC and it hasn't been restored before
  if ((main_crc_result & 0x02) == 0 && (configuration_get_register() & CONFIG_SECOND_FAIL_RESTORING) == 0) {
	  // restore default configuration
	  if (configuration_handler_restore_default_second() == 0) {

		  // if configuration has been restored successfully
		  configuration_set_bits_register(CONFIG_SECOND_RESTORED);

		  // set also CRC flag as if restoring is successfull the region has good CRC
		  configuration_set_bits_register(CONFIG_SECOND_CRC_OK);

	  }
	  else {
		  // if not store the flag in the backup register
		  configuration_set_bits_register(CONFIG_SECOND_FAIL_RESTORING);

		  configuration_clear_bits_register(CONFIG_SECOND_CRC_OK);
	  }


  }
  else {
	  // check failed restoring flag
	  if ((configuration_get_register() & CONFIG_SECOND_FAIL_RESTORING) == 0) {
		  // second configuration section has good CRC and can be used further
		  configuration_set_bits_register(CONFIG_SECOND_CRC_OK);
	  }
	  else {
		  ;
	  }
  }

  // at this point both sections have either verified CRC or restored values to default
  if ((configuration_get_register() & CONFIG_FIRST_CRC_OK) != 0 && (configuration_get_register() & CONFIG_SECOND_CRC_OK) != 0) {
	  // if both sections are OK check programming counters
	  if (config_data_pgm_cntr_first > config_data_pgm_cntr_second) {
		  // if first section has bigger programing counter use it
		  configuration_handler_load_configuration(REGION_FIRST);
	  }
	  else {
		  configuration_handler_load_configuration(REGION_SECOND);

	  }
  }
  else if ((configuration_get_register() & CONFIG_FIRST_CRC_OK) != 0 && (configuration_get_register() & CONFIG_SECOND_CRC_OK) == 0) {
	  // if only first region is OK use it
	  configuration_handler_load_configuration(REGION_FIRST);
  }
  else if ((configuration_get_register() & CONFIG_FIRST_CRC_OK) == 0 && (configuration_get_register() & CONFIG_SECOND_CRC_OK) != 0) {
	  // if only first region is OK use it
	  configuration_handler_load_configuration(REGION_FIRST);
  }
  else {
	  configuration_handler_load_configuration(REGION_DEFAULT);
  }

  // set function for left button
  main_button_one_left = configuration_get_left_button();

  // set function for right button
  main_button_two_right = configuration_get_right_button();

  // set packets intervals
  packet_tx_configure(main_config_data_basic->wx_transmit_period, main_config_data_basic->beacon_transmit_period, main_config_data_mode->powersave);

#if defined(STM32F10X_MD_VL)
  // disabling access to BKP registers
  RCC->APB1ENR &= (0xFFFFFFFF ^ (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN));
  PWR->CR &= (0xFFFFFFFF ^ PWR_CR_DBP);
#endif

  // converting latitude into string
  memset(main_string_latitude, 0x00, sizeof(main_string_latitude));
  float_to_string(main_config_data_basic->latitude, main_string_latitude, sizeof(main_string_latitude), 2, 2);

  // converting longitude into string
  memset(main_string_longitude, 0x00, sizeof(main_string_longitude));
  float_to_string(main_config_data_basic->longitude, main_string_longitude, sizeof(main_string_longitude), 2, 5);

  // make a string with callsign and ssid
  if (main_config_data_basic->ssid != 0) {
	sprintf(main_callsign_with_ssid, "%s-%d", main_config_data_basic->callsign, main_config_data_basic->ssid);
  }
  else {
	sprintf(main_callsign_with_ssid, "%s", main_config_data_basic->callsign);
  }

  switch(main_config_data_basic->symbol) {
  case 0:		// _SYMBOL_DIGI
	  main_symbol_f = '/';
	  main_symbol_s = '#';
	  break;
  case 1:		// _SYMBOL_WIDE1_DIGI
	  main_symbol_f = '1';
	  main_symbol_s = '#';
	  break;
  case 2:		// _SYMBOL_HOUSE
	  main_symbol_f = '/';
	  main_symbol_s = '-';
	  break;
  case 3:		// _SYMBOL_RXIGATE
	  main_symbol_f = 'I';
	  main_symbol_s = '&';
	  break;
  case 5:		// _SYMBOL_SAILBOAT
	  main_symbol_f = '/';
	  main_symbol_s = 'Y';
	  break;
  default:		// _SYMBOL_IGATE
	  main_symbol_f = 'R';
	  main_symbol_s = '&';
	  break;

  }

#if defined _RANDOM_DELAY
  // configuring a default delay value
  delay_set(_DELAY_BASE, 1);
#elif !defined _RANDOM_DELAY
  delay_set(_DELAY_BASE, 0);

#endif

#if defined(PARAMETEO)
  if (main_button_one_left != BUTTON_DISABLED || main_button_two_right != BUTTON_DISABLED) {
	  // initializing GPIO used for buttons
	  io_buttons_init();
  }

  // initialize all powersaving functions
  pwr_save_init(main_config_data_mode->powersave);

  // initialize B+ measurement
  io_vbat_meas_init(configuration_get_vbat_a_coeff(), configuration_get_vbat_b_coeff());
#endif

  // initalizing separated Open Collector output
  io_oc_init();

  // initializing GPIO used for swithing on and off voltages on pcb
  io_pwr_init();

  // initialize sensor power control and switch off supply voltage
  wx_pwr_switch_init();

  // call periodic handle to wait for 1 second and then switch on voltage
  wx_pwr_switch_periodic_handle();

#if defined(PARAMETEO)
  // swtich power to M4. turn on sensors but keep GSM modem turned off
  pwr_save_switch_mode_to_c1();

  delay_fixed(300);

#endif

  // waiting for 1 second to count number of ticks when the CPU is idle
  main_idle_cpu_ticks = delay_fixed_with_count(1000);

  // initializing UART gpio pins
  io_uart_init();

#if defined(STM32F10X_MD_VL)
  // enabling the clock for both USARTs
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
#endif

  main_kiss_srl_ctx_ptr = &main_kiss_srl_ctx;
  main_wx_srl_ctx_ptr = &main_wx_srl_ctx;
#if defined(STM32L471xx)
  main_gsm_srl_ctx_ptr = &main_gsm_srl_ctx;
#endif

  main_target_kiss_baudrate = 9600u;

#ifndef PARAMETEO
  // if Victron VE-direct protocol is enabled set the baudrate to the 19200u
  if (main_config_data_mode->victron == 1) {
    main_target_kiss_baudrate = 19200u;

    // and disable the kiss TNC option as it shares the same port
    main_kiss_enabled = 0;
  }
#endif

  // get target working mode of USART1
  if (main_config_data_mode->wx_davis == 1) {
	  main_usart1_kiss_mode = USART_MODE_DAVIS;
  }
  else if ((main_config_data_mode->wx_dust_sensor & WX_DUST_SDS011_SERIAL) > 0) {
	  main_usart1_kiss_mode = USART_MODE_DUST_SDS;
  }
  else if (main_config_data_mode->victron == 1) {
	  main_usart1_kiss_mode = USART_MODE_VICTRON;
  }
  else {
	  main_usart1_kiss_mode = USART_MODE_KISS;
  }

  // get target working mode for USART2
  if (main_config_data_mode->wx_modbus == 1) {
	  main_usart2_wx_mode = USART_MODE_MODBUS;
  }
  else if (main_config_data_mode->wx_umb == 1) {
	  main_usart2_wx_mode = USART_MODE_UMB_MASTER;
  }
  else {
	  main_usart2_wx_mode = USART_MODE_UNINIT;
  }

  switch (main_usart1_kiss_mode) {
	  case USART_MODE_DAVIS: {
		  // reinitialize the KISS serial port temporary to davis baudrate
		  main_target_kiss_baudrate = DAVIS_DEFAULT_BAUDRATE;

		  // reset RX state to allow reinitialization with changed baudrate
		  main_kiss_srl_ctx_ptr->srl_rx_state = SRL_RX_NOT_CONFIG;

		  // reinitializing serial hardware to wake up Davis wx station
		  srl_init(main_kiss_srl_ctx_ptr, USART1, srl_usart1_rx_buffer, RX_BUFFER_1_LN, srl_usart1_tx_buffer, TX_BUFFER_1_LN, main_target_kiss_baudrate, 1);

		  srl_switch_timeout(main_kiss_srl_ctx_ptr, SRL_TIMEOUT_ENABLE, 3000);

		  davis_init(main_kiss_srl_ctx_ptr);

		  // try to wake up the davis base
		  rte_wx_davis_station_avaliable = (davis_wake_up(DAVIS_BLOCKING_IO) == 0 ? 1 : 0);

		  // if davis weather stations is connected to SERIAL port
		  if (rte_wx_davis_station_avaliable == 1) {
			  // turn LCD backlight on..
			  davis_control_backlight(1);

			  // wait for a while
			  delay_fixed(1000);

			  // and then off to let the user know that communication is working
			  davis_control_backlight(0);

			  // disable the KISS modem as the UART will be used for DAVIS wx station
			  main_kiss_enabled = 0;

			  // enable the davis serial protocol client to allow pooling callbacks to be called in main loop.
			  // This only controls the callback it doesn't mean that the station itself is responding to
			  // communication. It stays set to one event if Davis station
			  main_davis_serial_enabled = 1;

			  // trigger the rxcheck to get all counter values
			  davis_trigger_rxcheck_packet();

		  }
		  else {
			  // if not revert back to KISS configuration
			  main_target_kiss_baudrate = 9600u;
			  main_kiss_srl_ctx_ptr->srl_rx_state = SRL_RX_NOT_CONFIG;

			  // initializing UART drvier
			  srl_init(main_kiss_srl_ctx_ptr, USART1, srl_usart1_rx_buffer, RX_BUFFER_1_LN, srl_usart1_tx_buffer, TX_BUFFER_1_LN, main_target_kiss_baudrate, 1);

			  main_usart1_kiss_mode = USART_MODE_KISS;
		  }
		  break;
	  }
	  case USART_MODE_DUST_SDS: {
		  srl_init(main_kiss_srl_ctx_ptr, USART1, srl_usart1_rx_buffer, RX_BUFFER_1_LN, srl_usart1_tx_buffer, TX_BUFFER_1_LN, 9600u, 1);

		  main_kiss_enabled = 0;

		  break;
	  }
	  case USART_MODE_VICTRON: {
		  break;
	  }
	  case USART_MODE_KISS: {
		  srl_init(main_kiss_srl_ctx_ptr, USART1, srl_usart1_rx_buffer, RX_BUFFER_1_LN, srl_usart1_tx_buffer, TX_BUFFER_1_LN, main_target_kiss_baudrate, 1);

		  main_kiss_enabled = 1;

		  break;
	  }
	  case USART_MODE_MODBUS:
	  case USART_MODE_UMB_MASTER:
	  case USART_MODE_UNINIT:
	  case USART_MODE_UNDEF:
		  main_kiss_enabled = 0;
		  break;
  }

  switch (main_usart2_wx_mode) {
	  case USART_MODE_MODBUS: {
		  rtu_serial_init(&rte_rtu_pool_queue, 1, main_wx_srl_ctx_ptr, main_config_data_rtu);

		  main_target_wx_baudrate = main_config_data_rtu->slave_speed;

		  srl_init(main_wx_srl_ctx_ptr, USART2, srl_usart2_rx_buffer, RX_BUFFER_2_LN, srl_usart2_tx_buffer, TX_BUFFER_2_LN, main_target_wx_baudrate, main_config_data_rtu->slave_stop_bits);
		  srl_switch_tx_delay(main_wx_srl_ctx_ptr, 1);

		  // enabling rtu master code
		  main_modbus_rtu_master_enabled = 1;

		  rtu_serial_start();

		  break;
	  }
	  case USART_MODE_UMB_MASTER: {
		  main_target_wx_baudrate = main_config_data_umb->serial_speed;

		  srl_init(main_wx_srl_ctx_ptr, USART2, srl_usart2_rx_buffer, RX_BUFFER_2_LN, srl_usart2_tx_buffer, TX_BUFFER_2_LN, main_target_wx_baudrate, 1);
		  umb_master_init(&rte_wx_umb_context, main_wx_srl_ctx_ptr, main_config_data_umb);

		  break;
	  }
	  case USART_MODE_DAVIS:
	  case USART_MODE_DUST_SDS:
	  case USART_MODE_VICTRON:
	  case USART_MODE_KISS:
	  case USART_MODE_UNINIT:
	  case USART_MODE_UNDEF:
		  break;
  }

#if defined(STM32F10X_MD_VL)
  main_wx_srl_ctx_ptr->te_pin = GPIO_Pin_8;
  main_wx_srl_ctx_ptr->te_port = GPIOA;
#endif
#if defined(STM32L471xx)
  main_wx_srl_ctx_ptr->te_pin = LL_GPIO_PIN_8;
  main_wx_srl_ctx_ptr->te_port = GPIOA;
  main_wx_srl_ctx_ptr->early_tx_assert = configuration_get_early_tx_assert();		// TODO: was 1

  srl_init(main_gsm_srl_ctx_ptr, USART3, srl_usart3_rx_buffer, RX_BUFFER_3_LN, srl_usart3_tx_buffer, TX_BUFFER_3_LN, 115200, 1);
#endif

  // initialize APRS path with zeros
  memset (main_own_path, 0x00, sizeof(main_own_path));

  // configuring an APRS path used to transmit own packets (telemetry, wx, beacons)
  main_own_path_ln = ConfigPath(main_own_path, main_config_data_basic);

#ifdef INTERNAL_WATCHDOG
#if defined(STM32F10X_MD_VL)
  // enable write access to watchdog registers
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

  // Set watchdog prescaler
  IWDG_SetPrescaler(IWDG_Prescaler_128);

  // Set the counter value to program watchdog for about 13 seconds
  IWDG_SetReload(0xFFF);

  // enable the watchdog
  IWDG_Enable();

  // do not disable the watchdog when the core is halted on a breakpoint
  DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

  // reload watchdog counter
  IWDG_ReloadCounter();
#endif

#if defined(STM32L471xx)
  // enable watchdog
  LL_IWDG_Enable(IWDG);

  // unlock write access to configuratio registers
  LL_IWDG_EnableWriteAccess(IWDG);

  // set prescaler - watchdog timeout on about 16 seconds
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_128);

  // wait for watchdog registers to update
  while (LL_IWDG_IsActiveFlag_PVU(IWDG) != 0) {
	  i++;

	  if (i > 0x9FF) {
		  break;
	  }
  }

  // reload watchdog which also close access to configiguration registers
  LL_IWDG_ReloadCounter(IWDG);

  i = 0;

  // do not disable watchdog when MCU halts on breakpoints
  DBGMCU->APB1FZR1 &= (0xFFFFFFFF ^ DBGMCU_APB1FZR1_DBG_IWDG_STOP);

#endif

#endif

  // initialize i2c controller
  i2cConfigure();

#if defined(STM32L471xx)
  // initialize SPI
  spi_init_full_duplex_pio(SPI_MASTER_MOTOROLA, CLOCK_REVERSED_RISING, SPI_SPEED_DIV256, SPI_ENDIAN_MSB);

  // initialize measurements averager
  max31865_init_average();

  // initialize MAX RDT amplifier
  max31865_init(main_config_data_mode->wx_pt_sensor & 0x3, (main_config_data_mode->wx_pt_sensor & 0xFC) >> 2);

#endif

  // initialize GPIO pins leds are connecting to
  led_init();

  // initialize AX25 & APRS stuff
  AFSK_Init(&main_afsk);
  ax25_init(&main_ax25, &main_afsk, 0, 0x00, 0x00);
  DA_Init();

  // configure external watchdog
  io_ext_watchdog_config();

  // initializing the digipeater configuration
  digi_init(main_config_data_mode);

  if ((main_config_data_mode->wx & WX_ENABLED) == 1) {
#if defined(STM32F10X_MD_VL)
	  dallas_init(GPIOC, GPIO_Pin_11, GPIO_PinSource11, &rte_wx_dallas_average);
#endif

#if defined(STM32L471xx)
	  // initialize dallas one-wire driver for termometer
	  dallas_init(GPIOC, LL_GPIO_PIN_11, 0x0, &rte_wx_dallas_average);
#endif

//	  if (main_config_data_mode->wx_umb == 1) {
//		  // client initialization
//		  umb_master_init(&rte_wx_umb_context, main_wx_srl_ctx_ptr, main_config_data_umb);
//	  }

	  if ((main_config_data_mode->wx & WX_INTERNAL_SPARKFUN_WIND) == 0) {
		  analog_anemometer_init(main_config_data_mode->wx_anemometer_pulses_constant, 38, 100, 1);
	  }
	  else {
		  analog_anemometer_init(main_config_data_mode->wx_anemometer_pulses_constant, 38, 100, 1);
	  }
  }

  // configuring interrupt priorities
  it_handlers_set_priorities();

  // read calibration data from I2C pressure / humidity sensor
	if (main_config_data_mode->wx_ms5611_or_bme == 0) {
		ms5611_reset(&rte_wx_ms5611_qf);
		ms5611_read_calibration(SensorCalData, &rte_wx_ms5611_qf);
		ms5611_trigger_measure(0, 0);
	}
	else if (main_config_data_mode->wx_ms5611_or_bme == 1) {
		bme280_reset(&rte_wx_bme280_qf);
		bme280_setup();
		bme280_read_calibration(bme280_calibration_data);
	}

 if (main_kiss_enabled == 1) {
	  // preparing initial beacon which will be sent to host PC using KISS protocol via UART
	  main_own_aprs_msg_len = sprintf(main_own_aprs_msg, "=%s%c%c%s%c%c %s", main_string_latitude, main_config_data_basic->n_or_s, main_symbol_f, main_string_longitude, main_config_data_basic->e_or_w, main_symbol_s, main_config_data_basic->comment);

	  // terminating the aprs message
	  main_own_aprs_msg[main_own_aprs_msg_len] = 0;

	  // 'sending' the message which will only encapsulate it inside AX25 protocol (ax25_starttx is not called here)
	  //ax25_sendVia(&main_ax25, main_own_path, (sizeof(main_own_path) / sizeof(*(main_own_path))), main_own_aprs_msg, main_own_aprs_msg_len);
	  ln = ax25_sendVia_toBuffer(main_own_path, (sizeof(main_own_path) / sizeof(*(main_own_path))), main_own_aprs_msg, main_own_aprs_msg_len, main_kiss_srl_ctx.srl_tx_buf_pointer, TX_BUFFER_1_LN);

	  // SendKISSToHost function cleares the output buffer hence routine need to wait till the UART will be ready for next transmission.
	  // Here this could be omitted because UART isn't used before but general idea
	  while(main_kiss_srl_ctx.srl_tx_state != SRL_TX_IDLE && main_kiss_srl_ctx.srl_tx_state != SRL_TX_ERROR);

	  // converting AX25 with beacon to KISS format
	  //ln = SendKISSToHost(main_afsk.tx_buf + 1, main_afsk.tx_fifo.tail - main_afsk.tx_fifo.head - 4, srl_tx_buffer, TX_BUFFER_LN);


	  // checking if KISS-framing was done correctly
	  if (ln != KISS_TOO_LONG_FRM) {
	#ifdef SERIAL_TX_TEST_MODE
		  // infinite loop for testing UART transmission
		  for (;;) {

			  retval = srl_receive_data(main_kiss_srl_ctx_ptr, 100, '\r', '\r', 0, 0, 0);
	#endif
			  retval = srl_start_tx(main_kiss_srl_ctx_ptr, ln);

	#ifdef SERIAL_TX_TEST_MODE
			  	  	while(main_kiss_srl_ctx_ptr->srl_tx_state != SRL_TX_IDLE);

				#if defined(PARAMETEO)
			  	 	LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_9);
				#else
		  	  		GPIOC->ODR = (GPIOC->ODR ^ GPIO_Pin_9);
				#endif

			  if (main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE) {
				#if defined(PARAMETEO)
			  	 		LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_9);
				#else
						GPIOC->ODR = (GPIOC->ODR ^ GPIO_Pin_9);
				#endif
				  retval = 200;
			  }
		  }
	#endif
	  }

 }

  // reinitializing AFSK and AX25 driver
  AFSK_Init(&main_afsk);

  ADCStartConfig();
  DACStartConfig();
  AFSK_Init(&main_afsk);
  ax25_init(&main_ax25, &main_afsk, 0, message_callback, 0);

	if ((main_config_data_mode->wx & WX_ENABLED) == 1) {
	  // getting all meteo measuremenets to be sure that WX frames want be sent with zeros
	  wx_get_all_measurements(main_config_data_wx_sources, main_config_data_mode, main_config_data_umb, main_config_data_rtu);
	}

#ifndef PARAMETEO
  // start serial port i/o transaction depending on station configuration
  if (main_config_data_mode->victron == 1) {
	  // initializing protocol parser
	  ve_direct_parser_init(&rte_pv_struct, &rte_pv_average);

	  // enabling timeout handling for serial port. This is required because VE protocol frame may vary in lenght
	  // and serial port driver could finish reception only either on stop character or when declared number of bytes
	  // has been received.
	  srl_switch_timeout(main_kiss_srl_ctx_ptr, 1, 50);

	  // switching UART to receive mode to be ready for data from charging controller
	  srl_receive_data(main_kiss_srl_ctx_ptr, VE_DIRECT_MAX_FRAME_LN, 0, 0, 0, 0, 0);
  }
  else {
	  // switching UART to receive mode to be ready for KISS frames from host
	  srl_receive_data(main_kiss_srl_ctx_ptr, 100, FEND, FEND, 0, 0, 0);
  }
#else
  if (main_kiss_enabled == 1) {
	  // switching UART to receive mode to be ready for KISS frames from host
	  srl_receive_data(main_kiss_srl_ctx_ptr, 100, FEND, FEND, 0, 0, 0);
  }
  else {
	  srl_receive_data(main_kiss_srl_ctx_ptr, 10, 0xAA, 0, 0, 0, 0);
  }
#endif

  io_oc_output_low();

  led_control_led1_upper(false);
  led_control_led2_bottom(false);

#if defined(PARAMETEO)
   rte_main_battery_voltage = io_vbat_meas_get();
   rte_main_average_battery_voltage = rte_main_battery_voltage;

   if (main_config_data_mode->gsm == 1) {
	   pwr_save_switch_mode_to_c0();
   }

   // sleep a little bit and wait for everything to power up completely
   delay_fixed(1000);

   led_control_led1_upper(true);
   led_control_led2_bottom(false);

   delay_fixed(1000);

   led_control_led1_upper(false);
   led_control_led2_bottom(true);

   delay_fixed(1000);

   led_control_led1_upper(true);
   led_control_led2_bottom(true);

   delay_fixed(1000);

   led_control_led1_upper(false);
   led_control_led2_bottom(false);

#endif

  // configuting system timers
  TimerConfig();

  // initialize UMB transaction
  if (main_config_data_mode->wx_umb == 1) {
	umb_0x26_status_request(&rte_wx_umb, &rte_wx_umb_context, main_config_data_umb);
  }

#ifdef INTERNAL_WATCHDOG
   // reload watchdog counter
	main_reload_internal_wdg();
#endif

   io_ext_watchdog_service();

#ifdef STM32L471xx

   if (main_config_data_mode->nvm_logger != 0) {
	   nvm_measurement_init();
   }

   if (main_config_data_mode->gsm == 1) {
	   it_handlers_inhibit_radiomodem_dcd_led = 1;

	   led_control_led1_upper(false);

	   gsm_sim800_init(&main_gsm_state, 1);

	   http_client_init(&main_gsm_state, main_gsm_srl_ctx_ptr, 0);

	   if (main_config_data_gsm->api_enable == 1 && main_config_data_gsm->aprsis_enable == 0) {
		   api_init((const char *)main_config_data_gsm->api_base_url, (const char *)(main_config_data_gsm->api_station_name));
	   }

	   if (main_config_data_gsm->api_enable == 0 && main_config_data_gsm->aprsis_enable == 1) {
		   aprsis_init(&main_gsm_srl_ctx,
				   	   &main_gsm_state,
					   (const char *)main_config_data_basic->callsign,
					   main_config_data_basic->ssid,
					   main_config_data_gsm->aprsis_passcode,
					   (const char *)main_config_data_gsm->aprsis_server_address,
					   main_config_data_gsm->aprsis_server_port,
					   configuration_get_power_cycle_gsmradio_on_no_communications(),
					   main_callsign_with_ssid);
	   }
   }

   if ((main_config_data_mode->wx_dust_sensor & WX_DUST_SDS011_PWM) > 0) {
	   pwm_input_io_init();

	   pwm_input_init(1);
   }
#endif

   if (main_config_data_basic-> beacon_at_bootup == 1) {
#if defined(PARAMETEO)
	   beacon_send_own(rte_main_battery_voltage, system_is_rtc_ok());
		main_wait_for_tx_complete();
	   delay_fixed(1500);
#else
	   beacon_send_own(0, 0);

#endif

#if defined(PARAMETEO)
	   telemetry_send_status_powersave_registers(REGISTER_LAST_SLEEP, REGISTER_LAST_WKUP, REGISTER_COUNTERS, REGISTER_MONITOR, REGISTER_LAST_SLTIM);
#endif
   }

	main_nvm_timestamp = main_get_nvm_timestamp();

	it_handlers_inhibit_radiomodem_dcd_led = 0;

  // Infinite loop
  while (1)
    {
	  main_set_monitor(-1);

	  // incrementing current cpu ticks
	  main_current_cpu_idle_ticks++;

	    if (rte_main_reboot_req == 1) {
	    	NVIC_SystemReset();
	    }
	    else {
	    	;
	    }

	  main_set_monitor(0);

#if defined(PARAMETEO)
	  	if (main_woken_up == 1) {

	  		// restart ADCs
	  		io_vbat_meas_enable();
	  		ADCStartConfig();
	  		DACStartConfig();

		    rte_main_battery_voltage = io_vbat_meas_get();
		    rte_main_average_battery_voltage = io_vbat_meas_average(rte_main_battery_voltage);

		    // meas average will return 0 if internal buffer isn't filled completely
		    if (rte_main_average_battery_voltage == 0) {
		    	rte_main_average_battery_voltage = rte_main_battery_voltage;
		    }

	  	    // reinitialize APRS radio modem to clear all possible intermittent state caused by
	  	    // switching power state in the middle of APRS packet reception
			ax25_new_msg_rx_flag = 0;
			main_ax25.dcd = false;

	  		main_woken_up = 0;

	  		main_set_monitor(1);
	  	}
#endif

  		main_set_monitor(11);

	  	// if new packet has been received from radio channel
		if(ax25_new_msg_rx_flag == 1) {

			// if serial port is currently not busy on transmission
			if (main_kiss_srl_ctx_ptr->srl_tx_state != SRL_TXING) {
				memset(main_kiss_srl_ctx.srl_tx_buf_pointer, 0x00, main_kiss_srl_ctx.srl_tx_buf_ln);

				if (main_kiss_enabled == 1) {
					// convert message to kiss format and send it to host
					srl_start_tx(main_kiss_srl_ctx_ptr, kiss_send_ax25_to_host(ax25_rxed_frame.raw_data, (ax25_rxed_frame.raw_msg_len - 2), main_kiss_srl_ctx.srl_tx_buf_pointer, main_kiss_srl_ctx.srl_tx_buf_ln));
				}
			}

			main_ax25.dcd = false;

			digi_check_with_viscous(&ax25_rxed_frame);

			// check if this packet needs to be repeated (digipeated) and do it if it is necessary
			digi_process(&ax25_rxed_frame, main_config_data_basic, main_config_data_mode);

			ax25_new_msg_rx_flag = 0;
			rx10m++;
#ifdef STM32L471xx
			rte_main_rx_total++;

			// if aprsis is logged
			if (aprsis_connected == 1) {
				aprsis_igate_to_aprsis(&ax25_rxed_frame, &main_callsign_with_ssid);
			}

#endif
		}

#ifdef STM32L471xx
		// if GSM communication is enabled
		if (main_config_data_mode->gsm == 1) {

			// if data has been received
			if (main_gsm_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE || main_gsm_srl_ctx_ptr->srl_rx_state == SRL_RX_ERROR) {

				// receive callback for communicatio with the modem
				gsm_sim800_rx_done_event_handler(main_gsm_srl_ctx_ptr, &main_gsm_state);

				//srl_reset(main_gsm_srl_ctx_ptr);
			}

			if (main_gsm_srl_ctx_ptr->srl_tx_state == SRL_TX_IDLE) {
				gsm_sim800_tx_done_event_handler(main_gsm_srl_ctx_ptr, &main_gsm_state);
			}

//			gsm_sim800_engineering_enable(main_gsm_srl_ctx_ptr, &main_gsm_state);
//			gsm_sim800_engineering_request_data(main_gsm_srl_ctx_ptr, &main_gsm_state);
			//gsm_sim800_engineering_disable(main_gsm_srl_ctx_ptr, &main_gsm_state);
		}
#endif

		// if Victron VE.direct client is enabled
		if (main_config_data_mode->victron == 1) {
#ifndef PARAMETEO
			// if new KISS message has been received from the host
			if (main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE || main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_ERROR) {

				// cutting received string to Checksum, everything after will be skipped
				ve_direct_cut_to_checksum(srl_get_rx_buffer(main_kiss_srl_ctx_ptr), TX_BUFFER_1_LN, &buffer_len);

				// checking if this frame is ok
				ve_direct_validate_checksum(srl_get_rx_buffer(main_kiss_srl_ctx_ptr), buffer_len, &retval);

				if (retval == 1) {
					// parsing data from input serial buffer to
					retval = ve_direct_parse_to_raw_struct(srl_get_rx_buffer(main_kiss_srl_ctx_ptr), buffer_len, &rte_pv_struct);

					if (retval == 0) {
						ve_direct_add_to_average(&rte_pv_struct, &rte_pv_average);

						ve_direct_get_averages(&rte_pv_average, &rte_pv_battery_current, &rte_pv_battery_voltage, &rte_pv_cell_voltage, &rte_pv_load_current);

						ve_direct_set_sys_voltage(&rte_pv_struct, &rte_pv_sys_voltage);

						ve_direct_store_errors(&rte_pv_struct, &rte_pv_last_error);

						rte_pv_messages_count++;
					}
				}
				else {
					rte_pv_corrupted_messages_count++;
				}

				//memset(srl_get_rx_buffer(main_kiss_srl_ctx_ptr), 0x00, TX_BUFFER_1_LN);

				srl_receive_data(main_kiss_srl_ctx_ptr, VE_DIRECT_MAX_FRAME_LN, 0, 0, 0, 0, 0);
			}
#endif
		}
		else if ((main_config_data_mode->wx_dust_sensor & WX_DUST_SDS011_SERIAL) > 0) {
			if (main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE) {

				sds011_get_pms(main_kiss_srl_ctx_ptr->srl_rx_buf_pointer, 10, &rte_wx_pm10, &rte_wx_pm2_5);

				// restart reception
				srl_receive_data(main_kiss_srl_ctx_ptr, 10, 0xAA, 0, 0, 0, 0);

			}
		}
		else {
			// if new KISS message has been received from the host
			if (main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE && main_kiss_enabled == 1) {
				// parse i ncoming data and then transmit on radio freq
				ln = kiss_parse_received(srl_get_rx_buffer(main_kiss_srl_ctx_ptr), srl_get_num_bytes_rxed(main_kiss_srl_ctx_ptr), &main_ax25, &main_afsk, main_small_buffer, KISS_CONFIG_DIAGNOSTIC_BUFFER_LN);
				if (ln == 0) {
					kiss10m++;	// increase kiss messages counter
				}
				else if (ln > 0) {
					// if a response (ACK) to this KISS frame shall be sent

					// wait for any pending transmission to complete
					srl_wait_for_tx_completion(main_kiss_srl_ctx_ptr);

					srl_send_data(main_kiss_srl_ctx_ptr, main_small_buffer, SRL_MODE_DEFLN, ln, SRL_INTERNAL);
				}

				// restart KISS receiving to be ready for next frame
				srl_receive_data(main_kiss_srl_ctx_ptr, 120, FEND, FEND, 0, 0, 0);
			}

			// if there were an error during receiving frame from host, restart rxing once again
			if (main_kiss_srl_ctx_ptr->srl_rx_state == SRL_RX_ERROR && main_kiss_enabled == 1) {
				srl_receive_data(main_kiss_srl_ctx_ptr, 120, FEND, FEND, 0, 0, 0);
			}
		}

		if (kiss_current_async_message != 0xFF && main_kiss_srl_ctx_ptr->srl_tx_state == SRL_TX_IDLE) {
			srl_start_tx(main_kiss_srl_ctx_ptr, kiss_async_pooler(main_kiss_srl_ctx.srl_tx_buf_pointer, main_kiss_srl_ctx.srl_tx_buf_ln));
		}

		// if Davis wx station is enabled and it is alive
		if (main_davis_serial_enabled == 1) {

			// pool the Davis wx station driver for LOOP packet
			davis_loop_packet_pooler(&rte_wx_davis_loop_packet_avaliable);

			davis_rxcheck_packet_pooler();
		}

		if (main_config_data_mode->wx_umb == 1) {
			// if some UMB data have been received
			if (main_wx_srl_ctx_ptr->srl_rx_state == SRL_RX_DONE) {
				umb_pooling_handler(&rte_wx_umb_context, REASON_RECEIVE_IDLE, master_time, main_config_data_umb);
			}

			// if there were an error during receiving frame from host, restart rxing once again
			if (main_wx_srl_ctx_ptr->srl_rx_state == SRL_RX_ERROR) {
				umb_pooling_handler(&rte_wx_umb_context, REASON_RECEIVE_ERROR, master_time, main_config_data_umb);
			}

			if (main_wx_srl_ctx_ptr->srl_tx_state == SRL_TX_IDLE) {
				umb_pooling_handler(&rte_wx_umb_context, REASON_TRANSMIT_IDLE, master_time, main_config_data_umb);
			}
		}
		// if modbus rtu master is enabled
		else if (main_modbus_rtu_master_enabled == 1) {
			rtu_serial_pool();
		}

		button_check_all(main_button_one_left, main_button_two_right);

		main_set_monitor(2);

		// get all meteo measuremenets each 65 seconds. some values may not be
		// downloaded from sensors if _METEO and/or _DALLAS_AS_TELEM aren't defined
		if (main_wx_sensors_pool_timer < 10) {
#ifdef PARAMETEO
			// get current battery voltage. for non parameteo this will return 0
		    rte_main_battery_voltage = io_vbat_meas_get();

		    // get average battery voltage
		    rte_main_average_battery_voltage = io_vbat_meas_average(rte_main_battery_voltage);

		    // meas average will return 0 if internal buffer isn't filled completely
		    if (rte_main_average_battery_voltage == 0) {
		    	rte_main_average_battery_voltage = rte_main_battery_voltage;
		    }
#endif
			if ((main_config_data_mode->wx & WX_ENABLED) == 1) {

				// notice: UMB-master and Modbus-RTU uses the same UART
				// so they cannot be enabled both at once

				// check if modbus rtu master is enabled and configured properly
				if (main_modbus_rtu_master_enabled == 1) {
					// start quering all Modbus RTU devices & registers one after another
					rtu_serial_start();
				}
				else if (main_config_data_mode->wx_umb == 1) {
					// request status from the slave if UMB master is enabled
					umb_0x26_status_request(&rte_wx_umb, &rte_wx_umb_context, main_config_data_umb);
				}
				else {
					;
				}

				// davis serial weather station is connected using UART / RS232 used normally
				// for KISS communcation between modem and host PC
				if (main_davis_serial_enabled == 1) {
					davis_trigger_rxcheck_packet();
				}

				// get all measurements from 'internal' sensors (except wind which is handled separately)
				wx_get_all_measurements(main_config_data_wx_sources, main_config_data_mode, main_config_data_umb, main_config_data_rtu);
			}

			// check if there is a request to send ModbusRTU error status message
			if (rte_main_trigger_modbus_status == 1 && main_modbus_rtu_master_enabled == 1) {
				rtu_serial_get_status_string(&rte_rtu_pool_queue, main_wx_srl_ctx_ptr, main_own_aprs_msg, OWN_APRS_MSG_LN, &main_own_aprs_msg_len);

			 	ax25_sendVia(&main_ax25, main_own_path, main_own_path_ln, main_own_aprs_msg, main_own_aprs_msg_len);

			 	afsk_txStart(&main_afsk);

			 	rte_main_trigger_modbus_status = 0;


			}

			main_set_monitor(3);

			main_wx_sensors_pool_timer = 65500;
		}

		/**
		 * ONE MINUTE POOLING
		 */
		if (main_one_minute_pool_timer < 10) {

			main_set_monitor(4);

			main_nvm_timestamp = main_get_nvm_timestamp();

			#ifndef _MUTE_OWN
			packet_tx_handler(main_config_data_basic, main_config_data_mode);
			#endif

			main_set_monitor(5);

			#ifdef STM32L471xx
			if (main_config_data_mode->gsm == 1) {

				if (http_client_connection_errors > HTTP_CLIENT_MAX_CONNECTION_ERRORS) {
					NVIC_SystemReset();
				}

				//aprsis_connect_and_login_default(1);

//				if (gsm_sim800_gprs_ready == 1) {
//
//					//api_send_json_status();
//					api_send_json_measuremenets();
//	//				retval = http_client_async_get("http://pogoda.cc:8080/meteo_backend/status", strlen("http://pogoda.cc:8080/meteo_backend/status"), 0xFFF0, 0x1, 0);
//				}

			}
			#endif

			main_one_minute_pool_timer = 60000;
		}

		/**
		 * ONE SECOND POOLING
		 */
		if (main_one_second_pool_timer < 10) {

			main_set_monitor(6);

			digi_pool_viscous();

			#ifdef STM32L471xx
			if (main_config_data_mode->gsm == 1) {

				if (gsm_sim800_gprs_ready == 1) {
					led_control_led1_upper(true);
				}
				else {
					led_control_led1_upper(false);
				}

				if (gsm_sim800_gprs_ready == 1) {
					/***
					 *
					 * TEST TEST TEST TODO
					 */
					//retval = http_client_async_get("http://pogoda.cc:8080/meteo_backend/status", strlen("http://pogoda.cc:8080/meteo_backend/status"), 0xFFF0, 0x1, dupa);
					//retval = http_client_async_post("http://pogoda.cc:8080/meteo_backend/parameteo/skrzyczne/status", strlen("http://pogoda.cc:8080/meteo_backend/parameteo/skrzyczne/status"), post_content, strlen(post_content), 0, dupa);
				}


				gsm_sim800_initialization_pool(main_gsm_srl_ctx_ptr, &main_gsm_state);

				gsm_sim800_poolers_one_second(main_gsm_srl_ctx_ptr, &main_gsm_state, main_config_data_gsm);

				aprsis_check_alive();
			}
			#endif

			if ((main_config_data_mode->wx & WX_ENABLED) == 1) {
				analog_anemometer_direction_handler();
			}

			main_set_monitor(7);

			main_one_second_pool_timer = 1000;
		}
		else if (main_one_second_pool_timer < -10) {

			if ((main_config_data_mode->wx & WX_ENABLED) == 1) {
				analog_anemometer_direction_reset();
			}

			main_one_second_pool_timer = 1000;
		}

		/**
		 * TWO SECOND POOLING
		 */
		if (main_two_second_pool_timer < 10) {

			if (configuration_get_inhibit_wx_pwr_handle() == 0) {
				wx_pwr_switch_periodic_handle();
			}

#ifdef PARAMETEO
			if (configuration_get_power_cycle_vbat_r() == 1 && !main_afsk.sending) {
				io_pool_vbat_r(packet_tx_get_minutes_to_next_wx());
			}
#endif

			wx_check_force_i2c_reset();

#ifdef STM32L471xx
			max31865_pool();
#endif
			#ifdef INTERNAL_WATCHDOG
			main_reload_internal_wdg();
			#endif

			main_two_second_pool_timer = 2000;
		}

		/**
		 * TEN SECOND POOLING
		 */
		if (main_ten_second_pool_timer < 10) {

			main_set_monitor(8);

			// check if consecutive weather frame has been triggered from 'packet_tx_handler'
			if (rte_main_trigger_wx_packet == 1) {

				packet_tx_send_wx_frame();

				rte_main_trigger_wx_packet = 0;
			}

			#ifdef PARAMETEO
			// inhibit any power save switching when modem transmits data
			if (!main_afsk.sending && main_woken_up == 0) {
				pwr_save_pooling_handler(main_config_data_mode, main_config_data_basic, packet_tx_get_minutes_to_next_wx(), rte_main_average_battery_voltage);
			}

			if ((main_config_data_mode->wx_dust_sensor & WX_DUST_SDS011_PWM) > 0) {
				pwm_input_pool();
			}

			if (pwm_first_channel != 0) {
				rte_wx_pm2_5 = pwm_first_channel;
			}

			if (pwm_second_channel != 0) {
				rte_wx_pm10 = pwm_second_channel;
			}
			#endif

			main_set_monitor(9);

			#ifdef STM32L471xx
			if (main_config_data_mode->gsm == 1) {
				gsm_sim800_poolers_ten_seconds(main_gsm_srl_ctx_ptr, &main_gsm_state);

				packet_tx_tcp_handler();
			}
			#endif

			if (main_config_data_mode->wx_umb == 1) {
				umb_channel_pool(&rte_wx_umb, &rte_wx_umb_context, main_config_data_umb);
			}

			if (main_config_data_mode->wx_umb == 1) {
				rte_wx_umb_qf = umb_get_current_qf(&rte_wx_umb_context, master_time);
			}

			wx_pool_anemometer(main_config_data_wx_sources, main_config_data_mode, main_config_data_umb, main_config_data_rtu);

			if (main_davis_serial_enabled == 1) {

				// if previous LOOP packet is ready for processing
				if (rte_wx_davis_loop_packet_avaliable == 1) {
					davis_parsers_loop(main_kiss_srl_ctx_ptr->srl_rx_buf_pointer, main_kiss_srl_ctx_ptr->srl_rx_buf_ln, &rte_wx_davis_loop_content);
				}

				// trigger consecutive LOOP packet
				davis_trigger_loop_packet();
			}

			main_ten_second_pool_timer = 10000;
		}

		  main_set_monitor(10);


    }
  // Infinite loop, never return.
}

uint16_t main_get_adc_sample(void) {
	return (uint16_t) ADC1->DR;
}

void main_service_cpu_load_ticks(void) {

	uint32_t cpu_ticks_load = 0;

	// the biggest this result will be the biggest load the CPU is handling
	cpu_ticks_load = main_idle_cpu_ticks - main_current_cpu_idle_ticks;

	// calculate the cpu load
	main_cpu_load = (int8_t) ((cpu_ticks_load * 100) / main_idle_cpu_ticks);

	// reset the tick counter back to zero;
	main_current_cpu_idle_ticks = 0;
}

void main_reload_internal_wdg(void){
#ifdef INTERNAL_WATCHDOG
#ifdef STM32F10X_MD_VL

	  // reload watchdog counter
	  IWDG_ReloadCounter();

#endif
#ifdef STM32L471xx
	  LL_IWDG_ReloadCounter(IWDG);
#endif
#endif
}

uint32_t main_get_nvm_timestamp(void) {
	uint32_t out = 0;

	/**
	 * Date-time timestamp in timezone local for a place where station is installed.
	 * Mixture of BCD and integer format, this is just sligtly processed RTC registers
	 * content.
	 *	bit 0  - bit 12 === number of minutes starting from midnight (max 1440)
	 *	bit 16 - bit 24 === days from new year (max 356)
	 *	bit 25 - bit 31 === years (from 00 to 99, from 2000 up to 2099)
	 */

#ifdef STM32L471xx

	uint16_t temp = 0;

	// minutes
	temp = 600 * ((RTC->TR & RTC_TR_HT) >> RTC_TR_HT_Pos) +
			60 * ((RTC->TR & RTC_TR_HU) >> RTC_TR_HU_Pos) +
			10 * ((RTC->TR & RTC_TR_MNT) >> RTC_TR_MNT_Pos) +
			 1 * ((RTC->TR & RTC_TR_MNU) >> RTC_TR_MNU_Pos);

	out = out | (temp & 0x7FF);

	// current month
	temp = 	 1 * ((RTC->DR & RTC_DR_MU) >> RTC_DR_MU_Pos) +
			10 * ((RTC->DR & RTC_DR_MT) >> RTC_DR_MT_Pos);

	switch (temp) {
	case 1: temp = 0; break;
	case 2: temp = 31; break;
	case 3: temp = 31 + 28; break;
	case 4:	temp = 31 + 28 + 31; break;
	case 5: temp = 31 + 28 + 31 + 30; break;
	case 6: temp = 31 + 28 + 31 + 30 + 31; break;
	case 7: temp = 31 + 28 + 31 + 30 + 31 + 30; break;
	case 8: temp = 31 + 28 + 31 + 30 + 31 + 30 + 31; break;
	case 9: temp = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31; break;
	case 10:temp = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30; break;
	case 11:temp = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31; break;
	case 12:temp = 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30; break;
	}

	// then add number of days from current month
	temp = temp +
			 1 * ((RTC->DR & RTC_DR_DU) >> RTC_DR_DU_Pos) +
			10 * ((RTC->DR & RTC_DR_DT) >> RTC_DR_DT_Pos);

	out = out | ((temp & 0x1FF) << 16);

	// years
	temp = 	10 * ((RTC->DR & RTC_DR_YT) >> RTC_DR_YT_Pos) +
			 1 * ((RTC->DR & RTC_DR_YU) >> RTC_DR_YU_Pos);

	out = out | ((temp & 0x7F) << 25);

#endif

	return out;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
