#ifndef IRPVHELPER_H
#define IRPVHELPER_H

#include <cmath>
#include <cstring>
#include <iostream>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QDir>

#ifndef USE_CUSTOM_WAV_DECODER
#include <QEventLoop>
#include <QAudioFormat>
#include <QAudioDecoder>
#include <QCoreApplication>
#else
#include "qwavdecoder.h"
#endif

#include "srpv.h"

//---------------------------------------------------

struct BiometricTemplate
{
    BiometricTemplate() {} // default constructor is needed for std::vector<BiometricTemplate>

    BiometricTemplate(size_t _label, SRPV::TemplateRole _role, std::vector<uint8_t> &&_data) :
        label(_label),
        role(_role),
        data(std::move(_data)) {}

    BiometricTemplate(BiometricTemplate&& other) :
        label(other.label),
        role(other.role),
        data(std::move(other.data)) {}

    BiometricTemplate& operator=(BiometricTemplate&& other)
    {
        if (this != &other) {
            label = other.label;
            role = other.role;
            data = std::move(other.data);
        }
        return *this;
    }

    size_t               label;
    SRPV::TemplateRole   role;
    std::vector<uint8_t> data;
};

//---------------------------------------------------

inline std::ostream&
operator<<(
    std::ostream &s,
    const QString &_qstring)
{
    return s << _qstring.toLocal8Bit().constData();
}

//---------------------------------------------------

inline std::ostream&
operator<<(
    std::ostream &s,
    QAudioFormat::SampleType _sampletype)
{
    switch(_sampletype) {
        case QAudioFormat::SampleType::SignedInt:
            return s << "SignedInt";
        case QAudioFormat::SampleType::UnSignedInt:
            return s << "UnSignedInt";
        case QAudioFormat::SampleType::Float:
            return s << "Float";
        default:
            return s << "Unknown";
    }
}

//---------------------------------------------------

inline std::ostream&
operator<<(
    std::ostream &s,
    QAudioFormat::Endian _endian)
{
    switch(_endian) {
        case QAudioFormat::Endian::LittleEndian:
            return s << "LittleEndian";
        case QAudioFormat::Endian::BigEndian:
            return s << "BigEndian";
    }
    return s << "Unknown";
}


//---------------------------------------------------

SRPV::SoundRecord readSoundRecord(const QString &_filename, bool _verbose=false)
{
    QAudioFormat _format;
    QByteArray _bytearray;

#ifndef USE_CUSTOM_WAV_DECODER

    // Target audio format
    QAudioFormat _tf;
    _tf.setCodec("audio/pcm");
    _tf.setByteOrder(QAudioFormat::LittleEndian);
    _tf.setSampleType(QAudioFormat::SignedInt);
    _tf.setSampleSize(16);

    QAudioDecoder _audiodecoder;
    if(_verbose)
        QObject::connect(&_audiodecoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),[&_audiodecoder](QAudioDecoder::Error _error) {
                            Q_UNUSED(_error);
                            std::cout << _audiodecoder.errorString() << std::endl;
                        });
    _audiodecoder.setAudioFormat(_tf);
    _audiodecoder.setSourceFilename(_filename);

    QEventLoop _el;
    QObject::connect(&_audiodecoder, SIGNAL(finished()), &_el, SLOT(quit()));
    QObject::connect(&_audiodecoder, SIGNAL(error(QAudioDecoder::Error)), &_el, SLOT(quit()));
    QObject::connect(&_audiodecoder, &QAudioDecoder::bufferReady, [&_audiodecoder, &_bytearray, &_format] () {
                QAudioBuffer _ab = _audiodecoder.read();
                _format = _ab.format();
                _bytearray.append(QByteArray(_ab.constData<char>(),_ab.byteCount()));
            });
    _audiodecoder.start();
    _el.exec();
    if(_verbose)
        std::cout << "\tRecord size (bytes): " << _bytearray.size() << std::endl;
#else
    QWavDecoder::readSoundRecord(_filename,_format,_bytearray,_verbose);
