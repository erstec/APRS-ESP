
static bool rx_att = false;

void AFSK_setRxAtt(bool _rx_att) {
  rx_att = _rx_att;
}

#if !defined(BOARD_TTWR)

#include <string.h>
#include "AFSK.h"
#include "Arduino.h"
#include <Wire.h>
#include <driver/adc.h>
#include <driver/i2s.h>
#include "esp_adc_cal.h"
#include "cppQueue.h"
#include "fir_filter.h"

extern "C"
{
#include "soc/syscon_reg.h"
#include "soc/syscon_struct.h"
}

#define DEBUG_TNC

extern unsigned long custom_preamble;
extern unsigned long custom_tail;
int adcVal;

bool input_HPF = false;

static const adc_unit_t unit = ADC_UNIT_1;

void sample_isr();
bool hw_afsk_dac_isr = false;

static filter_t bpf;
static filter_t lpf;

Afsk *AFSK_modem;

uint8_t CountOnesFromInteger(uint8_t value)
{
  uint8_t count;
  for (count = 0; value != 0; count++, value &= value - 1)
    ;
  return count;
}

#define IMPLEMENTATION FIFO

#ifndef I2S_INTERNAL
cppQueue adcq(sizeof(int8_t), 19200, IMPLEMENTATION); // Instantiate queue
#endif

#ifndef RTC_MODULE_TAG
#define RTC_MODULE_TAG "RTC_MODULE"
#endif

#ifdef I2S_INTERNAL
#define ADC_PATT_LEN_MAX (16)
#define ADC_CHECK_UNIT(unit) RTC_MODULE_CHECK(adc_unit < ADC_UNIT_2, "ADC unit error, only support ADC1 for now", ESP_ERR_INVALID_ARG)
#define RTC_MODULE_CHECK(a, str, ret_val)                                             \
  if (!(a))                                                                           \
  {                                                                                   \
    ESP_LOGE(RTC_MODULE_TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret_val);                                                                 \
  }

i2s_event_t i2s_evt;
static QueueHandle_t i2s_event_queue;

#if defined(CONFIG_IDF_TARGET_ESP32)
static esp_err_t adc_set_i2s_data_len(adc_unit_t adc_unit, int patt_len)
{
  ADC_CHECK_UNIT(adc_unit);
  RTC_MODULE_CHECK((patt_len < ADC_PATT_LEN_MAX) && (patt_len > 0), "ADC pattern length error", ESP_ERR_INVALID_ARG);
  // portENTER_CRITICAL(&rtc_spinlock);
  if (adc_unit & ADC_UNIT_1)
  {
    SYSCON.saradc_ctrl.sar1_patt_len = patt_len - 1;
  }
  if (adc_unit & ADC_UNIT_2)
  {
    SYSCON.saradc_ctrl.sar2_patt_len = patt_len - 1;
  }
  // portEXIT_CRITICAL(&rtc_spinlock);
  return ESP_OK;
}

static esp_err_t adc_set_i2s_data_pattern(adc_unit_t adc_unit, int seq_num, adc_channel_t channel, adc_bits_width_t bits, adc_atten_t atten)
{
  ADC_CHECK_UNIT(adc_unit);
  if (adc_unit & ADC_UNIT_1)
  {
    RTC_MODULE_CHECK((adc1_channel_t)channel < ADC1_CHANNEL_MAX, "ADC1 channel error", ESP_ERR_INVALID_ARG);
  }
  RTC_MODULE_CHECK(bits < ADC_WIDTH_MAX, "ADC bit width error", ESP_ERR_INVALID_ARG);
  RTC_MODULE_CHECK(atten < ADC_ATTEN_MAX, "ADC Atten Err", ESP_ERR_INVALID_ARG);

  // portENTER_CRITICAL(&rtc_spinlock);
  // Configure pattern table, each 8 bit defines one channel
  //[7:4]-channel [3:2]-bit width [1:0]- attenuation
  // BIT WIDTH: 3: 12BIT  2: 11BIT  1: 10BIT  0: 9BIT
  // ATTEN: 3: ATTEN = 11dB 2: 6dB 1: 2.5dB 0: 0dB
  uint8_t val = (channel << 4) | (bits << 2) | (atten << 0);
  if (adc_unit & ADC_UNIT_1)
  {
    SYSCON.saradc_sar1_patt_tab[seq_num / 4] &= (~(0xff << ((3 - (seq_num % 4)) * 8)));
    SYSCON.saradc_sar1_patt_tab[seq_num / 4] |= (val << ((3 - (seq_num % 4)) * 8));
  }
  if (adc_unit & ADC_UNIT_2)
  {
    SYSCON.saradc_sar2_patt_tab[seq_num / 4] &= (~(0xff << ((3 - (seq_num % 4)) * 8)));
    SYSCON.saradc_sar2_patt_tab[seq_num / 4] |= (val << ((3 - (seq_num % 4)) * 8));
  }
  // portEXIT_CRITICAL(&rtc_spinlock);
  return ESP_OK;
}
#endif /* CONFIG_IDF_TARGET_ESP32 */

void I2S_Init(i2s_mode_t MODE, i2s_bits_per_sample_t BPS)
{
  i2s_config_t i2s_config = {
#if defined(CONFIG_IDF_TARGET_ESP32)
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN | I2S_MODE_ADC_BUILT_IN),
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
#else
#error "This ESP32 variant is not supported!"
#endif
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
#if defined MONO_OUT
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#else
      .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB,
#else
      .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S_MSB,
#endif
      .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 5,
      .dma_buf_len = 768,
      //.tx_desc_auto_clear   = true,
      .use_apll = false // no Audio PLL ( I dont need the adc to be accurate )
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
//      .mclk_multiple = I2S_MCLK_MULTIPLE_256, // Unused
//      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT // Use bits per sample
#endif
  };

  if (MODE == I2S_MODE_RX || MODE == I2S_MODE_TX)
  {
    log_i("Using I2S_MODE");
    i2s_pin_config_t pin_config;
    pin_config.bck_io_num = PIN_I2S_BCLK;
    pin_config.ws_io_num = PIN_I2S_LRC;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
//    pin_config.mck_io_num = PIN_I2S_MCLK;
#endif

    if (MODE == I2S_MODE_RX)
    {
      pin_config.data_out_num = I2S_PIN_NO_CHANGE;
      pin_config.data_in_num = PIN_I2S_DIN;
    }
    else if (MODE == I2S_MODE_TX)
    {
      pin_config.data_out_num = PIN_I2S_DOUT;
      pin_config.data_in_num = I2S_PIN_NO_CHANGE;
    }

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BPS, I2S_CHANNEL_MONO);
  }
#if defined(CONFIG_IDF_TARGET_ESP32)
  else if (MODE == I2S_MODE_DAC_BUILT_IN || MODE == I2S_MODE_ADC_BUILT_IN)
  {
    log_i("Using I2S_DAC/ADC_BUILTIN");
    // install and start i2s driver
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 5, &i2s_event_queue) != ESP_OK)
    {
      log_e("ERROR: Unable to install I2S drives");
    }
    // GPIO36, VP
    // init ADC pad
    i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0);
    // i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BPS, I2S_CHANNEL_MONO);
    i2s_adc_enable(I2S_NUM_0);
    delay(500); // required for stability of ADC

    // ***IMPORTANT*** enable continuous adc sampling
    SYSCON.saradc_ctrl2.meas_num_limit = 0;
    // sample time SAR setting
    SYSCON.saradc_ctrl.sar_clk_div = 2;
    SYSCON.saradc_fsm.sample_cycle = 2;
    adc_set_i2s_data_pattern(ADC_UNIT_1, 0, ADC_CHANNEL_0, ADC_WIDTH_BIT_12, ADC_ATTEN_DB_0); // Input Vref 1.36V=4095,Offset 0.6324V=1744
    adc_set_i2s_data_len(ADC_UNIT_1, 1);

    i2s_set_pin(I2S_NUM_0, NULL);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN); // IO26
    i2s_zero_dma_buffer(I2S_NUM_0);
    // i2s_start(I2S_NUM_0);
    //  dac_output_enable(DAC_CHANNEL_1);
    dac_output_enable(DAC_CHANNEL_2);
    dac_i2s_enable();
  }
#endif /* CONFIG_IDF_TARGET_ESP32 */
}

