#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QString>

// Iconos usando Segoe MDL2 Assets (fuente de sistema Windows 10/11)
// Esta fuente provee iconos vectoriales nativos del sistema operativo
namespace Icon {

// ── Caracteres de Segoe MDL2 Assets ─────────────────────────────────────────
inline QString search()    { return QChar(0xE721); }  // Search
inline QString settings()  { return QChar(0xE713); }  // Settings
inline QString add()       { return QChar(0xE109); }  // Add/Plus
inline QString play()      { return QChar(0xE768); }  // Play
inline QString stop()      { return QChar(0xE71A); }  // Stop
inline QString grid()      { return QChar(0xE8A5); }  // ViewAll (Cards)
inline QString list()      { return QChar(0xE8FD); }  // BulletedList
inline QString volume()    { return QChar(0xE767); }  // Speaker
inline QString mute()      { return QChar(0xE74F); }  // Mute
inline QString connect()   { return QChar(0xE704); }  // Connected
inline QString disconnect_(){ return QChar(0xE703); } // Disconnected
inline QString library()   { return QChar(0xE8F1); }  // Library
inline QString folder()    { return QChar(0xED25); }  // OpenLocal
inline QString edit()      { return QChar(0xE70F); }  // Edit
inline QString remove()    { return QChar(0xE74D); }  // Delete
inline QString keyboard()  { return QChar(0xE765); }  // Keyboard
inline QString mini()      { return QChar(0xE8A0); }  // ResizeTouchSmaller
inline QString live()      { return QChar(0xEC57); }  // StatusCircleRing
inline QString signal_()   { return QChar(0xE87F); }  // SignalBars5
inline QString gift()      { return QChar(0xEB51); }  // Gift
inline QString comment()   { return QChar(0xE8BD); }  // Chat
inline QString heart()     { return QChar(0xEB51); }  // Heart
inline QString more()      { return QChar(0xE712); }  // More
inline QString cancel()    { return QChar(0xE711); }  // Cancel
inline QString accept()    { return QChar(0xE73E); }  // Accept
inline QString duck()      { return QChar(0xE96D); }  // VolumeDecreasing
inline QString record()    { return QChar(0xE7C8); }  // Record

// ── Genera QIcon desde un caracter de Segoe MDL2 Assets ─────────────────────
inline QIcon make(const QString& ch,
                  const QColor& color = QColor("#1A2340"),
                  int size = 20)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f("Segoe MDL2 Assets", static_cast<int>(size * 0.60));
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, ch);
    return QIcon(pm);
}

// Conveniencia: QLabel configurado como icono (para layouts)
inline QLabel* label(QWidget* parent, const QString& ch,
                     const QColor& color = QColor("#7B88A0"),
                     int size = 15)
{
    auto* lbl = new QLabel(ch, parent);
    QFont f("Segoe MDL2 Assets", size);
    lbl->setFont(f);
    lbl->setStyleSheet(QString("color: %1; background: transparent; border: none;")
                       .arg(color.name()));
    lbl->setFixedSize(size + 6, size + 6);
    lbl->setAlignment(Qt::AlignCenter);
    return lbl;
}

} // namespace Icon
