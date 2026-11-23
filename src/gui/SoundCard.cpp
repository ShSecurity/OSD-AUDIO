#include "SoundCard.h"
#include "Icon.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QFileInfo>

SoundCard::SoundCard(const SoundData& data, QWidget* parent)
    : QFrame(parent), data_(data)
{
    setObjectName("soundCard");
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(82);
    setToolTip("Clic para reproducir");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 10, 8, 10);
    layout->setSpacing(11);

    // ── Círculo de acento ─────────────────────────────────────────────────────
    circleWidget_ = new QWidget(this);
    circleWidget_->setFixedSize(42, 42);
    circleWidget_->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* playIcon = Icon::label(circleWidget_, Icon::play(), QColor("#3064E8"), 11);
    playIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    playIcon->setGeometry(0, 0, 42, 42);

    // ── Área de texto ─────────────────────────────────────────────────────────
    auto* textArea = new QWidget(this);
    textArea->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* textLayout = new QVBoxLayout(textArea);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    nameLabel_ = new QLabel(textArea);
    nameLabel_->setStyleSheet(
        "font-weight: 600; font-size: 13px; color: #1A2340; background: transparent; border: none;");
    nameLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameLabel_->setMaximumWidth(160);

    catLabel_ = new QLabel(textArea);
    catLabel_->setStyleSheet(
        "font-size: 11px; color: #7C8AA3; background: transparent; border: none;");
    catLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* bottomRow = new QWidget(textArea);
    bottomRow->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(6);

    statusDot_ = new QLabel(bottomRow);
    statusDot_->setFixedSize(8, 8);
    statusDot_->setAttribute(Qt::WA_TransparentForMouseEvents);

    badgeLabel_ = new QLabel(bottomRow);
    badgeLabel_->setStyleSheet(
        "background-color: #F4F7FC; border-radius: 4px; color: #586A87;"
        " font-size: 10px; padding: 1px 6px; border: none;");
    badgeLabel_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    badgeLabel_->setFixedHeight(18);
    badgeLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    badgeLabel_->hide();

    bottomLayout->addWidget(statusDot_);
    bottomLayout->addWidget(badgeLabel_);
    bottomLayout->addStretch();

    textLayout->addWidget(nameLabel_);
    textLayout->addWidget(catLabel_);
    textLayout->addWidget(bottomRow);

    // ── Botón ⋮ ───────────────────────────────────────────────────────────────
    moreBtn_ = new QPushButton(this);
    moreBtn_->setFixedSize(28, 28);
    moreBtn_->setCursor(Qt::PointingHandCursor);
    QFont mf("Segoe MDL2 Assets", 11);
    moreBtn_->setFont(mf);
    moreBtn_->setText(Icon::more());
    moreBtn_->setStyleSheet(R"(
        QPushButton { background: transparent; border: none;
                      color: #B0BCCC; border-radius: 6px; }
        QPushButton:hover { background: #EBF1FF; color: #3064E8; }
    )");
    connect(moreBtn_, &QPushButton::clicked, this, [this](){
        emit editRequested(data_.id);
    });

    layout->addWidget(circleWidget_);
    layout->addWidget(textArea, 1);
    layout->addWidget(moreBtn_);

    refreshUI();
    updateStyle(false);
}

void SoundCard::refreshUI() {
    circleWidget_->setStyleSheet(
        QString("background-color: %1; border-radius: 21px;").arg(data_.accentColor));
    catLabel_->setText(data_.category);

    // Nombre: si hay nombre personalizado, tiene prioridad; si no, se usa el filename
    QString displayName = data_.name.trimmed();
    if (displayName.isEmpty() && !data_.filePath.isEmpty())
        displayName = QFileInfo(data_.filePath).baseName();
    if (displayName.isEmpty())
        displayName = "Sin nombre";
    if (displayName.length() > 16) displayName = displayName.left(15) + "…";
    nameLabel_->setText(displayName);

    if (data_.filePath.isEmpty()) {
        statusDot_->setStyleSheet(
            "background-color: #D1D5DB; border-radius: 4px; border: none;");
        statusDot_->setToolTip("Sin archivo asignado");
    } else {
        statusDot_->setStyleSheet(
            "background-color: #22C55E; border-radius: 4px; border: none;");
        statusDot_->setToolTip(QFileInfo(data_.filePath).fileName());
    }

    // Badge de atajo (si tiene shortcut configurado)
    if (!data_.shortcut.isEmpty()) {
        badgeLabel_->setText(data_.shortcut);
        badgeLabel_->show();
    } else {
        badgeLabel_->hide();
    }
}

void SoundCard::updateData(const SoundData& data) {
    data_ = data;
    refreshUI();
}

void SoundCard::updateStyle(bool hovered) {
    if (hovered)
        setStyleSheet(
            "QFrame#soundCard { background: #F3F7FF; border: 1px solid #CFDDF8; border-radius: 12px; }");
    else
        setStyleSheet(
            "QFrame#soundCard { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
}

void SoundCard::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && !data_.filePath.isEmpty())
        emit triggered(data_.filePath, data_.volume, data_.id);
    QFrame::mousePressEvent(e);
}

void SoundCard::enterEvent(QEnterEvent* e) { updateStyle(true);  QFrame::enterEvent(e); }
void SoundCard::leaveEvent(QEvent*       e) { updateStyle(false); QFrame::leaveEvent(e);  }