#else

hw_timer_t *timer = NULL;

void AFSK_TimerEnable(bool sts)
{
  if (sts == true)
    timerAlarmEnable(timer);
  else
    timerAlarmDisable(timer);
}

#endif

void AFSK_hw_init(void)
{
  // Set up ADC
  pinMode(PTT_PIN, OUTPUT);

#if defined(TX_LED_PIN)
  pinMode(TX_LED_PIN, OUTPUT);
#endif

  pinMode(RX_LED_PIN, OUTPUT);
  RX_LED_OFF();

#if defined(INVERT_PTT)
  digitalWrite(PTT_PIN, HIGH);
#else
  digitalWrite(PTT_PIN, LOW);
#endif

#ifdef I2S_INTERNAL
#if defined(CONFIG_IDF_TARGET_ESP32)
  //  Initialize the I2S peripheral
  I2S_Init(I2S_MODE_DAC_BUILT_IN, I2S_BITS_PER_SAMPLE_16BIT);
#else
  /* TBD */
#endif /* CONFIG_IDF_TARGET_ESP32 */
#else
  pinMode(MIC_PIN, INPUT);
  // Init ADC and Characteristics
  // esp_adc_cal_characteristics_t characteristics;
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(SPK_PIN, ADC_ATTEN_DB_0); // Input 1.24Vp-p,Use R 33K-(10K//10K) divider input power 1.2Vref
  // adc1_config_channel_atten(SPK_PIN, ADC_ATTEN_DB_11); //Input 3.3Vp-p,Use R 10K divider input power 3.3V
  // esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, &characteristics);
  // Serial.printf("v_ref routed to 3.3V\n");

  // Start a timer at 9.6kHz to sample the ADC and play the audio on the DAC.
  timer = timerBegin(3, 10, true);                // 80 MHz / 10 = 8MHz hardware clock
  timerAttachInterrupt(timer, &sample_isr, true); // Attaches the handler function to the timer
  timerAlarmWrite(timer, 834, true);              // Interrupts when counter == 834, 9592.3 times a second

  // timerAlarmEnable(timer);
#endif
}

void AFSK_init(Afsk *afsk)
{
  // Allocate modem struct memory
  memset(afsk, 0, sizeof(*afsk));
  AFSK_modem = afsk;
  // Set phase increment
  afsk->phaseInc = MARK_INC;
  // Initialise FIFO buffers
  fifo_init(&afsk->rxFifo, afsk->rxBuf, sizeof(afsk->rxBuf));
  fifo_init(&afsk->txFifo, afsk->txBuf, sizeof(afsk->txBuf));

  // filter initialize
  filter_param_t flt = {
      .size = FIR_LPF_N,
      .sampling_freq = SAMPLERATE,
      .pass_freq = 0,
      .cutoff_freq = 1200,
  };
  int16_t *lpf_an, *bpf_an;
  // LPF
  lpf_an = filter_coeff(&flt);
  // BPF
  flt.size = FIR_BPF_N;
  flt.pass_freq = 1000;
  flt.cutoff_freq = 2700;
  bpf_an = filter_coeff(&flt);

  // LPF
  filter_init(&lpf, lpf_an, FIR_LPF_N);
  // BPF
  filter_init(&bpf, bpf_an, FIR_BPF_N);

  AFSK_hw_init();
}

static void AFSK_txStart(Afsk *afsk)
{
  if (!afsk->sending)
  {
    fifo_flush(&AFSK_modem->txFifo);
    // log_i("TX Start");
    afsk->sending = true;
    afsk->phaseInc = MARK_INC;
    afsk->phaseAcc = 0;
    afsk->bitstuffCount = 0;
#if defined(INVERT_PTT)
    digitalWrite(PTT_PIN, LOW);
#else
    digitalWrite(PTT_PIN, HIGH);
#endif
    afsk->preambleLength = DIV_ROUND(custom_preamble * BITRATE, 9600);
    AFSK_DAC_IRQ_START();
#if defined(I2S_INTERNAL) && defined(CONFIG_IDF_TARGET_ESP32)
    i2s_zero_dma_buffer(I2S_NUM_0);
    // i2s_adc_disable(I2S_NUM_0);
    dac_i2s_enable();
    // i2s_start(I2S_NUM_0);
#endif
  }
  noInterrupts();
  afsk->tailLength = DIV_ROUND(custom_tail * BITRATE, 9600);
  interrupts();
}

void afsk_putchar(char c)
{
  AFSK_txStart(AFSK_modem);
  while (fifo_isfull_locked(&AFSK_modem->txFifo))
  {
    /* Wait */
    // delay(10);
  }
  fifo_push_locked(&AFSK_modem->txFifo, c);
}

int afsk_getchar(void)
{
  if (fifo_isempty_locked(&AFSK_modem->rxFifo))
  {
    return EOF;
  }
  else
  {
    return fifo_pop_locked(&AFSK_modem->rxFifo);
  }
}

void AFSK_transmit(char *buffer, size_t size)
{
  fifo_flush(&AFSK_modem->txFifo);
  int i = 0;
  while (size--)
  {
    afsk_putchar(buffer[i++]);
  }
}

uint8_t AFSK_dac_isr(Afsk *afsk)
{
  if (afsk->sampleIndex == 0)
  {
    // RX_LED_ON();
    // digitalWrite(RX_LED_PIN), LOW);
    if (afsk->txBit == 0)
    {
      if (fifo_isempty(&afsk->txFifo) && afsk->tailLength == 0)
      {
        AFSK_DAC_IRQ_STOP();
        afsk->sending = false;
        // LED_TX_OFF();
#if defined(INVERT_PTT)
        // digitalWrite(PTT_PIN, HIGH);
#else
        // digitalWrite(PTT_PIN, LOW);
#endif
        return 0;
      }
      else
      {
        if (!afsk->bitStuff)
          afsk->bitstuffCount = 0;
        afsk->bitStuff = true;
        if (afsk->preambleLength == 0)
        {
          if (fifo_isempty(&afsk->txFifo))
          {
            afsk->tailLength--;
            afsk->currentOutputByte = HDLC_FLAG;
          }
          else
          {
            afsk->currentOutputByte = fifo_pop(&afsk->txFifo);
          }
        }
        else
        {
          afsk->preambleLength--;
          afsk->currentOutputByte = HDLC_FLAG;
        }
        if (afsk->currentOutputByte == AX25_ESC)
        {
          if (fifo_isempty(&afsk->txFifo))
          {
            AFSK_DAC_IRQ_STOP();
            afsk->sending = false;
            // LED_TX_OFF();
#if defined(INVERT_PTT)
            // digitalWrite(PTT_PIN, HIGH);
#else
            // digitalWrite(PTT_PIN, LOW);
#endif
            return 0;
          }
          else
          {
            afsk->currentOutputByte = fifo_pop(&afsk->txFifo);
          }
        }
        else if (afsk->currentOutputByte == HDLC_FLAG || afsk->currentOutputByte == HDLC_RESET)
        {
          afsk->bitStuff = false;
        }
      }
      afsk->txBit = 0x01;
    }

    if (afsk->bitStuff && afsk->bitstuffCount >= BIT_STUFF_LEN)
    {
      afsk->bitstuffCount = 0;
      afsk->phaseInc = SWITCH_TONE(afsk->phaseInc);
    }
    else
    {
      if (afsk->currentOutputByte & afsk->txBit)
      {
        afsk->bitstuffCount++;
      }
      else
      {
        afsk->bitstuffCount = 0;
        afsk->phaseInc = SWITCH_TONE(afsk->phaseInc);
      }
      afsk->txBit <<= 1;
    }

    afsk->sampleIndex = SAMPLESPERBIT;
  }

  afsk->phaseAcc += afsk->phaseInc;
  afsk->phaseAcc %= SIN_LEN;
  if (afsk->sampleIndex > 0)
    afsk->sampleIndex--;
  // LED_RX_OFF();

  return sinSample(afsk->phaseAcc);
}

