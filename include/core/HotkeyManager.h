#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <functional>
#include <unordered_map>
#include <string>

// Gestiona hotkeys globales (funcionan aunque la app este en segundo plano)
// Usa WinAPI RegisterHotKey + QAbstractNativeEventFilter en Windows
class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    // Registra un hotkey con modificadores WinAPI (MOD_ALT=0x0001, MOD_CONTROL=0x0002, MOD_SHIFT=0x0004)
    bool registerHotkey(const QString& id,
                        unsigned int modifiers,
                        unsigned int vkCode,
                        std::function<void()> callback);

    // Registra desde QKeySequence (convierte automaticamente)
    bool registerHotkey(const QString& id,
                        const QKeySequence& sequence,
                        std::function<void()> callback);

    void unregisterHotkey(const QString& id);
    void clearAll();
    void trigger(const QString& id);

    bool nativeEventFilter(const QByteArray& eventType,
                           void* message,
                           qintptr* result) override;

signals:
    void hotkeyTriggered(const QString& id);

private:
    struct HotkeyEntry {
        int                   winId;
        unsigned int          modifiers;
        unsigned int          vkCode;
        std::function<void()> callback;
    };

    std::unordered_map<QString, HotkeyEntry> hotkeys_;
    std::unordered_map<int, QString>         winIdMap_;
    int nextWinId_ = 2000;

    static unsigned int qtModToWin(Qt::KeyboardModifiers mods);
    static unsigned int qtKeyToVk(Qt::Key key);
    static unsigned int qtNumpadKeyToVk(Qt::Key key);
};
