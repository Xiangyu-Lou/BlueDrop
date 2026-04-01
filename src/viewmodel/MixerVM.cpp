#include "MixerVM.h"
#include "audio/AudioEngine.h"
#include "audio/SessionVolumeController.h"
#include "audio/DeviceEnumerator.h"
#include "system/Logger.h"
#include <QSettings>

namespace BlueDrop {

MixerVM::MixerVM(AudioEngine* engine, SessionVolumeController* sessionVol,
                 DeviceEnumerator* deviceEnum,
                 QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_sessionVol(sessionVol)
    , m_deviceEnum(deviceEnum)
{
    connect(m_engine, &AudioEngine::errorOccurred, this, [this](const QString& msg) {
        LOG_ERRORF("AudioEngine error: %s", qPrintable(msg));
        m_lastError = msg;
        emit lastErrorChanged();
    });

    if (m_sessionVol) {
        connect(m_sessionVol, &SessionVolumeController::sessionFound, this, [this](const QString& name) {
            LOG_INFOF("BT session found: %s", qPrintable(name));
            // Apply our saved volume/mute to the newly found session
            m_sessionVol->setVolume(m_btVolume);
            m_sessionVol->setMuted(m_btMuted);
            emit btMonitorVolumeChanged();
            emit btMonitorMutedChanged();
            emit btSessionFoundChanged();
        });
    }
}

MixerVM::~MixerVM()
{
    // Restore original audio endpoint if boost was active when app closes
    if (m_boostEnabled) {
        m_engine->stopBoost();
        if (!m_savedDefaultEndpoint.isEmpty()) {
            SessionVolumeController::setDefaultPlaybackEndpoint(m_savedDefaultEndpoint);
            LOG_INFOF("MixerVM dtor: restored endpoint %s", qPrintable(m_savedDefaultEndpoint));
        }
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
    QSettings().setValue("monitor/volume", m_btVolume);
    emit btMonitorVolumeChanged();
}

void MixerVM::setBtMonitorMuted(bool v) {
    m_btMuted = v;
    if (m_sessionVol) {
        m_sessionVol->setMuted(v);
    }
    QSettings().setValue("monitor/muted", m_btMuted);
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

void MixerVM::scanBtSession(const QString& sessionEndpointId,
                             const QString& monitorEndpointId,
                             const QString& deviceName) {
    LOG_INFOF("MixerVM::scanBtSession session=%s monitor=%s device=%s",
              qPrintable(sessionEndpointId), qPrintable(monitorEndpointId), qPrintable(deviceName));
    m_monitorDeviceId = monitorEndpointId;
    if (m_sessionVol) {
        m_sessionVol->findBluetoothSession(sessionEndpointId, deviceName);
    }
}

bool MixerVM::boostAvailable() const {
    return m_deviceEnum && m_deviceEnum->isVBCableInstalled();
}

void MixerVM::setBoostGain(float v) {
    v = std::clamp(v, 0.0f, 10.0f);
    if (m_boostGain == v) return;
    m_boostGain = v;
    if (!m_boostMuted) m_engine->setBoostGain(v);
    QSettings().setValue("boost/gain", m_boostGain);
    emit boostGainChanged();
}

void MixerVM::setBoostMuted(bool v) {
    if (m_boostMuted == v) return;
    m_boostMuted = v;
    m_engine->setBoostGain(v ? 0.0f : m_boostGain);
    QSettings().setValue("boost/muted", m_boostMuted);
    emit boostMutedChanged();
}

void MixerVM::setBoostEnabled(bool v) {
    if (m_boostEnabled == v) return;

    if (v) {
        // --- Enable boost ---
        if (!m_deviceEnum || !m_deviceEnum->isVBCableInstalled()) {
            LOG_ERROR("Boost: VB-Cable not installed");
            return;
        }

        // Find VB-Cable output device ID (render endpoint)
        QString vbCableOutputId;
        for (const auto& dev : m_deviceEnum->outputDevices()) {
            if (dev.isVirtual) {
                vbCableOutputId = dev.id;
                break;
            }
        }
        if (vbCableOutputId.isEmpty()) {
            LOG_ERROR("Boost: VB-Cable output device not found");
            return;
        }

        // Save current default endpoint
        m_savedDefaultEndpoint = SessionVolumeController::getDefaultPlaybackEndpoint();
        LOG_INFOF("Boost: saved default endpoint: %s", qPrintable(m_savedDefaultEndpoint));

        // Switch default to VB-Cable (BT audio follows default endpoint)
        if (!SessionVolumeController::setDefaultPlaybackEndpoint(vbCableOutputId)) {
            LOG_ERROR("Boost: failed to switch default endpoint to VB-Cable");
            return;
        }

        // Determine monitor device (headphone to render amplified audio to)
        QString monitorId = m_monitorDeviceId;
        if (monitorId.isEmpty()) {
            monitorId = m_savedDefaultEndpoint;  // Use the original default
        }

        // Start boost engine: capture from VB-Cable → amplify → render to headphone
        m_engine->setBoostGain(m_boostGain);
        m_engine->startBoost(vbCableOutputId, monitorId);

        m_boostEnabled = true;
        LOG_INFO("Boost: enabled");
    } else {
        // --- Disable boost ---
        m_engine->stopBoost();

        // Restore original default endpoint
        if (!m_savedDefaultEndpoint.isEmpty()) {
            SessionVolumeController::setDefaultPlaybackEndpoint(m_savedDefaultEndpoint);
            LOG_INFOF("Boost: restored default endpoint: %s", qPrintable(m_savedDefaultEndpoint));
            m_savedDefaultEndpoint.clear();
        }

        m_boostEnabled = false;
        LOG_INFO("Boost: disabled");
    }

    QSettings().setValue("boost/enabled", m_boostEnabled);
    emit boostEnabledChanged();
}

void MixerVM::autoRestoreState()
{
    QSettings s;

    // Restore monitor volume/mute (applied when BT session is found)
    m_btVolume = std::clamp(s.value("monitor/volume", 1.0f).toFloat(), 0.0f, 1.0f);
    m_btMuted  = s.value("monitor/muted", false).toBool();
    emit btMonitorVolumeChanged();
    emit btMonitorMutedChanged();

    // Restore boost gain/mute
    m_boostGain  = std::clamp(s.value("boost/gain", 1.0f).toFloat(), 0.0f, 10.0f);
    m_boostMuted = s.value("boost/muted", false).toBool();
    emit boostGainChanged();
    emit boostMutedChanged();

    // Re-enable boost if it was active on last exit
    if (s.value("boost/enabled", false).toBool()) {
        LOG_INFO("autoRestoreState: re-enabling boost from saved state");
        setBoostEnabled(true);
    }
}

} // namespace BlueDrop
