// Большая часть функциональности этой библиотеки основана на API VL53L0X,
// предоставленном ST (STSW-IMG005).

#include <stdint.h>
#include "string.h"
#include "VL53L0X.h"
#include "logger.h"
#include "main.h"

//---------------------------------------------------------
// Локальные переменные (общие буферы)
//---------------------------------------------------------
// Эти переменные можно оставить глобальными/статическими, так как они используются
// только во время передачи данных и не хранят состояние конкретного датчика.
#define I2C_TIMEOUT 100 // Таймаут I2C в мс
#define I2C_READ 1
#define I2C_WRITE 0

static uint8_t msgBuffer[4];
static HAL_StatusTypeDef i2cStat;

// Вспомогательная функция для отправки JSON логов через UART
static void debug_log_uart(const char* location, const char* message, const char* data_json) {
  extern UART_HandleTypeDef huart1;
  char log_buf[256];
  int len = snprintf(log_buf, sizeof(log_buf), 
    "{\"id\":\"log_%lu\",\"timestamp\":%lu,\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"sessionId\":\"debug-session\",\"runId\":\"run1\"}\r\n",
    HAL_GetTick(), HAL_GetTick(), location, message, data_json ? data_json : "{}");
  if (len > 0 && len < sizeof(log_buf)) {
    HAL_UART_Transmit(&huart1, (uint8_t*)log_buf, len, 100);
  }
}

//---------------------------------------------------------
// Прототипы локальных функций (приватные)
// Теперь они принимают указатель на dev
//---------------------------------------------------------
bool getSpadInfo(VL53L0X_Dev_t *dev, uint8_t *count, bool *type_is_aperture);
void getSequenceStepEnables(VL53L0X_Dev_t *dev, SequenceStepEnables * enables);
void getSequenceStepTimeouts(VL53L0X_Dev_t *dev, SequenceStepEnables const * enables, SequenceStepTimeouts * timeouts);
bool performSingleRefCalibration(VL53L0X_Dev_t *dev, uint8_t vhv_init_byte);
static uint16_t decodeTimeout(uint16_t value);
static uint16_t encodeTimeout(uint16_t timeout_mclks);
static uint32_t timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks);
static uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks);

//---------------------------------------------------------
// Функции связи I2C
//---------------------------------------------------------

// Запись 8-битного регистра
void writeReg(VL53L0X_Dev_t *dev, uint8_t reg, uint8_t value) {
  // #region agent log
  char data_buf[64];
  snprintf(data_buf, sizeof(data_buf), "{\"reg\":%u,\"value\":%u,\"addr\":%u}", reg, value, dev->i2c_addr);
  debug_log_uart("VL53L0X.c:49", "writeReg entry", data_buf);
  // #endregion
  msgBuffer[0] = value;
  i2cStat = HAL_I2C_Mem_Write(dev->i2c_handler, dev->i2c_addr | I2C_WRITE, reg, 1, msgBuffer, 1, I2C_TIMEOUT);
  // #region agent log
  snprintf(data_buf, sizeof(data_buf), "{\"i2cStat\":%d,\"reg\":%u}", i2cStat, reg);
  debug_log_uart("VL53L0X.c:54", "writeReg exit", data_buf);
  // #endregion
}

// Запись 16-битного регистра
void writeReg16Bit(VL53L0X_Dev_t *dev, uint8_t reg, uint16_t value){
  // В STM32 HAL порядок байт для I2C Mem Write обычно ожидается little-endian,
  // но VL53L0X big-endian. Библиотека ST делает перестановку.
  // Здесь мы копируем как есть.
  // В оригинале: memcpy(msgBuffer, &value, 2); (Little Endian MCU -> Little Endian buffer)
  // Но VL53L0X ожидает Big Endian (MSB first).
  // Если ваш код работал раньше, значит HAL или сам чип переваривали это.
  // Обычно нужно: msgBuffer[0] = (value >> 8) & 0xFF; msgBuffer[1] = value & 0xFF;
  // Оставим как было в вашем оригинале, чтобы не сломать логику, если она работала.

  msgBuffer[0] = (uint8_t)((value >> 8) & 0xFF);
  msgBuffer[1] = (uint8_t)(value & 0xFF);

  i2cStat = HAL_I2C_Mem_Write(dev->i2c_handler, dev->i2c_addr | I2C_WRITE, reg, 1, msgBuffer, 2, I2C_TIMEOUT);
}

