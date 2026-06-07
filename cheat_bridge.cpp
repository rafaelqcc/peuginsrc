#include "cheat_bridge.hpp"
#include "process.hpp"
#include <QMetaObject>
#include <thread>

CheatBridge::CheatBridge(CheatRuntime& runtime, QObject* parent)
    : QObject(parent), rt_(runtime), pid_(runtime.pid()), base_(runtime.base()) {
    emitAll();
}

QString CheatBridge::attachedText() const {
    const std::string comm = proc_comm(pid_);
    return QString("pid=%1 (%2)\nbase=0x%3")
        .arg(pid_)
        .arg(QString::fromStdString(comm))
        .arg(static_cast<qulonglong>(base_), 0, 16);
}

QString CheatBridge::statusMessage() const {
    return status_message_.isEmpty() ? QString::fromStdString(rt_.status_line())
                                     : status_message_;
}

bool CheatBridge::espOn() const { return rt_.esp_on(); }
bool CheatBridge::aimbotOn() const { return rt_.aimbot_on(); }
bool CheatBridge::walkspeedOn() const { return rt_.ws_on(); }
bool CheatBridge::jumpPowerOn() const { return rt_.jp_on(); }
float CheatBridge::aimSpeed() const { return rt_.aim_speed(); }
bool CheatBridge::aimFovOn() const { return rt_.aim_fov_on(); }
float CheatBridge::aimFov() const { return rt_.aim_fov(); }
QColor CheatBridge::aimFovColor() const { return QColor::fromRgbF(rt_.aim_fov_r(), rt_.aim_fov_g(), rt_.aim_fov_b()); }
float CheatBridge::aimFovAlpha() const { return rt_.aim_fov_a(); }
float CheatBridge::wsSpeed() const { return rt_.ws_speed(); }
float CheatBridge::jpPower() const { return rt_.jp_power(); }
QColor CheatBridge::espColor() const { return QColor::fromRgbF(rt_.esp_r(), rt_.esp_g(), rt_.esp_b()); }
bool CheatBridge::espFillOn() const { return rt_.esp_fill_on(); }
float CheatBridge::espFillAlpha() const { return rt_.esp_a(); }
int CheatBridge::overlayFps() const { return rt_.overlay_fps(); }
float CheatBridge::espMaxDist() const { return rt_.esp_max_dist(); }
bool CheatBridge::crosshairOn() const { return rt_.crosshair_on(); }
float CheatBridge::crosshairSize() const { return rt_.crosshair_size(); }
float CheatBridge::crosshairThickness() const { return rt_.crosshair_thickness(); }
float CheatBridge::crosshairGap() const { return rt_.crosshair_gap(); }
QColor CheatBridge::crosshairColor() const { return QColor::fromRgbF(rt_.ch_r(), rt_.ch_g(), rt_.ch_b()); }

void CheatBridge::emitAll() {
    emit attachedTextChanged();
    emit statusMessageChanged();
    emit espOnChanged();
    emit aimbotOnChanged();
    emit walkspeedOnChanged();
    emit jumpPowerOnChanged();
    emit aimSpeedChanged();
    emit aimFovOnChanged();
    emit aimFovChanged();
    emit aimFovColorChanged();
    emit aimFovAlphaChanged();
    emit wsSpeedChanged();
    emit jpPowerChanged();
    emit espColorChanged();
    emit espFillOnChanged();
    emit espFillAlphaChanged();
    emit overlayFpsChanged();
    emit espMaxDistChanged();
    emit crosshairOnChanged();
    emit crosshairSizeChanged();
    emit crosshairThicknessChanged();
    emit crosshairGapChanged();
    emit crosshairColorChanged();
}

void CheatBridge::runAsync(std::function<std::string()> work) {
    std::thread([this, w = std::move(work)]() {
        std::string msg;
        try {
            msg = w();
        } catch (const std::exception& e) {
            msg = e.what();
        }
        QMetaObject::invokeMethod(
            this,
            [this, msg]() {
                status_message_ = QString::fromStdString(msg);
                emit statusMessageChanged();
                emitAll();
            },
            Qt::QueuedConnection);
    }).detach();
}

void CheatBridge::setEspOn(bool on) { runAsync([this, on] { return rt_.set_esp(on); }); }

void CheatBridge::setAimbotOn(bool on) { runAsync([this, on] { return rt_.set_aimbot(on); }); }

void CheatBridge::setWalkspeedOn(bool on) { runAsync([this, on] { return rt_.set_walkspeed(on); }); }

void CheatBridge::setJumpPowerOn(bool on) { runAsync([this, on] { return rt_.set_jumppower(on); }); }

void CheatBridge::setAimSpeed(float speed) {
    runAsync([this, speed] { return rt_.set_aimbot_speed(speed); });
}

void CheatBridge::setAimFovOn(bool on) {
    rt_.set_aim_fov_on(on);
    emit aimFovOnChanged();
}

void CheatBridge::setAimFov(float fov) {
    rt_.set_aim_fov(fov);
    emit aimFovChanged();
}

void CheatBridge::setAimFovColor(QColor color) {
    rt_.set_aim_fov_color(color.redF(), color.greenF(), color.blueF());
    emit aimFovColorChanged();
}

void CheatBridge::setAimFovAlpha(float a) {
    rt_.set_aim_fov_alpha(a);
    emit aimFovAlphaChanged();
}

void CheatBridge::setEspColor(QColor color) {
    rt_.set_esp_color(color.redF(), color.greenF(), color.blueF());
    emit espColorChanged();
}

void CheatBridge::setEspFillOn(bool on) {
    rt_.set_esp_fill(on);
    emit espFillOnChanged();
}

void CheatBridge::setEspFillAlpha(float a) {
    rt_.set_esp_fill_alpha(a);
    emit espFillAlphaChanged();
}

void CheatBridge::setOverlayFps(int fps) {
    rt_.set_overlay_fps(fps);
    emit overlayFpsChanged();
}

void CheatBridge::setEspMaxDist(float dist) {
    rt_.set_esp_max_dist(dist);
    emit espMaxDistChanged();
}

void CheatBridge::setJpPower(float power) {
    runAsync([this, power] {
        if (power <= 0) return std::string("Jump power must be > 0");
        rt_.set_jp_power(power);
        if (rt_.jp_on()) return rt_.set_jumppower(true, power);
        return "Jump power set to " + std::to_string(static_cast<int>(power)) +
               " (enable jump power to apply)";
    });
}

void CheatBridge::setWsSpeed(float speed) {
    runAsync([this, speed] {
        if (speed <= 0) return std::string("Speed must be > 0");
        rt_.set_ws_speed(speed);
        if (rt_.ws_on()) return rt_.set_walkspeed(true, speed);
        return "Walkspeed set to " + std::to_string(static_cast<int>(speed)) +
               " (enable walkspeed to apply)";
    });
}

void CheatBridge::setCrosshairOn(bool on) {
    rt_.set_crosshair_on(on);
    emit crosshairOnChanged();
}

void CheatBridge::setCrosshairSize(float size) {
    rt_.set_crosshair_size(size);
    emit crosshairSizeChanged();
}

void CheatBridge::setCrosshairThickness(float thickness) {
    rt_.set_crosshair_thickness(thickness);
    emit crosshairThicknessChanged();
}

void CheatBridge::setCrosshairGap(float gap) {
    rt_.set_crosshair_gap(gap);
    emit crosshairGapChanged();
}

void CheatBridge::setCrosshairColor(QColor color) {
    rt_.set_crosshair_color(color.redF(), color.greenF(), color.blueF());
    emit crosshairColorChanged();
}

