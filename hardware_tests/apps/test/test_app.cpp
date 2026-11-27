// Test Application for the hardware suite (never shipped). Two Services exercise the Service
// framework; pytest picks a scenario by writing SCENARIO_ADDR over SWD while the core is held in
// reset (DTCM is unused by the linker and untouched by startup code). Issue #42.
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "gte/ApplicationService.h"
#include "gte/Gte.h"
#include "gte/LoggingHelper.h"
#include "gte/ServiceHelper.h"

using namespace gte;

namespace test_errors {
GTE_ERROR_CODE(TEST_WARNING_1, 1401, WARNING, "Test warning 1");
GTE_ERROR_CODE(TEST_WARNING_2, 1402, WARNING, "Test warning 2");
GTE_ERROR_CODE(TEST_WARNING_3, 1403, WARNING, "Test warning 3");
GTE_ERROR_CODE(TEST_WARNING_4, 1404, WARNING, "Test warning 4");
GTE_ERROR_CODE(TEST_WARNING_5, 1405, WARNING, "Test warning 5");
GTE_ERROR_CODE(TEST_WARNING_6, 1406, WARNING, "Test warning 6");
GTE_ERROR_CODE(TEST_WARNING_7, 1407, WARNING, "Test warning 7");
GTE_ERROR_CODE(TEST_WARNING_8, 1408, WARNING, "Test warning 8");
GTE_ERROR_CODE(TEST_WARNING_9, 1409, WARNING, "Test warning 9");
GTE_ERROR_CODE(TEST_FATAL, 1450, FATAL, "Test fatal error");
static_assert(code_belongs_to(TEST_WARNING_1.code, ServiceId::APPLICATION_TEST), "codes are 1401-1499");
}  // namespace test_errors
using namespace test_errors;

static const ErrorCode *const WARNINGS[] = {&TEST_WARNING_1, &TEST_WARNING_2, &TEST_WARNING_3,
                                            &TEST_WARNING_4, &TEST_WARNING_5, &TEST_WARNING_6,
                                            &TEST_WARNING_7, &TEST_WARNING_8, &TEST_WARNING_9};

enum Scenario : uint16_t { NORMAL = 0, WARNINGS_CYCLE = 1, NINE_WARNINGS = 2, FATAL_ERROR = 3, HANG = 4,
                           STACK_OVERFLOW_TASK = 5, LOG_LEVELS = 6 };

static volatile uint32_t *const SCENARIO_ADDR = (volatile uint32_t *)0x2001FFF0;
static uint16_t g_scenario = NORMAL;

static uint16_t take_scenario()
{
    uint32_t v = *SCENARIO_ADDR;
    *SCENARIO_ADDR = 0;  // one-shot: a later reset (e.g. by the watchdog) runs NORMAL
    return (v >> 16) == 0x6765 ? (uint16_t)(v & 0xFFFF) : NORMAL;
}

class TestAux : public BaseService {
public:
    TestAux() : BaseService(ServiceId::TEST_AUX, TaskSettings{250, 1024, 2}) {}

protected:
    void initialise() override { _info("Aux initialise"); }

    void tick() override
    {
        _n++;
        _debugf("aux debug %lu", (unsigned long)_n);
        _infof("aux tick %lu", (unsigned long)_n);
        if (g_scenario == HANG && _n == 4) {
            _info("Hanging on purpose");
            for (;;)
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (g_scenario == STACK_OVERFLOW_TASK && _n == 4) {
            _info("Overflowing the stack on purpose");
            recurse(0);
        }
    }

private:
    static uint32_t recurse(uint32_t depth)
    {
        volatile uint8_t pad[256];
        for (auto &b : pad)
            b = (uint8_t)depth;
        vTaskDelay(1);  // context switch: lets FreeRTOS check the stack
        return recurse(depth + 1) + pad[depth % sizeof(pad)];
    }
    uint32_t _n = 0;
};

class TestApp : public ApplicationService {
public:
    TestApp() : ApplicationService(ServiceId::APPLICATION_TEST, TaskSettings{100, 2048, 2}) {}

    bool validate_instrument_board() override
    {
        _info("validate_instrument_board called");
        return true;
    }

protected:
    void initialise() override
    {
        g_scenario = take_scenario();
        _infof("Test scenario %u", g_scenario);
    }