// Запись 32-битного регистра
void writeReg32Bit(VL53L0X_Dev_t *dev, uint8_t reg, uint32_t value){
  msgBuffer[0] = (uint8_t)((value >> 24) & 0xFF);
  msgBuffer[1] = (uint8_t)((value >> 16) & 0xFF);
  msgBuffer[2] = (uint8_t)((value >> 8) & 0xFF);
  msgBuffer[3] = (uint8_t)(value & 0xFF);

  i2cStat = HAL_I2C_Mem_Write(dev->i2c_handler, dev->i2c_addr | I2C_WRITE, reg, 1, msgBuffer, 4, I2C_TIMEOUT);
}

// Чтение 8-битного регистра
uint8_t readReg(VL53L0X_Dev_t *dev, uint8_t reg) {
  // #region agent log
  char data_buf[64];
  snprintf(data_buf, sizeof(data_buf), "{\"reg\":%u,\"addr\":%u}", reg, dev->i2c_addr);
  debug_log_uart("VL53L0X.c:87", "readReg entry", data_buf);
  // #endregion
  uint8_t value;
  i2cStat = HAL_I2C_Mem_Read(dev->i2c_handler, dev->i2c_addr | I2C_READ, reg, 1, msgBuffer, 1, I2C_TIMEOUT);
  value = msgBuffer[0];
  // #region agent log
  snprintf(data_buf, sizeof(data_buf), "{\"i2cStat\":%d,\"reg\":%u,\"value\":%u}", i2cStat, reg, value);
  debug_log_uart("VL53L0X.c:94", "readReg exit", data_buf);
  // #endregion
  return value;
}

// Чтение 16-битного регистра
uint16_t readReg16Bit(VL53L0X_Dev_t *dev, uint8_t reg) {
  uint16_t value;
  i2cStat = HAL_I2C_Mem_Read(dev->i2c_handler, dev->i2c_addr | I2C_READ, reg, 1, msgBuffer, 2, I2C_TIMEOUT);
  // Big Endian from sensor
  value = (uint16_t)((msgBuffer[0] << 8) | msgBuffer[1]);
  return value;
}

// Чтение 32-битного регистра
uint32_t readReg32Bit(VL53L0X_Dev_t *dev, uint8_t reg) {
  uint32_t value;
  i2cStat = HAL_I2C_Mem_Read(dev->i2c_handler, dev->i2c_addr | I2C_READ, reg, 1, msgBuffer, 4, I2C_TIMEOUT);
  value = (uint32_t)((msgBuffer[0] << 24) | (msgBuffer[1] << 16) | (msgBuffer[2] << 8) | msgBuffer[3]);
  return value;
}

// Запись произвольного количества байтов
void writeMulti(VL53L0X_Dev_t *dev, uint8_t reg, uint8_t const *src, uint8_t count){
  // Внимание: оригинальный код использовал memcpy в msgBuffer размером 4.
  // Если count > 4, будет переполнение стека!
  // Для безопасности лучше писать напрямую из src, если HAL позволяет, или использовать временный буфер.
  // Здесь используем буфер как в оригинале, но будьте осторожны с count > 4.
  // Для надежности отправим src напрямую:
  i2cStat = HAL_I2C_Mem_Write(dev->i2c_handler, dev->i2c_addr | I2C_WRITE, reg, 1, (uint8_t*)src, count, I2C_TIMEOUT);
}

// Чтение произвольного количества байтов
void readMulti(VL53L0X_Dev_t *dev, uint8_t reg, uint8_t * dst, uint8_t count) {
	i2cStat = HAL_I2C_Mem_Read(dev->i2c_handler, dev->i2c_addr | I2C_READ, reg, 1, dst, count, I2C_TIMEOUT);
}


// Публичные методы //////////////////////////////////////////////////////////////

void setAddress_VL53L0X(VL53L0X_Dev_t *dev, uint8_t new_addr) {
  // Записываем новый адрес в регистр датчика (сдвиг на 1, так как регистр 7-битный + бит RW)
  writeReg(dev, I2C_SLAVE_DEVICE_ADDRESS, (new_addr>>1) & 0x7F );
  // Обновляем адрес в структуре нашего объекта
  dev->i2c_addr = new_addr;
}

