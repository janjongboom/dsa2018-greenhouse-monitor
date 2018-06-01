#include "select_program.h"

#if PROGRAM == TEST_SLEEP

#include "mbed.h"
#include "rtc_api_hal.h"


enum WakeupType {
    WAKEUP_RESET,
    WAKEUP_TIMER,
    WAKEUP_PIN
};

DigitalOut active_led(LED1, 0);

#define WAKEUP_TIMEOUT_S 10 // timeout to remain in standby mode before waking up through reset

bool standby_allowed = true; // for locking out the standby if firmware update has started

static RTC_HandleTypeDef RtcHandle;

void rtc_set_wake_up_timer_s(uint32_t delta)
{
    uint32_t clock = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;

    // HAL_RTCEx_SetWakeUpTimer_IT will assert that delta is 0xFFFF at max
    if (delta > 0xFFFF) {
        delta -= 0x10000;
        clock = RTC_WAKEUPCLOCK_CK_SPRE_17BITS;
    }

    RtcHandle.Instance = RTC;

    HAL_StatusTypeDef status = HAL_RTCEx_SetWakeUpTimer_IT(&RtcHandle, delta, clock);

    if (status != HAL_OK) {
        error("Set wake up timer failed: %d\n", status);
     }
}

WakeupType get_wakeup_type() {

    if(READ_BIT(RTC->ISR, RTC_ISR_WUTF))
        return WAKEUP_TIMER;

    // this is set by timer too, but that's checked already
    // above.
    if(READ_BIT(PWR->CSR, PWR_CSR_WUF))
        return WAKEUP_PIN;

    return WAKEUP_RESET;
}

void standby() {

    printf("Going to sleep!\n");

    core_util_critical_section_enter();

    // Enable wakeup pin PA_0.
    // HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    // Clear wakeup flag, just in case.
    SET_BIT(PWR->CR, PWR_CR_CWUF);

    // Enable wakeup timer.
    rtc_set_wake_up_timer_s(WAKEUP_TIMEOUT_S);

    // Enable debug interface working in standby. Causes power consumption to increase drastically while in standby.
    //HAL_DBGMCU_EnableDBGStandbyMode();

    HAL_PWR_EnterSTANDBYMode();

    // this should not happen...
    rtc_deactivate_wake_up_timer();
    core_util_critical_section_exit();

    printf("Got out of standby mode...\n")

    // something went wrong, let's reset
    NVIC_SystemReset();
}


int main()
{
    printf("Hello world\n");

    // run_application() will first initialize the program and then call main_application()
    set_time(0);

    active_led = 1;
    wait_ms(200);
    active_led = 0;
    wait_ms(200);
    active_led = 1;

    standby();
}


#endif // PROGRAM == TEST_SLEEP
