/** \copyright
 * Copyright (c) 2015, Balazs Racz
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
 * \file MultiConfiguredConsumer.hxx
 *
 * Consumer class that uses CDI configuration and many GPIO pins to export a
 * multiple of output pins as two event consumers each to OpenLCB.
 *
 * @author Balazs Racz
 * @date 6 Sep 2015
 */

#ifndef _NMRANET_MULTICONFIGUREDCONSUMER_HXX_
#define _NMRANET_MULTICONFIGUREDCONSUMER_HXX_

#include "nmranet/ConfigRepresentation.hxx"

namespace nmranet
{

/// Version of the @ref ConfiguredConsumer class that can handle many GPIO pins
/// with two events each. This saves very significan amount of memory compared
/// to instantiating individual ConfigredConsumer instances -- helpful when a
/// single small MCU is exporting a large number of IOs via for example shift
/// register outputs.
class MultiConfiguredConsumer : public ConfigUpdateListener,
                                private SimpleEventHandler
{
public:
    typedef ConsumerConfig config_entry_type;

    template <unsigned N>
    __attribute__((noinline))
    MultiConfiguredConsumer(Node *node, const Gpio *const *pins, unsigned size,
        const RepeatedGroup<config_entry_type, N> &config)
        : node_(node)
        , pins_(pins)
        , size_(N)
        , offset_(config)
    {
        HASSERT(size_ == N);
        ConfigUpdateService::instance()->register_update_listener(this);
    }

    ~MultiConfiguredConsumer()
    {
        do_unregister();
        ConfigUpdateService::instance()->unregister_update_listener(this);
    }

    UpdateAction apply_configuration(int fd, bool initial_load,
                                     BarrierNotifiable *done) OVERRIDE
    {
        AutoNotify n(done);

        if (!initial_load)
        {
            // There is no way to figure out what the previously registered
            // eventid values were for the individual pins. Therefore we always
            // unregister everything and register them anew. It also causes us
            // to identify all. This is not a problem since apply_configuration
            // is coming from a user action.
            do_unregister();
        }
        RepeatedGroup<config_entry_type, UINT_MAX> grp_ref(offset_.offset());
        for (unsigned i = 0; i < size_; ++i)
        {
            const config_entry_type cfg_ref(grp_ref.entry(i));
            EventId cfg_event_on = cfg_ref.event_on().read(fd);
            EventId cfg_event_off = cfg_ref.event_off().read(fd);
            EventRegistry::instance()->register_handler(
                EventRegistryEntry(this, cfg_event_off, i * 2), 0);
            EventRegistry::instance()->register_handler(
                EventRegistryEntry(this, cfg_event_on, i * 2 + 1), 0);
        }
        return REINIT_NEEDED; // Causes events identify.
    }

    /// @todo(balazs.racz): implement
    void factory_reset(int fd) OVERRIDE
    {
    }

    // Implementations for the event handler functions.

    void HandleIdentifyGlobal(const EventRegistryEntry &registry_entry,
                              EventReport *event, BarrierNotifiable *done)
        OVERRIDE
    {
        if (event->dst_node && event->dst_node != node_)
        {
            return done->notify();
        }
        SendConsumerIdentified(registry_entry, done);
    }

    void HandleIdentifyConsumer(const EventRegistryEntry &registry_entry,
                                EventReport *event, BarrierNotifiable *done)
        OVERRIDE
    {
        if (event->event != registry_entry.event)
        {
            return done->notify();
        }
        SendConsumerIdentified(registry_entry, done);
    }

    void HandleEventReport(const EventRegistryEntry &registry_entry,
                           EventReport *event, BarrierNotifiable *done) OVERRIDE
    {
        if (event->event != registry_entry.event)
        {
            return done->notify();
        }
        const Gpio *pin = pins_[registry_entry.user_arg >> 1];
        const bool is_on = (registry_entry.user_arg & 1);
        pin->write(is_on);
        done->notify();
    }

private:
    /// Sends out a ConsumerIdentified message for the given registration
    /// entry.
    void SendConsumerIdentified(const EventRegistryEntry &registry_entry,
                                BarrierNotifiable *done)
    {
        Defs::MTI mti = Defs::MTI_CONSUMER_IDENTIFIED_VALID;
        if (pins_[registry_entry.user_arg >> 1]->is_clr())
        {
            mti++; // INVALID
        }
        event_write_helper3.WriteAsync(node_, mti, WriteHelper::global(),
                                       eventid_to_buffer(registry_entry.event),
                                       done);
    }

    /// Removed registration of this event handler from the global event
    /// registry.
    void do_unregister()
    {
        EventRegistry::instance()->unregister_handler(this);
    }

    Node *node_;              //< virtual node to export the consumer on
    const Gpio *const *pins_; //< array of all GPIO pins to use
    size_t size_;             //< number of GPIO pins to export
    ConfigReference offset_;  //< Offset in the configuration space for our
    // configs.
};

} // namespace nmranet

#endif // _NMRANET_MULTICONFIGUREDCONSUMER_HXX_