uint8_t getAddress_VL53L0X(VL53L0X_Dev_t *dev) {
  return dev->i2c_addr;
}

// Инициализация датчика
bool initVL53L0X(VL53L0X_Dev_t *dev, bool io_2v8, I2C_HandleTypeDef *handler){
  // #region agent log
  char data_buf[64];
  snprintf(data_buf, sizeof(data_buf), "{\"io_2v8\":%d}", io_2v8);
  debug_log_uart("VL53L0X.c:160", "initVL53L0X entry", data_buf);
  // #endregion

  // Инициализация структуры
  dev->i2c_handler = handler;
  dev->i2c_addr = ADDRESS_DEFAULT; // Изначально адрес всегда 0x52 (0x29 * 2)
  dev->io_timeout = 0;
  dev->did_timeout = false;

  // VL53L0X_DataInit() begin

#ifdef __LOGGING__
  save_log(INIT_LOG);
#endif

  // датчик использует режим 1V8 для ввода-вывода по умолчанию; переключение в режим 2V8 при необходимости
  if (io_2v8)
  {
    writeReg(dev, VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
      readReg(dev, VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01); // установка бита 0
  }

  // "Установка стандартного режима I2C"
  writeReg(dev, 0x88, 0x00);

  writeReg(dev, 0x80, 0x01);
  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);
  dev->stop_variable = readReg(dev, 0x91);
  writeReg(dev, 0x00, 0x01);
  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x00);

  // отключение проверки пределов SIGNAL_RATE_MSRC (бит 1) и SIGNAL_RATE_PRE_RANGE (бит 4)
  writeReg(dev, MSRC_CONFIG_CONTROL, readReg(dev, MSRC_CONFIG_CONTROL) | 0x12);

  // установка предела скорости сигнала конечного диапазона на 0.25 MCPS
  setSignalRateLimit(dev, 0.25);

  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0xFF);

  // VL53L0X_StaticInit() begin

  uint8_t spad_count;
  bool spad_type_is_aperture;
  // #region agent log
  debug_log_uart("VL53L0X.c:210", "before getSpadInfo", "{}");
  // #endregion
  if (!getSpadInfo(dev, &spad_count, &spad_type_is_aperture)) {
    // #region agent log
    debug_log_uart("VL53L0X.c:213", "getSpadInfo failed", "{}");
    // #endregion
    return false;
  }

  uint8_t ref_spad_map[6];
  readMulti(dev, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

  // -- VL53L0X_set_reference_spads() begin

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
  writeReg(dev, DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);

  uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0; // 12 - это первый апертурный spad
  uint8_t spads_enabled = 0;

  for (uint8_t i = 0; i < 48; i++)
  {
    if (i < first_spad_to_enable || spads_enabled == spad_count)
    {
      ref_spad_map[i / 8] &= ~(1 << (i % 8));
    }
    else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1)
    {
      spads_enabled++;
    }
  }

  writeMulti(dev, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

  // -- VL53L0X_load_tuning_settings() begin

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x09, 0x00);
  writeReg(dev, 0x10, 0x00);
  writeReg(dev, 0x11, 0x00);

  writeReg(dev, 0x24, 0x01);
  writeReg(dev, 0x25, 0xFF);
  writeReg(dev, 0x75, 0x00);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x4E, 0x2C);
  writeReg(dev, 0x48, 0x00);
  writeReg(dev, 0x30, 0x20);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x30, 0x09);
  writeReg(dev, 0x54, 0x00);
  writeReg(dev, 0x31, 0x04);
  writeReg(dev, 0x32, 0x03);
  writeReg(dev, 0x40, 0x83);
  writeReg(dev, 0x46, 0x25);
  writeReg(dev, 0x60, 0x00);
  writeReg(dev, 0x27, 0x00);
  writeReg(dev, 0x50, 0x06);
  writeReg(dev, 0x51, 0x00);
  writeReg(dev, 0x52, 0x96);
  writeReg(dev, 0x56, 0x08);
  writeReg(dev, 0x57, 0x30);
  writeReg(dev, 0x61, 0x00);
  writeReg(dev, 0x62, 0x00);
  writeReg(dev, 0x64, 0x00);
  writeReg(dev, 0x65, 0x00);
  writeReg(dev, 0x66, 0xA0);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x22, 0x32);
  writeReg(dev, 0x47, 0x14);
  writeReg(dev, 0x49, 0xFF);
  writeReg(dev, 0x4A, 0x00);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x7A, 0x0A);
  writeReg(dev, 0x7B, 0x00);
  writeReg(dev, 0x78, 0x21);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x23, 0x34);
  writeReg(dev, 0x42, 0x00);
  writeReg(dev, 0x44, 0xFF);
  writeReg(dev, 0x45, 0x26);
  writeReg(dev, 0x46, 0x05);
  writeReg(dev, 0x40, 0x40);
  writeReg(dev, 0x0E, 0x06);
  writeReg(dev, 0x20, 0x1A);
  writeReg(dev, 0x43, 0x40);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x34, 0x03);
  writeReg(dev, 0x35, 0x44);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x31, 0x04);
  writeReg(dev, 0x4B, 0x09);
  writeReg(dev, 0x4C, 0x05);
  writeReg(dev, 0x4D, 0x04);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x44, 0x00);
  writeReg(dev, 0x45, 0x20);
  writeReg(dev, 0x47, 0x08);
  writeReg(dev, 0x48, 0x28);
  writeReg(dev, 0x67, 0x00);
  writeReg(dev, 0x70, 0x04);
  writeReg(dev, 0x71, 0x01);
  writeReg(dev, 0x72, 0xFE);
  writeReg(dev, 0x76, 0x00);
  writeReg(dev, 0x77, 0x00);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x0D, 0x01);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x01);
  writeReg(dev, 0x01, 0xF8);

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x8E, 0x01);
  writeReg(dev, 0x00, 0x01);
  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x00);

  // -- VL53L0X_load_tuning_settings() конец

  // "Установка конфигурации прерывания на готовность нового сэмпла"
  writeReg(dev, SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
  writeReg(dev, GPIO_HV_MUX_ACTIVE_HIGH, readReg(dev, GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10); // активный низкий
  writeReg(dev, SYSTEM_INTERRUPT_CLEAR, 0x01);

  dev->measurement_timing_budget_us = getMeasurementTimingBudget(dev);

  // "Отключить MSRC и TCC по умолчанию"
  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0xE8);

  // "Пересчет бюджета времени"
  setMeasurementTimingBudget(dev, dev->measurement_timing_budget_us);

  // VL53L0X_StaticInit() конец

  // VL53L0X_PerformRefCalibration() начало

  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0x01);
  // #region agent log
  debug_log_uart("VL53L0X.c:372", "before calibration 1", "{}");
  // #endregion
  if (!performSingleRefCalibration(dev, 0x40)) {
    // #region agent log
    debug_log_uart("VL53L0X.c:375", "calibration 1 failed", "{}");
    // #endregion
    return false;
  }

  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0x02);
  // #region agent log
  debug_log_uart("VL53L0X.c:382", "before calibration 2", "{}");
  // #endregion
  if (!performSingleRefCalibration(dev, 0x00)) {
    // #region agent log
    debug_log_uart("VL53L0X.c:386", "calibration 2 failed", "{}");
    // #endregion
    return false;
  }

  // "восстановление предыдущей конфигурации последовательности"
  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0xE8);

  // VL53L0X_PerformRefCalibration() конец

  // #region agent log
  debug_log_uart("VL53L0X.c:393", "initVL53L0X success", "{}");
  // #endregion
  return true;
}

