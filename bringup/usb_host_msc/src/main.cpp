// THROWAWAY spike for the wayfinder ticket "USB spike on the LCD Board".
// Stage 1: FreeRTOS + TinyUSB host (MSC) on OTG_FS (PA11/PA12, rhport 0).
// Stage 2 (env:stage2, SPIKE_DEVICE=1): adds a CDC+MSC device on OTG_HS internal FS PHY (PB14/PB15, rhport 1).
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "tusb.h"

// Debug UART (USART2) as in sw-001 pinout.h: TX PD5, RX PD6
Uart dbg(PD_6, PD_5);

// Host port power switch (TPS2553): enable PD14, fault (open-drain, active low) PD15
constexpr int PIN_HOST_VBUS_EN = PD14;
constexpr int PIN_HOST_VBUS_FAULT = PD15;
static volatile uint32_t faultEdges = 0, faultAsserts = 0, lastFaultEdgeMs = 0;

static SemaphoreHandle_t logMutex;
// RAM log ring, readable over SWD (spike_log.py) independent of the UART path
extern "C" {
volatile uint32_t spikeLogHead = 0;            // total bytes ever written
alignas(32) char spikeLogRing[16384];
}
static void ram_put(const char *s)
{
  for (; *s; s++)
  {
    spikeLogRing[spikeLogHead % sizeof(spikeLogRing)] = *s;
    spikeLogHead = spikeLogHead + 1;
  }
}

static void uart_put(const char *s)
{
  ram_put(s);
  // Polled write straight to USART2 so logging never depends on UART interrupts
  for (; *s; s++)
  {
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF)) {}
    USART2->TDR = (uint8_t)*s;
  }
  while (!(USART2->ISR & USART_ISR_TC)) {}
}

static void logf(const char *fmt, ...);
void spike_logf(const char *fmt, ...)
{
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  logf("%s", buf);
}

static void logf(const char *fmt, ...)
{
  char buf[220];
  int n = snprintf(buf, sizeof(buf), "[%8lu] ", (unsigned long)millis());
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + n, sizeof(buf) - n - 3, fmt, ap);
  va_end(ap);
  strcat(buf, "\r\n");
  bool rtos = logMutex && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
  if (rtos)
    xSemaphoreTake(logMutex, portMAX_DELAY);
  uart_put(buf);
  if (rtos)
    xSemaphoreGive(logMutex);
}

extern "C" void OTG_FS_IRQHandler(void) { tusb_int_handler(0, true); }
extern "C" void OTG_HS_IRQHandler(void) { tusb_int_handler(1, true); }

static void usb_host_hw_init()
{
  // PA11 = DM, PA12 = DP, AF10 (OTG2_FS - "USB2_OTG_FS" is TinyUSB rhport 0)
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef g = {};
  g.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Alternate = GPIO_AF10_OTG2_HS; // HAL name for the OTG2 (FS) AF on PA11/PA12
  HAL_GPIO_Init(GPIOA, &g);

  HAL_PWREx_EnableUSBVoltageDetector();
  __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}

#if SPIKE_DEVICE
static void usb_device_hw_init()
{
  // PB14 = DM, PB15 = DP, AF12 = OTG1_FS (OTG_HS core with its internal full-speed PHY)
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef g = {};
  g.Pin = GPIO_PIN_14 | GPIO_PIN_15;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Alternate = GPIO_AF12_OTG1_FS;
  HAL_GPIO_Init(GPIOB, &g);

  HAL_PWREx_EnableUSBVoltageDetector();
  __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
  // No external ULPI PHY: the ULPI clock must stay off (also in sleep) or the core waits for it
  __HAL_RCC_USB1_OTG_HS_ULPI_CLK_DISABLE();
  __HAL_RCC_USB1_OTG_HS_ULPI_CLK_SLEEP_DISABLE();
  NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}

