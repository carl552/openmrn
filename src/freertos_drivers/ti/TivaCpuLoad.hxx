/** \copyright
 * Copyright (c) 2012, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file TivaCpuLoad.hxx
 *
 * Collect CPU load information using a tiva hardware timer.
 *
 * @author Balazs Racz
 * @date 11 Jul 2016
 */

#ifndef _FREERTOS_DRIVERS_TI_TIVACPULOAD_HXX_
#define _FREERTOS_DRIVERS_TI_TIVACPULOAD_HXX_

#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "freertos_drivers/common/CpuLoad.hxx"

/// Default hardware structure for the TivaCpuLoad driver.
struct TivaCpuLoadDefHw
{
    static constexpr auto TIMER_BASE = TIMER4_BASE;
    static constexpr auto TIMER_PERIPH = SYSCTL_PERIPH_TIMER4;
    static constexpr auto TIMER_INTERRUPT = INT_TIMER4A;
    static constexpr unsigned TIMER_PERIOD = 80000000 / 127;
};

/// Driver to collect CPU load information (in freertos) using a Tiva hardware
/// timer. At any point in time there can be only one instance of this class.
template<class HW>
class TivaCpuLoad {
public:
    /// Constructor.
    TivaCpuLoad() {
        MAP_SysCtlPeripheralEnable(HW::TIMER_PERIPH);
        MAP_TimerDisable(HW::TIMER_BASE, TIMER_A);
        MAP_TimerClockSourceSet(HW::TIMER_BASE, TIMER_CLOCK_SYSTEM);
        MAP_TimerConfigure(HW::TIMER_BASE, TIMER_CFG_PERIODIC);

        MAP_TimerLoadSet(HW::TIMER_BASE, TIMER_A, HW::TIMER_PERIOD);

        MAP_IntDisable(HW::TIMER_INTERRUPT);
        MAP_IntPrioritySet(
            HW::TIMER_INTERRUPT, configKERNEL_INTERRUPT_PRIORITY);
        MAP_TimerIntEnable(HW::TIMER_BASE, TIMER_TIMA_TIMEOUT);
        MAP_TimerEnable(HW::TIMER_BASE, TIMER_A);
        MAP_IntEnable(HW::TIMER_INTERRUPT);
    }

    /// Call this function from extern "C" void timer4a_interrupt_handler().
    void interrupt_handler() {
        MAP_TimerIntClear(HW::TIMER_BASE, TIMER_TIMA_TIMEOUT);
        cpuload_tick();
    }

    /// The singleton. implementation we delegate collecting the CPULoad
    /// information.
    CpuLoad load_;
};



#endif  //  _FREERTOS_DRIVERS_TI_TIVACPULOAD_HXX_