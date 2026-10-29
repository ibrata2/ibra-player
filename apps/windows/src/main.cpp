#include <QApplication>
#include <QAudioOutput>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSlider>
#include <QStackedLayout>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <climits>

namespace {
QString clockText(qint64 milliseconds) {
    const auto seconds = qMax<qint64>(0, milliseconds / 1000);
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QPushButton *makeButton(const QString &text, const QString &objectName = {}) {
    auto *button = new QPushButton(text);
    if (!objectName.isEmpty()) button->setObjectName(objectName);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(42);
    return button;
}
}

class PlayerWindow final : public QMainWindow {
public:
    PlayerWindow() {
        setWindowTitle(QStringLiteral("iBRA PLAYER"));
        setMinimumSize(980, 640);
        resize(1440, 900);
        setAcceptDrops(true);

        auto *root = new QWidget(this);
        root->setObjectName("root");
        auto *rootLayout = new QHBoxLayout(root);
        rootLayout->setContentsMargins(16, 16, 16, 16);
        rootLayout->setSpacing(14);
        rootLayout->setDirection(QBoxLayout::RightToLeft);

        auto *sidebar = buildSidebar();
        auto *content = buildContent();
        rootLayout->addWidget(sidebar);
        rootLayout->addWidget(content, 1);
        setCentralWidget(root);
        setLayoutDirection(Qt::RightToLeft);
        setStyleSheet(styleSheetText());

        player = new QMediaPlayer(this);
        audio = new QAudioOutput(this);
        player->setAudioOutput(audio);
        player->setVideoOutput(video);
        audio->setVolume(0.72);
        connect(openButton, &QPushButton::clicked, this, [this] { openFiles(); });
        connect(addButton, &QPushButton::clicked, this, [this] { addFiles(); });
        connect(playButton, &QPushButton::clicked, this, [this] { togglePlayback(); });
        connect(stopButton, &QPushButton::clicked, player, &QMediaPlayer::stop);
        connect(muteButton, &QPushButton::clicked, this, [this] {
            audio->setMuted(!audio->isMuted());
            muteButton->setText(audio->isMuted() ? QStringLiteral("◖×") : QStringLiteral("◖"));
        });
        connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
            audio->setVolume(value / 100.0);
        });
        connect(positionSlider, &QSlider::sliderMoved, player, &QMediaPlayer::setPosition);
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
            if (!positionSlider->isSliderDown()) positionSlider->setValue(static_cast<int>(position));
            currentTime->setText(clockText(position));
        });
        connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
            positionSlider->setRange(0, static_cast<int>(qMin<qint64>(duration, INT_MAX)));
            totalTime->setText(clockText(duration));
        });
        connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
            playButton->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("❚❚") : QStringLiteral("▶"));
            playButton->setToolTip(state == QMediaPlayer::PlayingState ? QStringLiteral("إيقاف مؤقت") : QStringLiteral("تشغيل"));
        });
        connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) {
            setStatus(QStringLiteral("تعذّر تشغيل الملف: %1").arg(message));
        });
        connect(queue, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            const int index = queue->row(item);
            if (index >= 0 && index < mediaFiles.size()) playAt(index);
        });
        connect(queue, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row < mediaFiles.size() && row != currentIndex) playAt(row);
        });
        connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia && currentIndex + 1 < mediaFiles.size()) playAt(currentIndex + 1);
        });
        connect(fullscreenButton, &QPushButton::clicked, this, [this] {
            isFullScreen() ? showNormal() : showFullScreen();
        });
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        QStringList files;
        for (const auto &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) files << url.toLocalFile();
        }
        addPaths(files, true);
        event->acceptProposedAction();
    }

