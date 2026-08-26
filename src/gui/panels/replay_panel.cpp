#include "replay_panel.h"
#include <QStyle>

ReplayPanel::ReplayPanel(QWidget* parent)
    : QWidget(parent)
    , m_playBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_slider(nullptr)
    , m_positionLabel(nullptr)
    , m_speedSpin(nullptr)
    , m_speedLabel(nullptr)
    , m_timer(nullptr)
    , m_currentIndex(0)
    , m_playing(false)
    , m_speedMultiplier(1)
    , m_baseTimeUs(0)
{
    setupUi();
}

ReplayPanel::~ReplayPanel() = default;

void ReplayPanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 控制按钮行
    QHBoxLayout* controlLayout = new QHBoxLayout();

    m_playBtn = new QPushButton(this);
    m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playBtn->setToolTip("播放/暂停");
    m_playBtn->setMaximumWidth(40);
    controlLayout->addWidget(m_playBtn);

    m_stopBtn = new QPushButton(this);
    m_stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopBtn->setToolTip("停止");
    m_stopBtn->setMaximumWidth(40);
    controlLayout->addWidget(m_stopBtn);

    m_positionLabel = new QLabel("0 / 0", this);
    controlLayout->addWidget(m_positionLabel);

    controlLayout->addStretch();

    m_speedLabel = new QLabel("倍速:", this);
    controlLayout->addWidget(m_speedLabel);

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setRange(1, 100);
    m_speedSpin->setValue(1);
    m_speedSpin->setSuffix("x");
    m_speedSpin->setMaximumWidth(60);
    controlLayout->addWidget(m_speedSpin);

    mainLayout->addLayout(controlLayout);

    // 进度条
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 0);
    mainLayout->addWidget(m_slider);

    // 定时器
    m_timer = new QTimer(this);
    m_timer->setInterval(10);

    // 信号连接
    connect(m_playBtn, &QPushButton::clicked, this, &ReplayPanel::onPlayPause);
    connect(m_stopBtn, &QPushButton::clicked, this, &ReplayPanel::onStop);
    connect(m_speedSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ReplayPanel::onSpeedChanged);
    connect(m_slider, &QSlider::sliderMoved, this, &ReplayPanel::onSliderMoved);
    connect(m_timer, &QTimer::timeout, this, &ReplayPanel::onTimerTick);
}

void ReplayPanel::setFrames(const std::vector<CanFrame>& frames)
{
    m_frames = frames;
    m_currentIndex = 0;
    m_playing = false;
    m_timer->stop();

    m_slider->setRange(0, static_cast<int>(frames.size()) - 1);
    m_slider->setValue(0);
    updatePositionLabel();
}

void ReplayPanel::onPlayPause()
{
    if (m_frames.empty()) return;

    if (m_playing) {
        m_playing = false;
        m_timer->stop();
        m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        if (m_currentIndex >= m_frames.size()) {
            m_currentIndex = 0;
        }
        m_playing = true;
        m_baseTimeUs = CanFrame::currentTimestampUs() - m_frames[m_currentIndex].timestamp_us;
        m_timer->start();
        m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void ReplayPanel::onStop()
{
    m_playing = false;
    m_timer->stop();
    m_currentIndex = 0;
    m_slider->setValue(0);
    m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    updatePositionLabel();
}

void ReplayPanel::onSpeedChanged(int value)
{
    m_speedMultiplier = value;
}

void ReplayPanel::onSliderMoved(int value)
{
    if (value >= 0 && value < static_cast<int>(m_frames.size())) {
        m_currentIndex = static_cast<size_t>(value);
        updatePositionLabel();
    }
}

void ReplayPanel::onTimerTick()
{
    if (!m_playing || m_frames.empty()) return;

    uint64_t now = CanFrame::currentTimestampUs();
    uint64_t elapsed = (now - m_baseTimeUs) * m_speedMultiplier;

    while (m_currentIndex < m_frames.size() &&
           m_frames[m_currentIndex].timestamp_us <= elapsed) {
        emit frameReplayed(m_frames[m_currentIndex]);
        m_currentIndex++;
    }

    m_slider->blockSignals(true);
    m_slider->setValue(static_cast<int>(m_currentIndex - 1));
    m_slider->blockSignals(false);
    updatePositionLabel();

    if (m_currentIndex >= m_frames.size()) {
        m_playing = false;
        m_timer->stop();
        m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
}

void ReplayPanel::updatePositionLabel()
{
    m_positionLabel->setText(QString("%1 / %2").arg(m_currentIndex).arg(m_frames.size()));
}
