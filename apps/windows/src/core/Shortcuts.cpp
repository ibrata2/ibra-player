#include "Shortcuts.h"
#include "AppSettings.h"

const QVector<ShortcutDef> &Shortcuts::all() {
    static const QVector<ShortcutDef> defs = {
        {"open_file", "act.openFile", "Ctrl+O"},
        {"open_folder", "act.openFolder", "Ctrl+Shift+O"},
        {"open_url", "act.openUrl", "Ctrl+U"},
        {"open_disc", "act.openDisc", "Ctrl+D"},
        {"open_subtitle", "act.loadSubtitle", "Ctrl+L"},
        {"save_playlist", "act.savePlaylist", "Ctrl+S"},
        {"close_file", "act.closeFile", "Ctrl+W"},
        {"exit", "act.exit", "Alt+X"},
        {"play_pause", "act.playPause", "Space"},
        {"stop", "act.stop", "."},
        {"previous", "act.previous", "PgUp"},
        {"next", "act.next", "PgDown"},
        {"seek_back", "act.seekBack", "Left"},
        {"seek_fwd", "act.seekFwd", "Right"},
        {"seek_back_large", "act.seekBackLarge", "Shift+Left"},
        {"seek_fwd_large", "act.seekFwdLarge", "Shift+Right"},
        {"frame_step", "act.frameStep", "Ctrl+Right"},
        {"frame_back", "act.frameBack", "Ctrl+Left"},
        {"jump_to", "act.jumpTo", "Ctrl+G"},
        {"vol_up", "act.volUp", "Up"},
        {"vol_down", "act.volDown", "Down"},
        {"mute", "act.mute", "M"},
        {"speed_up", "act.speedUp", "]"},
        {"speed_down", "act.speedDown", "["},
        {"speed_reset", "act.speedReset", "Backspace"},
        {"ab_loop", "act.abLoop", "L"},
        {"repeat", "act.repeatMode", "R"},
        {"shuffle", "act.shuffle", "Ctrl+H"},
        {"cycle_audio", "act.cycleAudio", "A"},
        {"cycle_sub", "act.cycleSub", "S"},
        {"sub_delay_minus", "act.subDelayMinus", "Z"},
        {"sub_delay_plus", "act.subDelayPlus", "X"},
        {"audio_delay_minus", "act.audioDelayMinus", "Shift+Z"},
        {"audio_delay_plus", "act.audioDelayPlus", "Shift+X"},
        {"fullscreen", "act.fullscreen", "F11"},
        {"fullscreen_alt", "act.fullscreen", "Alt+Return"},
        {"always_on_top", "act.alwaysOnTop", "Ctrl+T"},
        {"zoom_in", "act.zoomIn", "Ctrl+="},
        {"zoom_out", "act.zoomOut", "Ctrl+-"},
        {"zoom_reset", "act.zoomReset", "Ctrl+0"},
        {"rotate", "act.rotate", "Ctrl+R"},
        {"screenshot", "act.screenshot", "Ctrl+Shift+S"},
        {"media_info", "act.mediaInfo", "Ctrl+I"},
        {"playlist", "act.playlist", "Ctrl+P"},
        {"favorite", "act.addFavorite", "Ctrl+B"},
        {"settings", "act.settings", "O"},
        {"theme_studio", "act.themeStudio", "Ctrl+Shift+T"},
    };
    return defs;
}

const ShortcutDef *Shortcuts::find(const QString &id) {
    for (const auto &d : all()) if (id == d.id) return &d;
    return nullptr;
}

QKeySequence Shortcuts::get(const QString &id) {
    const ShortcutDef *d = find(id);
    const QString def = d ? QString::fromLatin1(d->defaultKeys) : QString();
    return QKeySequence(AppSettings::instance()->raw()->value("shortcuts/" + id, def).toString(),
                        QKeySequence::PortableText);
}

void Shortcuts::set(const QString &id, const QKeySequence &seq) {
    AppSettings::instance()->raw()->setValue("shortcuts/" + id, seq.toString(QKeySequence::PortableText));
}

void Shortcuts::resetAll() {
    AppSettings::instance()->raw()->remove("shortcuts");
}
