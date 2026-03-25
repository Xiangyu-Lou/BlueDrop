#include "MixerVM.h"
#include "audio/AudioEngine.h"

namespace BlueDrop {

MixerVM::MixerVM(AudioEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

float MixerVM::phoneVolume() const { return m_engine->phoneVolume(); }
float MixerVM::micVolume() const { return m_engine->micVolume(); }
float MixerVM::phoneMixRatio() const { return m_engine->phoneMixRatio(); }
bool MixerVM::phoneMuted() const { return m_engine->phoneMuted(); }
bool MixerVM::micMuted() const { return m_engine->micMuted(); }
bool MixerVM::engineRunning() const { return m_engine->isRunning(); }

void MixerVM::setPhoneVolume(float v) {
    m_engine->setPhoneVolume(v);
    emit phoneVolumeChanged();
}

void MixerVM::setMicVolume(float v) {
    m_engine->setMicVolume(v);
    emit micVolumeChanged();
}

void MixerVM::setPhoneMixRatio(float v) {
    m_engine->setPhoneMixRatio(v);
    emit phoneMixRatioChanged();
}

void MixerVM::setPhoneMuted(bool v) {
    m_engine->setPhoneMuted(v);
    emit phoneMutedChanged();
}

void MixerVM::setMicMuted(bool v) {
    m_engine->setMicMuted(v);
    emit micMutedChanged();
}

void MixerVM::startEngine() {
    m_engine->start();
    emit engineRunningChanged();
}

void MixerVM::stopEngine() {
    m_engine->stop();
    emit engineRunningChanged();
}

} // namespace BlueDrop
