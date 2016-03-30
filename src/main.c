#include <pebble.h>
#include <limits.h>

static Window *s_main_window;
static GBitmap *s_bitmap;
static Layer *s_canvas_layer;
static BitmapLayer *s_bitmap_layer;
static TextLayer *s_time_layer;
static AppTimer *s_timer;

static uint32_t delta = 100; // 10UPS
static int updates = 0;

static int current_idle_frame = 0;
static int hatch_animation_frame = 0;


// make start time install time or first run time
// then store it in memory and get from there instead of setting to 0 every time
static int start_time = 0L;
static int last_time = 0L;

struct Monster {
  int id;
  char name[16];
  // 0: egg, 1: hatching, 2: idle, 3: sleeping, 4: sick, 5: hungry, 6: dirty, 7: bored, 8: dead
  int state;
  int age;

  // between 0 and 1
  // having these as doubles allows smaller increments than 1 per minute
  // because logic is only updated once per minute
  double hatch_points;
  double heat_points;
  double health;
  double hunger;
  double cleanliness;
  double boredom;

  uint32_t egg_states[2];
  uint32_t cracked_states[2];
  uint32_t idle_states[2];
  uint32_t sleep_states[2];
  uint32_t sick_states[2];
  uint32_t hungry_states[2];
  uint32_t dirty_states[2];
  uint32_t bored_states[2];
  uint32_t dead_states[2];

  int hatch_update;
};

static struct Monster sprat;


// Make a default monster (sprat)
const struct Monster MONSTER_DEFAULT = {
  1,  // id
  "sprat",
  0,  // state
  0,  // age
  0.0,  // hatch_points
  0.0,  // heat_points
  1.0,  // health
  0.0,  // hunger
  1.0,  // cleanliness
  0.0,  // boredom
  { RESOURCE_ID_001_SPRAT_EGG1, RESOURCE_ID_001_SPRAT_EGG2 },
  { RESOURCE_ID_001_SPRAT_CRACKED },
  { RESOURCE_ID_001_SPRAT_IDLE1, RESOURCE_ID_001_SPRAT_IDLE2 },
  { RESOURCE_ID_001_SPRAT_SLEEP },
  { RESOURCE_ID_001_SPRAT_SICK },
  { RESOURCE_ID_001_SPRAT_HUNGRY },
  { RESOURCE_ID_001_SPRAT_DIRTY },
  { RESOURCE_ID_001_SPRAT_BORED },
  { RESOURCE_ID_001_SPRAT_DEAD },
  INT_MAX
};

/****** Button handling ******/
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(sprat.state == 0) {
    sprat.heat_points += 0.01; // 1%
  }
}

static void click_config_provider(void *context) {
  ButtonId id = BUTTON_ID_SELECT;
  window_single_click_subscribe(id, select_click_handler);
  window_single_repeating_click_subscribe(id, 100, select_click_handler);
}

/****** Drawing lines and stuff ******/
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  if(sprat.state == 0) {
    int l_edge = 10;
    int r_edge = 134;
    int bottom = 158;
    int heat_point_line_end = l_edge + (r_edge - l_edge) * sprat.heat_points;

    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 5);
    graphics_context_set_antialiased(ctx, false);

    GPoint start = GPoint(l_edge, bottom);
    GPoint end = GPoint(r_edge, bottom);
    graphics_draw_line(ctx, start, end);

    graphics_context_set_stroke_color(ctx, GColorRed);

    end = GPoint(heat_point_line_end, bottom);
    graphics_draw_line(ctx, start, end);
  }
}

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
}

static void display_monster_state(struct Monster m) {
  switch(m.state){
    case 0: // egg
      display_image(m.egg_states[current_idle_frame]);
      current_idle_frame = (current_idle_frame + 1) % (sizeof(m.egg_states) / sizeof(uint32_t));
      break;

    case 1: // hatching
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "frame: %d", hatch_animation_frame/4);

      switch(hatch_animation_frame / 4){
        case 0:
          display_image(m.egg_states[1]);
          break;
        case 1:
          display_image(m.cracked_states[0]);
          break;
        case 2:
          display_image(m.idle_states[0]);
          break;
        default:
          break;
      }
      hatch_animation_frame = hatch_animation_frame + 1;
      break;

    case 2: // idle
      display_image(m.idle_states[current_idle_frame]);
      current_idle_frame = (current_idle_frame + 1) % (sizeof(m.idle_states) / sizeof(uint32_t));
      break;
  }
}