void tud_mount_cb(void) { logf("DEVICE: configured by the PC (mounted)"); }
void tud_umount_cb(void) { logf("DEVICE: unmounted"); }
void tud_suspend_cb(bool) { logf("DEVICE: suspended"); }
void tud_resume_cb(void) { logf("DEVICE: resumed"); }
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) { logf("DEVICE: CDC%u line state DTR=%d RTS=%d", itf, dtr, rts); }

// CDC echo: every received chunk is sent back prefixed by "ECHO:"
void tud_cdc_rx_cb(uint8_t itf)
{
  uint8_t buf[64];
  uint32_t n = tud_cdc_n_read(itf, buf, sizeof(buf));
  tud_cdc_n_write_str(itf, "ECHO:");
  tud_cdc_n_write(itf, buf, n);
  tud_cdc_n_write_flush(itf);
}

#if GTE_LUN0_SD
void sd_lun_begin();
extern volatile uint32_t sdReads, sdWrites, sdErrors, sdBytesRead, sdBytesWritten;
#endif
static void device_task(void *)
{
#if GTE_LUN0_SD
  sd_lun_begin(); // before the D+ pull-up connects, so drive 0 is ready when the PC first asks
#endif
  logf("device_task: tusb_init(rhport %d, DEVICE)", BOARD_TUD_RHPORT);
  tusb_rhport_init_t init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  if (!tusb_init(BOARD_TUD_RHPORT, &init))
    logf("device_task: tusb_init FAILED");
  // Present a clean, long unplug after a reset so the PC (and a VM USB filter) sees a disconnect
  // and a brand-new device, rather than a reset of the old one.
  tud_disconnect();
  vTaskDelay(pdMS_TO_TICKS(1500));
  tud_connect();
  logf("device_task: D+ pull-up connected");
  for (;;)
    tud_task(); // blocks on the TinyUSB event queue
}
#endif

//--------------------------------------------------------------------+
// Host MSC test
//--------------------------------------------------------------------+
volatile uint8_t mscDevAddr = 0;
static SemaphoreHandle_t ioDone;
static volatile bool ioOk;
static scsi_inquiry_resp_t inquiryResp;
alignas(32) static uint8_t sectorBuf[64 * 512];

static bool io_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
  (void)dev_addr;
  ioOk = (cb_data->csw->status == 0);
  xSemaphoreGive(ioDone);
  return true;
}

static bool wait_io(uint32_t timeoutMs)
{
  return xSemaphoreTake(ioDone, pdMS_TO_TICKS(timeoutMs)) == pdTRUE && ioOk;
}

void tuh_mount_cb(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  logf("HOST: device mounted addr=%u VID=%04X PID=%04X", dev_addr, vid, pid);
}

void tuh_umount_cb(uint8_t dev_addr) { logf("HOST: device unmounted addr=%u", dev_addr); }

void tuh_msc_mount_cb(uint8_t dev_addr)
{
  logf("HOST: MSC mounted addr=%u, LUNs=%u", dev_addr, tuh_msc_get_maxlun(dev_addr));
  mscDevAddr = dev_addr;
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
  logf("HOST: MSC unmounted addr=%u", dev_addr);
  mscDevAddr = 0;
}

static void host_task(void *)
{
  logf("host_task: tusb_init(rhport %d, HOST)", BOARD_TUH_RHPORT);
  tusb_rhport_init_t init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
  if (!tusb_init(BOARD_TUH_RHPORT, &init))
    logf("host_task: tusb_init FAILED");
  for (;;)
    tuh_task(); // blocks on the TinyUSB event queue
}

