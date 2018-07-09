/*
 * Image Recognition Performance Verification
 *
 * This file contains SRPV API description along with the
 * supplied data types definitions and API's interface declaration
 * to be implemented by the sound recognition software vendors
 *
 * This software is not subject to copyright protection
 */

#ifndef SRPV_H_
#define SRPV_H_

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef BUILD_SHARED_LIBRARY
    #ifdef Q_OS_LINUX
        #define DLLSPEC __attribute__((visibility("default")))
    #else
        #define DLLSPEC __declspec(dllexport)
    #endif
#else
    #ifdef Q_OS_LINUX
        #define DLLSPEC
    #else
        #define DLLSPEC __declspec(dllimport)
    #endif
#endif

namespace SRPV {
	
/** =================================================================
 * @brief
 * Struct representing a single SoundRecord
 */
typedef struct SoundRecord {
    /** Number of frames in record (one frame contains single sample count for all channels)*/
    uint32_t length;
    /** Number of channels in record*/
    uint8_t channels;
    /** Number of bits per sample. Legal values are 8, 16, 24 and 32*/
    uint8_t depth;
    /** Managed pointer to record data*/
    std::shared_ptr<uint8_t> data;

    /** Audio format notes
     * Sample of the SoundRecord always represents signed integer value, that stored in memory as sucessive bytes set with little endian layout
     * Little endian layout means that least significant byte stored in the smallest address
     *
     * Example: if depth == 16 and channels == 2 then SoundRecord's data points to [lA][mA][lB][mB][lA][mA][lB][mB]...,
     * where l - least significant, m - most significant, A - first channel, B - second channel
     * */

    SoundRecord() :
        length{0},
        channels{0},
        depth{0}
        {}

    SoundRecord(
        uint32_t length,
        uint8_t channels,
        uint8_t depth,
        const std::shared_ptr<uint8_t> &data
        ) :
        length{length},
        channels{channels},
        depth{depth},
        data{data}
        {}

    /** @brief This function returns the size of the SoundRecord data*/
    size_t
    size() const { return (length * channels * (depth / 8)); }
} SoundRecord;


/** =================================================================
 * Labels describing the type/role of the template 
 * to be generated (provided as input to template generation)
 */
enum class TemplateRole {
    /** Enrollment template for 1:1 */
    Enrollment_11,
    /** Verification template for 1:1 */
    Verification_11
};

/** =================================================================
 * @brief
 * Return codes for functions specified in this API
 */
enum class ReturnCode {
    /** Success */
    Success = 0,
    /** Error reading configuration files */
    ConfigError,
    /** Elective refusal to produce a template */
    TemplateCreationError,
    /** There was a problem setting or accessing the GPU */
    GPUError,
    /** Vendor-defined failure */
    VendorError
};

/** Output stream operator for a ReturnCode object. */
inline std::ostream&
operator<<(
    std::ostream &s,
    const ReturnCode &rc)
{
    switch (rc) {
    case ReturnCode::Success:
        return (s << "Success");
    case ReturnCode::ConfigError:
        return (s << "Error reading configuration files");
    case ReturnCode::TemplateCreationError:
        return (s << "Elective refusal to produce a template");   
    case ReturnCode::GPUError:
        return (s << "Problem setting or accessing the GPU");
    case ReturnCode::VendorError:
        return (s << "Vendor-defined error");
    default:
        return (s << "Undefined error");
    }
}

/** =================================================================
 * @brief
 * A structure to contain information about a failure by the software
 * under test
 *
 * @details
 * An object of this class allows the software to return some information
 * from a function call. The string within this object can be optionally
 * set to provide more information for debugging etc. The status code
 * will be set by the function to Success on success, or one of the
 * other codes on failure
 */
typedef struct ReturnStatus {
    /** @brief Return status code */
    ReturnCode code;
    /** @brief Optional information string */
    std::string info;

    ReturnStatus() {}
    /**
     * @brief
     * Create a ReturnStatus object.
     *
     * @param[in] code
     * The return status code; required.
     * @param[in] info
     * The optional information string.
     */
    ReturnStatus(
        const ReturnCode code,
        const std::string &info = ""
        ) :
        code{code},
        info{info}
        {}
} ReturnStatus;

/** =================================================================
 * @brief
 * The interface to IRPV 1:1 implementation (1:1 means one to one verification scheme)
 *
 * @details
 * The submission software under test will implement this interface by
 * sub-classing this class and implementing each method therein.
 */
class DLLSPEC VerifInterface {
public:
    virtual ~VerifInterface() {}

    /**
     * @brief This function initializes the implementation under test.  It will
     * be called by the SRPVTest application before any call to createTemplate() or
     * matchTemplates().  The implementation under test should set all parameters.
     * This function will be called N=1 times by the SRPest application.
     *
     * @param[in] configDir
     * A read-only directory containing any developer-supplied configuration
     * parameters or run-time data files.  The name of this directory is
     * assigned by SRPest, not hardwired by the provider.  The names of the
     * files in this directory are hardwired in the implementation and are
     * unrestricted.
     */
    virtual ReturnStatus
    initialize(const std::string &configDir) = 0;

    /**
     * @brief This function takes an SoundRecord and outputs a proprietary template.
	 * The vector to store the template will be
     * initially empty, and it is up to the implementation
     * to populate them with the appropriate data.  In all cases, even when unable
     * to extract features, the output shall be a template that may be passed to
     * the match_templates function without error. That is, this routine must
     * internally encode "template creation failed" and the matcher must
     * transparently handle this. Also if unable to extract features this
	 * function should return TemplateCreationError.
     *
     * param[in] record
     * Input sound record
     * param[in] role
     * Label describing the type/role of the template to be generated
     * param[out] templ
     * The output template.  The format is entirely unregulated. This will be
     * an empty vector when passed into the function, and the implementation
     * can resize and populate it with the appropriate data.
     */
    virtual ReturnStatus
    createTemplate(
        const SoundRecord &record,
        TemplateRole role,
        std::vector<uint8_t> &templ) = 0;

    /**
     * @brief This function compares two proprietary templates and outputs a
     * similarity score, which need not satisfy the metric properties. When
     * either or both of the input templates are the result of a failed
     * template generation, the similarity score shall be -1.
     *
     * param[in] verifTemplate
     * A verification template from createTemplate(role=Verification_11).
     * The underlying data can be accessed via verifTemplate.data().  The size,
     * in bytes, of the template could be retrieved as verifTemplate.size().
     * param[in] enrollTemplate
     * An enrollment template from createTemplate(role=Enrollment_11).
     * The underlying data can be accessed via enrollTemplate.data().  The size,
     * in bytes, of the template could be retrieved as enrollTemplate.size().
     * param[out] similarity
     * A similarity score resulting from comparison of the templates,
     * on the range [0,DBL_MAX].
     *
     */
    virtual ReturnStatus
    matchTemplates(
        const std::vector<uint8_t> &verifTemplate,
        const std::vector<uint8_t> &enrollTemplate,
        double &similarity) = 0;

    /**
     * @brief
     * Factory method to return a managed pointer to the VerifInterface object.
     * @details
     * This function is implemented by the submitted library and must return
     * a managed pointer to the VerifInterface object.
     *
     * @note
     * A possible implementation might be:
     * return (std::make_shared<YourImplementation>());
     */
    static std::shared_ptr<VerifInterface>
    getImplementation();
};
/* End of VerifInterface */
}

#endif /* IRPV_H_ */

