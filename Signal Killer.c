#include "signalkiller.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <subghz/subghz.h>
#include <notification/notification_messages.h>

static void signalkiller_draw_callback(Canvas* canvas, void* ctx) {
    SignalKiller* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "SignalKiller v2.4");
    if(app->jamming) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 20, 128, 44);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "JAMMING!");
        char jam[32];
        snprintf(jam, sizeof(jam), "%.3f MHz", (double)app->jam_frequency / 1000000.0f);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, jam);
    } else if(app->scanning) {
        canvas_draw_str(canvas, 2, 25, "Scanning 280-928...");
        char freq[32];
        snprintf(freq, sizeof(freq), "Peak: %.3f MHz", (double)app->frequency / 1000000.0f);
        canvas_draw_str(canvas, 2, 38, freq);
        canvas_draw_str(canvas, 2, 50, "Signal:");
        canvas_draw_rectangle(canvas, 50, 45, 70, 10);
        canvas_draw_box(canvas, 51, 46, (uint8_t)(app->strength * 68), 8);
    } else {
        canvas_draw_str(canvas, 2, 30, "Press OK to scan");
        if(app->frequency > 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Jam: %.3f MHz (DOWN)", (double)app->jam_frequency / 1000000.0f);
            canvas_draw_str(canvas, 2, 50, msg);
        }
    }
}

static void scan_worker(void* context) {
    SignalKiller* app = context;
    const uint32_t freqs[] = {300000000, 303850000, 315000000, 390000000, 418000000, 433920000, 434790000, 868350000, 915000000};
    uint32_t best = 0;
    float best_rssi = -100.0f;
    for(size_t i = 0; i < sizeof(freqs)/sizeof(freqs[0]); i++) {
        subghz_manual_set_frequency(app->subghz, freqs[i]);
        furi_delay_ms(60);
        float rssi = subghz_get_rssi(app->subghz);
        if(rssi > best_rssi + 5.0f) {
            best_rssi = rssi;
            best = freqs[i];
        }
    }
    if(best) {
        app->frequency = best;
        app->strength = (best_rssi + 90.0f) / 60.0f;
        if(app->strength > 1.0f) app->strength = 1.0f;
        uint32_t offset = 20000 + (furi_hal_random_get() % 60000);
        app->jam_frequency = best + offset;
        if(app->jam_frequency > 928000000) app->jam_frequency = best - offset;
    }
    app->scanning = false;
}

static void input_callback(InputEvent* input, void* ctx) {
    SignalKiller* app = ctx;
    if(input->type != InputTypePress) return;
    if(input->key == InputKeyOk) {
        if(app->jamming) {
            subghz_tx_stop(app->subghz);
            app->jamming = false;
        } else if(!app->scanning) {
            app->scanning = true;
            furi_thread_start(scan_worker, app);
        }
    }
    else if(input->key == InputKeyDown && app->frequency && !app->jamming) {
        subghz_manual_set_frequency(app->subghz, app->jam_frequency);
        subghz_tx_carrier(app->subghz, true);
        app->jamming = true;
        notification_message(app->notifications, &sequence_blink_red_100);
    }
    else if(input->key == InputKeyBack && app->jamming) {
        subghz_tx_stop(app->subghz);
        app->jamming = false;
    }
}

int32_t signalkiller_app(void* p) {
    UNUSED(p);
    SignalKiller* app = malloc(sizeof(SignalKiller));
    app->subghz = subghz_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->frequency = app->jam_frequency = 0;
    app->scanning = app->jamming = false;
    app->strength = 0;
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, signalkiller_draw_callback, app);
    view_port_input_callback_set(vp, input_callback, app);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    while(!furi_hal_power_is_shutdown()) {
        furi_delay_ms(100);
    }
    if(app->jamming) subghz_tx_stop(app->subghz);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    subghz_free(app->subghz);
    free(app);
    return 0;
}