static void msc_test_task(void *)
{
  for (;;)
  {
    while (mscDevAddr == 0)
      vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t addr = mscDevAddr;
    const uint8_t lun = 0;

    tuh_msc_inquiry(addr, lun, &inquiryResp, io_complete_cb, 0);
    if (!wait_io(2000))
    {
      logf("MSC: inquiry failed/timeout");
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    logf("MSC: inquiry '%.8s' '%.16s' rev '%.4s'", inquiryResp.vendor_id, inquiryResp.product_id, inquiryResp.product_rev);

    // Capacity is read by TinyUSB during mount
    uint32_t blocks = tuh_msc_get_block_count(addr, lun);
    uint32_t bsize = tuh_msc_get_block_size(addr, lun);
    logf("MSC: %lu blocks x %lu bytes = %lu MiB", (unsigned long)blocks, (unsigned long)bsize,
         (unsigned long)((uint64_t)blocks * bsize / (1024 * 1024)));

    tuh_msc_read10(addr, lun, sectorBuf, 0, 1, io_complete_cb, 0);
    if (!wait_io(2000))
    {
      logf("MSC: read of sector 0 failed/timeout");
    }
    else
    {
      logf("MSC: sector 0 signature %02X %02X (expect 55 AA), first bytes %02X %02X %02X",
           sectorBuf[510], sectorBuf[511], sectorBuf[0], sectorBuf[1], sectorBuf[2]);
    }

#if SPIKE_PASSTHROUGH
    logf("MSC: pass-through build - sustained host read test skipped, the PC reads the stick via LUN 1");
    while (mscDevAddr == addr)
      vTaskDelay(pdMS_TO_TICKS(200));
    continue;
#endif
    // Sustained read test: 64-sector (32 KiB) chunks across the first 64 MiB, repeated,
    // looking for the intermittent hang reported in tinyusb#4605.
    const uint16_t chunk = 64;
    const uint32_t span = (blocks < 131072 ? blocks : 131072);
    uint32_t okCount = 0, failCount = 0, timeouts = 0;
    for (int pass = 0; pass < 3 && mscDevAddr == addr; pass++)
    {
      uint32_t t0 = millis();
      uint32_t bytes = 0;
      for (uint32_t lba = 0; lba + chunk <= span && mscDevAddr == addr; lba += chunk)
      {
        if (!tuh_msc_read10(addr, lun, sectorBuf, lba, chunk, io_complete_cb, 0))
        {
          failCount++;
          vTaskDelay(1);
          continue;
        }
        if (xSemaphoreTake(ioDone, pdMS_TO_TICKS(3000)) != pdTRUE)
        {
          timeouts++;
          logf("MSC: TIMEOUT at lba %lu (pass %d) - possible hang", (unsigned long)lba, pass);
          break;
        }
        if (ioOk)
        {
          okCount++;
          bytes += chunk * bsize;
        }
        else
          failCount++;
      }
      uint32_t dt = millis() - t0;
      logf("MSC: pass %d read %lu KiB in %lu ms = %lu KiB/s (ok=%lu fail=%lu timeouts=%lu) VBUS fault asserts so far=%lu", pass,
           (unsigned long)(bytes / 1024), (unsigned long)dt, (unsigned long)(dt ? bytes / dt * 1000 / 1024 : 0),
           (unsigned long)okCount, (unsigned long)failCount, (unsigned long)timeouts, (unsigned long)faultAsserts);
      if (timeouts)
        break;
    }
    logf("MSC: test complete, waiting for re-insert");
    while (mscDevAddr == addr)
      vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// VBUS fault monitor: every edge on PD15 is counted in the ISR and logged by fault_task
static void fault_isr()
{
  faultEdges++;
  lastFaultEdgeMs = millis();
  if (digitalRead(PIN_HOST_VBUS_FAULT) == LOW)
    faultAsserts++;
}

static void fault_task(void *)
{
  int last = digitalRead(PIN_HOST_VBUS_FAULT);
  uint32_t lastEdges = 0;
  logf("FAULT: monitor started, PD15=%d (1 = no fault)", last);
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(5));
    int now = digitalRead(PIN_HOST_VBUS_FAULT);
    uint32_t edges = faultEdges;
    if (now != last || edges != lastEdges)
    {
      logf("FAULT: PD15 %d -> %d (%s), edges=%lu asserts=%lu last edge at %lu ms", last, now,
           now ? "no fault" : "FAULT ASSERTED", (unsigned long)edges, (unsigned long)faultAsserts,
           (unsigned long)lastFaultEdgeMs);
      last = now;
      lastEdges = edges;
    }
  }
}

void spike_logf(const char *fmt, ...);
#if SPIKE_PASSTHROUGH
extern volatile uint32_t ptReads, ptErrors, ptBytes, ptBusy;
#endif

static void heartbeat_task(void *)
{
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));
#if GTE_LUN0_SD
    logf("sdlun: reads=%lu (%lu B) writes=%lu (%lu B) errors=%lu", (unsigned long)sdReads, (unsigned long)sdBytesRead,
         (unsigned long)sdWrites, (unsigned long)sdBytesWritten, (unsigned long)sdErrors);
