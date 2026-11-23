#pragma once
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>
#include <QMediaPlayer>
#include <QAudioOutput>

// TTS usando edge-tts (voces neurales Microsoft Edge).
// La cola espera a que el audio TERMINE de reproducirse antes de generar el siguiente,
// evitando que un comentario corte al anterior.
// Reproduce internamente con su propio QMediaPlayer (no pasa por AudioMixer).
class TtsManager : public QObject {
    Q_OBJECT
public:
    explicit TtsManager(QObject* parent = nullptr);
    ~TtsManager() override;

    void speak(const QString& text);
    void stopAll();
    void setEnabled(bool en)             { enabled_ = en; }
    void setVoice(const QString& voice)  { voice_ = voice; }
    void setReadEmojis(bool en)          { readEmojis_ = en; }
    void setMaxQueue(int n)              { maxQueue_ = qMax(1, n); }
    void setRate(int percent)            { rate_ = qBound(-50, percent, 100); }
    bool isEnabled() const               { return enabled_; }
    bool readEmojis() const              { return readEmojis_; }
    int  maxQueue()  const               { return maxQueue_; }
    int  rate()      const               { return rate_; }

    static QString stripEmojis(const QString& text);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    void processNext();

    QProcess*      process_    = nullptr;
    QMediaPlayer*  player_     = nullptr;
    QAudioOutput*  audioOut_   = nullptr;
    QQueue<QString> queue_;
    QString        currentFile_;
    QString        voice_      = "es-MX-DaliaNeural";
    bool           enabled_    = true;
    bool           readEmojis_ = false;
    bool           busy_       = false;
    int            maxQueue_   = 3;
    int            rate_       = 25;  // +25% por defecto (más ágil)
};