    void declare_config(ConfigList &l) override
    {
        static const char *const modes[] = {"OFF", "SLOW", "FAST"};
        l.integer("count", "Count", &_c.count, 0, 100, 5);
        l.number("gain", "Gain", &_c.gain, 0.0f, 10.0f, 0.5f, 1, "dB");
        l.boolean("enabled", "Enabled", &_c.enabled);
        l.text("name", "Name", _c.name, sizeof(_c.name));
        l.enumeration("mode", "Mode", &_c.mode, modes, 3);
        secret(l.text("password", "Password", _c.password, sizeof(_c.password)));
        reset_mode(l.integer("channels", "Channels", &_c.channels, 1, 8, 1));
    }

    bool validate_config_field(const char *id, const ConfigValue &v, char *reason, size_t size) override
    {
        if (!strcmp(id, "gain") && v.number > 8.0f && _c.enabled) {
            snprintf(reason, size, "Gain above 8 dB needs Enabled OFF");
            return false;
        }
        return true;
    }

    void on_config_changed(const char *id) override { _infof("Side effect for %s", id); }

    void get_status(StatusList &s) override
    {
        s.integer("dataUsed", "Data used", 1288490188LL, "B", DisplayHint::Bytes);
        s.number("temperature", "Temperature", 23.5f, 1, "°C");
        s.boolean("ready", "Ready", true);
        s.text("label", "Label", "TEST");
    }

    void tick() override
    {
        _n++;
        _infof("tick %lu", (unsigned long)_n);
        switch (g_scenario) {
        case NORMAL:
            if (_n == 1)
                exercise_config_and_status();
            break;
        case WARNINGS_CYCLE:
            if (_n == 5) _warning(TEST_WARNING_1, "first");
            if (_n == 10) _warning(TEST_WARNING_1, "second");
            if (_n == 15) _warning(TEST_WARNING_2, "other");
            if (_n == 20) _clear_warning(TEST_WARNING_1);
            if (_n == 25) _clear_warning(TEST_WARNING_2);
            if (_n == 5 || _n == 10 || _n == 15 || _n == 20 || _n == 25)
                _infof("active warnings=%u", (unsigned)log().warning_count());
            break;
        case NINE_WARNINGS:
            if (_n >= 5 && _n < 14)
                _warningf(*WARNINGS[_n - 5], "number %lu", (unsigned long)(_n - 4));
            break;
        case FATAL_ERROR:
            if (_n == 10)
                _error(TEST_FATAL, "boom");
            break;
        case LOG_LEVELS:
            if (_n == 10)
                services().get(ServiceId::CORE)->set_config("logLevel", "DEBUG");
            if (_n == 20) {
                // TEST_AUX's own confirmation is INFO, which its new level hides, so report it here.
                ConfigResult r = services().get(ServiceId::TEST_AUX)->set_config("logLevel", "WARNING");
                _infof("TEST_AUX logLevel -> WARNING: %s", r.ok ? "accepted" : r.message);
            }
            break;
        default:
            break;
        }
    }

private:
    void exercise_config_and_status()
    {
        set_config("count", "105");      // out of range
        set_config("count", "12");       // off step
        set_config("gain", "9.0");       // refused by validate_config_field (Enabled is ON)
        set_config("gain", "2.5");       // Live: applied, side effect runs
        set_config("channels", "6");     // Reset: restart required
        set_config("password", "newsecret");
        _infof("restart required=%d", services().restart_required() ? 1 : 0);
        StatusList list;
        status(list);
        for (size_t i = 0; i < list.count(); i++) {
            char shown[64], exact[64];
            list[i].display(shown, sizeof(shown));
            list[i].exact(exact, sizeof(exact));
            _infof("Status %s (%s): %s [exact %s]", list[i].id, list[i].label, shown, exact);
        }
    }

    struct {
        int32_t count = 10;
        float gain = 1.5f;
        bool enabled = true;
        char name[17] = "GTE";
        uint8_t mode = 1;
        char password[17] = "hunter2";
        int32_t channels = 4;
    } _c;
    uint32_t _n = 0;
};

static TestAux g_aux;
static TestApp g_app;
static BaseService *g_extra[] = {&g_aux};

ApplicationService *gte_application() { return &g_app; }
void gte_extra_services(BaseService **&list, size_t &count)
{
    list = g_extra;
    count = 1;
}
