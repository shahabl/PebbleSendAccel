#include "pebble.h"

#define ACCEL_STEP_MS 100

typedef struct Vec2d {
  double x;
  double y;
} Vec2d;

typedef struct Disc {
  Vec2d pos;
  double radius;
} Disc;

static Disc discs[1];

static Window *window;
static GRect window_frame;
static Layer *disc_layer;

static AppTimer *timer;

static void disc_init(Disc *disc) {
  GRect frame = window_frame;
  disc->pos.x = frame.size.w/2;
  disc->pos.y = frame.size.h/2;
  disc->radius = 10;
}

static void disc_update_accel(Disc *disc, AccelData accel) {
    const GRect frame = window_frame;
    disc->pos.x = window_frame.size.w/2 + (accel.x)/2;
    disc->pos.y = window_frame.size.h/2 - (accel.y)/2;
    if(disc->pos.x + disc->radius > frame.size.w)
        disc->pos.x = frame.size.w - disc->radius /2;
    if(disc->pos.x - disc->radius < 0)
        disc->pos.x = disc->radius /2;
    if(disc->pos.y + disc->radius > frame.size.h)
        disc->pos.y = frame.size.h - disc->radius /2;
    if(disc->pos.y - disc->radius < 0)
        disc->pos.y = disc->radius /2;
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "xy: %d %d", (int) disc->pos.x, (int) disc->pos.y);
}

static void disc_draw(GContext *ctx, Disc *disc) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(disc->pos.x, disc->pos.y), disc->radius);
}

static void disc_layer_update_callback(Layer *me, GContext *ctx) {
    disc_draw(ctx, &discs[0]);
}

void send_accel(AccelData accel) {
    
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
 
    dict_write_int16(iter, 'x', accel.x);
    dict_write_int16(iter, 'y', accel.y);
    dict_write_int16(iter, 'z', accel.z);
    
    app_message_outbox_send();
}

static void timer_callback(void *data) {
    AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

    accel_service_peek(&accel);

    Disc *disc = &discs[0];
    disc_update_accel(disc, accel);
    layer_mark_dirty(disc_layer);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Accel: %d %d %d", accel.x, accel.y, accel.z);
    //Send an arbitrary message, the response will be handled by in_received_handler()
    send_accel(accel);

    timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect frame = window_frame = layer_get_frame(window_layer);

    disc_layer = layer_create(frame);
    layer_set_update_proc(disc_layer, disc_layer_update_callback);
    layer_add_child(window_layer, disc_layer);

    disc_init(&discs[0]);
}

static void window_unload(Window *window) {
    layer_destroy(disc_layer);
}

static void init() {
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });
    window_stack_push(window, true /* Animated */);
    window_set_background_color(window, GColorBlack);
    
    app_message_open(64, 64);  // inbound_size, outbound_size
 
    accel_data_service_subscribe(0, NULL);
    timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit() {
    accel_data_service_unsubscribe();
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
