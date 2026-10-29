#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

class PlayerWindow final : public QMainWindow {
public:
    PlayerWindow() {
        setWindowTitle("iBRA PLAYER");
        resize(1100, 700);
        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(12);
        auto *video = new QVideoWidget(central);
        video->setStyleSheet("background:#08090c;border:1px solid #353944;border-radius:12px;");
        layout->addWidget(video, 1);
        auto *controls = new QHBoxLayout();
        auto *open = new QPushButton("Open");
        auto *play = new QPushButton("Play / Pause");
        auto *stop = new QPushButton("Stop");
        auto *time = new QLabel("Ready");
        auto *volume = new QSlider(Qt::Horizontal);
        volume->setRange(0, 100);
        volume->setValue(70);
        volume->setMaximumWidth(140);
        controls->addWidget(open);
        controls->addWidget(play);
        controls->addWidget(stop);
        controls->addWidget(time, 1);
        controls->addWidget(new QLabel("Volume"));
        controls->addWidget(volume);
        layout->addLayout(controls);
        setCentralWidget(central);

        player = new QMediaPlayer(this);
        audio = new QAudioOutput(this);
        player->setAudioOutput(audio);
        player->setVideoOutput(video);
        audio->setVolume(0.7);
        connect(open, &QPushButton::clicked, this, [this, time] {
            const auto path = QFileDialog::getOpenFileName(this, "Open media");
            if (path.isEmpty()) return;
            player->setSource(QUrl::fromLocalFile(path));
            player->play();
            time->setText(path);
        });
        connect(play, &QPushButton::clicked, player, &QMediaPlayer::play);
        connect(stop, &QPushButton::clicked, player, &QMediaPlayer::stop);
        connect(volume, &QSlider::valueChanged, this, [this](int value) {
            audio->setVolume(value / 100.0);
        });
        connect(player, &QMediaPlayer::errorOccurred, this,
                [time](QMediaPlayer::Error, const QString &message) {
                    time->setText("Playback error: " + message);
                });
        setStyleSheet("QMainWindow{background:#111318;color:#f5f5f7;} QPushButton{background:#252832;color:white;padding:9px 14px;border:1px solid #414653;border-radius:8px;} QPushButton:hover{border-color:#ed1c24;} QLabel{color:#e8e8ec;}");
    }
private:
    QMediaPlayer *player{};
    QAudioOutput *audio{};
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    PlayerWindow window;
    window.show();
    return app.exec();
}
