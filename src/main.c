#include <pebble.h>

static Window *s_main_window;
static GBitmap *s_bitmap;
static BitmapLayer *s_bitmap_layer;
static TextLayer *s_time_layer;

static uint32_t creature_states[2] = {RESOURCE_ID_001_SPRAT_IDLE1, RESOURCE_ID_001_SPRAT_IDLE2};
static int current_state = 0;

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void change_creature_state() {
  current_state = (current_state + 1) % (sizeof(creature_states) / sizeof(uint32_t));
  gbitmap_destroy(s_bitmap);
  s_bitmap = gbitmap_create_with_resource(creature_states[current_state]);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  app_timer_register(500, change_creature_state, NULL);
}

static Animation * create_animation(struct GRect s) {
  int r = (rand() % 11) - 5;
  GRect end = GRect(s.origin.x + r, s.origin.y, s.size.w, s.size.h);

  PropertyAnimation *prop_anim = property_animation_create_layer_frame((Layer*)s_bitmap_layer, &s, &end);
  Animation *anim = property_animation_get_animation(prop_anim);

  int delay_ms = 0;
  int duration_ms = rand() % 50 + 50;

  animation_set_curve(anim, AnimationCurveLinear);
  animation_set_delay(anim, delay_ms);
  animation_set_duration(anim, duration_ms);
  return anim;
}

static void start_egg_sequence() {
  gbitmap_destroy(s_bitmap);
  s_bitmap = gbitmap_create_with_resource(RESOURCE_ID_001_SPRAT_EGG);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));

  int closeness_to_hatching = 1500;

  GRect start = GRect(0, 30, 144, 168);
  Animation *anim0 = create_animation(start);
  Animation *anim1 = create_animation(start);
  Animation *anim2 = create_animation(start);
  Animation *anim3 = create_animation(start);
  Animation *anim4 = create_animation(start);

  Animation *final_anim = animation_sequence_create(anim0, anim1, anim2, anim3, anim4, NULL);


  animation_schedule(final_anim);
  app_timer_register(closeness_to_hatching, start_egg_sequence, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  // Make the egg show slightly lower
  GRect egg_bounds = (GRect(0, 30, bounds.size.w, bounds.size.h));

  // Time stuff
  s_time_layer = text_layer_create(GRect(0, 8, bounds.size.w, 50));

  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // s_bitmap_layer = bitmap_layer_create(bounds);
  s_bitmap_layer = bitmap_layer_create(egg_bounds);

  //change_creature_state();
  start_egg_sequence();

  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
}

static void main_window_unload(Window *window) {
  bitmap_layer_destroy(s_bitmap_layer);
  gbitmap_destroy(s_bitmap);
  text_layer_destroy(s_time_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  update_time();
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
