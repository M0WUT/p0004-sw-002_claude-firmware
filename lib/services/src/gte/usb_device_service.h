// The USB_DEVICE Service (issue #44): TinyUSB on the LCD Board's device port,
// CDC only for now (MSC comes with the Storage Service, #47). Identity from
// Release USB identity (issue #38): CAFE:4004 until the pid.codes PID is
// granted, Manufacturer "M0WUT", Product = the Instrument name, Serial = the
// STM32 unique ID, bcdDevice = the Core version.
#pragma once
#include <cstddef>
#include <cstdint>

#include "gte/base_service.h"
#include "gte/error_code.h"

namespace gte {

GTE_ERROR_CODE(kUsbDeviceInitFailed, 901, kWarning,
               "USB device stack failed to start");
static_assert(CodeBelongsTo(kUsbDeviceInitFailed.code, ServiceId::kUsbDevice),
              "USB_DEVICE codes are 901-999");

class UsbDeviceService : public BaseService {
 public:
  static constexpr uint16_t kVid = 0xCAFE;
  static constexpr uint16_t kPid = 0x4004;

  // USB Services run at the highest priority (issue #10).
  UsbDeviceService()
      : BaseService(ServiceId::kUsbDevice, TaskSettings{1, 2048, 4}) {}

  // The CDC port, for the SCPI Service: open while the Connected PC has
  // configured the device and holds DTR high.
  bool CdcSessionOpen();
  size_t CdcRead(uint8_t *buf, size_t size);
  // Blocks while the PC is slow to read; gives up if the session closes.
  size_t CdcWrite(const uint8_t *buf, size_t size);
  void CdcFlush();
  void CdcDiscardInput();

  // TinyUSB callbacks (USB_DEVICE task).
  void OnMounted(bool mounted);

 protected:
  void Initialise() override;
  void Tick() override;

 private:
  enum class State : uint8_t { kStart, kDisconnected, kRunning };
  State state_ = State::kStart;
  uint32_t disconnected_at_ = 0;
};

}  // namespace gte
