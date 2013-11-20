#include "pebble.h"

typedef struct {
	const char *name;
	char plusOrMinus[2];
	struct tm now;
	Layer *layer;
} timezone_t;


#define NUM_TIMEZONES 2
#define PEBBLE_SCREEN_HEIGHT 168
#define PEBBLE_SCREEN_WIDTH 144
#define LAYER_HEIGHT (PEBBLE_SCREEN_HEIGHT / NUM_TIMEZONES)
#define MY_TICK_UNITS MINUTE_UNIT

static char tzNames[8];

static timezone_t timezones[NUM_TIMEZONES] =
{
	{ .name = tzNames, .plusOrMinus = " "},
	{ .name = "Zulu"},
};


static Window *window;
static GFont font_thin;
static GFont font_thick;
static int tzOffset = 0;
static int tzOffsetHours = 0;
static int tzOffsetMinutes = 0;

#define INT_DIGITS 12		/* enough for 32 bit integer */

char *itoa(int i)
{
  /* Room for INT_DIGITS digits, - and '\0' */
  static char buf[INT_DIGITS + 2];
  char *p = buf + INT_DIGITS + 1;	/* points to terminating '\0' */
  if (i >= 0) {
    do {
      *--p = '0' + (i % 10);
      i /= 10;
    } while (i != 0);
    return p;
  }
  else {			/* i < 0 */
    do {
      *--p = '0' - (i % 10);
      i /= 10;
    } while (i != 0);
    *--p = '-';
  }
  return p;
}

static const timezone_t * containerOf(Layer *me)
{
	for (int i = 0 ; i < NUM_TIMEZONES ; i++) {
           if (me == timezones[i].layer)
				return &timezones[i];
	}
	return &timezones[0];
}

static void timezone_layer_update( Layer * const me, GContext *ctx)
{
	const timezone_t * const tz = containerOf(me);
	
	char buf[32];
	if (MY_TICK_UNITS == SECOND_UNIT)
		strftime( buf, sizeof(buf), "%T", &tz->now);
	else
		strftime( buf, sizeof(buf), "%H:%M", &tz->now);
	
	const int night_time = (tz->now.tm_hour > 18 || tz->now.tm_hour < 6);

	GRect rect = layer_get_bounds(tz->layer);
	const int w = rect.size.w;
	const int h = rect.size.h;
	
	// it is night there, draw in black video
	graphics_context_set_fill_color(ctx, night_time ? GColorBlack : GColorWhite);
	graphics_context_set_text_color(ctx, !night_time ? GColorBlack : GColorWhite);
	graphics_fill_rect(ctx, GRect(0, 0, w, h), 0, 0);
	
	graphics_draw_text(ctx,
					   tz->name,
					   font_thin,
					   GRect(0, 0, w, h/2),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentCenter,
					   NULL
					  );
	graphics_draw_text(ctx,
					   tz->plusOrMinus,
					   font_thin,
					   GRect(0, 0, w, h/2),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentRight,
					   NULL
					  );
	graphics_draw_text(ctx,
					   buf,
					   font_thick,
					   GRect(0, h/3, w, 2*h/3),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentCenter,
					   NULL
					  );
}


/** Called once per minute */
static void handle_tick(struct tm* tick_time, TimeUnits units_changed)
{

  	time_t now = time(NULL);
 	timezones[1].now = *gmtime(&now);
	timezones[1].plusOrMinus[0] = ' ';
	timezones[1].plusOrMinus[1] = 0;
	timezones[1].now.tm_min += tzOffsetMinutes;
	if (timezones[1].now.tm_min > 60) {
		timezones[1].now.tm_hour++;
		timezones[1].now.tm_min -= 60;
	} else if (timezones[1].now.tm_min < 0) {
		timezones[1].now.tm_hour--;
		timezones[1].now.tm_min += 60;
	}
	timezones[1].now.tm_hour += tzOffsetHours;
	if (timezones[1].now.tm_hour >= 24) {
		timezones[1].now.tm_hour -= 24;
		timezones[1].plusOrMinus[0] = '+';
	}
	if (timezones[1].now.tm_hour < 0) {
		timezones[1].now.tm_hour += 24;
		timezones[1].plusOrMinus[0] = '-';
	}
	timezones[0].now = *tick_time;
	snprintf(tzNames, sizeof(tzNames), "GMT+%d", -tzOffset / 60);

	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
		layer_mark_dirty(timezones[i].layer);
}

	enum {
		AKEY_TZOFFSET
	};

static void in_received_handler(DictionaryIterator *iter, void *context) {
	Tuple *int_tuple = dict_find(iter, AKEY_TZOFFSET);
	if (int_tuple) {
		tzOffset = int_tuple->value->int32;
		tzOffsetHours = tzOffset / 60;
		tzOffsetMinutes = tzOffset % 60;
		time_t now = time(NULL);
		struct tm *current_time = localtime(&now);
		handle_tick(current_time, MY_TICK_UNITS);
	}
}

#define ConstantGRect(x, y, w, h) {{(x), (y)}, {(w), (h)}}

void init()
{
	const bool animated = true;
	window = window_create();
	window_stack_push(window, animated);
	window_set_background_color(window, GColorBlack);

	font_thin = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_30));
	if (MY_TICK_UNITS == SECOND_UNIT)
		font_thick = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_30));
	else
		font_thick = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49));

	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
	{
		timezone_t * const tz = &timezones[i];
		GRect rect = ConstantGRect(0, i * LAYER_HEIGHT, PEBBLE_SCREEN_WIDTH, LAYER_HEIGHT);
		tz->layer = layer_create(rect);
		layer_set_update_proc(tz->layer, timezone_layer_update);
  		layer_add_child(window_get_root_layer(window), tz->layer);
		layer_mark_dirty(tz->layer);
	}
	tick_timer_service_subscribe(MY_TICK_UNITS, &handle_tick);
	app_message_register_inbox_received(in_received_handler);
	const uint32_t inbound_size = 64;
	const uint32_t outbound_size = 64;
	app_message_open(inbound_size, outbound_size);
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	handle_tick(current_time, MY_TICK_UNITS);
}

void deinit()
{
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
	{
		timezone_t * const tz = &timezones[i];
		layer_destroy(tz->layer);
	}
	fonts_unload_custom_font(font_thin);
	fonts_unload_custom_font(font_thick);
	window_destroy(window);
}

int main(void)
{
	init();
	app_event_loop();
  	deinit();
}