int hdlc_flag_count = 0;
bool hdlc_flage_end = false;
static bool hdlcParse(Hdlc *hdlc, bool bit, FIFOBuffer *fifo)
{
  // Initialise a return value. We start with the
  // assumption that all is going to end well :)
  bool ret = true;

  // Bitshift our byte of demodulated bits to
  // the left by one bit, to make room for the
  // next incoming bit
  hdlc->demodulatedBits <<= 1;
  // And then put the newest bit from the
  // demodulator into the byte.
  hdlc->demodulatedBits |= bit ? 1 : 0;

  // Now we'll look at the last 8 received bits, and
  // check if we have received a HDLC flag (01111110)
  if (hdlc->demodulatedBits == HDLC_FLAG)
  {
    // If we have, check that our output buffer is
    // not full.
    if (!fifo_isfull(fifo))
    {
      // If it isn't, we'll push the HDLC_FLAG into
      // the buffer and indicate that we are now
      // receiving data. For bling we also turn
      // on the RX LED.
      fifo_push(fifo, HDLC_FLAG);
      hdlc->receiving = true;
      if (++hdlc_flag_count >= 3)
      {
        RX_LED_ON();
      }
    }
    else
    {
      // If the buffer is full, we have a problem
      // and abort by setting the return value to
      // false and stopping the here.

      ret = false;
      hdlc->receiving = false;
      RX_LED_OFF();
      hdlc_flag_count = 0;
      hdlc_flage_end = false;
    }

    // Everytime we receive a HDLC_FLAG, we reset the
    // storage for our current incoming byte and bit
    // position in that byte. This effectively
    // synchronises our parsing to  the start and end
    // of the received bytes.
    hdlc->currentByte = 0;
    hdlc->bitIndex = 0;
    return ret;
  }

  // Check if we have received a RESET flag (01111111)
  // In this comparison we also detect when no transmission
  // (or silence) is taking place, and the demodulator
  // returns an endless stream of zeroes. Due to the NRZ
  // coding, the actual bits send to this function will
  // be an endless stream of ones, which this AND operation
  // will also detect.
  if ((hdlc->demodulatedBits & HDLC_RESET) == HDLC_RESET)
  {
    // If we have, something probably went wrong at the
    // transmitting end, and we abort the reception.
    hdlc->receiving = false;
    RX_LED_OFF();
    hdlc_flag_count = 0;
    hdlc_flage_end = false;
    return ret;
  }

  // If we have not yet seen a HDLC_FLAG indicating that
  // a transmission is actually taking place, don't bother
  // with anything.
  if (!hdlc->receiving)
    return ret;

  hdlc_flage_end = true;

  // First check if what we are seeing is a stuffed bit.
  // Since the different HDLC control characters like
  // HDLC_FLAG, HDLC_RESET and such could also occur in
  // a normal data stream, we employ a method known as
  // "bit stuffing". All control characters have more than
  // 5 ones in a row, so if the transmitting party detects
  // this sequence in the _data_ to be transmitted, it inserts
  // a zero to avoid the receiving party interpreting it as
  // a control character. Therefore, if we detect such a
  // "stuffed bit", we simply ignore it and wait for the
  // next bit to come in.
  //
  // We do the detection by applying an AND bit-mask to the
  // stream of demodulated bits. This mask is 00111111 (0x3f)
  // if the result of the operation is 00111110 (0x3e), we
  // have detected a stuffed bit.
  if ((hdlc->demodulatedBits & 0x3f) == 0x3e)
    return ret;

  // If we have an actual 1 bit, push this to the current byte
  // If it's a zero, we don't need to do anything, since the
  // bit is initialized to zero when we bitshifted earlier.
  if (hdlc->demodulatedBits & 0x01)
    hdlc->currentByte |= 0x80;

  // Increment the bitIndex and check if we have a complete byte
  if (++hdlc->bitIndex >= 8)
  {
    // If we have a HDLC control character, put a AX.25 escape
    // in the received data. We know we need to do this,
    // because at this point we must have already seen a HDLC
    // flag, meaning that this control character is the result
    // of a bitstuffed byte that is equal to said control
    // character, but is actually part of the data stream.
    // By inserting the escape character, we tell the protocol
    // layer that this is not an actual control character, but
    // data.
    if ((hdlc->currentByte == HDLC_FLAG ||
         hdlc->currentByte == HDLC_RESET ||
         hdlc->currentByte == AX25_ESC))
    {
      // We also need to check that our received data buffer
      // is not full before putting more data in
      if (!fifo_isfull(fifo))
      {
        fifo_push(fifo, AX25_ESC);
      }
      else
      {
        // If it is, abort and return false
        hdlc->receiving = false;
        RX_LED_OFF();
        hdlc_flag_count = 0;
        ret = false;
#ifdef DEBUG_TNC
        log_e("FIFO IS FULL");
#endif
      }
    }

    // Push the actual byte to the received data FIFO,
    // if it isn't full.
    if (!fifo_isfull(fifo))
    {
      fifo_push(fifo, hdlc->currentByte);
    }
    else
    {
      // If it is, well, you know by now!
      hdlc->receiving = false;
      RX_LED_OFF();
      hdlc_flag_count = 0;
      ret = false;
#ifdef DEBUG_TNC
      log_e("FIFO IS FULL");
#endif
    }

    // Wipe received byte and reset bit index to 0
    hdlc->currentByte = 0;
    hdlc->bitIndex = 0;
  }
  else
  {
    // We don't have a full byte yet, bitshift the byte
    // to make room for the next bit
    hdlc->currentByte >>= 1;
  }

  return ret;
}
 
 #define DECODE_DELAY 4.458981479161393e-4 // sample delay
#define DELAY_DIVIDEND 325
#define DELAY_DIVISOR 728866
#define DELAYED_N ((DELAY_DIVIDEND * SAMPLERATE + DELAY_DIVISOR/2) / DELAY_DIVISOR)

 static int delayed[DELAYED_N];
 static int delay_idx = 0;