// Установка значения проверки предела скорости обратного сигнала
uint8_t setSignalRateLimit(VL53L0X_Dev_t *dev, float limit_Mcps)
{
#ifdef __LOGGING__
  save_log(VLX_PULSE_PERIOD);
#endif
  if (limit_Mcps < 0 || limit_Mcps > 511.99) { return false; }

  // формат с фиксированной точкой Q9.7 (9 целых бит, 7 дробных бит)
  writeReg16Bit(dev, FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, limit_Mcps * (1 << 7));
  return true;
}

// Получение значения предела скорости обратного сигнала
float getSignalRateLimit(VL53L0X_Dev_t *dev)
{
  return (float)readReg16Bit(dev, FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT) / (1 << 7);
}

// Установка бюджета времени измерения
uint8_t setMeasurementTimingBudget(VL53L0X_Dev_t *dev, uint32_t budget_us)
{
#ifdef __LOGGING__
  save_log(VLX_TIME_BETWEEN_MEAS);
#endif
  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;

  uint16_t const StartOverhead      = 1320;
  uint16_t const EndOverhead        = 960;
  uint16_t const MsrcOverhead       = 660;
  uint16_t const TccOverhead        = 590;
  uint16_t const DssOverhead        = 690;
  uint16_t const PreRangeOverhead   = 660;
  uint16_t const FinalRangeOverhead = 550;

  uint32_t const MinTimingBudget = 20000;

  if (budget_us < MinTimingBudget) { return false; }

  uint32_t used_budget_us = StartOverhead + EndOverhead;

  getSequenceStepEnables(dev, &enables);
  getSequenceStepTimeouts(dev, &enables, &timeouts);

  if (enables.tcc)
  {
    used_budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }

  if (enables.dss)
  {
    used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  }
  else if (enables.msrc)
  {
    used_budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }

  if (enables.pre_range)
  {
    used_budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }

  if (enables.final_range)
  {
    used_budget_us += FinalRangeOverhead;

    if (used_budget_us > budget_us)
    {
      return false;
    }

    uint32_t final_range_timeout_us = budget_us - used_budget_us;

    // set_sequence_step_timeout() begin
    uint16_t final_range_timeout_mclks =
      timeoutMicrosecondsToMclks(final_range_timeout_us,
                                 timeouts.final_range_vcsel_period_pclks);

    if (enables.pre_range)
    {
      final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    writeReg16Bit(dev, FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      encodeTimeout(final_range_timeout_mclks));

    dev->measurement_timing_budget_us = budget_us;
  }
  return true;
}

// Получение бюджета времени измерения
uint32_t getMeasurementTimingBudget(VL53L0X_Dev_t *dev)
{
  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;

  uint16_t const StartOverhead     = 1910;
  uint16_t const EndOverhead        = 960;
  uint16_t const MsrcOverhead       = 660;
  uint16_t const TccOverhead        = 590;
  uint16_t const DssOverhead        = 690;
  uint16_t const PreRangeOverhead   = 660;
  uint16_t const FinalRangeOverhead = 550;

  uint32_t budget_us = StartOverhead + EndOverhead;

  getSequenceStepEnables(dev, &enables);
  getSequenceStepTimeouts(dev, &enables, &timeouts);

  if (enables.tcc)
  {
    budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }

  if (enables.dss)
  {
    budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  }
  else if (enables.msrc)
  {
    budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }

  if (enables.pre_range)
  {
    budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }

  if (enables.final_range)
  {
    budget_us += (timeouts.final_range_us + FinalRangeOverhead);
  }

  dev->measurement_timing_budget_us = budget_us;
  return budget_us;
}

// Установка периода импульса VCSEL
uint8_t setVcselPulsePeriod(VL53L0X_Dev_t *dev, vcselPeriodType type, uint8_t period_pclks)
{
  uint8_t vcsel_period_reg = encodeVcselPeriod(period_pclks);

  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;

  getSequenceStepEnables(dev, &enables);
  getSequenceStepTimeouts(dev, &enables, &timeouts);

  if (type == VcselPeriodPreRange)
  {
    switch (period_pclks)
    {
      case 12:
        writeReg(dev, PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x18);
        break;
      case 14:
        writeReg(dev, PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x30);
        break;
      case 16:
        writeReg(dev, PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x40);
        break;
      case 18:
        writeReg(dev, PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x50);
        break;
      default:
        return false;
    }
    writeReg(dev, PRE_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);

    writeReg(dev, PRE_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

    uint16_t new_pre_range_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.pre_range_us, period_pclks);

    writeReg16Bit(dev, PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      encodeTimeout(new_pre_range_timeout_mclks));

    uint16_t new_msrc_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.msrc_dss_tcc_us, period_pclks);

    writeReg(dev, MSRC_CONFIG_TIMEOUT_MACROP,
      (new_msrc_timeout_mclks > 256) ? 255 : (new_msrc_timeout_mclks - 1));
  }
  else if (type == VcselPeriodFinalRange)
  {
    switch (period_pclks)
    {
      case 8:
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x10);
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        writeReg(dev, GLOBAL_CONFIG_VCSEL_WIDTH, 0x02);
        writeReg(dev, ALGO_PHASECAL_CONFIG_TIMEOUT, 0x0C);
        writeReg(dev, 0xFF, 0x01);
        writeReg(dev, ALGO_PHASECAL_LIM, 0x30);
        writeReg(dev, 0xFF, 0x00);
        break;

      case 10:
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x28);
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        writeReg(dev, GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        writeReg(dev, ALGO_PHASECAL_CONFIG_TIMEOUT, 0x09);
        writeReg(dev, 0xFF, 0x01);
        writeReg(dev, ALGO_PHASECAL_LIM, 0x20);
        writeReg(dev, 0xFF, 0x00);
        break;

      case 12:
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        writeReg(dev, GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        writeReg(dev, ALGO_PHASECAL_CONFIG_TIMEOUT, 0x08);
        writeReg(dev, 0xFF, 0x01);
        writeReg(dev, ALGO_PHASECAL_LIM, 0x20);
        writeReg(dev, 0xFF, 0x00);
        break;

      case 14:
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x48);
        writeReg(dev, FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        writeReg(dev, GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        writeReg(dev, ALGO_PHASECAL_CONFIG_TIMEOUT, 0x07);
        writeReg(dev, 0xFF, 0x01);
        writeReg(dev, ALGO_PHASECAL_LIM, 0x20);
        writeReg(dev, 0xFF, 0x00);
        break;

      default:
        return false;
    }

    writeReg(dev, FINAL_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

    uint16_t new_final_range_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.final_range_us, period_pclks);

    if (enables.pre_range)
    {
      new_final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    writeReg16Bit(dev, FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      encodeTimeout(new_final_range_timeout_mclks));
  }
  else
  {
    return false;
  }

  setMeasurementTimingBudget(dev, dev->measurement_timing_budget_us);

  // "Выполнение калибровки фазы"
  uint8_t sequence_config = readReg(dev, SYSTEM_SEQUENCE_CONFIG);
  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, 0x02);
  performSingleRefCalibration(dev, 0x0);
  writeReg(dev, SYSTEM_SEQUENCE_CONFIG, sequence_config);

  return true;
}

// Получение периода импульса VCSEL
uint8_t getVcselPulsePeriod(VL53L0X_Dev_t *dev, vcselPeriodType type)
{
  if (type == VcselPeriodPreRange)
  {
    return decodeVcselPeriod(readReg(dev, PRE_RANGE_CONFIG_VCSEL_PERIOD));
  }
  else if (type == VcselPeriodFinalRange)
  {
    return decodeVcselPeriod(readReg(dev, FINAL_RANGE_CONFIG_VCSEL_PERIOD));
  }
  else { return 255; }
}

// Запуск непрерывных измерений
void startContinuous(VL53L0X_Dev_t *dev, uint32_t period_ms)
{
  writeReg(dev, 0x80, 0x01);
  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);
  writeReg(dev, 0x91, dev->stop_variable);
  writeReg(dev, 0x00, 0x01);
  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x00);

  if (period_ms != 0)
  {
    // непрерывный режим по таймеру
    uint16_t osc_calibrate_val = readReg16Bit(dev, OSC_CALIBRATE_VAL);

    if (osc_calibrate_val != 0)
    {
      period_ms *= osc_calibrate_val;
    }

    writeReg32Bit(dev, SYSTEM_INTERMEASUREMENT_PERIOD, period_ms);
    writeReg(dev, SYSRANGE_START, 0x04); // TIMED MODE
  }
  else
  {
    // непрерывный последовательный режим (back-to-back)
    writeReg(dev, SYSRANGE_START, 0x02); // BACKTOBACK MODE
  }
}

