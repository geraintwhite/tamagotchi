#include <pebble.h>

#define length(array) (sizeof(array) / sizeof(array[0]))

static Window * s_main_window;
static GBitmap * s_bitmap;
static BitmapLayer * s_bitmap_layer;
static TextLayer * s_time_layer;
static AppTimer * creature_state_timer;

static uint32_t idle_states[2] = {RESOURCE_ID_001_SPRAT_IDLE1, RESOURCE_ID_001_SPRAT_IDLE2};

typedef struct {
  int age;
  int health;
  int hunger;
  int cleanliness;
  int boredom;
  int creature_type;
  int idle_state;
  char name[];
} Creature;

typedef struct {
  int points;
  int current_creature;
  Creature * creatures[];
} User;

static User * user;


static void
change_idle_state(Creature * creature)
{
  creature->idle_state = (creature->idle_state + 1) % length(idle_states);
  gbitmap_destroy(s_bitmap);
  s_bitmap = gbitmap_create_with_resource(idle_states[creature->idle_state]);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  creature_state_timer = app_timer_register(500, (AppTimerCallback) change_idle_state, creature);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {

}

static void
update_time()
{
  time_t temp = time(NULL);
  struct tm * tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

static void
tick_handler(struct tm * tick_time, TimeUnits units_changed)
{
  update_time();
}

static void
main_window_load(Window * window)
{
  Layer * window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_time_layer = text_layer_create(GRect(0, 8, bounds.size.w, 50));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_bitmap_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
}

static void
main_window_unload(Window * window)
{
  bitmap_layer_destroy(s_bitmap_layer);
  gbitmap_destroy(s_bitmap);
  text_layer_destroy(s_time_layer);
}

static void
init()
{
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  Creature creature;

  accel_tap_service_subscribe(accel_tap_handler);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  change_idle_state(&creature);
  update_time();
}

static void
deinit()
{
  window_destroy(s_main_window);
}

int
main()
{
  init();
  app_event_loop();
  deinit();
}
