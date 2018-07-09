/*
 * Image recognition performance verification
 *
 * This software is not subject to copyright protection
 */

#include <cstring>

#include "nullimplsrpv11.h"

using namespace std;
using namespace SRPV;

NullImplSRPV11::NullImplSRPV11() {}

NullImplSRPV11::~NullImplSRPV11() {}

ReturnStatus
NullImplSRPV11::initialize(const std::string &configDir)
{
    return ReturnStatus(ReturnCode::Success);
}

ReturnStatus
NullImplSRPV11::createTemplate(const SoundRecord &record,
        TemplateRole role,
        std::vector<uint8_t> &templ)
{
    string blurb{"I've seen things you people wouldn't believe...\n"};

    templ.resize(blurb.size());
    memcpy(templ.data(), blurb.c_str(), blurb.size());

    return ReturnStatus(ReturnCode::Success);
}

ReturnStatus
NullImplSRPV11::matchTemplates(
        const std::vector<uint8_t> &verifTemplate,
        const std::vector<uint8_t> &enrollTemplate,
        double &similarity)
{
    similarity = std::rand();
    return ReturnStatus(ReturnCode::Success);
}

std::shared_ptr<VerifInterface>
VerifInterface::getImplementation()
{
    return std::make_shared<NullImplSRPV11>();
}