void AFSK_adc_isr(Afsk *afsk, int16_t currentSample)
{
  /*
   * Frequency discrimination is achieved by simply multiplying
   * the sample with a delayed sample of (samples per bit) / 2.
   * Then the signal is lowpass filtered with a first order,
   * 600 Hz filter. The filter implementation is selectable
   */

  // BUTTERWORTH Filter
  // afsk->iirX[0] = afsk->iirX[1];
  // afsk->iirX[1] = ((int8_t)fifo_pop(&afsk->delayFifo) * currentSample) / 6.027339492F;
  // afsk->iirY[0] = afsk->iirY[1];
  // afsk->iirY[1] = afsk->iirX[0] + afsk->iirX[1] + afsk->iirY[0] * 0.6681786379F;

  // Fast calcultor
  // int16_t tmp16t;
  // afsk->iirX[0] = afsk->iirX[1];
  // tmp16t = (int16_t)((int8_t)fifo_pop(&afsk->delayFifo) * (int8_t)currentSample);
  // afsk->iirX[1] = (tmp16t >> 2) + (tmp16t >> 5);
  // afsk->iirY[0] = afsk->iirY[1];
  // tmp16t = (afsk->iirY[0] >> 2) + (afsk->iirY[0] >> 3) + (afsk->iirY[0] >> 4);
  // afsk->iirY[1] = afsk->iirX[0] + afsk->iirX[1] + tmp16t;

// deocde bell 202 AFSK from ADC value
	int m = (int)currentSample * delayed[delay_idx];
  // Put the current raw sample in the delay FIFO
  delayed[delay_idx] = (int)currentSample;
  delay_idx = (delay_idx + 1) % DELAYED_N;
   afsk->iirY[1] = filter(&lpf, m>>7);

  // We put the sampled bit in a delay-line:
  // First we bitshift everything 1 left
  afsk->sampledBits <<= 1;
  // And then add the sampled bit to our delay line
  afsk->sampledBits |= (afsk->iirY[1] > 0) ? 1 : 0;

  // Put the current raw sample in the delay FIFO
  //fifo_push16(&afsk->delayFifo, currentSample);

  // We need to check whether there is a signal transition.
  // If there is, we can recalibrate the phase of our
  // sampler to stay in sync with the transmitter. A bit of
  // explanation is required to understand how this works.
  // Since we have PHASE_MAX/PHASE_BITS = 8 samples per bit,
  // we employ a phase counter (currentPhase), that increments
  // by PHASE_BITS everytime a sample is captured. When this
  // counter reaches PHASE_MAX, it wraps around by modulus
  // PHASE_MAX. We then look at the last three samples we
  // captured and determine if the bit was a one or a zero.
  //
  // This gives us a "window" looking into the stream of
  // samples coming from the ADC. Sort of like this:
  //
  //   Past                                      Future
  //       0000000011111111000000001111111100000000
  //                   |________|
  //                       ||
  //                     Window
  //
  // Every time we detect a signal transition, we adjust
  // where this window is positioned little. How much we
  // adjust it is defined by PHASE_INC. If our current phase
  // phase counter value is less than half of PHASE_MAX (ie,
  // the window size) when a signal transition is detected,
  // add PHASE_INC to our phase counter, effectively moving
  // the window a little bit backward (to the left in the
  // illustration), inversely, if the phase counter is greater
  // than half of PHASE_MAX, we move it forward a little.
  // This way, our "window" is constantly seeking to position
  // it's center at the bit transitions. Thus, we synchronise
  // our timing to the transmitter, even if it's timing is
  // a little off compared to our own.
  if (SIGNAL_TRANSITIONED(afsk->sampledBits))
  {
    if (afsk->currentPhase < PHASE_THRESHOLD)
    {
      afsk->currentPhase += PHASE_INC;
    }
    else
    {
      afsk->currentPhase -= PHASE_INC;
    }
  }

  // We increment our phase counter
  afsk->currentPhase += PHASE_BITS;

  // Check if we have reached the end of
  // our sampling window.
  if (afsk->currentPhase >= PHASE_MAX)
  {
    // If we have, wrap around our phase
    // counter by modulus
    afsk->currentPhase %= PHASE_MAX;

    // Bitshift to make room for the next
    // bit in our stream of demodulated bits
    afsk->actualBits <<= 1;

    //// Alternative using 5 bits ////////////////
    uint8_t bits = afsk->sampledBits & 0x1f;
    uint8_t c = CountOnesFromInteger(bits);
    if (c >= 3)
      afsk->actualBits |= 1;
    /////////////////////////////////////////////////

    // Now we can pass the actual bit to the HDLC parser.
    // We are using NRZ coding, so if 2 consecutive bits
    // have the same value, we have a 1, otherwise a 0.
    // We use the TRANSITION_FOUND function to determine this.
    //
    // This is smart in combination with bit stuffing,
    // since it ensures a transmitter will never send more
    // than five consecutive 1's. When sending consecutive
    // ones, the signal stays at the same level, and if
    // this happens for longer periods of time, we would
    // not be able to synchronize our phase to the transmitter
    // and would start experiencing "bit slip".
    //
    // By combining bit-stuffing with NRZ coding, we ensure
    // that the signal will regularly make transitions
    // that we can use to synchronize our phase.
    //
    // We also check the return of the Link Control parser
    // to check if an error occured.

    if (!hdlcParse(&afsk->hdlc, !TRANSITION_FOUND(afsk->actualBits), &afsk->rxFifo))
    {
      afsk->status |= 1;
      if (fifo_isfull(&afsk->rxFifo))
      {
        fifo_flush(&afsk->rxFifo);
        afsk->status = 0;
#ifdef DEBUG_TNC
        log_e("FIFO IS FULL");
#endif
      }
    }
  }
}

#define ADC_SAMPLES_COUNT 768
int16_t abufPos = 0;
// extern TaskHandle_t taskSensorHandle;

extern void APRS_poll();
uint8_t poll_timer = 0;
// int adc_count = 0;
int offset_new = 0, offset = 2303, offset_count = 0;

#ifndef I2S_INTERNAL
// int x=0;
portMUX_TYPE DRAM_ATTR timerMux = portMUX_INITIALIZER_UNLOCKED;

bool sqlActive = false;

void IRAM_ATTR sample_isr()
{
  // portENTER_CRITICAL_ISR(&timerMux); // ISR start

  if (hw_afsk_dac_isr)
  {
    uint8_t sinwave = AFSK_dac_isr(AFSK_modem);
    //   abufPos += 45;
    // sinwave =(uint8_t)(127+(sin((float)abufPos/57)*128));
    // if(abufPos>=315) abufPos=0;
    // if(x++<16) Serial.printf("%d,",sinwave);
    dacWrite(MIC_PIN, sinwave);
    if (AFSK_modem->sending == false)
#if defined(INVERT_PTT)
      digitalWrite(PTT_PIN, HIGH);
#else
      digitalWrite(PTT_PIN, LOW);
#endif
  }
  else
  {
#ifdef SQL
    if (digitalRead(SQL_PIN) == LOW)
    {
      if (sqlActive == false)
      { // Falling Edge SQL
        log_d("RX Signal");
        sqlActive = true;
      }
#endif
      // digitalWrite(4, HIGH);
      adcVal = adc1_get_raw(SPK_PIN); // Read ADC1_0 From PIN 36(VP)
      // Auto offset level
      offset_new += adcVal;
      offset_count++;
      if (offset_count >= 192)
      {
        offset = offset_new / offset_count;
        offset_count = 0;
        offset_new = 0;
        if (offset > 3300 || offset < 1300) // Over dc offset to default
          offset = 2303;
      }
      // Convert unsign wave to sign wave form
      adcVal -= offset;
      // adcVal-=2030;
      int8_t adcR = (int8_t)((int16_t)(adcVal >> 4)); // Reduce 12bit to 8bit
      // int8_t adcR = (int8_t)(adcVal / 16); // Reduce 12bit to 8bit
      adcq.push(&adcR); // Add queue buffer
// digitalWrite(4, LOW);
#ifdef SQL
    }
    else
    {
      if (sqlActive == true)
      { // Falling Edge SQL
        log_d("End Signal");
        sqlActive = false;
      }
    }
#endif
  }
  // portEXIT_CRITICAL_ISR(&timerMux); // ISR end
}
#else
bool tx_en = false;
#endif

extern int mVrms;
extern float dBV;
extern bool afskSync;

long mVsum = 0;
int mVsumCount = 0;
long lastVrms = 0;
bool VrmsFlag = false;
bool sqlActive = false;

