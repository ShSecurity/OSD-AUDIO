#include "../../include/core/HotkeyManager.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

HotkeyManager::~HotkeyManager() {
    clearAll();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool HotkeyManager::registerHotkey(const QString& id,
                                    unsigned int modifiers,
                                    unsigned int vkCode,
                                    std::function<void()> callback)
{
    // Desregistrar anterior si existe
    unregisterHotkey(id);

#ifdef Q_OS_WIN
    int wid = nextWinId_++;
    if (!RegisterHotKey(nullptr, wid, modifiers, vkCode)) {
        return false;
    }
    hotkeys_[id] = { wid, modifiers, vkCode, std::move(callback) };
    winIdMap_[wid] = id;
    return true;
#else
    return false;
#endif
}

bool HotkeyManager::registerHotkey(const QString& id,
                                    const QKeySequence& sequence,
                                    std::function<void()> callback)
{
    if (sequence.isEmpty()) return false;
    int combined  = sequence[0].toCombined();
    Qt::Key key   = Qt::Key(combined & ~Qt::KeyboardModifierMask);
    Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);

    // Qt::KeypadModifier no existe en WinAPI: el VK code del numpad ya lo diferencia
    bool isNumpad = (mods & Qt::KeypadModifier) != 0;
    mods &= ~Qt::KeypadModifier;

    unsigned int winMod = qtModToWin(mods);
    unsigned int vk     = isNumpad ? qtNumpadKeyToVk(key) : qtKeyToVk(key);
    if (vk == 0) return false;
    return registerHotkey(id, winMod, vk, std::move(callback));
}

void HotkeyManager::unregisterHotkey(const QString& id) {
    auto it = hotkeys_.find(id);
    if (it == hotkeys_.end()) return;
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, it->second.winId);
#endif
    winIdMap_.erase(it->second.winId);
    hotkeys_.erase(it);
}

void HotkeyManager::clearAll() {
    for (auto& [id, entry] : hotkeys_) {
#ifdef Q_OS_WIN
        UnregisterHotKey(nullptr, entry.winId);
#endif
    }
    hotkeys_.clear();
    winIdMap_.clear();
}

void HotkeyManager::trigger(const QString& id) {
    auto it = hotkeys_.find(id);
    if (it != hotkeys_.end() && it->second.callback)
        it->second.callback();
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType,
                                       void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int wid = static_cast<int>(msg->wParam);
            auto it = winIdMap_.find(wid);
            if (it != winIdMap_.end()) {
                const QString& id = it->second;
                emit hotkeyTriggered(id);
                auto hit = hotkeys_.find(id);
                if (hit != hotkeys_.end() && hit->second.callback)
                    hit->second.callback();
                return true;
            }
        }
    }
#endif
    return false;
}

// Convierte modificadores Qt a WinAPI
unsigned int HotkeyManager::qtModToWin(Qt::KeyboardModifiers mods) {
    unsigned int w = 0;
#ifdef Q_OS_WIN
    if (mods & Qt::AltModifier)     w |= MOD_ALT;
    if (mods & Qt::ControlModifier) w |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)   w |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)    w |= MOD_WIN;
#endif
    return w;
}

// Convierte tecla de numpad Qt a VK_NUMPADx de Windows
unsigned int HotkeyManager::qtNumpadKeyToVk(Qt::Key key) {
#ifdef Q_OS_WIN
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<unsigned int>(VK_NUMPAD0 + (key - Qt::Key_0));
    switch (key) {
        case Qt::Key_Asterisk: return VK_MULTIPLY;
        case Qt::Key_Plus:     return VK_ADD;
        case Qt::Key_Minus:    return VK_SUBTRACT;
        case Qt::Key_Period:   return VK_DECIMAL;
        case Qt::Key_Slash:    return VK_DIVIDE;
        default: break;
    }
#endif
    return qtKeyToVk(key);
}

// Convierte tecla Qt a VK code de Windows
unsigned int HotkeyManager::qtKeyToVk(Qt::Key key) {
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<unsigned int>('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<unsigned int>('A' + (key - Qt::Key_A));
#ifdef Q_OS_WIN
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return static_cast<unsigned int>(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
        case Qt::Key_Space:     return VK_SPACE;
        case Qt::Key_Return:    return VK_RETURN;
        case Qt::Key_Escape:    return VK_ESCAPE;
        case Qt::Key_Tab:       return VK_TAB;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Delete:    return VK_DELETE;
        case Qt::Key_Insert:    return VK_INSERT;
        case Qt::Key_Home:      return VK_HOME;
        case Qt::Key_End:       return VK_END;
        case Qt::Key_PageUp:    return VK_PRIOR;
        case Qt::Key_PageDown:  return VK_NEXT;
        case Qt::Key_Left:      return VK_LEFT;
        case Qt::Key_Right:     return VK_RIGHT;
        case Qt::Key_Up:        return VK_UP;
        case Qt::Key_Down:      return VK_DOWN;
        default: break;
    }
#endif
    return 0;
}
