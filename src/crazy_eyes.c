#include <pebble.h>

static Layer *hands_layer;
static Window *window;

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  const int16_t eye_radius = 32;
  const int16_t pupil_radius = 8;
  const int16_t eye_distance = 6;
  const int16_t pupil_center_dist = eye_radius - pupil_radius - 4;
  
  GPoint left_eye_center = center;
  GPoint right_eye_center = center;
  
  left_eye_center.x-=eye_radius+eye_distance;
  right_eye_center.x+=eye_radius+eye_distance;
  
  if (watch_info_get_model() == WATCH_INFO_MODEL_PEBBLE_STEEL) {
    left_eye_center.y+=12;
    right_eye_center.y+=12;
  }
  
  // draw the eye circles
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
#else
  graphics_context_set_fill_color(ctx, GColorBlack);
#endif
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
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
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(hands_layer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // init hands
  hands_layer = layer_create(bounds);
  layer_set_update_proc(hands_layer, hands_update_proc);
  layer_add_child(window_layer, hands_layer);
  
  // force update
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_tick(t, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
}

static void window_unload(Window *window) {
  layer_destroy(hands_layer);
  tick_timer_service_unsubscribe();
}

static void init(void) {
  
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
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