void AFSK_Poll(bool isRF)
{
  int mV;
  int x = 0;
  // uint8_t sintable[8] = {127, 217, 254, 217, 127, 36, 0, 36};
#ifdef I2S_INTERNAL
  size_t bytesRead;
  uint16_t pcm_in[ADC_SAMPLES_COUNT];
  uint16_t pcm_out[ADC_SAMPLES_COUNT];
#else
  int8_t adc;
#endif

  if (hw_afsk_dac_isr)
  {
#if defined(I2S_INTERNAL) && defined(CONFIG_IDF_TARGET_ESP32)
    memset(pcm_out, 0, sizeof(pcm_out));
#if defined MONO_OUT
    int sampleCnt = ADC_SAMPLES_COUNT / 2;
#else
    int sampleCnt = ADC_SAMPLES_COUNT;
#endif
    for (x = 0; x < sampleCnt; x++)
    {
      // RX_LED_ON();
      adcVal = (int)AFSK_dac_isr(AFSK_modem);
      //  Serial.print(adcVal,HEX);
      //  Serial.print(",");
      if (AFSK_modem->sending == false && adcVal == 0)
        break;

      // float adcF = hp_filter.Update((float)adcVal);
      // adcVal = (int)adcF;

    uint16_t val = (uint16_t)adcVal; // MSB
    if (isRF) {
      val <<= 7;
      val += 10000;
      // val += (1 << 15);
    } else {
      val <<= 8;
    }

#if !defined MONO_OUT
        // Right Channel GPIO 26
        pcm_out[x++] = val;
#endif

        // Left Channel GPIO 25
        pcm_out[x] = val;
    }

    size_t bytesWritten = 0;
    if (x > 0) {
        ESP_ERROR_CHECK(i2s_write(I2S_NUM_0, (void*)&pcm_out, x * sizeof(uint16_t), &bytesWritten, portMAX_DELAY));
    }

    // Wait for the I2S DAC to pass all buffers before turning the DAC/PTT off.
    if (AFSK_modem->sending == false)
    {
      int txEvents = 0;
      memset(pcm_out, 0, sizeof(pcm_out));
      // Serial.println("TX TAIL");
      //  Clear Delay DMA Buffer
      for (int i = 0; i < 5; i++)
        ESP_ERROR_CHECK(i2s_write(I2S_NUM_0, (void*)&pcm_out, ADC_SAMPLES_COUNT * sizeof(uint16_t), &bytesWritten, portMAX_DELAY));
      // wait on I2S event queue until a TX_DONE is found
      while (xQueueReceive(i2s_event_queue, &i2s_evt, portMAX_DELAY) == pdPASS)
      {
        if (i2s_evt.type == I2S_EVENT_TX_DONE) // I2S DMA finish sent 1 buffer
        {
          if (++txEvents > 6)
          {
            // Serial.println("TX DONE");
            break;
          }
          delay(10);
        }
      }
      dac_i2s_disable();
      i2s_zero_dma_buffer(I2S_NUM_0);
      // i2s_adc_enable(I2S_NUM_0);
#if defined(INVERT_PTT)
      digitalWrite(PTT_PIN, HIGH);
#else
      digitalWrite(PTT_PIN, LOW);
#endif
      if (isRF) {
        // pinMode(POWER_PIN, OUTPUT);
        // digitalWrite(POWER_PIN, LOW);  // RF Power LOW
      }
    }
#endif
  }
  else
  {
#ifdef I2S_INTERNAL
#ifdef SQL
    if (digitalRead(SQL_PIN) == LOW)
    {
      if (sqlActive == false)
      { // Falling Edge SQL
        // Serial.println("RX Signal");
        sqlActive = true;
        mVsum = 0;
        mVsumCount = 0;
      }
#endif
      // if (i2s_read(I2S_NUM_0, (char *)&pcm_in, (ADC_SAMPLES_COUNT * sizeof(uint16_t)), &bytesRead, portMAX_DELAY) == ESP_OK)
      if (i2s_read(I2S_NUM_0, (char *)&pcm_in, (ADC_SAMPLES_COUNT * sizeof(uint16_t)), &bytesRead, portMAX_DELAY) == ESP_OK)
      {
        // Serial.printf("%i,%i,%i,%i\r\n", pcm_in[0], pcm_in[1], pcm_in[2], pcm_in[3]);
        for (int i = 0; i < (bytesRead / sizeof(uint16_t)); i += 2)
        {
          adcVal = (int)pcm_in[i];
          offset_new += adcVal;
          offset_count++;
          if (offset_count >= ADC_SAMPLES_COUNT) // 192
          {
            offset = offset_new / offset_count;
            offset_count = 0;
            offset_new = 0;
            if (offset > 3300 || offset < 1300) // Over dc offset to default
              offset = 2303;
          }
          adcVal -= offset; // Convert unsinewave to sinewave

          if (afskSync == false)
          {
            mV = abs((int)adcVal);         // mVp-p
            mV = (int)((float)mV / 3.68F); // mV=(mV*625)/36848;
            mVsum += powl(mV, 2);
            mVsumCount++;
          }

          if (input_HPF)
          {
            adcVal = (int)filter(&bpf, (int16_t)adcVal);
          }

          int16_t adcR = (int16_t)(adcVal);

          AFSK_adc_isr(AFSK_modem, adcR); // Process signal IIR
          if (i % 32 == 0)
            APRS_poll(); // Poll check every 1 bit
        }
        // Get mVrms on Sync flage 0x7E
        if (afskSync == false)
        {
          if (hdlc_flag_count > 5 && hdlc_flage_end == true)
          {
            if (mVsumCount > 3840)
            {
              mVrms = sqrtl(mVsum / mVsumCount);
              mVsum = 0;
              mVsumCount = 0;
              lastVrms = millis() + 500;
              VrmsFlag = true;
              // if(millis()>lastVrms){
              //  double Vrms=(double)mVrms/1000;
              //  dBV = 20.0F * log10(Vrms);
              //  //dBu = 20 * log10(Vrms / 0.7746);
              //  Serial.printf("mVrms=%d dBV=%0.1f agc=%0.2f\n",mVrms,dBV);
              // afskSync = true;
              //}
            }
          }
          if (millis() > lastVrms && VrmsFlag)
          {
            afskSync = true;
            VrmsFlag = false;
          }
        }
      }
#else
    if (adcq.getCount() >= ADC_SAMPLES_COUNT)
    {
      for (x = 0; x < ADC_SAMPLES_COUNT; x++)
      {
        if (!adcq.pop(&adc)) // Pull queue buffer
          break;
        // audiof[x] = (float)adc;
        //  Serial.printf("%02x ", (unsigned char)adc);
        AFSK_adc_isr(AFSK_modem, adc); // Process signal IIR
        if (x % 4 == 0)
          APRS_poll(); // Poll check every 1 byte
      }
      APRS_poll();
    }
#endif
#ifdef SQL
    }
    else
    {
      // if(sqlActive==true){
      //   i2s_stop(I2S_NUM_0);
      // }
      hdlc_flag_count = 0;
      hdlc_flage_end = false;
      RX_LED_OFF();
      sqlActive = false;
    }
#endif
  }
}

bool getReceive() {
  bool ret = false;
  int ptt = digitalRead(PTT_PIN);
#if defined(INVERT_PTT)
  ptt = !ptt;
#endif
  if (ptt == 1) return true; //PTT Protection receive
  if (AFSK_modem->hdlc.receiving == true) ret = true;
  return ret;
}


#else

#include <string.h>
#include "AFSK.h"
#include "Arduino.h"
#include <Wire.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "cppQueue.h"
#include "fir_filter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/sigmadelta.h"

extern "C"
{
#include "soc/syscon_reg.h"
#include "soc/syscon_struct.h"
}

#define DEBUG_TNC

extern unsigned long custom_preamble;
extern unsigned long custom_tail;
int adcVal;

bool input_HPF = false;
bool input_BPF = false;

static const adc_unit_t unit = ADC_UNIT_1;

void sample_isr();
bool hw_afsk_dac_isr = false;

static filter_t bpf;
static filter_t lpf;
static filter_t hpf;

Afsk *AFSK_modem;

#define ADC_RESULT_BYTE 4
#define ADC_CONV_LIMIT_EN 0

static bool check_valid_data(const adc_digi_output_data_t *data);

#define TIMES 1920
#define GET_UNIT(x) ((x >> 3) & 0x1)
static uint16_t adc1_chan_mask = BIT(0);
static uint16_t adc2_chan_mask = 0;
static adc_channel_t channel[1] = {(adc_channel_t)ADC1_CHANNEL_0};
static const char *TAG = "--(TAG ADC DMA)--";
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2

static void continuous_adc_init(uint16_t adc1_chan_mask, uint16_t adc2_chan_mask, adc_channel_t *channel, uint8_t channel_num)
{
  // channel[0] = (adc_channel_t)digitalPinToAnalogChannel(1);
  log_d("adc_digi_initialize");

  adc_digi_init_config_t adc_dma_config = {
      .max_store_buf_size = 8 * TIMES,
      .conv_num_each_intr = TIMES,
      .adc1_chan_mask = adc1_chan_mask,
      .adc2_chan_mask = adc2_chan_mask,
  };

  ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

  log_d("adc_digi_config");
  adc_digi_configuration_t dig_cfg = {
      .conv_limit_en = ADC_CONV_LIMIT_EN,
      .conv_limit_num = 250,
      .sample_freq_hz = SAMPLERATE,
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,   ////ESP32 only supports ADC1 DMA mode
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2 // ADC_DIGI_OUTPUT_FORMAT_TYPE2,
  };

  adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
  dig_cfg.pattern_num = channel_num;

  log_d("adc_digi_controller");
  int i = 0;
  for (int i = 0; i < channel_num; i++)
  {
    uint8_t unit = GET_UNIT(channel[i]);
    uint8_t ch = channel[i] & 0x7;
    if (!rx_att) {
      adc_pattern[i].atten = ADC_ATTEN_DB_11; // STOCK
    } else {
      adc_pattern[i].atten = ADC_ATTEN_DB_2_5; // 2633 for T-TWR-PLUS with replced R22
      //adc_pattern[i].atten = ADC_ATTEN_DB_6; // 1800
      //adc_pattern[i].atten = ADC_ATTEN_DB_0;
    }
    adc_pattern[i].channel = ch;
    adc_pattern[i].unit = unit;
    adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH; // 11 data bits limit

    ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
    ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
    ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
  }
  dig_cfg.adc_pattern = adc_pattern;
  ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
}