#endif

    if((_format.sampleSize() % 8) != 0) {
        std::cout << "Unsupported sample size (" << _format.sampleSize() << ")!" << std::endl;
        return SRPV::SoundRecord();
    }
    if(_format.byteOrder() != QAudioFormat::LittleEndian) {
        std::cout << "Unsupported byte order (" << _format.byteOrder() << ")!" << std::endl;
        return SRPV::SoundRecord();
    }
    if(_format.sampleType() != QAudioFormat::SignedInt) {
        std::cout << "Unsupported sample type " << _format.sampleType() << ")!" << std::endl;
        return SRPV::SoundRecord();
    }

    std::shared_ptr<uint8_t>_sharedptr(new uint8_t[_bytearray.size()], std::default_delete<uint8_t[]>());
    std::memcpy(_sharedptr.get(), _bytearray.constData(), static_cast<size_t>(_bytearray.size()));
    return SRPV::SoundRecord(static_cast<uint32_t>(_bytearray.size() / _format.bytesPerFrame()),
                             static_cast<uint8_t>(_format.channelCount()),
                             static_cast<uint8_t>(_format.sampleSize()),
                             _sharedptr);
}

//---------------------------------------------------

struct ROCPoint
{
    ROCPoint() {}
    double mTAR, mFAR, similarity;
};

//---------------------------------------------------

std::vector<ROCPoint> computeROC(size_t _points, const std::vector<uint8_t> &_issameperson, size_t _totalpositive, size_t _totalnegative, const std::vector<double> &_similarity)
{
    std::vector<ROCPoint> _vROC(_points,ROCPoint());

    const double _maxsim = *std::max_element(_similarity.begin(), _similarity.end());
    const double _minsim = *std::min_element(_similarity.begin(), _similarity.end());
    const double _simstep = (_maxsim - _minsim)/_points;

    #pragma omp parallel for
    for(int i = 0; i < static_cast<int>(_points); ++i) {
        const double _thresh = _minsim + i*_simstep;
        uint8_t _same;
        size_t  _truepositive = 0, _truenegative = 0;
        for(size_t j = 0; j < _similarity.size(); ++j) {
            _same = (_similarity[j] < _thresh) ? 0 : 1; // 1 - same, 0 - not the same
            if((_same == 1) && (_issameperson[j] == 1))
                _truepositive++;
            else if((_same == 0) && (_issameperson[j] == 0))
                _truenegative++;
        }
        _vROC[i].mTAR = static_cast<double>(_truepositive) / _totalpositive;
        _vROC[i].mFAR = 1.0 - static_cast<double>(_truenegative) / _totalnegative;
        _vROC[i].similarity = _thresh;
    }
    return _vROC;
}

//---------------------------------------------------

double findArea(const std::vector<ROCPoint> &_roc)
{
    double _area = 0;
    for(size_t i = 1; i < _roc.size(); ++i) {
        _area += (_roc[i-1].mFAR - _roc[i].mFAR)*(_roc[i-1].mTAR + _roc[i].mTAR)/2.0;
    }
    return _area;
}

//---------------------------------------------------

double findFRR(const std::vector<ROCPoint> &_roc, double _targetmFAR)
{
    for(size_t i = (_roc.size()-1); i >= 1; --i) { // not to 0 because of unsigned data type, we will hadle last value in final return
        if(_roc[i].mFAR > _targetmFAR)
            return 1.0 - _roc[i].mTAR;
    }
    return 1.0 - _roc[0].mTAR;
}

//---------------------------------------------------

QJsonArray serializeROC(const std::vector<ROCPoint> &_roc)
{
    QJsonArray _jsonarr;
    for(size_t i = 0; i < _roc.size(); ++i) {
        QJsonObject _jsonobj({
                                 qMakePair(QLatin1String("FAR"),QJsonValue(_roc[i].mFAR)),
                                 qMakePair(QLatin1String("TAR"),QJsonValue(_roc[i].mTAR)),
                                 qMakePair(QLatin1String("similarity"),QJsonValue(_roc[i].similarity))
                             });
        _jsonarr.push_back(qMove(_jsonobj));
    }
    return _jsonarr;
}

//--------------------------------------------------
void showTimeConsumption(qint64 secondstotal)
{
    qint64 days    = secondstotal / 86400;
    qint64 hours   = (secondstotal - days * 86400) / 3600;
    qint64 minutes = (secondstotal - days * 86400 - hours * 3600) / 60;
    qint64 seconds = secondstotal - days * 86400 - hours * 3600 - minutes * 60;
    std::cout << std::endl << "Test has been complited successfully" << std::endl
              << " It took: " << days << " days "
              << hours << " hours "
              << minutes << " minutes and "
              << seconds << " seconds" << std::endl;
}

#endif // IRPVHELPER_H
