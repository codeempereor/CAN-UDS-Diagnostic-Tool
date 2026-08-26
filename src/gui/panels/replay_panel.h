#ifndef REPLAY_PANEL_H
#define REPLAY_PANEL_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <memory>
#include <vector>

#include "can/can_frame.h"

class ReplayPanel : public QWidget {
    Q_OBJECT

public:
    explicit ReplayPanel(QWidget* parent = nullptr);
    ~ReplayPanel() override;

    void setFrames(const std::vector<CanFrame>& frames);

signals:
    void frameReplayed(const CanFrame& frame);

private slots:
    void onPlayPause();
    void onStop();
    void onSpeedChanged(int value);
    void onSliderMoved(int value);
    void onTimerTick();

private:
    void setupUi();
    void updatePositionLabel();

    QPushButton* m_playBtn;
    QPushButton* m_stopBtn;
    QSlider* m_slider;
    QLabel* m_positionLabel;
    QSpinBox* m_speedSpin;
    QLabel* m_speedLabel;

    QTimer* m_timer;

    std::vector<CanFrame> m_frames;
    size_t m_currentIndex;
    bool m_playing;
    int m_speedMultiplier; // 倍速
    uint64_t m_baseTimeUs;
};

#endif // REPLAY_PANEL_H