// Остановка непрерывных измерений
void stopContinuous(VL53L0X_Dev_t *dev)
{
  writeReg(dev, SYSRANGE_START, 0x01); // SINGLESHOT

  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);
  writeReg(dev, 0x91, 0x00);
  writeReg(dev, 0x00, 0x01);
  writeReg(dev, 0xFF, 0x00);
}

// Возвращает показание дальности (вспомогательная функция для continuous режима)
// Примечание: В оригинале это была глобальная функция. Для совместимости с новым
// Single Shot, она тоже должна принимать dev.
uint16_t readRangeContinuousMillimeters(VL53L0X_Dev_t *dev, statInfo_t_VL53L0X *extraStats) {
  uint8_t tempBuffer[12];
  uint16_t temp;

  // startTimeout logic manually implemented
  dev->timeout_start_ms = HAL_GetTick();

  while ((readReg(dev, RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
    // checkTimeoutExpired logic
    if (dev->io_timeout > 0 && ((uint16_t)HAL_GetTick() - dev->timeout_start_ms) > dev->io_timeout)
    {
      dev->did_timeout = true;
      return 65535;
    }
  }

  if( extraStats == 0 ){
    temp = readReg16Bit(dev, RESULT_RANGE_STATUS + 10);
  } else {
    readMulti(dev, 0x14, tempBuffer, 12);
    extraStats->rangeStatus =  tempBuffer[0x00]>>3;
    extraStats->spadCnt     = (tempBuffer[0x02]<<8) | tempBuffer[0x03];
    extraStats->signalCnt   = (tempBuffer[0x06]<<8) | tempBuffer[0x07];
    extraStats->ambientCnt  = (tempBuffer[0x08]<<8) | tempBuffer[0x09];    
    temp                    = (tempBuffer[0x0A]<<8) | tempBuffer[0x0B];
    extraStats->rawDistance = temp;
  }

  writeReg(dev, SYSTEM_INTERRUPT_CLEAR, 0x01);
  return temp;
}

// Выполняет однократное измерение дальности
uint16_t readRangeSingleMillimeters(VL53L0X_Dev_t *dev, statInfo_t_VL53L0X *extraStats) {
#ifdef __LOGGING__
  save_log(VLX_READ_DATA);
#endif
  writeReg(dev, 0x80, 0x01);
  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);
  writeReg(dev, 0x91, dev->stop_variable);
  writeReg(dev, 0x00, 0x01);
  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x00);
  writeReg(dev, SYSRANGE_START, 0x01);

  // "Ждать, пока стартовый бит не будет очищен"
  dev->timeout_start_ms = HAL_GetTick();

  while (readReg(dev, SYSRANGE_START) & 0x01){
    if (dev->io_timeout > 0 && ((uint16_t)HAL_GetTick() - dev->timeout_start_ms) > dev->io_timeout){
      dev->did_timeout = true;
      return 65535;
    }
  }

  return readRangeContinuousMillimeters(dev, extraStats);
}