#if !CONFIG_IDF_TARGET_ESP32
static bool check_valid_data(const adc_digi_output_data_t *data)
{
  const unsigned int unit = data->type2.unit;
  if (unit > 0)
    return false;
  if (data->type2.channel > 0)
    return false;
  return true;
}
#endif


esp_err_t adc_init()
{
  esp_err_t error_code;

  continuous_adc_init(adc1_chan_mask, adc2_chan_mask, channel, sizeof(channel) / sizeof(adc_channel_t));
  // continuous_adc_init(0, 0, channel, 1);
  //  error_code = adc1_config_width(ADC_WIDTH_BIT_12);
  //  log_d("adc1_config_width error_code=%d", error_code);

  // if (error_code != ESP_OK)
  // {
  //   return error_code;
  // }
  adc_digi_start();

  return ESP_OK;
}

void adcActive(bool sts)
{
  if (sts){
    //adc_init();
    adc_digi_start();
    hw_afsk_dac_isr=0;
  }else{
    adc_digi_stop();
    //adc_digi_deinitialize();
  }
}

int IRAM_ATTR read_adc_dma(uint32_t *ret_num, uint8_t *result)
{
  esp_err_t ret;
  ret = adc_digi_read_bytes(result, TIMES, ret_num, ADC_MAX_DELAY);
  return ret;
}

uint8_t CountOnesFromInteger(uint8_t value)
{
  uint8_t count;
  for (count = 0; value != 0; count++, value &= value - 1)
    ;
  return count;
}

#define IMPLEMENTATION FIFO

cppQueue adcq(sizeof(int16_t), 19200, IMPLEMENTATION); // Instantiate queue

// Forward declerations
int afsk_getchar(void);
void afsk_putchar(char c);

hw_timer_t *timer = NULL;

void AFSK_TimerEnable(bool sts)
{
  if (timer == NULL)
    return;
  if (sts == true)
    timerAlarmEnable(timer);
  else
    timerAlarmDisable(timer);
}

bool getTransmit()
{
  bool ret=false;
  int ptt = digitalRead(PTT_PIN);
#if defined(INVERT_PTT)
  ptt = !ptt;
#endif
  if (ptt == 1)
    ret = true;
  if(hw_afsk_dac_isr) ret=true;
  return ret;
}

bool getReceive()
{
  bool ret=false;
  int ptt = digitalRead(PTT_PIN);
#if defined(INVERT_PTT)
  ptt = !ptt;
#endif
  if (ptt == 1) return true; //PTT Protection receive
  if(AFSK_modem->hdlc.receiving==true) ret=true;
  return ret;
}

void afskSetHPF(bool val)
{
  input_HPF=val;
}
void afskSetBPF(bool val)
{
  input_BPF=val;
}

uint32_t ret_num;
uint8_t *resultADC;

void AFSK_hw_init(void)
{
  // Set up ADC
  log_d("AFSK hardware Initialize");
  // pinMode(RSSI_PIN, INPUT_PULLUP);
  pinMode(PTT_PIN, OUTPUT);
  pinMode(MIC_CH_SEL, OUTPUT); // MIC_SEL
  pinMode(18, OUTPUT); // ESP2MIC
  pinMode(SPK_PIN, ANALOG);  // AUDIO2ESP

  digitalWrite(MIC_CH_SEL, HIGH);

#if defined(INVERT_PTT)
  digitalWrite(PTT_PIN, HIGH); // PTT not active
#else
  digitalWrite(PTT_PIN, LOW); // PTT not active
#endif

  resultADC = (uint8_t *)malloc(TIMES * sizeof(uint8_t));

  adc_init();

  sigmadelta_config_t sdm_config{
      .channel = SIGMADELTA_CHANNEL_0,
      .sigmadelta_duty = 127,    /*!< Sigma-delta duty, duty ranges from -128 to 127. */
      .sigmadelta_prescale = 96, /*!< Sigma-delta prescale, prescale ranges from 0 to 255. */
      .sigmadelta_gpio = MIC_PIN};
  sigmadelta_config(&sdm_config);
  // Sample the play the audio on the DAC.
  timer = timerBegin(0, 4, true);                                                // 80MHz%4 = 20MHz hardware clock
  timerAttachInterrupt(timer, &sample_isr, true);                                // Attaches the handler function to the timer
  timerAlarmWrite(timer, (uint64_t)20000000 / CONFIG_AFSK_DAC_SAMPLERATE, true); // Interrupts when counter == 20MHz/CONFIG_AFSK_DAC_SAMPLERATE
  timerAlarmEnable(timer);
  AFSK_TimerEnable(false);
}

void AFSK_init(Afsk *afsk)
{
  log_d("AFSK software Initialize");
  // Allocate modem struct memory
  memset(afsk, 0, sizeof(*afsk));
  AFSK_modem = afsk;
  // Set phase increment
  afsk->phaseInc = MARK_INC;
  // Initialise FIFO buffers
  fifo_init(&afsk->rxFifo, afsk->rxBuf, sizeof(afsk->rxBuf));
  fifo_init(&afsk->txFifo, afsk->txBuf, sizeof(afsk->txBuf));

  // filter initialize
  filter_param_t flt = {
      .size = FIR_LPF_N,
      .sampling_freq = SAMPLERATE,
      .pass_freq = 0,
      .cutoff_freq = 1200,
  };
  int16_t *lpf_an, *bpf_an, *hpf_an;
  // LPF
  lpf_an = filter_coeff(&flt);
  // BPF
  flt.size = FIR_BPF_N;
  flt.pass_freq = 1000;
  flt.cutoff_freq = 2500;
  bpf_an = filter_coeff(&flt);

  // LPF
  filter_init(&lpf, lpf_an, FIR_LPF_N);
  // BPF
  filter_init(&bpf, bpf_an, FIR_BPF_N);

  // HPF
  flt.size = FIR_BPF_N;
  flt.pass_freq = 1000;
  flt.cutoff_freq = 6000;
  hpf_an = filter_coeff(&flt);
  filter_init(&hpf, hpf_an, FIR_BPF_N);

  AFSK_hw_init();
}

static void AFSK_txStart(Afsk *afsk)
{
  if (!afsk->sending)
  {
    fifo_flush(&AFSK_modem->txFifo);
    // Serial.println("TX Start");
    afsk->sending = true;
    afsk->phaseInc = MARK_INC;
    afsk->phaseAcc = 0;
    afsk->bitstuffCount = 0;
  #if defined(INVERT_PTT)
    digitalWrite(PTT_PIN, LOW);
  #else
    digitalWrite(PTT_PIN, HIGH);
  #endif
    afsk->preambleLength = DIV_ROUND(custom_preamble * BITRATE, 4800);
    AFSK_DAC_IRQ_START();
    // LED_TX_ON();
    AFSK_TimerEnable(true);
  }
  noInterrupts();
  afsk->tailLength = DIV_ROUND(custom_tail * BITRATE, 4800);
  interrupts();
}

void afsk_putchar(char c)
{
  AFSK_txStart(AFSK_modem);
  while (fifo_isfull_locked(&AFSK_modem->txFifo))
  {
    /* Wait */
    // delay(10);
  }
  fifo_push_locked(&AFSK_modem->txFifo, c);
}

int afsk_getchar(void)
{
  if (fifo_isempty_locked(&AFSK_modem->rxFifo))
  {
    return EOF;
  }
  else
  {
    return fifo_pop_locked(&AFSK_modem->rxFifo);
  }
}

void AFSK_transmit(char *buffer, size_t size)
{
  fifo_flush(&AFSK_modem->txFifo);
  int i = 0;
  while (size--)
  {
    afsk_putchar(buffer[i++]);
  }
}

