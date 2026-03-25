#include "MixerVM.h"
#include "audio/AudioEngine.h"
#include "audio/SessionVolumeController.h"
#include "system/Logger.h"

namespace BlueDrop {

MixerVM::MixerVM(AudioEngine* engine, SessionVolumeController* sessionVol,
                 QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_sessionVol(sessionVol)
{
    connect(m_engine, &AudioEngine::errorOccurred, this, [this](const QString& msg) {
        LOG_ERRORF("AudioEngine error: %s", qPrintable(msg));
        m_lastError = msg;
        emit lastErrorChanged();
    });

    if (m_sessionVol) {
        connect(m_sessionVol, &SessionVolumeController::sessionFound, this, [this](const QString& name) {
            LOG_INFOF("BT session found: %s", qPrintable(name));
            // Read current volume from the session
            m_btVolume = m_sessionVol->volume();
            m_btMuted = m_sessionVol->isMuted();
            emit btMonitorVolumeChanged();
            emit btMonitorMutedChanged();
            emit btSessionFoundChanged();
        });
    }
}

float MixerVM::phoneVolume() const { return m_engine->phoneVolume(); }
float MixerVM::micVolume() const { return m_engine->micVolume(); }
float MixerVM::phoneMixRatio() const { return m_engine->phoneMixRatio(); }
bool MixerVM::phoneMuted() const { return m_engine->phoneMuted(); }
bool MixerVM::micMuted() const { return m_engine->micMuted(); }
bool MixerVM::engineRunning() const { return m_engine->isRunning(); }

float MixerVM::btMonitorVolume() const { return m_btVolume; }
bool MixerVM::btMonitorMuted() const { return m_btMuted; }
bool MixerVM::btSessionFound() const {
    return m_sessionVol && m_sessionVol->hasSession();
}

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

void MixerVM::setBtMonitorVolume(float v) {
    m_btVolume = std::clamp(v, 0.0f, 1.0f);
    if (m_sessionVol) {
        m_sessionVol->setVolume(m_btVolume);
    }
    emit btMonitorVolumeChanged();
}

void MixerVM::setBtMonitorMuted(bool v) {
    m_btMuted = v;
    if (m_sessionVol) {
        m_sessionVol->setMuted(v);
    }
    emit btMonitorMutedChanged();
}

void MixerVM::startEngine() {
    LOG_INFO("MixerVM::startEngine() called");
    m_engine->start();
    emit engineRunningChanged();
}

void MixerVM::stopEngine() {
    LOG_INFO("MixerVM::stopEngine() called");
    m_engine->stop();
    emit engineRunningChanged();
}

void MixerVM::scanBtSession(const QString& endpointId, const QString& deviceName) {
    LOG_INFOF("MixerVM::scanBtSession(%s, %s)", qPrintable(endpointId), qPrintable(deviceName));
    if (m_sessionVol) {
        m_sessionVol->findBluetoothSession(endpointId, deviceName);
    }
}

} // namespace BlueDrop
