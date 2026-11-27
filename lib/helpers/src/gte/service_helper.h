// Registers Services, initialises them in order, then gives each its own
// FreeRTOS task.
#pragma once
#include <cstddef>

#include "gte/base_service.h"
#include "gte/service_id.h"

namespace gte {

class ServiceHelper {
 public:
  static constexpr size_t kMaxServices = 24;

  void RegisterService(BaseService *s);
  void InitialiseAll();
  [[noreturn]] void
  Start();  // creates the tasks, starts the watchdog and the scheduler

  BaseService *Get(ServiceId id);
  size_t count() const { return count_; }
  BaseService *At(size_t i) { return services_[i]; }
  BaseService *ForTask(void *task);

  void SetRestartRequired() { restart_required_ = true; }
  bool restart_required() const { return restart_required_; }

 private:
  static void TaskMain(void *service);
  BaseService *services_[kMaxServices] = {};
  size_t count_ = 0;
  bool restart_required_ = false;
};

}  // namespace gte
