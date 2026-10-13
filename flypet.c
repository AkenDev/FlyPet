#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include "brain.h"
#include "network.h"
#include "network_loader.h"

#define BRAIN_TICK_MS 200
#define AUTO_STIMULUS_TICKS 2
#define OUTPUT_LEFT_NEURON 3
#define OUTPUT_RIGHT_NEURON 4
#define POSITION_MAX 20
#define CONTACT_LEFT_NEURON 2
#define CONTACT_RIGHT_NEURON 1
#define CONTACT_STIMULUS 60

typedef struct{
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool show_spike_counts;
    bool network_from_sd;
    bool auto_stimulus_enabled;
    uint8_t auto_stimulus_remaining;
    int16_t position;

    Brain brain;

} FlyPetApp;

static void flypet_apply_auto_stimulus(FlyPetApp* app) {
    if(!app->auto_stimulus_enabled) {
        return;
    }

    app->auto_stimulus_remaining--;
    if(app->auto_stimulus_remaining == 0) {
        brain_stimulate(&app->brain, 0, 60);
        app->auto_stimulus_remaining = AUTO_STIMULUS_TICKS;
    }
}

static void flypet_apply_contact_sensors(FlyPetApp* app) {
    if(app->position == 0) {
        brain_stimulate(&app->brain, CONTACT_LEFT_NEURON, CONTACT_STIMULUS);
    } else if(app->position == POSITION_MAX) {
        brain_stimulate(&app->brain, CONTACT_RIGHT_NEURON, CONTACT_STIMULUS);
    }
}

static void flypet_update_position(FlyPetApp* app) {
    int16_t movement =
        (int16_t)(OUTPUT_RIGHT_NEURON < app->brain.neuron_count &&
                  app->brain.neurons[OUTPUT_RIGHT_NEURON].fired) -
        (int16_t)(OUTPUT_LEFT_NEURON < app->brain.neuron_count &&
                  app->brain.neurons[OUTPUT_LEFT_NEURON].fired);

    app->position += movement;
    if(app->position < 0) {
        app->position = 0;
    } else if(app->position > POSITION_MAX) {
        app->position = POSITION_MAX;
    }
}

static void flypet_draw_callback(Canvas* canvas, void* ctx) {
    FlyPetApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    const bool show_spike_counts = app->show_spike_counts;
    canvas_draw_str(canvas, 2, 9, show_spike_counts ? "Disparos" : "FlyPet LIF");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 78, 9, app->auto_stimulus_enabled ? "AUTO ON" : "AUTO OFF");

    char buffer[24];

    for(uint16_t i = 0; i < app->brain.neuron_count; i++){
        uint8_t x;
        uint8_t y;
        if(show_spike_counts) {
            x = 2 + (i % 2) * 64;
            y = 18 + (i / 2) * 10;
            if(app->brain.neurons[i].spike_count > 9999) {
                snprintf(buffer, sizeof(buffer), "N%d:9999+", i);
            } else {
                snprintf(
                    buffer,
                    sizeof(buffer),
                    "N%d:%lu",
                    i,
                    (unsigned long)app->brain.neurons[i].spike_count);
            }
        } else {
            x = 2 + (i % 4) * 32;
            y = 18 + (i / 4) * 10;
            snprintf(
                buffer,
                sizeof(buffer),
                "N%d:%d%s",
                i,
                app->brain.neurons[i].potential,
                app->brain.neurons[i].fired ? "*" :
                    (app->brain.neurons[i].refractory_remaining > 0 ? "R" : ""));
        }
        
        canvas_draw_str(canvas, x, y, buffer);
    }

    if(!show_spike_counts) {
        const int16_t position = app->position;
        const uint8_t marker_x = 14 + position * 5;
        canvas_draw_line(canvas, 14, 38, 114, 38);
        canvas_draw_line(canvas, 14, 35, 14, 41);
        canvas_draw_line(canvas, 114, 35, 114, 41);
        canvas_draw_box(canvas, marker_x - 2, 36, 5, 5);
        snprintf(buffer, sizeof(buffer), "N3 <  P:%d  > N4", position);
        canvas_draw_str(canvas, 14, 49, buffer);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "Tick:%lu",
        (unsigned long)app->brain.tick
    );

    canvas_draw_str(canvas, 3, 56, buffer);
    canvas_draw_str(canvas, 90, 56, app->network_from_sd ? "SD" : "ROM");
    canvas_draw_str(canvas, 2, 63, "UP:+ DOWN:auto OK:v");
}

static void flypet_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}

int32_t flypet_app(void* p) {
    UNUSED(p);

    FlyPetApp app;
    app.show_spike_counts = false;
    app.position = 0;
    app.auto_stimulus_enabled = false;
    app.auto_stimulus_remaining = AUTO_STIMULUS_TICKS;

    NetworkDefinition network;
    app.network_from_sd = network_load_file(FLYPET_NETWORK_PATH, &network) &&
                          brain_init(&app.brain, &network);
    if(!app.network_from_sd) {
        FURI_LOG_W("FlyPet", "Network file unavailable or invalid; using built-in network");
    }
    if(!app.network_from_sd && !brain_init(&app.brain, &flypet_small_network)) {
        FURI_LOG_E("FlyPet", "Invalid network definition");
        return -1;
    }

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.view_port = view_port_alloc();

    view_port_draw_callback_set(app.view_port, flypet_draw_callback, &app);
    view_port_input_callback_set(app.view_port, flypet_input_callback, app.input_queue);

    app.gui = furi_record_open(RECORD_GUI);
    
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    bool running = true;

    InputEvent event;

    const uint32_t brain_interval = furi_ms_to_ticks(BRAIN_TICK_MS);
    uint32_t last_brain_tick = furi_get_tick();

    while(running) {
        uint32_t elapsed = furi_get_tick() - last_brain_tick;
        uint32_t wait_ticks = elapsed < brain_interval ? brain_interval - elapsed : 0;

        FuriStatus status = furi_message_queue_get(app.input_queue, &event, wait_ticks);

        if(status == FuriStatusOk && event.type == InputTypePress) {
            if(event.key == InputKeyBack) {
                running = false;
            } else if(event.key == InputKeyUp) {
                brain_stimulate(&app.brain, 0, 60);
                view_port_update(app.view_port);
            } else if(event.key == InputKeyDown) {
                app.auto_stimulus_enabled = !app.auto_stimulus_enabled;
                app.auto_stimulus_remaining = AUTO_STIMULUS_TICKS;
                view_port_update(app.view_port);
            } else if(event.key == InputKeyOk) {
                app.show_spike_counts = !app.show_spike_counts;
                view_port_update(app.view_port);
            }
        }
        
        if(running && (uint32_t)(furi_get_tick() - last_brain_tick) >= brain_interval) {
            last_brain_tick += brain_interval;
            flypet_apply_contact_sensors(&app);
            flypet_apply_auto_stimulus(&app);
            brain_step(&app.brain);
            flypet_update_position(&app);
            view_port_update(app.view_port);
        }
    }

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}