uint8_t AFSK_dac_isr(Afsk *afsk)
{
  if (afsk->sampleIndex == 0)
  {
    if (afsk->txBit == 0)
    {
      if (fifo_isempty(&afsk->txFifo) && afsk->tailLength == 0)
      {
        AFSK_DAC_IRQ_STOP();
        afsk->sending = false;
        return 0;
      }
      else
      {
        if (!afsk->bitStuff)
          afsk->bitstuffCount = 0;
        afsk->bitStuff = true;
        if (afsk->preambleLength == 0)
        {
          if (fifo_isempty(&afsk->txFifo))
          {
            afsk->tailLength--;
            afsk->currentOutputByte = HDLC_FLAG;
          }
          else
          {
            afsk->currentOutputByte = fifo_pop(&afsk->txFifo);
          }
        }
        else
        {
          afsk->preambleLength--;
          afsk->currentOutputByte = HDLC_FLAG;
        }
        if (afsk->currentOutputByte == AX25_ESC)
        {
          if (fifo_isempty(&afsk->txFifo))
          {
            AFSK_DAC_IRQ_STOP();
            afsk->sending = false;
            return 0;
          }
          else
          {
            afsk->currentOutputByte = fifo_pop(&afsk->txFifo);
          }
        }
        else if (afsk->currentOutputByte == HDLC_FLAG || afsk->currentOutputByte == HDLC_RESET)
        {
          afsk->bitStuff = false;
        }
      }
      afsk->txBit = 0x01;
    }

    if (afsk->bitStuff && afsk->bitstuffCount >= BIT_STUFF_LEN)
    {
      afsk->bitstuffCount = 0;
      afsk->phaseInc = SWITCH_TONE(afsk->phaseInc);
    }
    else
    {
      if (afsk->currentOutputByte & afsk->txBit)
      {
        afsk->bitstuffCount++;
      }
      else
      {
        afsk->bitstuffCount = 0;
        afsk->phaseInc = SWITCH_TONE(afsk->phaseInc);
      }
      afsk->txBit <<= 1;
    }

    afsk->sampleIndex = SAMPLESPERBIT;
  }

  afsk->phaseAcc += afsk->phaseInc;
  afsk->phaseAcc %= SIN_LEN;
  if (afsk->sampleIndex > 0)
    afsk->sampleIndex--;

  return sinSample(afsk->phaseAcc);
}

int hdlc_flag_count = 0;
bool hdlc_flage_end = false;
bool sync_flage = false;
static bool hdlcParse(Hdlc *hdlc, bool bit, FIFOBuffer *fifo)
{
  // Initialise a return value. We start with the
  // assumption that all is going to end well :)
  bool ret = true;

  // Bitshift our byte of demodulated bits to
  // the left by one bit, to make room for the
  // next incoming bit
  hdlc->demodulatedBits <<= 1;
  // And then put the newest bit from the
  // demodulator into the byte.
  hdlc->demodulatedBits |= bit ? 1 : 0;

  // Now we'll look at the last 8 received bits, and
  // check if we have received a HDLC flag (01111110)
  if (hdlc->demodulatedBits == HDLC_FLAG)
  {
    // If we have, check that our output buffer is
    // not full.
    if (!fifo_isfull(fifo))
    {
      // If it isn't, we'll push the HDLC_FLAG into
      // the buffer and indicate that we are now
      // receiving data. For bling we also turn
      // on the RX LED.

      hdlc->receiving = true;
      if (++hdlc_flag_count >= 3)
      {
        fifo_flush(fifo);
        RX_LED_ON();
        sync_flage = true;
      }
      fifo_push(fifo, HDLC_FLAG);
    }
    else
    {
      // If the buffer is full, we have a problem
      // and abort by setting the return value to
      // false and stopping the here.

      ret = false;
      hdlc->receiving = false;
      RX_LED_OFF();
      hdlc_flag_count = 0;
      hdlc_flage_end = false;
    }

    // Everytime we receive a HDLC_FLAG, we reset the
    // storage for our current incoming byte and bit
    // position in that byte. This effectively
    // synchronises our parsing to  the start and end
    // of the received bytes.
    hdlc->currentByte = 0;
    hdlc->bitIndex = 0;
    return ret;
  }
  sync_flage = false;

  // Check if we have received a RESET flag (01111111)
  // In this comparison we also detect when no transmission
  // (or silence) is taking place, and the demodulator
  // returns an endless stream of zeroes. Due to the NRZ
  // coding, the actual bits send to this function will
  // be an endless stream of ones, which this AND operation
  // will also detect.
  if ((hdlc->demodulatedBits & HDLC_RESET) == HDLC_RESET)
  {
    // If we have, something probably went wrong at the
    // transmitting end, and we abort the reception.
    hdlc->receiving = false;
    RX_LED_OFF();
    hdlc_flag_count = 0;
    hdlc_flage_end = false;
    return ret;
  }

  // If we have not yet seen a HDLC_FLAG indicating that
  // a transmission is actually taking place, don't bother
  // with anything.
  if (!hdlc->receiving)
    return ret;

  hdlc_flage_end = true;

  // First check if what we are seeing is a stuffed bit.
  // Since the different HDLC control characters like
  // HDLC_FLAG, HDLC_RESET and such could also occur in
  // a normal data stream, we employ a method known as
  // "bit stuffing". All control characters have more than
  // 5 ones in a row, so if the transmitting party detects
  // this sequence in the _data_ to be transmitted, it inserts
  // a zero to avoid the receiving party interpreting it as
  // a control character. Therefore, if we detect such a
  // "stuffed bit", we simply ignore it and wait for the
  // next bit to come in.
  //
  // We do the detection by applying an AND bit-mask to the
  // stream of demodulated bits. This mask is 00111111 (0x3f)
  // if the result of the operation is 00111110 (0x3e), we
  // have detected a stuffed bit.
  if ((hdlc->demodulatedBits & 0x3f) == 0x3e)
    return ret;

  // If we have an actual 1 bit, push this to the current byte
  // If it's a zero, we don't need to do anything, since the
  // bit is initialized to zero when we bitshifted earlier.
  if (hdlc->demodulatedBits & 0x01)
    hdlc->currentByte |= 0x80;

  // Increment the bitIndex and check if we have a complete byte
  if (++hdlc->bitIndex >= 8)
  {
    // If we have a HDLC control character, put a AX.25 escape
    // in the received data. We know we need to do this,
    // because at this point we must have already seen a HDLC
    // flag, meaning that this control character is the result
    // of a bitstuffed byte that is equal to said control
    // character, but is actually part of the data stream.
    // By inserting the escape character, we tell the protocol
    // layer that this is not an actual control character, but
    // data.
    if ((hdlc->currentByte == HDLC_FLAG ||
         hdlc->currentByte == HDLC_RESET ||
         hdlc->currentByte == AX25_ESC))
    {
      // We also need to check that our received data buffer
      // is not full before putting more data in
      if (!fifo_isfull(fifo))
      {
        fifo_push(fifo, AX25_ESC);
      }
      else
      {
        // If it is, abort and return false
        hdlc->receiving = false;
        RX_LED_OFF();
        hdlc_flag_count = 0;
        ret = false;
#ifdef DEBUG_TNC
        log_e("FIFO IS FULL");
#endif
      }
    }

    // Push the actual byte to the received data FIFO,
    // if it isn't full.
    if (!fifo_isfull(fifo))
    {
      fifo_push(fifo, hdlc->currentByte);
    }
    else
    {
      // If it is, well, you know by now!
      hdlc->receiving = false;
      RX_LED_OFF();
      hdlc_flag_count = 0;
      ret = false;
#ifdef DEBUG_TNC
      log_e("FIFO IS FULL");
#endif
    }

    // Wipe received byte and reset bit index to 0
    hdlc->currentByte = 0;
    hdlc->bitIndex = 0;
  }
  else
  {
    // We don't have a full byte yet, bitshift the byte
    // to make room for the next bit
    hdlc->currentByte >>= 1;
  }

  return ret;
}

#define DECODE_DELAY 4.458981479161393e-4 // sample delay
#define DELAY_DIVIDEND 325
#define DELAY_DIVISOR 728866
#define DELAYED_N ((DELAY_DIVIDEND * SAMPLERATE + DELAY_DIVISOR / 2) / DELAY_DIVISOR)

static int delayed[DELAYED_N];
static int delay_idx = 0;

