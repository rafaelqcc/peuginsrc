#pragma once
#include "runtime.hpp"
#include <QColor>
#include <QObject>
#include <QString>
#include <functional>
#include <string>

class CheatBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString attachedText READ attachedText NOTIFY attachedTextChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool espOn READ espOn NOTIFY espOnChanged)
    Q_PROPERTY(bool aimbotOn READ aimbotOn NOTIFY aimbotOnChanged)
    Q_PROPERTY(bool walkspeedOn READ walkspeedOn NOTIFY walkspeedOnChanged)
    Q_PROPERTY(bool jumpPowerOn READ jumpPowerOn NOTIFY jumpPowerOnChanged)
    Q_PROPERTY(float aimSpeed READ aimSpeed NOTIFY aimSpeedChanged)
    Q_PROPERTY(bool aimFovOn READ aimFovOn NOTIFY aimFovOnChanged)
    Q_PROPERTY(float aimFov READ aimFov NOTIFY aimFovChanged)
    Q_PROPERTY(QColor aimFovColor READ aimFovColor WRITE setAimFovColor NOTIFY aimFovColorChanged)
    Q_PROPERTY(float aimFovAlpha READ aimFovAlpha WRITE setAimFovAlpha NOTIFY aimFovAlphaChanged)
    Q_PROPERTY(float wsSpeed READ wsSpeed NOTIFY wsSpeedChanged)
    Q_PROPERTY(float jpPower READ jpPower NOTIFY jpPowerChanged)
    Q_PROPERTY(QColor espColor READ espColor WRITE setEspColor NOTIFY espColorChanged)
    Q_PROPERTY(bool espFillOn READ espFillOn WRITE setEspFillOn NOTIFY espFillOnChanged)
    Q_PROPERTY(float espFillAlpha READ espFillAlpha WRITE setEspFillAlpha NOTIFY espFillAlphaChanged)
    Q_PROPERTY(int overlayFps READ overlayFps WRITE setOverlayFps NOTIFY overlayFpsChanged)
    Q_PROPERTY(float espMaxDist READ espMaxDist WRITE setEspMaxDist NOTIFY espMaxDistChanged)
    Q_PROPERTY(bool crosshairOn READ crosshairOn WRITE setCrosshairOn NOTIFY crosshairOnChanged)
    Q_PROPERTY(float crosshairSize READ crosshairSize WRITE setCrosshairSize NOTIFY crosshairSizeChanged)
    Q_PROPERTY(float crosshairThickness READ crosshairThickness WRITE setCrosshairThickness NOTIFY crosshairThicknessChanged)
    Q_PROPERTY(float crosshairGap READ crosshairGap WRITE setCrosshairGap NOTIFY crosshairGapChanged)
    Q_PROPERTY(QColor crosshairColor READ crosshairColor WRITE setCrosshairColor NOTIFY crosshairColorChanged)

public:
    explicit CheatBridge(CheatRuntime& runtime, QObject* parent = nullptr);

    QString attachedText() const;
    QString statusMessage() const;
    bool espOn() const;
    bool aimbotOn() const;
    bool walkspeedOn() const;
    bool jumpPowerOn() const;
    float aimSpeed() const;
    bool aimFovOn() const;
    float aimFov() const;
    QColor aimFovColor() const;
    float aimFovAlpha() const;
    float wsSpeed() const;
    float jpPower() const;
    QColor espColor() const;
    bool espFillOn() const;
    float espFillAlpha() const;
    int overlayFps() const;
    float espMaxDist() const;
    bool crosshairOn() const;
    float crosshairSize() const;
    float crosshairThickness() const;
    float crosshairGap() const;
    QColor crosshairColor() const;

public slots:
    void setEspOn(bool on);
    void setAimbotOn(bool on);
    void setWalkspeedOn(bool on);
    void setJumpPowerOn(bool on);
    void setAimSpeed(float speed);
    void setAimFovOn(bool on);
    void setAimFov(float fov);
    void setAimFovColor(QColor color);
    void setAimFovAlpha(float a);
    void setWsSpeed(float speed);
    void setJpPower(float power);
    void setEspColor(QColor color);
    void setEspFillOn(bool on);
    void setEspFillAlpha(float a);
    void setOverlayFps(int fps);
    void setEspMaxDist(float dist);
    void setCrosshairOn(bool on);
    void setCrosshairSize(float size);
    void setCrosshairThickness(float thickness);
    void setCrosshairGap(float gap);
    void setCrosshairColor(QColor color);

signals:
    void attachedTextChanged();
    void statusMessageChanged();
    void espOnChanged();
    void aimbotOnChanged();
    void walkspeedOnChanged();
    void jumpPowerOnChanged();
    void aimSpeedChanged();
    void aimFovOnChanged();
    void aimFovChanged();
    void aimFovColorChanged();
    void aimFovAlphaChanged();
    void wsSpeedChanged();
    void jpPowerChanged();
    void espColorChanged();
    void espFillOnChanged();
    void espFillAlphaChanged();
    void overlayFpsChanged();
    void espMaxDistChanged();
    void crosshairOnChanged();
    void crosshairSizeChanged();
    void crosshairThicknessChanged();
    void crosshairGapChanged();
    void crosshairColorChanged();

private:
    CheatRuntime& rt_;
    int pid_;
    std::uint64_t base_;
    QString status_message_;

    void emitAll();
    void runAsync(std::function<std::string()> work);
};