// Произошел ли таймаут
bool timeoutOccurred(VL53L0X_Dev_t *dev)
{
  bool tmp = dev->did_timeout;
  dev->did_timeout = false;
  return tmp;
}

void setTimeout(VL53L0X_Dev_t *dev, uint16_t timeout){
  dev->io_timeout = timeout;
}

uint16_t getTimeout(VL53L0X_Dev_t *dev){
  return dev->io_timeout;
}

// Приватные методы /////////////////////////////////////////////////////////////

bool getSpadInfo(VL53L0X_Dev_t *dev, uint8_t * count, bool * type_is_aperture)
{
  uint8_t tmp;

  writeReg(dev, 0x80, 0x01);
  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x00);

  writeReg(dev, 0xFF, 0x06);
  writeReg(dev, 0x83, readReg(dev, 0x83) | 0x04);
  writeReg(dev, 0xFF, 0x07);
  writeReg(dev, 0x81, 0x01);

  writeReg(dev, 0x80, 0x01);

  writeReg(dev, 0x94, 0x6b);
  writeReg(dev, 0x83, 0x00);

  dev->timeout_start_ms = HAL_GetTick();
  while (readReg(dev, 0x83) == 0x00)
  {
    if (dev->io_timeout > 0 && ((uint16_t)HAL_GetTick() - dev->timeout_start_ms) > dev->io_timeout) { return false; }
  }

  writeReg(dev, 0x83, 0x01);
  tmp = readReg(dev, 0x92);

  *count = tmp & 0x7f;
  *type_is_aperture = (tmp >> 7) & 0x01;

  writeReg(dev, 0x81, 0x00);
  writeReg(dev, 0xFF, 0x06);
  writeReg(dev, 0x83, readReg(dev, 0x83)  & ~0x04);
  writeReg(dev, 0xFF, 0x01);
  writeReg(dev, 0x00, 0x01);

  writeReg(dev, 0xFF, 0x00);
  writeReg(dev, 0x80, 0x00);

  return true;
}

