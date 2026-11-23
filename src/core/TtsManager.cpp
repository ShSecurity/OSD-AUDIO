#include "TtsManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QUrl>

TtsManager::TtsManager(QObject* parent) : QObject(parent) {
    // Proceso para edge-tts
    process_ = new QProcess(this);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TtsManager::onProcessFinished);
    process_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        process_->readAllStandardError(); // descartar mensajes de progreso
    });

    // Player propio: TTS no pasa por AudioMixer ni activa ducking
    player_   = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);
    audioOut_->setVolume(1.0f);
    connect(player_, &QMediaPlayer::mediaStatusChanged,
            this, &TtsManager::onMediaStatusChanged);
}

TtsManager::~TtsManager() {
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(1000);
    }
    player_->stop();
    if (!currentFile_.isEmpty()) QFile::remove(currentFile_);
}

// Elimina caracteres emoji del texto para que la voz no los lea como "cara sonriente" etc.
QString TtsManager::stripEmojis(const QString& text) {
    QString result;
    result.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        QChar c = text[i];
        if (c.isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate()) {
            // Par sustituto: casi siempre emoji fuera del BMP, descartar
            i += 2;
        } else {
            ushort u = c.unicode();
            // Símbolos misceláneos, flechas decorativas, bloques emoji en BMP
            bool isEmojiRange = (u >= 0x2190 && u <= 0x27BF)   // flechas, misc
                             || (u >= 0x2300 && u <= 0x2BFF)   // misc técnicos, geométricos
                             || (u >= 0xFE00 && u <= 0xFE0F)   // selectores de variación
                             || (u >= 0xFE30 && u <= 0xFE4F);  // formas CJK
            if (!isEmojiRange) result += c;
            ++i;
        }
    }
    return result.simplified();
}

void TtsManager::stopAll() {
    queue_.clear();
    busy_ = false;
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(500);
    }
    player_->stop();
    player_->setSource({});
    if (!currentFile_.isEmpty()) { QFile::remove(currentFile_); currentFile_.clear(); }
}

void TtsManager::speak(const QString& text) {
    if (!enabled_ || text.trimmed().isEmpty()) return;
    QString processed = readEmojis_ ? text : stripEmojis(text);
    // Limitar longitud
    if (processed.length() > 120) processed = processed.left(117) + "...";
    if (processed.trimmed().isEmpty()) return;
    // Si hay demasiados en cola, descartar todo y quedarse con el más reciente
    if (queue_.size() >= maxQueue_) queue_.clear();
    queue_.enqueue(processed);
    if (!busy_) processNext();
}

void TtsManager::processNext() {
    if (queue_.isEmpty()) { busy_ = false; return; }
    busy_ = true;

    QString text = queue_.dequeue();
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    currentFile_ = tmpDir + "/osd_tts_" +
                   QString::number(QDateTime::currentMSecsSinceEpoch()) + ".mp3";

    QString rateStr = (rate_ >= 0 ? "+" : "") + QString::number(rate_) + "%";
    process_->start("edge-tts", {
        "--text",        text,
        "--voice",       voice_,
        "--rate",        rateStr,
        "--write-media", currentFile_
    });
}

void TtsManager::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0 && QFile::exists(currentFile_)) {
        // Reproducir; processNext() se llama cuando termine la reproducción
        player_->setSource(QUrl::fromLocalFile(currentFile_));
        player_->play();
    } else {
        // Fallo en edge-tts: limpiar y pasar al siguiente
        if (!currentFile_.isEmpty()) { QFile::remove(currentFile_); currentFile_.clear(); }
        processNext();
    }
}

void TtsManager::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
        player_->setSource({}); // liberar el archivo
        if (!currentFile_.isEmpty()) { QFile::remove(currentFile_); currentFile_.clear(); }
        processNext(); // ahora sí procesar el siguiente en cola
    }
}
