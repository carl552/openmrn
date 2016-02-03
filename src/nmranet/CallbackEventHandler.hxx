/** \copyright
 * Copyright (c) 2013, Balazs Racz
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
 * \file CallbackEventHandler.hxx
 *
 * Defines an event handler implementation that is parametrized by callback
 * functions to define its properties.
 *
 * @author Balazs Racz
 * @date 18 December 2015
 */

#ifndef _NMRANET_CALLBACKEVENTHANDLER_HXX_
#define _NMRANET_CALLBACKEVENTHANDLER_HXX_

#include <functional>

#include "nmranet/EventHandlerTemplates.hxx"

namespace nmranet
{

class CallbackEventHandler : public SimpleEventHandler
{
public:
    /// This function (signature) is called every time a given event report
    /// arrives.
    ///
    /// @param registry_entry is the included event registry entry. The args
    /// bits are partially used internally.
    ///
    /// @param done may be used to create additional children; it does not need
    /// to be notified in the handler (the caller will do that once).
    typedef std::function<void(const EventRegistryEntry &registry_entry,
        EventReport *report, BarrierNotifiable *done)> EventReportHandlerFn;
    /// Function signature that returns the event state for the current
    /// registry entry. Implementors must note that the registry entry has to
    /// be used to figure out which bit this is and whether it is on or off.
    typedef std::function<EventState(const EventRegistryEntry &registry_entry,
        EventReport *report)> EventStateHandlerFn;

    enum RegistryEntryBits
    {
        IS_PRODUCER = (1U << 31),
        IS_CONSUMER = (1U << 30),
        USER_BIT_MASK = IS_CONSUMER - 1,
    };

    CallbackEventHandler(Node *node, EventReportHandlerFn report_handler,
        EventStateHandlerFn state_handler)
        : reportHandler_(std::move(report_handler))
        , stateHandler_(std::move(state_handler))
        , node_(node)
    {
    }

    ~CallbackEventHandler()
    {
        EventRegistry::instance()->unregister_handler(this);
    }

    void add_entry(EventId event, uint32_t entry_bits)
    {
        EventRegistry::instance()->register_handler(
            EventRegistryEntry(this, event, entry_bits), 0);
    }

    void HandleEventReport(const EventRegistryEntry &entry, EventReport *event,
        BarrierNotifiable *done) override
    {
        reportHandler_(entry, event, done);
        done->notify();
    }

    void HandleIdentifyConsumer(const EventRegistryEntry &registry_entry,
        EventReport *event, BarrierNotifiable *done) override
    {
        if (registry_entry.user_arg & IS_CONSUMER)
        {
            SendConsumerIdentified(registry_entry, event, done);
        }
        done->notify();
    };

    void HandleIdentifyProducer(const EventRegistryEntry &registry_entry,
        EventReport *event, BarrierNotifiable *done) override
    {
        if (registry_entry.user_arg & IS_PRODUCER)
        {
            SendProducerIdentified(registry_entry, event, done);
        }
        done->notify();
    };

    void HandleIdentifyGlobal(const EventRegistryEntry &registry_entry,
        EventReport *event, BarrierNotifiable *done) override
    {
        if (registry_entry.user_arg & IS_PRODUCER)
        {
            SendProducerIdentified(registry_entry, event, done);
        }
        if (registry_entry.user_arg & IS_CONSUMER)
        {
            SendConsumerIdentified(registry_entry, event, done);
        }
        done->notify();
    };

protected:
    void SendProducerIdentified(const EventRegistryEntry &entry,
        EventReport *event, BarrierNotifiable *done)
    {
        EventState state = stateHandler_(entry, event);
        Defs::MTI mti = Defs::MTI_PRODUCER_IDENTIFIED_VALID + state;
        event_write_helper1.WriteAsync(node_, mti, WriteHelper::global(),
            eventid_to_buffer(entry.event), done->new_child());
    }

    void SendConsumerIdentified(const EventRegistryEntry &entry,
        EventReport *event, BarrierNotifiable *done)
    {
        EventState state = stateHandler_(entry, event);
        Defs::MTI mti = Defs::MTI_CONSUMER_IDENTIFIED_VALID + state;
        event_write_helper3.WriteAsync(node_, mti, WriteHelper::global(),
            eventid_to_buffer(entry.event), done->new_child());
    }

private:
    EventReportHandlerFn reportHandler_;
    EventStateHandlerFn stateHandler_;
    Node *node_;
};

} // namespace nmranet

#endif // _NMRANET_CALLBACKEVENTHANDLER_HXX_