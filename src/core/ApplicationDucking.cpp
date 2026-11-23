#include "core/ApplicationDucking.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
struct Session {
    QString id;
    QString applicationKey;
    QString name;
    ComPtr<ISimpleAudioVolume> volume;
};

QString processPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    wchar_t path[32768];
    DWORD length = sizeof(path) / sizeof(path[0]);
    const bool found = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    return found ? QString::fromWCharArray(path, length) : QString{};
}

QString sessionString(IAudioSessionControl2* control, bool instance) {
    LPWSTR value = nullptr;
    const HRESULT result = instance
        ? control->GetSessionInstanceIdentifier(&value) : control->GetSessionIdentifier(&value);
    const QString text = SUCCEEDED(result) && value ? QString::fromWCharArray(value) : QString{};
    CoTaskMemFree(value);
    return text;
}

void collectSessions(IMMDevice* device, QList<Session>& sessions) {
    ComPtr<IAudioSessionManager2> manager;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(manager.GetAddressOf())))) return;
    ComPtr<IAudioSessionEnumerator> list;
    int count = 0;
    if (FAILED(manager->GetSessionEnumerator(list.GetAddressOf()))
        || FAILED(list->GetCount(&count))) return;

    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> control;
        ComPtr<IAudioSessionControl2> details;
        AudioSessionState status;
        DWORD pid = 0;
        if (FAILED(list->GetSession(i, control.GetAddressOf()))
            || FAILED(control->GetState(&status)) || status != AudioSessionStateActive
            || FAILED(control.As(&details)) || FAILED(details->GetProcessId(&pid))
            || pid == 0 || pid == GetCurrentProcessId()) continue;

        Session session;
        session.id = sessionString(details.Get(), true);
        if (session.id.isEmpty() || FAILED(control.As(&session.volume))) continue;
        const QString path = processPath(pid);
        session.applicationKey = path.isEmpty() ? sessionString(details.Get(), false) : path.toCaseFolded();
        if (session.applicationKey.isEmpty()) continue;
        session.name = path.isEmpty() ? QString("Aplicación (PID %1)").arg(pid) : QFileInfo(path).completeBaseName();
        sessions.append(std::move(session));
    }
}

bool sameVolume(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}
}

struct ApplicationDucking::State {
    struct AttenuatedSession {
        ComPtr<ISimpleAudioVolume> volume;
        float original;
        float applied;
    };
    bool initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ComPtr<IMMDeviceEnumerator> devices;
    QMap<QString, AttenuatedSession> attenuated;

    State() {
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(devices.GetAddressOf()));
    }

    ~State() {
        devices.Reset();
        if (initialized) CoUninitialize();
    }
};
#else
struct ApplicationDucking::State {};
#endif

ApplicationDucking::ApplicationDucking(QSettings& settings)
    : state_(std::make_unique<State>()), settings_(settings) {}

ApplicationDucking::~ApplicationDucking() {
    setActive(false);
}

QMap<QString, QString> ApplicationDucking::refresh() {
    QMap<QString, QString> applications;
#ifdef Q_OS_WIN
    ComPtr<IMMDeviceCollection> endpoints;
    UINT count = 0;
    if (!state_->devices
        || FAILED(state_->devices->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, endpoints.GetAddressOf()))
        || FAILED(endpoints->GetCount(&count))) return applications;

    QList<Session> sessions;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> endpoint;
        if (SUCCEEDED(endpoints->Item(i, endpoint.GetAddressOf())))
            collectSessions(endpoint.Get(), sessions);
    }

    for (const auto& session : sessions) {
        applications.insert(session.applicationKey, session.name);
        if (!active_) continue;
        float current;
        if (FAILED(session.volume->GetMasterVolume(&current))) continue;
        const auto existing = state_->attenuated.constFind(session.id);
        const float original = existing != state_->attenuated.cend() && sameVolume(current, existing->applied)
            ? existing->original : current;
        const int overridePercent = applicationPercent(session.applicationKey);
        const int percent = overridePercent < 0 ? globalPercent_ : overridePercent;
        const float target = original * percent / 100.0f;
        if (!sameVolume(current, target) && FAILED(session.volume->SetMasterVolume(target, nullptr))) continue;
        if (!sameVolume(original, target))
            state_->attenuated.insert(session.id, {session.volume, original, target});
        else
            state_->attenuated.remove(session.id);
    }
#endif
    return applications;
}

void ApplicationDucking::setActive(bool active) {
    if (active_ == active) return;
    active_ = active;
    if (active_) {
        refresh();
        return;
    }
#ifdef Q_OS_WIN
    for (const auto& session : state_->attenuated) {
        float current;
        if (SUCCEEDED(session.volume->GetMasterVolume(&current)) && sameVolume(current, session.applied))
            session.volume->SetMasterVolume(session.original, nullptr);
    }
    state_->attenuated.clear();
#endif
}

void ApplicationDucking::setGlobalPercent(int percent) {
    globalPercent_ = std::clamp(percent, 0, 100);
    if (active_) refresh();
}

static QString settingKey(const QString& applicationKey) {
    const auto hash = QCryptographicHash::hash(applicationKey.toUtf8(), QCryptographicHash::Sha256);
    return "applicationDucking/" + QString::fromLatin1(hash.toHex());
}

int ApplicationDucking::applicationPercent(const QString& key) const {
    return std::clamp(settings_.value(settingKey(key), -1).toInt(), -1, 100);
}

void ApplicationDucking::setApplicationPercent(const QString& key, int percent) {
    if (percent < 0)
        settings_.remove(settingKey(key));
    else
        settings_.setValue(settingKey(key), std::min(percent, 100));
    if (active_) refresh();
}
