/*
 * Face Recognition Performance Test
 *
 * This software is not subject to copyright protection and is in the public domain.
 */

#ifndef NULLIMPLSRPV11_H_
#define NULLIMPLSRPV11_H_

#include "srpv.h"

/*
 * Declare the implementation class of the SRPV VERIF (1:1) Interface
 */
namespace SRPV {
	
    class NullImplSRPV11 : public SRPV::VerifInterface {
public:

    NullImplSRPV11();
    ~NullImplSRPV11() override;

    ReturnStatus
    initialize(const std::string &configDir) override;

    ReturnStatus
    createTemplate(const SoundRecord &record,
            TemplateRole role,
            std::vector<uint8_t> &templ) override;

    ReturnStatus
    matchTemplates(
            const std::vector<uint8_t> &verifTemplate,
            const std::vector<uint8_t> &enrollTemplate,
            double &similarity) override;

private:
    // Some other members
};
}

#endif /* NULLIMPLSRPV11_H_ */
