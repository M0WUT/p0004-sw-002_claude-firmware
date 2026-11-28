#include "gte/usb_device_service.h"

#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include <cstring>

#include "generated/version.h"
#include "gte/gte.h"
#include "tusb.h"  // NOLINT(build/include_subdir): TinyUSB names it so

namespace gte {

namespace {

constexpr uint8_t kRhport = GTE_USB_DEVICE_RHPORT;
constexpr uint32_t kDisconnectMs = 1500;
constexpr uint32_t kWriteTimeoutMs = 1000;

UsbDeviceService *g_service = nullptr;

// String descriptors 1-3 (0 is the language), filled in at Initialise.
char g_product[48] = "GTE Core";
char g_serial[25] = "";

void HardwareInit() {
#if GTE_USB_DEVICE_OTG_HS
  // PB14 = D-, PB15 = D+, AF12: OTG_HS with its internal full-speed PHY.
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
  // No external ULPI PHY: its clock must stay off (also in sleep), or the
  // core waits for it.
  __HAL_RCC_USB1_OTG_HS_ULPI_CLK_DISABLE();
  __HAL_RCC_USB1_OTG_HS_ULPI_CLK_SLEEP_DISABLE();
  NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#else
#error "Only the OTG_HS device port is supported"
#endif
}

}  // namespace

void UsbDeviceService::Initialise() {
  g_service = this;
  snprintf(g_product, sizeof(g_product), "%s", InstrumentName());
  UniqueIdString(g_serial, sizeof(g_serial));
  HardwareInit();
  LogInfof("%04X:%04X \"M0WUT\" \"%s\" serial %s bcdDevice %04X, rhport %u",
           kVid, kPid, g_product, g_serial, GTE_CORE_VERSION_BCD, kRhport);
}

void UsbDeviceService::Tick() {
  switch (state_) {
    case State::kStart: {
      tusb_rhport_init_t init = {TUSB_ROLE_DEVICE, TUSB_SPEED_AUTO};
      if (!tusb_init(kRhport, &init)) Warn(kUsbDeviceInitFailed, "tusb_init");
      // A clean, long unplug after every reset, so the PC (and a VM's USB
      // filter) sees a new device rather than a reset of the old one.
      tud_disconnect();
      disconnected_at_ = millis();
      state_ = State::kDisconnected;
      break;
    }
    case State::kDisconnected:
      if (millis() - disconnected_at_ >= kDisconnectMs) {
        tud_connect();
        LogInfo("D+ pull-up connected");
        state_ = State::kRunning;
      }
      break;
    case State::kRunning:
      tud_task_ext(1, false);
      break;
  }
}

void UsbDeviceService::OnMounted(bool mounted) {
  LogInfo(mounted ? "Configured by the Connected PC" : "Unconfigured");
}

bool UsbDeviceService::CdcSessionOpen() {
  return state_ == State::kRunning && tud_mounted() && tud_cdc_connected();
}

size_t UsbDeviceService::CdcRead(uint8_t *buf, size_t size) {
  if (state_ != State::kRunning || !tud_cdc_available()) return 0;
  return tud_cdc_read(buf, size);
}

size_t UsbDeviceService::CdcWrite(const uint8_t *buf, size_t size) {
  size_t done = 0;
  const uint32_t start = millis();
  while (done < size && CdcSessionOpen()) {
    done += tud_cdc_write(buf + done, size - done);
    if (done < size) {
      tud_cdc_write_flush();
      if (millis() - start > kWriteTimeoutMs) break;
      vTaskDelay(1);
    }
  }
  return done;
}

void UsbDeviceService::CdcFlush() {
  if (state_ == State::kRunning) tud_cdc_write_flush();
}

void UsbDeviceService::CdcDiscardInput() {
  if (state_ == State::kRunning) tud_cdc_read_flush();
}

}  // namespace gte

// ------------------------------------------------------- TinyUSB callbacks

extern "C" void OTG_HS_IRQHandler(void) {
  tusb_int_handler(GTE_USB_DEVICE_RHPORT, true);
}

void tud_mount_cb(void) {
  if (gte::g_service) gte::g_service->OnMounted(true);
}

void tud_umount_cb(void) {
  if (gte::g_service) gte::g_service->OnMounted(false);
}

namespace {

enum StringIndex : uint8_t {
  kLangId = 0,
  kManufacturer,
  kProduct,
  kSerial,
  kCdcInterface
};

constexpr uint8_t kItfCdc = 0;
constexpr uint8_t kItfCount = 2;  // CDC control + data
constexpr uint8_t kEpCdcNotify = 0x81;
constexpr uint8_t kEpCdcOut = 0x02;
constexpr uint8_t kEpCdcIn = 0x82;
constexpr uint16_t kConfigLength = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN;

const tusb_desc_device_t kDevice = {
    sizeof(tusb_desc_device_t),
    TUSB_DESC_DEVICE,
    0x0200,  // USB 2.0
    TUSB_CLASS_MISC,
    MISC_SUBCLASS_COMMON,
    MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE,
    gte::UsbDeviceService::kVid,
    gte::UsbDeviceService::kPid,
    GTE_CORE_VERSION_BCD,
    kManufacturer,
    kProduct,
    kSerial,
    1,
};

const uint8_t kConfiguration[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfCount, 0, kConfigLength, 0x00, 100),
    TUD_CDC_DESCRIPTOR(kItfCdc, kCdcInterface, kEpCdcNotify, 8, kEpCdcOut,
                       kEpCdcIn, 64),
};

}  // namespace

uint8_t const *tud_descriptor_device_cb(void) {
  return reinterpret_cast<uint8_t const *>(&kDevice);
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return kConfiguration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  static uint16_t desc[1 + 48];
  size_t n = 0;
  if (index == kLangId) {
    desc[1] = 0x0409;  // English (US)
    n = 1;
  } else {
    const char *s = nullptr;
    switch (index) {
      case kManufacturer:
        s = "M0WUT";
        break;
      case kProduct:
        s = gte::g_product;
        break;
      case kSerial:
        s = gte::g_serial;
        break;
      case kCdcInterface:
        s = "SCPI";
        break;
      default:
        return nullptr;
    }
    for (; s[n] && n < 48; n++) desc[1 + n] = static_cast<uint8_t>(s[n]);
  }
  desc[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * n + 2));
  return desc;
}
