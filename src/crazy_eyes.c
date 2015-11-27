#include <pebble.h>
#include "autoconfig.h"

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

static void update_color() {
#ifdef PBL_COLOR
    background_color=(background_color+1)%64;
#endif
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  autoconfig_in_received_handler(iter, context);
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
  if(getVibrate() && !connected)
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

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  // blink if enabled
  if(getBlinking() && !blink_timer)
      blink_timer = app_timer_register(300,blink_down_callback,NULL);
  layer_mark_dirty(hands_layer);
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  GColor color= PBL_IF_COLOR_ELSE((GColor8){.argb=((uint8_t)(0xC0|background_color))},GColorBlack);
  
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

#ifdef PBL_COLOR
  graphics_context_set_stroke_width(ctx,2);
  graphics_context_set_stroke_color(ctx,GColorBlack);
  graphics_draw_circle(ctx,left_eye_center, eye_radius+1);
  graphics_draw_circle(ctx,right_eye_center, eye_radius+1);
#endif
  
  // calculate the center of the hour and minute circles (pupils)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t minute_angle =  TRIG_MAX_ANGLE * t->tm_min / 60;
  GPoint minute_center = {
    .x = (int16_t)(sin_lookup(minute_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.x,
    .y = (int16_t)(-cos_lookup(minute_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + right_eye_center.y,
  };
  int32_t hour_angle =  (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6);
  GPoint hour_center = {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)pupil_center_dist / TRIG_MAX_RATIO) + left_eye_center.y,
  };
  
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
  
  if(getEyebrows()) {
    // Draw the eyebrows
    int16_t angryness = getAngryness();
    int16_t charge_diff = (angryness == 11) ? ((charge_percentage /5) -10) : (angryness*2-10);
    int16_t bluetooth_diff_x = (bluetooth_connected||!getBluetooth())? 0:eye_radius;
    int16_t bluetooth_diff_y = (bluetooth_connected||!getBluetooth())? 0:charge_diff/2;
    GPoint left = {
      .x = (int16_t) left_eye_center.x-eye_radius+bluetooth_diff_x,
      .y = (int16_t) left_eye_center.y-eye_radius-20-charge_diff+bluetooth_diff_y,
    };
    
    GPoint right = {
      .x = (int16_t) left_eye_center.x+eye_radius,
      .y = (int16_t) left_eye_center.y-eye_radius-20+charge_diff,
    };
    
    graphics_draw_line(ctx,left,right);
#ifndef PBL_COLOR
    left.y--;right.y--;
    graphics_draw_line(ctx,left,right);
#endif
    left.x = (int16_t) right_eye_center.x-eye_radius;
    left.y = (int16_t) right_eye_center.y-eye_radius-20+charge_diff;
    right.x = (int16_t) right_eye_center.x+eye_radius-bluetooth_diff_x;
    right.y = (int16_t) right_eye_center.y-eye_radius-20-charge_diff+bluetooth_diff_y,
    
    graphics_draw_line(ctx,left,right);
#ifndef PBL_COLOR
    left.y--;right.y--;
    graphics_draw_line(ctx,left,right);
#endif
  }
  
  if(getMouth()) 
  {
    // Draw the mouth (with weekdays)
    GRect mouth;
    mouth.size.w = (3 * eye_radius + eye_distance)/7;
    mouth.size.w*=7;
    mouth.size.h = 20;
    mouth.origin.x = (bounds.size.w - mouth.size.w)/2;
    mouth.origin.y = left_eye_center.y + eye_radius + 15;
    graphics_context_set_fill_color(ctx,GColorWhite);
    graphics_context_set_stroke_color(ctx,GColorBlack);

    // Draw the teeth
    GRect tooth;
    tooth.size.w = mouth.size.w / 7 - 2;
#ifdef PBL_COLOR
    tooth.size.h = mouth.size.h-1;
    tooth.origin.y = mouth.origin.y;
#else
    tooth.size.h = mouth.size.h-2;
    tooth.origin.y = mouth.origin.y+1;
#endif
    // checkout with which weekday the week starts
    int curr_weekday = t->tm_wday;
    if(getMonday()) {
      curr_weekday--;
      if(curr_weekday<0)
        curr_weekday = 6;
    }
    for(int8_t weekday = 0; weekday < 7; weekday++) {
      tooth.origin.x = mouth.origin.x+1 + weekday * (tooth.size.w+2);
      if (weekday == curr_weekday) {
        graphics_context_set_fill_color(ctx,GColorBlack);
#ifdef PBL_COLOR
        if((bluetooth_connected||!getBluetooth()))
          graphics_context_set_fill_color(ctx,GColorBlack);
        else
          graphics_context_set_fill_color(ctx,GColorBlue);
#endif
        graphics_fill_rect(ctx,tooth,4,GCornersBottom);
      } else {
        graphics_context_set_fill_color(ctx,GColorWhite);
        graphics_fill_rect(ctx,tooth,4,GCornersBottom);
#ifdef PBL_COLOR
        //graphics_draw_round_rect(ctx,tooth,4);
#endif
      }
    }
#ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx,GColorBlack);
    graphics_context_set_stroke_width(ctx,2);
#else
    graphics_context_set_stroke_color(ctx,GColorWhite);
#endif
    graphics_draw_round_rect(ctx,mouth,4);

  }
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_color();
  if(getBlinking() && !blink_timer)
    blink_timer = app_timer_register(300,blink_down_callback,NULL);
  layer_mark_dirty(hands_layer);
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
  autoconfig_init();
  app_message_register_inbox_received(in_received_handler);
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  
  
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);
  
}

static void deinit(void) {
  window_destroy(window);
  autoconfig_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

