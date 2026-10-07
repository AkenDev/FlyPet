#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#define NEURON_COUNT 8
#define SYNAPSE_COUNT 10
#define REFRACTORY_TICKS 1
#define BRAIN_TICK_MS 200
// ===== INICIO CAMBIO: salidas neuronales y limites de movimiento =====
#define OUTPUT_LEFT_NEURON 3
#define OUTPUT_RIGHT_NEURON 4
#define POSITION_MAX 20
// ===== FIN CAMBIO: salidas neuronales y limites de movimiento =====

typedef struct {
    int16_t potential;
    uint16_t threshold;
    bool fired;
    uint32_t spike_count;
    uint8_t refractory_remaining;
} Neuron;

typedef struct {
    uint8_t source;
    uint8_t target;
    uint8_t weight;
} Synapse;

typedef struct{
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    bool show_spike_counts;
    // ===== INICIO CAMBIO: posicion del marcador =====
    int16_t position;
    // ===== FIN CAMBIO: posicion del marcador =====

    Neuron neurons[NEURON_COUNT];
    Synapse synapses[SYNAPSE_COUNT];

    uint32_t tick;

} FlyPetApp;

static void flypet_init_network(FlyPetApp* app) {
    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        app->neurons[i].potential = 0;
        app->neurons[i].threshold = 50;
        app->neurons[i].fired = false;
        app->neurons[i].spike_count = 0;
        app->neurons[i].refractory_remaining = 0;
    }

    app->tick = 0;
}

static void flypet_init_synapses(FlyPetApp* app){
    // ===== INICIO CAMBIO: diferenciar el ritmo de las ramas neuronales =====
    app->synapses[0] = (Synapse){0, 1, 35};
    // ===== FIN CAMBIO: diferenciar el ritmo de las ramas neuronales =====
    app->synapses[1] = (Synapse){0, 2, 25};
    app->synapses[2] = (Synapse){1, 3, 35};
    app->synapses[3] = (Synapse){2, 3, 20};
    app->synapses[4] = (Synapse){2, 4, 40};
    app->synapses[5] = (Synapse){3, 5, 30};
    app->synapses[6] = (Synapse){4, 5, 25};
    app->synapses[7] = (Synapse){5, 6, 35};
    app->synapses[8] = (Synapse){6, 7, 30};
    app->synapses[9] = (Synapse){1, 4, 15};
}

static void flypet_stimulate(FlyPetApp* app){
    if(app->neurons[0].refractory_remaining > 0) {
        return;
    }
    app-> neurons[0].potential += 60;

    if(app->neurons[0].potential > 100){
        app->neurons[0].potential = 100;
    }
}

static void flypet_step_network(FlyPetApp* app) {
    int16_t incoming[NEURON_COUNT] = {0};

    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        app->neurons[i].fired =
            app->neurons[i].refractory_remaining == 0 &&
            app->neurons[i].potential >= app->neurons[i].threshold;
    }

    for(uint8_t s = 0; s < SYNAPSE_COUNT; s++) {
        uint8_t source = app->synapses[s].source;
        uint8_t target = app->synapses[s].target;
        
        if(app->neurons[source].fired) {
            incoming[target] += app->synapses[s].weight;
        }
    }

    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        if(app->neurons[i].fired) {
            app->neurons[i].spike_count++;
            app->neurons[i].potential = 0;
            app->neurons[i].refractory_remaining = REFRACTORY_TICKS;
            continue;
        }

        if(app->neurons[i].refractory_remaining > 0) {
            app->neurons[i].refractory_remaining--;
            app->neurons[i].potential = 0;
            continue;
        }

        app->neurons[i].potential = (app->neurons[i].potential * 8)/10;

        app->neurons[i].potential += incoming[i];

        if(app->neurons[i].potential > 100) {
            app->neurons[i].potential = 100;
        }

        if(app->neurons[i].potential < 0) {
            app->neurons[i].potential = 0;
        }
    }

    app->tick++;
}

// ===== INICIO CAMBIO: convertir disparos en movimiento =====
static void flypet_update_position(FlyPetApp* app) {
    int16_t movement =
        (int16_t)app->neurons[OUTPUT_RIGHT_NEURON].fired -
        (int16_t)app->neurons[OUTPUT_LEFT_NEURON].fired;

    // Sumamos ambos efectos antes de limitar: dos disparos se cancelan.
    app->position += movement;
    if(app->position < 0) {
        app->position = 0;
    } else if(app->position > POSITION_MAX) {
        app->position = POSITION_MAX;
    }
}
// ===== FIN CAMBIO: convertir disparos en movimiento =====

static void flypet_draw_callback(Canvas* canvas, void* ctx) {
    FlyPetApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    const bool show_spike_counts = app->show_spike_counts;
    canvas_draw_str(canvas, 2, 9, show_spike_counts ? "Disparos" : "FlyPet LIF");

    canvas_set_font(canvas, FontSecondary);

    char buffer[24];

    for(uint8_t i = 0; i < NEURON_COUNT; i++){
        uint8_t x;
        uint8_t y;
        if(show_spike_counts) {
            x = 2 + (i % 2) * 64;
            y = 18 + (i / 2) * 10;
            if(app->neurons[i].spike_count > 9999) {
                snprintf(buffer, sizeof(buffer), "N%d:9999+", i);
            } else {
                snprintf(
                    buffer,
                    sizeof(buffer),
                    "N%d:%lu",
                    i,
                    (unsigned long)app->neurons[i].spike_count);
            }
        } else {
            x = 2 + (i % 4) * 32;
            // ===== INICIO CAMBIO: espacio para la pista de movimiento =====
            y = 18 + (i / 4) * 10;
            // ===== FIN CAMBIO: espacio para la pista de movimiento =====
            snprintf(
                buffer,
                sizeof(buffer),
                "N%d:%d%s",
                i,
                app->neurons[i].potential,
                app->neurons[i].fired ? "*" :
                    (app->neurons[i].refractory_remaining > 0 ? "R" : ""));
        }
        
        canvas_draw_str(canvas, x, y, buffer);
    }

    // ===== INICIO CAMBIO: visualizar la salida neuronal =====
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
    // ===== FIN CAMBIO: visualizar la salida neuronal =====

    snprintf(
        buffer,
        sizeof(buffer),
        "Tick:%lu",
        (unsigned long)app->tick
    );

    canvas_draw_str(canvas, 3, 56, buffer);
    canvas_draw_str(canvas, 4, 63, "UP stim OK vista");
}

static void flypet_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}

int32_t flypet_app(void* p) {
    UNUSED(p);

    FlyPetApp app;
    app.show_spike_counts = false;
    // ===== INICIO CAMBIO: comenzar en el centro =====
    app.position = POSITION_MAX / 2;
    // ===== FIN CAMBIO: comenzar en el centro =====

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.view_port = view_port_alloc();

    flypet_init_network(&app);
    flypet_init_synapses(&app);

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
                flypet_stimulate(&app);
                view_port_update(app.view_port);
            } else if(event.key == InputKeyOk) {
                app.show_spike_counts = !app.show_spike_counts;
                view_port_update(app.view_port);
            }
        }
        
        if(running && (uint32_t)(furi_get_tick() - last_brain_tick) >= brain_interval) {
            last_brain_tick += brain_interval;
            flypet_step_network(&app);
            // ===== INICIO CAMBIO: aplicar una salida por tick =====
            flypet_update_position(&app);
            // ===== FIN CAMBIO: aplicar una salida por tick =====
            view_port_update(app.view_port);
        }
    }

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}