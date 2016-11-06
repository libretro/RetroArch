#ifndef WIIU_H
#define WIIU_H

#define wiiu_bus_clock             (17 * 13 * 5*5*5 * 5*5*5     * 3*3 * 2*2*2) //   248.625000 Mhz
#define wiiu_cpu_clock             (17 * 13 * 5*5*5 * 5*5*5 * 5 * 3*3 * 2*2*2) //  1243.125000 Mhz
#define wiiu_timer_clock           (17 * 13 * 5*5*5 * 5*5*5     * 3*3 * 2)     //    62.156250 Mhz

#define sec_to_ticks(s)          (((17 * 13 * 5*5*5 * 5*5*5 * 3*3 * 2) * (uint64_t)(s)))
#define ms_to_ticks(ms)          (((17 * 13 * 5*5*5 * 3*3) * (uint64_t)(ms)) / (2*2))
#define us_to_ticks(us)          (((17 * 13 * 3*3) * (uint64_t)(us)) / (2*2* 2*2*2))
#define ns_to_ticks(ns)          (((17 * 13 * 3*3) * (uint64_t)(ns)) / (2*2* 2*2*2* 2*2*2 *5*5*5))

#define ticks_to_sec(ticks)      (((uint64_t)(ticks)) / (17 * 13 * 5*5*5 * 5*5*5 * 3*3 * 2))
#define ticks_to_ms(ticks)       (((uint64_t)(ticks) * (2*2)) / (17 * 13 * 5*5*5 * 3*3))
#define ticks_to_us(ticks)       (((uint64_t)(ticks) * (2*2 * 2*2*2)) / (17 * 13 * 3*3))
#define ticks_to_ns(ticks)       (((uint64_t)(ticks) * (2*2 * 2*2*2 * 2*2*2 * 5*5*5)) / (17 * 13 * 3*3))

#endif // WIIU_H
