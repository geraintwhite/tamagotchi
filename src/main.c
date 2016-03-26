#include <pebble.h>

static Window *s_main_window;
static GBitmap *s_bitmap;
static BitmapLayer *s_bitmap_layer;
static TextLayer *s_time_layer;

static uint32_t creature_states[2] = { RESOURCE_ID_001_SPRAT_IDLE1, RESOURCE_ID_001_SPRAT_IDLE2 };
static uint32_t animation_sequence[10];
static int current_state = 0;
static int animation_frame = 0;
static int total_frames = 0;

// make start time install time or first run time
// then store it in memory and get from there instead of setting to 0 every time
static int start_time = 0L;
static int last_time = 0L;

static bool hatched = 0;

//#define time_t START_TIME = time(NULL);

struct Monster {
  int id;
  char name[16];
  // 0: egg
  // 1: idle
  // 2: sleeping
  // 3: sick
  // 4: hungry
  // 5: dirty
  // 6: bored
  // 7: dead
  int state;

  // both between 0 and 1
  // having these as doubles allows smaller increments than 1 per minute
  double hatch_points;
  double heat_points;

  // Couldn't think of a better way to store each type of state, this'll do for now
  uint32_t egg_states[2];
  uint32_t cracked_states[2];
  uint32_t idle_states[2];
  uint32_t sleep_states[2];
  uint32_t sick_states[2];
  uint32_t hungry_states[2];
  uint32_t dirty_states[2];
  uint32_t bored_states[2];
  uint32_t dead_states[2];
};

static struct Monster sprat;

// Make a default monster (sprat)
const struct Monster MONSTER_DEFAULT = {
  1,
  "sprat",
  0,
  0.0,
  1.0, // set to 1 for testing
  { RESOURCE_ID_001_SPRAT_EGG1, RESOURCE_ID_001_SPRAT_EGG2 },
  { RESOURCE_ID_001_SPRAT_CRACKED },
  { RESOURCE_ID_001_SPRAT_IDLE1, RESOURCE_ID_001_SPRAT_IDLE2 },
  { RESOURCE_ID_001_SPRAT_SLEEP },
  { RESOURCE_ID_001_SPRAT_SICK },
  { RESOURCE_ID_001_SPRAT_HUNGRY },
  { RESOURCE_ID_001_SPRAT_DIRTY },
  { RESOURCE_ID_001_SPRAT_BORED },
  { RESOURCE_ID_001_SPRAT_DEAD }
};

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

static void display_image(uint32_t resource) {
  gbitmap_destroy(s_bitmap);
  s_bitmap = gbitmap_create_with_resource(resource);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
}

static void animation_loop() {
  display_image(animation_sequence[animation_frame]);
  animation_frame = (animation_frame + 1) % total_frames;
  app_timer_register(1000, animation_loop, NULL);
}

static void start_idle_animation(struct Monster monster) {
  animation_sequence[0] = monster.idle_states[0];
  animation_sequence[1] = monster.idle_states[1];
  total_frames = 2;

  animation_loop();
}

static void single_animation() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Frame: %d, Total frames: %d", animation_frame, total_frames);
  display_image(animation_sequence[animation_frame]);
  if(animation_frame == total_frames - 1) {
    memset(animation_sequence, 0, sizeof(animation_sequence));
    total_frames = 0;
    // This made the last image of this single animation not appear
    // and is also wrong because not generic, was just for testing
    // start_idle_animation(sprat);
    return;
  }
  animation_frame = (animation_frame + 1) % total_frames;
  app_timer_register(2000, single_animation, NULL);
}

static void start_hatch_animation(struct Monster monster) {
  // uint32_t sequence[4] = { monster.egg_states[0], monster.egg_states[1], monster.cracked_states[0], monster.idle_states[0] };
  animation_sequence[0] = monster.egg_states[0];
  animation_sequence[1] = monster.egg_states[1];
  animation_sequence[2] = monster.cracked_states[0];
  animation_sequence[3] = monster.idle_states[0];
  total_frames = 4;

  single_animation();
  return;
}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  time_t temp = mktime(tick_time);
  int current_time = (int)temp;

  if(start_time == 0) {
    start_time = current_time;
    last_time = current_time;
  }

  int total_elapsed_time = current_time - start_time;
  int elapsed_time = current_time - last_time;
  last_time = current_time;

  // this is OP for testing purposes
  // adding 0.01 per second atm (elapsed_time/100) @ full heat points
  // 0.01 per minute = 0.00016667 per second (elapsed_time/6000)
  if(!hatched) {
    sprat.hatch_points += ((double)elapsed_time / 100) * sprat.heat_points;
  }

  if(sprat.hatch_points >= 1 && !hatched) {
    hatched = true;
    start_hatch_animation(sprat);
    sprat.state = 1;
    // don't know how to get this to run AFTER the hatch animation has finished
    // There's probably a better way to implement animating that app_timer_register
    // but idk what it is at the moment
    // start_idle_animation(sprat);
  }

  // This is just for printing and testing delete before release!!
  int big_hatch_points = (int)(sprat.hatch_points * 10000);
  int hatch_whole = big_hatch_points / 100;
  int hatch_decimal = big_hatch_points % 100;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Current time: %d, Total elapsed time: %d, Hatch percent: %d.%d",
                                current_time,
                                total_elapsed_time,
                                hatch_whole,
                                hatch_decimal
  );
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Last elapsed time: %d", elapsed_time);

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

static Animation * create_animation(struct GRect start, struct GRect end) {
  PropertyAnimation *prop_anim = property_animation_create_layer_frame((Layer*)s_bitmap_layer, &start, &end);
  Animation *anim = property_animation_get_animation(prop_anim);

  int delay_ms = 0;
  int duration_ms = rand() % 50 + 50;

  animation_set_curve(anim, AnimationCurveLinear);
  animation_set_delay(anim, delay_ms);
  animation_set_duration(anim, duration_ms);
  return anim;
}

static GRect get_random_end_rect(struct GRect start) {
  int r = (rand() % 9) - 4;
  GRect end = GRect(start.origin.x + r, start.origin.y, start.size.w, start.size.h);
  return end;
}

static void start_egg_sequence(struct Monster m) {
  display_image(RESOURCE_ID_001_SPRAT_EGG1);

  int closeness_to_hatching = 1500;

  GRect start = GRect(0, 0, 144, 168);
  Animation *anim0 = create_animation(start, get_random_end_rect(start));
  Animation *anim1 = create_animation(start, get_random_end_rect(start));
  Animation *anim2 = create_animation(start, get_random_end_rect(start));
  Animation *anim3 = create_animation(start, get_random_end_rect(start));
  Animation *anim4 = create_animation(start, get_random_end_rect(start));

  Animation *final_anim = animation_sequence_create(anim0, anim1, anim2, anim3, anim4, NULL);


  animation_schedule(final_anim);
  // app_timer_register(closeness_to_hatching, start_egg_sequence, NULL);
}

static void main_window_load(Window *window) {
  // lmao app logs can't be that long
  // abcd on the end isn't printed
  APP_LOG(APP_LOG_LEVEL_DEBUG, "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456abcd");
  sprat = MONSTER_DEFAULT;

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Time stuff
  s_time_layer = text_layer_create(GRect(0, 8, bounds.size.w, 50));

  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_bitmap_layer = bitmap_layer_create(bounds);

  //change_creature_state();
  start_egg_sequence(sprat);

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
