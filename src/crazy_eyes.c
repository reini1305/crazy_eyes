#include <pebble.h>
//#include "autoconfig.h"
#include <nightstand/nightstand.h>

static Layer *hands_layer;
static Window *window;
static int8_t charge_percentage = 100;
static bool bluetooth_connected = true;
static bool first_time = true;
static AppTimer *blink_timer;
static int8_t blink_y = 0;

static const int16_t eye_radius = 32;
static const int16_t pupil_radius = 8;
static const int16_t eye_distance = 6;
static int background_color;
static bool googly_mode;
static AppTimer *googly_timer;
static int32_t googly_angle;
static int32_t googly_acceleration;
#define GOOGLY_ACCEL_INCREMENT (TRIG_MAX_ANGLE / 60)
static AccelData googly_data;

// Preferences
#define KEY_SETTINGS 1
typedef struct settings{
  bool show_eyebrows;
  bool show_mouth;
  bool bluetooth_status;
  bool vibrate_on_disconnect;
  bool minute_blink;
  bool first_day_monday;
  bool nightstand_mode;
  bool googly_eyes;
  uint8_t angryness;
} settings;

static settings s_settings;

static void loadSettings(void) {
  if(persist_exists(KEY_SETTINGS)) {
    persist_read_data(KEY_SETTINGS,&s_settings,sizeof(settings));
  } else {
    s_settings.angryness=11;
    s_settings.bluetooth_status=true;
    s_settings.first_day_monday=true;
    s_settings.googly_eyes=false;
    s_settings.minute_blink=false;
    s_settings.nightstand_mode=true;
    s_settings.show_eyebrows=true;
    s_settings.show_mouth=false;
    s_settings.vibrate_on_disconnect=true;
  }
}

static void update_color() {
#ifdef PBL_COLOR
    background_color=(background_color+1)%64;
#endif
}

static void in_received_handler(DictionaryIterator *iter, void *context) {

  // Read preferences
  Tuple *t = dict_find(iter, MESSAGE_KEY_eyebrows);
  if(t) {
    s_settings.show_eyebrows = t->value->int32 == 1;
  }
  if((t = dict_find(iter, MESSAGE_KEY_bluetooth)))
    s_settings.bluetooth_status = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_blinking)))
    s_settings.minute_blink = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_googly)))
  s_settings.googly_eyes = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_eyebrows)))
  s_settings.show_eyebrows = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_nightstand)))
  s_settings.nightstand_mode = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_mouth)))
  s_settings.show_mouth = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_vibrate)))
  s_settings.vibrate_on_disconnect = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_angryness)))
  s_settings.angryness = t->value->int32;
  update_color();
  layer_mark_dirty(hands_layer);
}
static void handle_battery(BatteryChargeState battery)
{
  charge_percentage =  battery.charge_percent ;
  layer_mark_dirty(hands_layer);
}

static void handle_bluetooth(bool connected){
  bluetooth_connected = connected;
  if(s_settings.vibrate_on_disconnect && !connected)
    vibes_long_pulse();
  first_time = false;
  layer_mark_dirty(hands_layer);
}

static void blink_up_callback(void* data){
  if(blink_y<0){
    blink_y=0;
    blink_timer = NULL;
  }
  else {
    blink_y-=8;
    blink_timer = app_timer_register(33,blink_up_callback,NULL);
  }
  layer_mark_dirty(hands_layer);
}

static void blink_down_callback(void* data){
  if (blink_y < 2*(eye_radius)) {
    blink_y+=8;
    blink_timer = app_timer_register(33,blink_down_callback,NULL);
  }
  else
    blink_timer = app_timer_register(33,blink_up_callback,NULL);
  layer_mark_dirty(hands_layer);
}