void AFSK_adc_isr(Afsk *afsk, int16_t currentSample)
{
  /*
   * Frequency discrimination is achieved by simply multiplying
   * the sample with a delayed sample of (samples per bit) / 2.
   * Then the signal is lowpass filtered with a first order,
   * 600 Hz filter. The filter implementation is selectable
   */

  // deocde bell 202 AFSK from ADC value
  int m = (int)currentSample * delayed[delay_idx];
  // Put the current raw sample in the delay FIFO
  delayed[delay_idx] = (int)currentSample;
  delay_idx = (delay_idx + 1) % DELAYED_N;
  afsk->iirY[1] = filter(&lpf, m >> 7);

  // We put the sampled bit in a delay-line:
  // First we bitshift everything 1 left
  afsk->sampledBits <<= 1;
  // And then add the sampled bit to our delay line
  afsk->sampledBits |= (afsk->iirY[1] > 0) ? 1 : 0;

  // We need to check whether there is a signal transition.
  // If there is, we can recalibrate the phase of our
  // sampler to stay in sync with the transmitter. A bit of
  // explanation is required to understand how this works.
  // Since we have PHASE_MAX/PHASE_BITS = 8 samples per bit,
  // we employ a phase counter (currentPhase), that increments
  // by PHASE_BITS everytime a sample is captured. When this
  // counter reaches PHASE_MAX, it wraps around by modulus
  // PHASE_MAX. We then look at the last three samples we
  // captured and determine if the bit was a one or a zero.
  //
  // This gives us a "window" looking into the stream of
  // samples coming from the ADC. Sort of like this:
  //
  //   Past                                      Future
  //       0000000011111111000000001111111100000000
  //                   |________|
  //                       ||
  //                     Window
  //
  // Every time we detect a signal transition, we adjust
  // where this window is positioned little. How much we
  // adjust it is defined by PHASE_INC. If our current phase
  // phase counter value is less than half of PHASE_MAX (ie,
  // the window size) when a signal transition is detected,
  // add PHASE_INC to our phase counter, effectively moving
  // the window a little bit backward (to the left in the
  // illustration), inversely, if the phase counter is greater
  // than half of PHASE_MAX, we move it forward a little.
  // This way, our "window" is constantly seeking to position
  // it's center at the bit transitions. Thus, we synchronise
  // our timing to the transmitter, even if it's timing is
  // a little off compared to our own.
  if (SIGNAL_TRANSITIONED(afsk->sampledBits))
  {
    if (afsk->currentPhase < PHASE_THRESHOLD)
    {
      afsk->currentPhase += PHASE_INC;
    }
    else
    {
      afsk->currentPhase -= PHASE_INC;
    }
  }

  // We increment our phase counter
  afsk->currentPhase += PHASE_BITS;

  // Check if we have reached the end of
  // our sampling window.
  if (afsk->currentPhase >= PHASE_MAX)
  {
    // If we have, wrap around our phase
    // counter by modulus
    afsk->currentPhase %= PHASE_MAX;

    // Bitshift to make room for the next
    // bit in our stream of demodulated bits
    afsk->actualBits <<= 1;

    //// Alternative using 5 bits ////////////////
    uint8_t bits = afsk->sampledBits & 0x1f;
    uint8_t c = CountOnesFromInteger(bits);
    if (c >= 3)
      afsk->actualBits |= 1;
    /////////////////////////////////////////////////

    // Now we can pass the actual bit to the HDLC parser.
    // We are using NRZ coding, so if 2 consecutive bits
    // have the same value, we have a 1, otherwise a 0.
    // We use the TRANSITION_FOUND function to determine this.
    //
    // This is smart in combination with bit stuffing,
    // since it ensures a transmitter will never send more
    // than five consecutive 1's. When sending consecutive
    // ones, the signal stays at the same level, and if
    // this happens for longer periods of time, we would
    // not be able to synchronize our phase to the transmitter
    // and would start experiencing "bit slip".
    //
    // By combining bit-stuffing with NRZ coding, we ensure
    // that the signal will regularly make transitions
    // that we can use to synchronize our phase.
    //
    // We also check the return of the Link Control parser
    // to check if an error occured.

    if (!hdlcParse(&afsk->hdlc, !TRANSITION_FOUND(afsk->actualBits), &afsk->rxFifo))
    {
      afsk->status |= 1;
      if (fifo_isfull(&afsk->rxFifo))
      {
        fifo_flush(&afsk->rxFifo);
        afsk->status = 0;
#ifdef DEBUG_TNC
        log_e("FIFO IS FULL");
#endif
      }
    }
  }
}

#define ADC_SAMPLES_COUNT 192 //
int16_t abufPos = 0;
// extern TaskHandle_t taskSensorHandle;

extern void APRS_poll();
uint8_t poll_timer = 0;
// int adc_count = 0;
int offset_new = 0, offset = 2090, offset_count = 0;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
double sinwave;

void IRAM_ATTR sample_isr()
{
  if (hw_afsk_dac_isr)
  {
    portENTER_CRITICAL_ISR(&timerMux);          // ISR start
    sinwave = (double)AFSK_dac_isr(AFSK_modem); // Return sine wave value 0-255
    // Sigma-delta duty of one channel, the value ranges from -128 to 127, recommended range is -90 ~ 90.The waveform is more like a random one in this range.
    int8_t sine = (int8_t)((sinwave - 127) / 1.5); // Redue sine = -85 ~ 85
    sigmadelta_set_duty(SIGMADELTA_CHANNEL_0, sine);

    // End of packet
    if (AFSK_modem->sending == false)
    {
#if defined(INVERT_PTT)
      digitalWrite(PTT_PIN, HIGH);
#else
      digitalWrite(PTT_PIN, LOW);
#endif
      AFSK_TimerEnable(false);
      // LED_TX_OFF();
    }
    portEXIT_CRITICAL_ISR(&timerMux); // ISR end
  }
}

bool tx_en = false;

extern int mVrms;
extern float dBV;
extern bool afskSync;

double Vrms = 0;
long mVsum = 0;
int mVsumCount = 0;
long lastVrms = 0;
bool VrmsFlag = false;
bool sqlActive = false;

void AFSK_Poll(bool isRF)
{
  int mV;
  int x = 0;
  size_t writeByte;
  int16_t adc;

  if (!hw_afsk_dac_isr)
  {
    memset(resultADC, 0xcc, TIMES * sizeof(uint8_t));
    esp_err_t ret = read_adc_dma(&ret_num, &resultADC[0]);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
    {
      // ESP_LOGI("TASK:", "ret is %x, ret_num is %d", ret, ret_num);
      for (int i = 0; i < ret_num; i += ADC_RESULT_BYTE)
      {
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&resultADC[i];
        if (check_valid_data(p))
        {
          // if(i<100) log_d("ret%x i=%d  Unit: %d,_Channel: %d, Value: %d\n",ret, i, p->type2.unit, p->type2.channel, p->type2.data);
          adcVal = p->type2.data;
          offset_new += adcVal;
          offset_count++;
          if (offset_count >= ADC_SAMPLES_COUNT) // 192
          {
            offset = offset_new / offset_count;
            offset_count = 0;
            offset_new = 0;
            if (offset > 3000 || offset < 1000) // Over dc offset to default
              offset = 2090;
          }
          adcVal -= offset; // Convert unsinewave to sinewave, offset=1.762V ,raw=2090

          if (sync_flage == true)
          {
            mV = (int)((float)adcVal / 1.1862F); // ADC Raw to mV
            mVsum += powl(mV, 2);                // VRMS = √(1/n)(V1^2 +V2^2 + … + Vn^2)
            mVsumCount++;
          }

          if (input_HPF)
          {
            adcVal = (int)filter(&hpf, (int16_t)adcVal);
          }
          if (input_BPF)
          {
            adcVal = (int)filter(&bpf, (int16_t)adcVal);
          }

          AFSK_adc_isr(AFSK_modem, (int16_t)adcVal); // Process signal IIR
          if (i % PHASE_BITS == 0)
            APRS_poll(); // Poll check every 1 bit
        }
      }
      // Get mVrms on Sync affter flage 0x7E
      // if (afskSync == false)
      if (sync_flage == false)
      {
        if (mVsumCount > 500)
        {
          mVrms = sqrtl(mVsum / mVsumCount); // VRMS = √(1/mVsumCount)(mVsum)
          mVsum = 0;
          mVsumCount = 0;
          lastVrms = millis() + 500;
          VrmsFlag = true;
          // Tool conversion dBv <--> Vrms at http://sengpielaudio.com/calculator-db-volt.htm
          // dBV = 20.0F * log10(Vrms);
          log_d("Audio dc_offset=%d mVrms=%d", offset, mVrms);
        }
        if (millis() > lastVrms && VrmsFlag)
        {
          afskSync = true;
          VrmsFlag = false;
        }
      }
    }
  }
}

#endif
