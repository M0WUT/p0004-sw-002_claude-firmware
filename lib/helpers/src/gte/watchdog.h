// Copyright 2026 M0WUT
// The independent watchdog (issue #37): 4 s timeout, fed by a supervisor task
// only while every registered task has checked in within its deadline. Frozen
// while a debugger halts the core. The fatal loop feeds it directly, so the
// Fatal Error screen stays up.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gte {

class Watchdog {
 public:
  static constexpr uint32_t kTimeoutMs = 4000;
  static constexpr uint32_t kDefaultDeadlineMs = 2000;
  static constexpr size_t kMaxTasks = 32;

  int RegisterTask(const char *name, uint32_t deadline_ms = kDefaultDeadlineMs);
  void Checkin(int id);
  void Start();  // starts the IWDG and the supervisor task (just before the
                 // scheduler)
  static void Feed();

 private:
  static void Supervisor(void *self);
  struct Entry {
    const char *name;
    uint32_t deadline_ms;
    volatile uint32_t last_ms;
  };
  Entry tasks_[kMaxTasks];
  size_t count_ = 0;
};

const char *ResetReason();  // why the last reset happened, read once from the
                            // RCC flags at boot

}  // namespace gte