#endif
#if SPIKE_PASSTHROUGH
    logf("passthrough: reads=%lu bytes=%lu errors=%lu busy=%lu", (unsigned long)ptReads, (unsigned long)ptBytes, (unsigned long)ptErrors, (unsigned long)ptBusy);
#endif
    logf("heartbeat: PD15=%d fault edges=%lu asserts=%lu, free heap=%u", digitalRead(PIN_HOST_VBUS_FAULT), (unsigned long)faultEdges, (unsigned long)faultAsserts, (unsigned)xPortGetFreeHeapSize());
  }
}

void setup()
{
  dbg.begin(115200);
  delay(50);
  logf("=== USB spike stage %d (host MSC on OTG_FS%s%s) ===", SPIKE_PASSTHROUGH ? 3 : (SPIKE_DEVICE ? 2 : 1), SPIKE_DEVICE ? " + device CDC/MSC on OTG_HS" : "", SPIKE_PASSTHROUGH ? (GTE_LUN0_SD ? " + SD card LUN 0 + stick pass-through LUN 1" : " + stick pass-through LUN 1") : "");
  logf("RCC_CR=%08lX CFGR=%08lX PLLCKSELR=%08lX D2CCIP2R=%08lX SYSCLK=%lu",
       (unsigned long)RCC->CR, (unsigned long)RCC->CFGR, (unsigned long)RCC->PLLCKSELR, (unsigned long)RCC->D2CCIP2R,
       (unsigned long)HAL_RCC_GetSysClockFreq());
  logf("USBSEL=%lu (1=PLL1Q, 2=PLL3Q, 3=HSI48)", (unsigned long)((RCC->D2CCIP2R >> 20) & 3));

  pinMode(PIN_HOST_VBUS_FAULT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_HOST_VBUS_FAULT), fault_isr, CHANGE);
  pinMode(PIN_HOST_VBUS_EN, OUTPUT);
  digitalWrite(PIN_HOST_VBUS_EN, HIGH); // TPS2553: EN active high
  delay(20);
  logf("Host VBUS enabled (PD14 high); fault pin PD15=%d (1 = no fault)", digitalRead(PIN_HOST_VBUS_FAULT));

  usb_host_hw_init();
#if SPIKE_DEVICE
  usb_device_hw_init();
#endif

  logMutex = xSemaphoreCreateMutex();
  ioDone = xSemaphoreCreateBinary();
  xTaskCreate(host_task, "usbh", 8192 / 4, nullptr, configMAX_PRIORITIES - 1, nullptr);
#if SPIKE_DEVICE
  xTaskCreate(device_task, "usbd", 8192 / 4, nullptr, configMAX_PRIORITIES - 1, nullptr);
#endif
  xTaskCreate(msc_test_task, "msc", 8192 / 4, nullptr, 2, nullptr);
  xTaskCreate(fault_task, "fault", 8192 / 4, nullptr, 3, nullptr);
  xTaskCreate(heartbeat_task, "hb", 8192 / 4, nullptr, 1, nullptr);
  logf("Starting scheduler");
  vTaskStartScheduler();
  logf("Scheduler returned - should never happen");
}

void loop() {}
