#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "itoa.h"

#define MY_UUID { 0xF3, 0xEF, 0xB8, 0x62, 0xFE, 0xE8, 0x47, 0x41, 0x91, 0x42, 0x9E, 0xAB, 0xDF, 0x85, 0x73, 0x98 }
PBL_APP_INFO(HTTP_UUID,
             "Zulu Time", "MikeBikeMusic",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_WATCH_FACE);

typedef struct {
	const char * name;
	int offset;
	Layer layer;
} timezone_t;


// Local timezone GMT offset
#define GMT 0
#define EDT -4
#define CDT -5
#define MDT -6
#define MST -7
#define PDT -7
#define PST -8
#define AKDT -8
#define HST -10
static int gmt_offset = GMT * 60;

#define NUM_TIMEZONES 2
#define PEBBLE_SCREEN_HEIGHT 168
#define PEBBLE_SCREEN_WIDTH 144
#define LAYER_HEIGHT (PEBBLE_SCREEN_HEIGHT / NUM_TIMEZONES)

static timezone_t timezones[NUM_TIMEZONES] =
{
	{ .name = "http wait", .offset = 0 },
	{ .name = "local time", .offset = 0 },
};

const char *tzNames[] =
{
	"GMT-12",
	"GMT-11.5",
	"GMT-11",
	"GMT-10.5",
	"GMT-10",
	"GMT-9.5",
	"GMT-9",
	"GMT-8.5",
	"GMT-8",
	"GMT-7.5",
	"GMT-7",
	"GMT-6.5",
	"GMT-6",
	"GMT-5.5",
	"GMT-5",
	"GMT-4.5",
	"GMT-4",
	"GMT-3.5",
	"GMT-3",
	"GMT-2.5",
	"GMT-2",
	"GMT-1.5",
	"GMT-1",
	"GMT-.5",
	"GMT",
	"GMT+.5",
	"GMT+1",
	"GMT+1.5",
	"GMT+2",
	"GMT+2.5",
	"GMT+3",
	"GMT+3.5",
	"GMT+4",
	"GMT+4.5",
	"GMT+5",
	"GMT+5.5",
	"GMT+6",
	"GMT+6.5",
	"GMT+7",
	"GMT+7.5",
	"GMT+8",
	"GMT+8.5",
	"GMT+9",
	"GMT+9.5",
	"GMT+10",
	"GMT+10.5",
	"GMT+11",
	"GMT+11.5",
	"GMT+12",
};

static Window window;
static PblTm now;
static GFont font_thin;
static GFont font_thick;

static const timezone_t * containerOf(Layer *me)
{
	for (int i = 0 ; i < NUM_TIMEZONES ; i++) {
           if (me == &timezones[i].layer)
				return &timezones[i];
	}
	return &timezones[0];
}

void failed(int32_t req_id, int http_status, void* context) {
	static char fail[] = "http fail";
	timezones[0].name = fail;
}

void have_time(int32_t utc_offset_seconds, bool is_dst, uint32_t unixtime, const char* tz_name, void* context) {
	timezones[0].name = tzNames[(utc_offset_seconds / 1800) + 24];
	gmt_offset = utc_offset_seconds / 60;
	timezones[0].offset = gmt_offset;
	static char zulu[] = "Zulu";
	timezones[1].name = zulu;

	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
		layer_mark_dirty(&timezones[i].layer);
}

static void
timezone_layer_update(
	Layer * const me,
	GContext * ctx
)
{
	const timezone_t * const tz = containerOf(me);
	
	const int orig_hour = now.tm_hour;
	const int orig_min = now.tm_min;
	
	char plusOrMinus[2];
	plusOrMinus[0] = ' ';
	plusOrMinus[1] = 0;
	
	now.tm_min += (tz->offset - gmt_offset) % 60;
	if (now.tm_min > 60) {
		now.tm_hour++;
		now.tm_min -= 60;
	} else if (now.tm_min < 0) {
		now.tm_hour--;
		now.tm_min += 60;
	}
	
	now.tm_hour += (tz->offset - gmt_offset) / 60;
	if (now.tm_hour >= 24) {
		now.tm_hour -= 24;
		plusOrMinus[0] = '+';
	}
	if (now.tm_hour < 0) {
		now.tm_hour += 24;
		plusOrMinus[0] = '-';
	}
	
	char buf[32];
	string_format_time( buf, sizeof(buf), "%H:%M", &now
	);
	
	const int night_time = (now.tm_hour > 18 || now.tm_hour < 6);
	now.tm_hour = orig_hour;
	now.tm_min = orig_min;
	
	const int w = me->bounds.size.w;
	const int h = me->bounds.size.h;
	
	// it is night there, draw in black video
	graphics_context_set_fill_color(ctx, night_time ? GColorBlack : GColorWhite);
	graphics_context_set_text_color(ctx, !night_time ? GColorBlack : GColorWhite);
	graphics_fill_rect(ctx, GRect(0, 0, w, h), 0, 0);
	
	graphics_text_draw(ctx,
					   tz->name,
					   font_thin,
					   GRect(0, 0, w, h/2),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentCenter,
					   NULL
					  );
	graphics_text_draw(ctx,
					   plusOrMinus,
					   font_thin,
					   GRect(0, 0, w, h/2),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentRight,
					   NULL
					  );
	graphics_text_draw(ctx,
					   buf,
					   font_thick,
					   GRect(0, h/3, w, 2*h/3),
					   GTextOverflowModeTrailingEllipsis,
					   GTextAlignmentCenter,
					   NULL
					  );
}


/** Called once per minute */
static void handle_tick(AppContextRef ctx, PebbleTickEvent * const event)
{
	(void) ctx;

	now = *event->tick_time;

	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
		layer_mark_dirty(&timezones[i].layer);
}


static void handle_init(AppContextRef ctx)
{
	(void) ctx;
	get_time(&now);

	window_init(&window, "Main");
	window_stack_push(&window, true);
	window_set_background_color(&window, GColorBlack);

	resource_init_current_app(&RESOURCES);

	font_thin = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_30));
	font_thick = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49));

	for (int i = 0 ; i < NUM_TIMEZONES ; i++)
	{
		timezone_t * const tz = &timezones[i];
		layer_init(
			&tz->layer,
			GRect(0, i * LAYER_HEIGHT, PEBBLE_SCREEN_WIDTH, LAYER_HEIGHT)
		);
		
		tz->layer.update_proc = timezone_layer_update;
		layer_add_child(&window.layer, &tz->layer);
		layer_mark_dirty(&tz->layer);
	}
	http_set_app_id(9150941);
	http_register_callbacks((HTTPCallbacks){.time=have_time, .failure=failed,}, NULL);
	http_time_request();
}


static void handle_deinit(AppContextRef ctx)
{
	(void) ctx;
	
	fonts_unload_custom_font(font_thin);
	fonts_unload_custom_font(font_thick);
}


void pbl_main(void * const params)
{
	PebbleAppHandlers handlers = {
		.init_handler   = &handle_init,
		.deinit_handler = &handle_deinit,
		.tick_info      = {
			.tick_handler = &handle_tick,
			.tick_units = MINUTE_UNIT,
		},
			.messaging_info = {
			.buffer_sizes = {
				.inbound = 256,
				.outbound = 256,
			}
		},
	};
	
	app_event_loop(params, &handlers);
}
