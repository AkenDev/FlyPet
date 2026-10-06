#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#define NEURON_COUNT 8
#define SYNAPSE_COUNT 10
//Inicio: Velocidad del cerebro
#define BRAIN_TICK_MS 200
// Fin: Velocidad del cerebro

typedef struct {
    int16_t potential;
    uint16_t threshold;
    bool fired;
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

    Neuron neurons[NEURON_COUNT];
    Synapse synapses[SYNAPSE_COUNT];

    uint8_t tick;
} FlyPetApp;

static void flypet_init_network(FlyPetApp* app) {
    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        app->neurons[i].potential = 0;
        app->neurons[i].threshold = 50;
        app->neurons[i].fired = false;
    }

    app->tick = 0;
}

static void flypet_init_synapses(FlyPetApp* app){
    app->synapses[0] = (Synapse){0, 1, 30};
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
    app-> neurons[0].potential += 60;

    if(app->neurons[0].potential > 100){
        app->neurons[0].potential = 100;
    }
}

static void flypet_step_network(FlyPetApp* app) {
    int16_t incoming[NEURON_COUNT] = {0};

    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        app->neurons[i].fired = app->neurons[i].potential >= app->neurons[i].threshold;
    }

    for(uint8_t s = 0; s < SYNAPSE_COUNT; s++) {
        uint8_t source = app->synapses[s].source;
        uint8_t target = app->synapses[s].target;
        
        if(app->neurons[source].fired) {
            incoming[target] += app->synapses[s].weight;
        }
    }

    for(uint8_t i = 0; i < NEURON_COUNT; i++) {
        
        if(app->neurons[i].fired){
            app->neurons[i].potential = 0;
        } else {
            app->neurons[i].potential = (app->neurons[i].potential * 8)/10;
        }

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

static void flypet_draw_callback(Canvas* canvas, void* ctx) {
    // Drawing code for the FlyPet app
    FlyPetApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 33, 9, "FlyPet LIF");
    // Add more drawing logic here

    canvas_set_font(canvas, FontSecondary);

    char buffer[24];

    for(uint8_t i = 0; i < NEURON_COUNT; i++){
        uint8_t column = i % 4;
        uint8_t row = i / 4;

        uint8_t x = 2 + (column * 32);
        uint8_t y = 25 + (row * 18);

        snprintf(
            buffer,
            sizeof(buffer),
            "N%d:%d%s",
            i,
            app->neurons[i].potential,
            app->neurons[i].fired ? "*" : ""
        );
        
        canvas_draw_str(canvas, x, y, buffer);
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "Tick:%lu",
        (unsigned long)app->tick
    );

    canvas_draw_str(canvas, 3, 56, buffer);
    canvas_draw_str(canvas, 4, 63, "UP stim OK Step");
}

static void flypet_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}

int32_t flypet_app(void* p) {
    UNUSED(p);

    FlyPetApp app;

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.view_port = view_port_alloc();

    flypet_init_network(&app);
    flypet_init_synapses(&app);

    view_port_draw_callback_set(app.view_port, flypet_draw_callback, &app);
    view_port_input_callback_set(app.view_port, flypet_input_callback, app.input_queue);

    app.gui = furi_record_open(RECORD_GUI);
    
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    bool running = true;

    // Inicio: relog automático del cerebro
    while(running) {
        FuriStatus status= furi_message_queue_get(
            app.input_queue,
            &event,
            BRAIN_TICK_MS
        );

        if(status == FuriStatusOk){
            //Recibimos un botón antes de que pasaron los 200ms.
            if (event.type == InputTypePress){
                if(event.key == InputKeyBack){
                    running = false;
                } else if(event.key == InputKeyUp){
                    // Intectamos el estímulo con el botón UP a la neurona 0
                    flupet_stimulate(&app);

                    view_port_update(app.view_port);
                }
            }
        } else {
            //No recibimos un botón antes de que pasaron los 200ms.
            flypet_step_network(&app);
            view_port_update(app.view_port);
        }
    }
    // Fin: relog automático del cerebro

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}