static void googly_update_countdown(void* data){
  // read accelerometer data
  accel_service_peek(&googly_data);

  // super simple mode
  //googly_angle = atan2_lookup(googly_data.x,googly_data.y);

  // linear acceleration
  int32_t goal_angle = atan2_lookup(googly_data.x,googly_data.y);
  if((abs(googly_angle-goal_angle) < GOOGLY_ACCEL_INCREMENT) && abs(googly_acceleration) < GOOGLY_ACCEL_INCREMENT) {
    googly_acceleration = 0;
  } else {
    if(googly_angle<goal_angle)
      googly_acceleration+=GOOGLY_ACCEL_INCREMENT;
    else
      googly_acceleration-=GOOGLY_ACCEL_INCREMENT;
    googly_acceleration = (googly_acceleration*9)/10;
  }
  googly_angle+=googly_acceleration;
  if (googly_data.did_vibrate) {
    googly_mode=false;
  }
  if(googly_mode)
    googly_timer = app_timer_register(33,googly_update_countdown,NULL);
  layer_mark_dirty(hands_layer);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  // blink if enabled
//  if(getBlinking() && !blink_timer)
//      blink_timer = app_timer_register(300,blink_down_callback,NULL);
  if(s_settings.googly_eyes) {
    if(!googly_mode) {
      googly_timer = app_timer_register(33,googly_update_countdown,NULL);
    }
    googly_mode = !googly_mode;
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  GColor color= PBL_IF_COLOR_ELSE((GColor8){.argb=((uint8_t)(0xC0|background_color))},GColorLightGray);

  const int16_t pupil_center_dist = eye_radius - pupil_radius - 4;
  const int16_t offset = (watch_info_get_model() == WATCH_INFO_MODEL_PEBBLE_STEEL) ? 12:0;

  GPoint left_eye_center = center;
  GPoint right_eye_center = center;

  left_eye_center.x-=eye_radius+eye_distance;
  right_eye_center.x+=eye_radius+eye_distance;
  left_eye_center.y+=offset;
  right_eye_center.y+=offset;

  // draw the eye circles
  graphics_context_set_fill_color(ctx, color);

  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx,GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx,left_eye_center, eye_radius);
  graphics_fill_circle(ctx,right_eye_center, eye_radius);

  graphics_context_set_stroke_width(ctx,2);
  graphics_context_set_stroke_color(ctx,GColorBlack);
  graphics_draw_circle(ctx,left_eye_center, eye_radius+1);
  graphics_draw_circle(ctx,right_eye_center, eye_radius+1);


  // calculate the center of the hour and minute circles (pupils)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t minute_angle =  TRIG_MAX_ANGLE * t->tm_min / 60;
  int32_t hour_angle =  (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6);
  GPoint minute_center;
  GPoint hour_center;
  if (googly_mode) {
    minute_center.x = (int16_t)(sin_lookup(googly_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.x;
    minute_center.y = (int16_t)(-cos_lookup(googly_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.y;
    hour_center.x = (int16_t)(sin_lookup(googly_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.x;
    hour_center.y = (int16_t)(-cos_lookup(googly_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.y;
  } else {
    minute_center.x = (int16_t)(sin_lookup(minute_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.x;
    minute_center.y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.y;
    hour_center.x = (int16_t)(sin_lookup(hour_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.x;
    hour_center.y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.y;
  }
  // Draw the pupils
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx,hour_center, pupil_radius);
  graphics_fill_circle(ctx,minute_center, pupil_radius);

  graphics_context_set_fill_color(ctx, GColorWhite);
  hour_center.x+=2;hour_center.y-=2;
  minute_center.x+=2;minute_center.y-=2;
  graphics_fill_circle(ctx,hour_center, 2);
  graphics_fill_circle(ctx,minute_center, 2);

  // Draw the blinking
  const GPathInfo BOLT_PATH_INFO = {
    .num_points = 4,
    .points = (GPoint []) {{-eye_radius-3, -eye_radius-3}, {eye_radius+3, -eye_radius-3}, {eye_radius+3, eye_radius+13}, {-eye_radius-3, eye_radius-3}}
  };
  GPath *s_my_path_ptr = gpath_create(&BOLT_PATH_INFO);
  GPoint blink_center = {
    .x = (int16_t) left_eye_center.x,
    .y = (int16_t) left_eye_center.y - 2 * eye_radius + blink_y - 8,
  };
  gpath_move_to(s_my_path_ptr, blink_center);

  // Fill the path:
  graphics_context_set_fill_color(ctx, color);

  gpath_draw_filled(ctx, s_my_path_ptr);
  gpath_destroy(s_my_path_ptr);

  if(s_settings.show_eyebrows) {
    // Draw the eyebrows
    int16_t charge_diff = (s_settings.angryness == 11) ? ((charge_percentage /5) -10) : (s_settings.angryness*2-10);
    int16_t bluetooth_diff_x = (bluetooth_connected||!s_settings.bluetooth_status)? 0:eye_radius;
    int16_t bluetooth_diff_y = (bluetooth_connected||!s_settings.bluetooth_status)? 0:charge_diff/2;
    GPoint left = {
      .x = (int16_t) left_eye_center.x-eye_radius+bluetooth_diff_x,
      .y = (int16_t) left_eye_center.y-eye_radius-20-charge_diff+bluetooth_diff_y,
    };

    GPoint right = {
      .x = (int16_t) left_eye_center.x+eye_radius,
      .y = (int16_t) left_eye_center.y-eye_radius-20+charge_diff,
    };

    graphics_draw_line(ctx,left,right);

    left.x = (int16_t) right_eye_center.x-eye_radius;
    left.y = (int16_t) right_eye_center.y-eye_radius-20+charge_diff;
    right.x = (int16_t) right_eye_center.x+eye_radius-bluetooth_diff_x;
    right.y = (int16_t) right_eye_center.y-eye_radius-20-charge_diff+bluetooth_diff_y,

    graphics_draw_line(ctx,left,right);
  }

  if(s_settings.show_mouth)
  {
    // Draw the mouth (day in binary)
    GRect mouth;
    mouth.size.w = (3 * eye_radius + eye_distance)/8;
    mouth.size.w*=8;
    mouth.size.h = 20;
    mouth.origin.x = (bounds.size.w - mouth.size.w)/2;
    mouth.origin.y = left_eye_center.y + eye_radius + 10;
    graphics_context_set_fill_color(ctx,GColorWhite);
    graphics_context_set_stroke_color(ctx,GColorBlack);

    // Draw the teeth
    GRect tooth;
    tooth.size.w = mouth.size.w / 8 - 2;
    tooth.size.h = mouth.size.h-1;
    tooth.origin.y = mouth.origin.y;

    int curr_day = t->tm_mday;
    for(int8_t tooth_id = 0; tooth_id < 8; tooth_id++) {
      tooth.origin.x = mouth.origin.x+1 + tooth_id * (tooth.size.w+2);
      graphics_context_set_fill_color(ctx,(curr_day & (1 << (7-tooth_id)))?GColorBlack:GColorWhite);
      graphics_fill_rect(ctx,tooth,4,GCornersBottom);
    }
    graphics_context_set_stroke_color(ctx,GColorBlack);
    graphics_context_set_stroke_width(ctx,2);
    graphics_draw_round_rect(ctx,mouth,4);

  }
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  bool update_time = true;
  if (s_settings.nightstand_mode) {
    update_time = !nightstand_window_update();
  }
  if(update_time) {
    update_color();
    if(s_settings.minute_blink && !blink_timer)
      blink_timer = app_timer_register(300,blink_down_callback,NULL);
    layer_mark_dirty(hands_layer);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // init hands
  hands_layer = layer_create(bounds);
  layer_set_update_proc(hands_layer, hands_update_proc);
  layer_add_child(window_layer, hands_layer);

  accel_tap_service_subscribe(accel_tap_handler);

  // force update
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_tick(t, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

  bluetooth_connection_service_subscribe(handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek());

  battery_state_service_subscribe(handle_battery);
  handle_battery(battery_state_service_peek());
}

static void window_unload(Window *window) {
  layer_destroy(hands_layer);
  tick_timer_service_unsubscribe();
}

static void init(void) {
  srand(time(NULL));
  background_color = rand()%64;
  loadSettings();
  app_message_open(100,100);
  app_message_register_inbox_received(in_received_handler);

  nightstand_window_init();
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  googly_mode=false;
  googly_angle = 0;
  googly_acceleration = 0;
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);

}

static void deinit(void) {
  window_destroy(window);
  nightstand_window_deinit();
  persist_write_data(KEY_SETTINGS,&s_settings,sizeof(settings));
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
