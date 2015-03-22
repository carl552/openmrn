/** \copyright
 * Copyright (c) 2014, Balazs Racz
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
 * \file SimpleNodeInfo.hxx
 *
 * Handler for the Simple Node Ident Info protocol.
 *
 * @author Balazs Racz
 * @date 24 Jul 2013
 */

#include "nmranet/If.hxx"
#include "nmranet/SimpleInfoProtocol.hxx"

namespace nmranet
{

struct SimpleNodeStaticValues {
  const uint8_t version;
  const char manufacturer_name[41];
  const char model_name[41];
  const char hardware_version[21];
  const char software_version[21];
};

struct SimpleNodeDynamicValues {
  const uint8_t version;
  const char user_name[63];
  const char user_description[64];
};

/** This static data will be exported as the first block of SNIP. The version
 *  field must contain "4". */
extern const SimpleNodeStaticValues SNIP_STATIC_DATA;
/** The SNIP dynamic data will be read from this file. It should be 128 bytes
 *  long, and include the version number of "2" at the beginning. */
extern const char* const SNIP_DYNAMIC_FILENAME;

/** Helper function for test nodes. Fills a file with the given SNIP user
 * values. */
void init_snip_user_file(int fd, const string &user_name,
                         const string &user_description);

class SNIPHandler : public IncomingMessageStateFlow
{
public:
    SNIPHandler(If *interface, SimpleInfoFlow *response_flow)
        : IncomingMessageStateFlow(interface)
        , responseFlow_(response_flow)
    {
        HASSERT(SNIP_STATIC_DATA.version == 4);
        HASSERT(sizeof(SNIP_STATIC_DATA) == 125);
        interface->dispatcher()->register_handler(
            this, Defs::MTI_IDENT_INFO_REQUEST, Defs::MTI_EXACT);
    }

    ~SNIPHandler()
    {
        interface()->dispatcher()->unregister_handler(
            this, Defs::MTI_IDENT_INFO_REQUEST, Defs::MTI_EXACT);
    }

    Action entry() OVERRIDE
    {
        if (!nmsg()->dstNode)
            return release_and_exit();
        return allocate_and_call(responseFlow_, STATE(send_response_request));
    }

    Action send_response_request()
    {
        auto *b = get_allocation_result(responseFlow_);
        b->data()->reset(nmsg(), SNIP_RESPONSE, Defs::MTI_IDENT_INFO_REPLY);
        responseFlow_->send(b);
        return release_and_exit();
    }

private:
    /** Defines the SNIP response fields. */
    static const SimpleInfoDescriptor SNIP_RESPONSE[];

    SimpleInfoFlow *responseFlow_;
};

} // namespace nmranet