static void update() {
  uint16_t ms;
  time_ms(NULL, &ms);
  time_t temp = time(NULL);
  int current_time = (int)temp;

  if(start_time == 0) {
    start_time = current_time;
    last_time = 0;
  }

  int total_elapsed_time_ms = (current_time - start_time) * 1000 + ms; // delta from app start in ms
  int elapsed_time_ms = total_elapsed_time_ms - last_time; // delta from last update in ms
  last_time = total_elapsed_time_ms;

  if(sprat.state == 0) {
    sprat.hatch_points += (elapsed_time_ms * sprat.heat_points) / (60*60*1000);
    sprat.heat_points -= ((double)1/12000) * sprat.heat_points; // 5 every 3 minutes (i think?)
    sprat.heat_points = sprat.heat_points >= 1 ? 1 :
                        sprat.heat_points <= 0 ? 0 :
                        sprat.heat_points;

    if(sprat.hatch_points >= 1) {
      sprat.state = 1;
      sprat.hatch_update = updates;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Hatching!");
    }
  }

  updates++;
  if(updates % (int)(5 / (sprat.state == 0 ? sprat.heat_points : 1)) == 0) {
    display_monster_state(sprat);
  }

  if(sprat.state == 1 && (sprat.hatch_update + 5*4*3) <= updates) {
    sprat.state = 2;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Hatched");
  }

  /*
  int big_hatch_points = sprat.hatch_points * 10000;
  int hatch_whole = big_hatch_points / 100;
  int hatch_decimal = big_hatch_points % 100;

  int big_heat_points = sprat.heat_points * 100000;
  int heat_whole = big_heat_points / 1000;
  int heat_decimal = big_heat_points % 1000;


  APP_LOG(APP_LOG_LEVEL_INFO, "s: %d, ha: %d.%02d%%, he: %d.%03d%%, et: %d, tet: %d",
                              sprat.state,
                              hatch_whole,
                              hatch_decimal,
                              heat_whole,
                              heat_decimal,
                              elapsed_time_ms,
                              total_elapsed_time_ms);
  */
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
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

  GRect start = GRect(0, 0, 144, 168);
  Animation *anim0 = create_animation(start, get_random_end_rect(start));
  Animation *anim1 = create_animation(start, get_random_end_rect(start));
  Animation *anim2 = create_animation(start, get_random_end_rect(start));
  Animation *anim3 = create_animation(start, get_random_end_rect(start));
  Animation *anim4 = create_animation(start, get_random_end_rect(start));

  Animation *final_anim = animation_sequence_create(anim0, anim1, anim2, anim3, anim4, NULL);


  animation_schedule(final_anim);
  // app_timer_register(1500, start_egg_sequence, NULL);
}

static void timer_callback() {
  update();
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  layer_mark_dirty(s_canvas_layer);
  s_timer = app_timer_register(delta, timer_callback, NULL);
}

static void start() {
  display_monster_state(sprat);
  s_timer = app_timer_register(delta, timer_callback, NULL);
}

static void main_window_load(Window *window) {
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

  // Primitive drawing stuff
  s_canvas_layer = layer_create(bounds);

  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_get_root_layer(window), s_canvas_layer);

  // change_creature_state();
  // start_egg_sequence(sprat);

  // Bitmap drawing stuff
  s_bitmap_layer = bitmap_layer_create(bounds);

  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));
  start();
}

static void main_window_unload(Window *window) {
  bitmap_layer_destroy(s_bitmap_layer);
  gbitmap_destroy(s_bitmap);
  text_layer_destroy(s_time_layer);
  app_timer_cancel(s_timer);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  window_set_click_config_provider(s_main_window, click_config_provider);

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