private:
    QWidget *buildSidebar() {
        auto *panel = new QFrame;
        panel->setObjectName("sidebar");
        panel->setFixedWidth(248);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(18, 20, 18, 18);
        layout->setSpacing(12);

        auto *brandRow = new QHBoxLayout;
        auto *mark = new QLabel(QStringLiteral("▶"));
        mark->setObjectName("brandMark");
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(42, 42);
        auto *brand = new QLabel(QStringLiteral("iBRA PLAYER"));
        brand->setObjectName("brandName");
        auto *brandStack = new QVBoxLayout;
        brandStack->setSpacing(1);
        brandStack->addWidget(brand);
        auto *tagline = new QLabel(QStringLiteral("مشغّل الوسائط"));
        tagline->setObjectName("muted");
        brandStack->addWidget(tagline);
        brandRow->addWidget(mark);
        brandRow->addLayout(brandStack, 1);
        layout->addLayout(brandRow);

        auto *section = new QLabel(QStringLiteral("المكتبة"));
        section->setObjectName("eyebrow");
        section->setContentsMargins(2, 28, 0, 4);
        layout->addWidget(section);
        auto *playing = makeButton(QStringLiteral("◉    المشغّل"), "navActive");
        playing->setLayoutDirection(Qt::RightToLeft);
        playing->setMinimumHeight(48);
        layout->addWidget(playing);

        auto *queueTitle = new QLabel(QStringLiteral("قائمة التشغيل"));
        queueTitle->setObjectName("eyebrow");
        queueTitle->setContentsMargins(2, 24, 0, 4);
        layout->addWidget(queueTitle);
        auto *add = makeButton(QStringLiteral("＋    إضافة ملفات"));
        add->setObjectName("quietButton");
        add->setLayoutDirection(Qt::RightToLeft);
        addButton = add;
        layout->addWidget(add);

        auto *open = makeButton(QStringLiteral("فتح ملف وسائط"), "primaryButton");
        open->setLayoutDirection(Qt::RightToLeft);
        openButton = open;
        layout->addWidget(open);

        auto *tip = new QFrame;
        tip->setObjectName("tipCard");
        auto *tipLayout = new QVBoxLayout(tip);
        tipLayout->setContentsMargins(14, 13, 14, 13);
        auto *tipTitle = new QLabel(QStringLiteral("اختصار سريع"));
        tipTitle->setObjectName("cardTitle");
        auto *tipText = new QLabel(QStringLiteral("اسحب ملف فيديو أو صوت وأسقطه هنا لبدء التشغيل."));
        tipText->setObjectName("muted");
        tipText->setWordWrap(true);
        tipLayout->addWidget(tipTitle);
        tipLayout->addWidget(tipText);
        layout->addWidget(tip);
        layout->addStretch(1);

        auto *version = new QLabel(QStringLiteral("الإصدار 0.1.0  ·  Windows"));
        version->setObjectName("mutedSmall");
        layout->addWidget(version);
        return panel;
    }

    QWidget *buildContent() {
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        auto *top = new QHBoxLayout;
        auto *welcomeStack = new QVBoxLayout;
        welcomeStack->setSpacing(2);
        auto *welcome = new QLabel(QStringLiteral("مساء النور"));
        welcome->setObjectName("welcomeTitle");
        auto *sub = new QLabel(QStringLiteral("جهّز مشاهدتك، والباقي علينا."));
        sub->setObjectName("muted");
        welcomeStack->addWidget(welcome);
        welcomeStack->addWidget(sub);
        top->addLayout(welcomeStack, 1);
        auto *statusCard = new QFrame;
        statusCard->setObjectName("statusPill");
        auto *statusLayout = new QHBoxLayout(statusCard);
        statusLayout->setContentsMargins(12, 7, 12, 7);
        auto *statusDot = new QLabel(QStringLiteral("●"));
        statusDot->setObjectName("statusDot");
        statusLabel = new QLabel(QStringLiteral("جاهز للتشغيل"));
        statusLabel->setObjectName("muted");
        statusLayout->addWidget(statusDot);
        statusLayout->addWidget(statusLabel);
        top->addWidget(statusCard, 0, Qt::AlignVCenter);
        layout->addLayout(top);

        auto *body = new QHBoxLayout;
        body->setSpacing(14);
        auto *playerCard = buildPlayerCard();
        auto *queueCard = buildQueueCard();
        body->addWidget(playerCard, 1);
        body->addWidget(queueCard);
        layout->addLayout(body, 1);

        auto *bottom = new QLabel(QStringLiteral("صُمّم للاستماع والمشاهدة بلا تعقيد"));
        bottom->setObjectName("mutedSmall");
        bottom->setAlignment(Qt::AlignCenter);
        layout->addWidget(bottom);
        return content;
    }

    QWidget *buildPlayerCard() {
        auto *card = new QFrame;
        card->setObjectName("playerCard");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *videoFrame = new QFrame;
        videoFrame->setObjectName("videoFrame");
        auto *stack = new QStackedLayout(videoFrame);
        stack->setStackingMode(QStackedLayout::StackAll);
        video = new QVideoWidget(videoFrame);
        video->setObjectName("videoSurface");
        auto *empty = new QWidget(videoFrame);
        empty->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *emptyLayout = new QVBoxLayout(empty);
        emptyLayout->setAlignment(Qt::AlignCenter);
        auto *emptyMark = new QLabel(QStringLiteral("▶"));
        emptyMark->setObjectName("emptyMark");
        emptyMark->setAlignment(Qt::AlignCenter);
        emptyMark->setFixedSize(74, 74);
        auto *emptyTitle = new QLabel(QStringLiteral("مساحة المشاهدة"));
        emptyTitle->setObjectName("emptyTitle");
        emptyTitle->setAlignment(Qt::AlignCenter);
        auto *emptyText = new QLabel(QStringLiteral("افتح ملفًا محليًا لبدء التشغيل"));
        emptyText->setObjectName("muted");
        emptyText->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyMark, 0, Qt::AlignHCenter);
        emptyLayout->addSpacing(12);
        emptyLayout->addWidget(emptyTitle);
        emptyLayout->addWidget(emptyText);
        placeholder = empty;
        stack->addWidget(video);
        stack->addWidget(empty);
        stack->setCurrentWidget(empty);
        layout->addWidget(videoFrame, 1);

        auto *trackRow = new QHBoxLayout;
        auto *trackInfo = new QVBoxLayout;
        trackInfo->setSpacing(3);
        currentTitle = new QLabel(QStringLiteral("لا يوجد ملف قيد التشغيل"));
        currentTitle->setObjectName("trackTitle");
        currentTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *trackSub = new QLabel(QStringLiteral("iBRA PLAYER  ·  مشغّل الوسائط"));
        trackSub->setObjectName("mutedSmall");
        trackInfo->addWidget(currentTitle);
        trackInfo->addWidget(trackSub);
        trackRow->addLayout(trackInfo, 1);
        fullscreenButton = makeButton(QStringLiteral("⛶"), "iconButton");
        fullscreenButton->setFixedSize(42, 42);
        fullscreenButton->setToolTip(QStringLiteral("ملء الشاشة"));
        trackRow->addWidget(fullscreenButton);
        layout->addLayout(trackRow);

        auto *timeline = new QHBoxLayout;
        currentTime = new QLabel(QStringLiteral("00:00"));
        totalTime = new QLabel(QStringLiteral("00:00"));
        currentTime->setObjectName("timeLabel");
        totalTime->setObjectName("timeLabel");
        positionSlider = new QSlider(Qt::Horizontal);
        positionSlider->setObjectName("timeline");
        positionSlider->setRange(0, 0);
        timeline->addWidget(currentTime);
        timeline->addWidget(positionSlider, 1);
        timeline->addWidget(totalTime);
        layout->addLayout(timeline);

        auto *controls = new QHBoxLayout;
        controls->setSpacing(8);
        controls->addStretch(1);
        stopButton = makeButton(QStringLiteral("■"), "iconButton");
        stopButton->setFixedSize(42, 42);
        stopButton->setToolTip(QStringLiteral("إيقاف"));
        playButton = makeButton(QStringLiteral("▶"), "playButton");
        playButton->setFixedSize(54, 54);
        playButton->setToolTip(QStringLiteral("تشغيل"));
        controls->addWidget(stopButton);
        controls->addWidget(playButton);
        controls->addStretch(1);
        muteButton = makeButton(QStringLiteral("◖"), "iconButton");
        muteButton->setFixedSize(42, 42);
        muteButton->setToolTip(QStringLiteral("كتم الصوت"));
        volumeSlider = new QSlider(Qt::Horizontal);
        volumeSlider->setObjectName("volumeSlider");
        volumeSlider->setRange(0, 100);
        volumeSlider->setValue(72);
        volumeSlider->setMaximumWidth(130);
        controls->addWidget(muteButton);
        controls->addWidget(volumeSlider);
        layout->addLayout(controls);
        return card;
    }

    QWidget *buildQueueCard() {
        auto *card = new QFrame;
        card->setObjectName("queueCard");
        card->setMinimumWidth(260);
        card->setMaximumWidth(320);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);
        auto *header = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("قائمة التشغيل"));
        title->setObjectName("panelTitle");
        queueCount = new QLabel(QStringLiteral("0"));
        queueCount->setObjectName("countBadge");
        header->addWidget(title, 1);
        header->addWidget(queueCount);
        layout->addLayout(header);
        auto *hint = new QLabel(QStringLiteral("ملفاتك الحالية"));
        hint->setObjectName("mutedSmall");
        layout->addWidget(hint);
        queue = new QListWidget;
        queue->setObjectName("queueList");
        queue->setSpacing(5);
        queue->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(queue, 1);
        auto *queueNote = new QLabel(QStringLiteral("انقر مرتين على عنصر لتشغيله"));
        queueNote->setObjectName("mutedSmall");
        queueNote->setAlignment(Qt::AlignCenter);
        layout->addWidget(queueNote);
        return card;
    }

    void openFiles() {
        const auto paths = QFileDialog::getOpenFileNames(this, QStringLiteral("اختر ملفات الوسائط"), {},
            QStringLiteral("ملفات الوسائط (*.mp4 *.mkv *.avi *.mov *.wmv *.webm *.mp3 *.flac *.wav *.m4a *.ogg);;كل الملفات (*.*)"));
        if (!paths.isEmpty()) addPaths(paths, true);
    }
    void addFiles() {
        const auto paths = QFileDialog::getOpenFileNames(this, QStringLiteral("أضف إلى قائمة التشغيل"), {},
            QStringLiteral("ملفات الوسائط (*.mp4 *.mkv *.avi *.mov *.wmv *.webm *.mp3 *.flac *.wav *.m4a *.ogg);;كل الملفات (*.*)"));
        addPaths(paths, false);
    }
    void addPaths(const QStringList &paths, bool playFirst) {
        int firstNew = -1;
        for (const auto &path : paths) {
            if (!QFileInfo::exists(path) || mediaFiles.contains(path)) continue;
            if (firstNew < 0) firstNew = mediaFiles.size();
            mediaFiles.append(path);
            auto *item = new QListWidgetItem(QStringLiteral("♫   %1").arg(QFileInfo(path).completeBaseName()));
            item->setToolTip(path);
            queue->addItem(item);
        }
        queueCount->setText(QString::number(mediaFiles.size()));
        if (playFirst && firstNew >= 0) playAt(firstNew);
        else if (firstNew >= 0) setStatus(QStringLiteral("أُضيفت ملفات إلى القائمة"));
    }
    void playAt(int index) {
        if (index < 0 || index >= mediaFiles.size()) return;
        currentIndex = index;
        queue->blockSignals(true);
        queue->setCurrentRow(index);
        queue->blockSignals(false);
        const auto path = mediaFiles.at(index);
        player->setSource(QUrl::fromLocalFile(path));
        player->play();
        currentTitle->setText(QFileInfo(path).fileName());
        placeholder->hide();
        setStatus(QStringLiteral("يعمل الآن"));
    }
    void togglePlayback() {
        if (player->source().isEmpty()) { openFiles(); return; }
        if (player->playbackState() == QMediaPlayer::PlayingState) player->pause();
        else player->play();
    }
    void setStatus(const QString &text) { statusLabel->setText(text); }

    static QString styleSheetText() {
        return QStringLiteral(R"(
            QWidget#root { background:#0d0f13; color:#f4f4f6; font-family:'Segoe UI'; font-size:13px; }
            QFrame#sidebar, QFrame#playerCard, QFrame#queueCard { background:#15181e; border:1px solid #252a33; border-radius:18px; }
            QLabel#brandMark { color:white; font-size:22px; font-weight:800; background:#e9343f; border-radius:13px; }
            QLabel#brandName { color:#f7f7f8; font-size:17px; font-weight:800; letter-spacing:0.5px; }
            QLabel#eyebrow { color:#7e8592; font-size:11px; font-weight:700; }
            QLabel#muted { color:#a0a5b0; font-size:12px; }
            QLabel#mutedSmall { color:#747b88; font-size:11px; }
            QPushButton { color:#e8e9ec; background:#20242c; border:1px solid #303641; border-radius:10px; padding:8px 12px; font-weight:600; }
            QPushButton:hover { background:#292e38; border-color:#545c69; }
            QPushButton#quietButton { color:#dfe1e6; background:transparent; border-color:transparent; text-align:right; }
            QPushButton#quietButton:hover { background:#20242c; }
            QPushButton#primaryButton { color:white; background:#e9343f; border:0; font-weight:700; }
            QPushButton#primaryButton:hover { background:#f04751; }
            QPushButton#navActive { color:#fff; background:#29222a; border:1px solid #4b2e35; text-align:right; }
            QLabel#welcomeTitle { color:#f5f5f7; font-size:24px; font-weight:700; }
            QFrame#statusPill { background:#171d1c; border:1px solid #26352f; border-radius:12px; }
            QLabel#statusDot { color:#70d6a4; font-size:10px; }
            QFrame#videoFrame { background:#090a0d; border:1px solid #303641; border-radius:14px; }
            QVideoWidget#videoSurface { background:#07080a; border-radius:12px; }
            QLabel#emptyMark { color:#fff; background:#2b1c22; border:1px solid #57313b; border-radius:24px; font-size:27px; }
            QLabel#emptyTitle { color:#eceef1; font-size:18px; font-weight:700; }
            QLabel#trackTitle { color:#f2f2f4; font-size:15px; font-weight:700; }
            QLabel#timeLabel { color:#9299a6; font-size:11px; font-family:Consolas; }
            QLabel#panelTitle { color:#f2f2f4; font-size:15px; font-weight:700; }
            QLabel#countBadge { color:#f6a6ac; background:#352128; border-radius:10px; padding:4px 9px; font-weight:700; }
            QFrame#tipCard { background:#1b1d23; border:1px solid #2b303a; border-radius:13px; }
            QLabel#cardTitle { color:#e5e6e9; font-weight:700; }
            QListWidget#queueList { background:transparent; border:0; outline:0; color:#d8dae0; }
            QListWidget#queueList::item { padding:12px 10px; border-radius:9px; background:#1b1e25; }
            QListWidget#queueList::item:selected { color:white; background:#39242b; border:1px solid #59333d; }
            QSlider#timeline::groove:horizontal { height:4px; background:#353a44; border-radius:2px; }
            QSlider#timeline::sub-page:horizontal { background:#e9343f; border-radius:2px; }
            QSlider#timeline::handle:horizontal { width:12px; margin:-5px 0; border-radius:6px; background:#f7f7f8; }
            QSlider#volumeSlider::groove:horizontal { height:4px; background:#353a44; border-radius:2px; }
            QSlider#volumeSlider::sub-page:horizontal { background:#e9343f; border-radius:2px; }
            QSlider#volumeSlider::handle:horizontal { width:10px; margin:-4px 0; border-radius:5px; background:#f7f7f8; }
            QPushButton#iconButton { font-size:16px; }
            QPushButton#playButton { color:white; background:#e9343f; border:0; border-radius:27px; font-size:19px; font-weight:800; }
            QPushButton#playButton:hover { background:#f04751; }
        )");
    }

    QMediaPlayer *player{};
    QAudioOutput *audio{};
    QVideoWidget *video{};
    QWidget *placeholder{};
    QPushButton *openButton{};
    QPushButton *addButton{};
    QPushButton *playButton{};
    QPushButton *stopButton{};
    QPushButton *muteButton{};
    QPushButton *fullscreenButton{};
    QLabel *statusLabel{};
    QLabel *currentTitle{};
    QLabel *currentTime{};
    QLabel *totalTime{};
    QLabel *queueCount{};
    QSlider *positionSlider{};
    QSlider *volumeSlider{};
    QListWidget *queue{};
    QStringList mediaFiles;
    int currentIndex{-1};
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("iBRA PLAYER"));
    app.setLayoutDirection(Qt::RightToLeft);
    PlayerWindow window;
    window.show();
    return app.exec();
}