void getSequenceStepEnables(VL53L0X_Dev_t *dev, SequenceStepEnables * enables)
{
  uint8_t sequence_config = readReg(dev, SYSTEM_SEQUENCE_CONFIG);

  enables->tcc          = (sequence_config >> 4) & 0x1;
  enables->dss          = (sequence_config >> 3) & 0x1;
  enables->msrc         = (sequence_config >> 2) & 0x1;
  enables->pre_range    = (sequence_config >> 6) & 0x1;
  enables->final_range  = (sequence_config >> 7) & 0x1;
}

void getSequenceStepTimeouts(VL53L0X_Dev_t *dev, SequenceStepEnables const * enables, SequenceStepTimeouts * timeouts)
{
  timeouts->pre_range_vcsel_period_pclks = getVcselPulsePeriod(dev, VcselPeriodPreRange);

  timeouts->msrc_dss_tcc_mclks = readReg(dev, MSRC_CONFIG_TIMEOUT_MACROP) + 1;
  timeouts->msrc_dss_tcc_us =
    timeoutMclksToMicroseconds(timeouts->msrc_dss_tcc_mclks,
                               timeouts->pre_range_vcsel_period_pclks);

  timeouts->pre_range_mclks =
    decodeTimeout(readReg16Bit(dev, PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
  timeouts->pre_range_us =
    timeoutMclksToMicroseconds(timeouts->pre_range_mclks,
                               timeouts->pre_range_vcsel_period_pclks);

  timeouts->final_range_vcsel_period_pclks = getVcselPulsePeriod(dev, VcselPeriodFinalRange);

  timeouts->final_range_mclks =
    decodeTimeout(readReg16Bit(dev, FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));

  if (enables->pre_range)
  {
    timeouts->final_range_mclks -= timeouts->pre_range_mclks;
  }

  timeouts->final_range_us =
    timeoutMclksToMicroseconds(timeouts->final_range_mclks,
                               timeouts->final_range_vcsel_period_pclks);
}

// Декодирование таймаута
uint16_t decodeTimeout(uint16_t reg_val)
{
  // формат: "(LSByte * 2^MSByte) + 1"
  return (uint16_t)((reg_val & 0x00FF) <<
         (uint16_t)((reg_val & 0xFF00) >> 8)) + 1;
}

// Кодирование таймаута
uint16_t encodeTimeout(uint16_t timeout_mclks)
{
  uint32_t ls_byte = 0;
  uint16_t ms_byte = 0;

  if (timeout_mclks > 0)
  {
    ls_byte = timeout_mclks - 1;

    while ((ls_byte & 0xFFFFFF00) > 0)
    {
      ls_byte >>= 1;
      ms_byte++;
    }

    return (ms_byte << 8) | (ls_byte & 0xFF);
  }
  else { return 0; }
}

// Конвертация MCLK в микросекунды
uint32_t timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks)
{
  uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);
  return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;
}

// Конвертация микросекунд в MCLK
uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks)
{
  uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);
  return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}

// Калибровка референса
bool performSingleRefCalibration(VL53L0X_Dev_t *dev, uint8_t vhv_init_byte)
{
  writeReg(dev, SYSRANGE_START, 0x01 | vhv_init_byte);

  dev->timeout_start_ms = HAL_GetTick();
  while ((readReg(dev, RESULT_INTERRUPT_STATUS) & 0x07) == 0)
  {
    if (dev->io_timeout > 0 && ((uint16_t)HAL_GetTick() - dev->timeout_start_ms) > dev->io_timeout) { return false; }
  }

  writeReg(dev, SYSTEM_INTERRUPT_CLEAR, 0x01);
  writeReg(dev, SYSRANGE_START, 0x00);

  return true;
}
