#include <pebble.h>

#define STORAGE_KEY 42

static Window* g_window, * notification_window;
static TextLayer *g_time_layer;
static TextLayer *g_counter_layer;
static GBitmap *g_disconnect;
static BitmapLayer *g_bitmap_layer;

static WakeupId s_wakeup_id; 

static int wakeupCount;

// Key values for AppMessage Dictionary
enum {
	ID_KEY = 0,	
	TIMESTAMP_KEY = 1,
  TITLE_KEY = 2,
  MESSAGE_KEY = 3,
  EVENT_KEY = 4
};

static TextLayer *notification_title_text_layer;
static TextLayer *notification_message_text_layer;
static ScrollLayer *notification_scroll_layer;

struct NOTIFICATION {
  char message[175];
  char title[50];
  char identifier[25];
};

void printNotification(struct NOTIFICATION *newNotification) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "+----------------------------------------------------------+");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Size of Notification: %d", (int)sizeof(*newNotification));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message: %s", newNotification -> message);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Title: %s", newNotification -> title);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Id: %s", (newNotification -> identifier));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "+----------------------------------------------------------+");
}

static void update_counter(){
  static char buffer[] = "0 pings";

  // Write the current hours and minutes into the buffer
  snprintf(buffer, sizeof (buffer), "%d pings", wakeupCount);

  // Display this time on the TextLayer
  text_layer_set_text(g_counter_layer, buffer);
}

static void increase_counter(){
  wakeupCount++;
  update_counter();
}
static void decrease_counter(){
  wakeupCount--;
  update_counter();
}

static void resize_scroll_area(Window *window){
  GRect bounds = layer_get_frame(window_get_root_layer(window));
  GSize message_size = text_layer_get_content_size(notification_message_text_layer);
  GSize title_size = text_layer_get_content_size(notification_title_text_layer);
  
  //update position of message text layer
  Layer *messageTextLayer_layer = text_layer_get_layer(notification_message_text_layer);
  layer_set_frame(messageTextLayer_layer, GRect(5, title_size.h + 5, 144, 2000));
  layer_mark_dirty(messageTextLayer_layer);
  
  //adjust for height of both text layers
  title_size.h += 5;
  message_size.h += (title_size.h);
  title_size.w = bounds.size.w;
  message_size.w = bounds.size.w;

  //resize the text layers to the size of their contents
  text_layer_set_size(notification_message_text_layer, message_size);
  text_layer_set_size(notification_title_text_layer, title_size);
  
  //set size of the scroll layer
  scroll_layer_set_content_size(notification_scroll_layer, GSize(bounds.size.w, message_size.h + 5));
};

static bool check_notification_existence(time_t time, char * mongoId) {
  static struct NOTIFICATION readNotification;
  
  persist_read_data(time, &readNotification, sizeof(readNotification));
  if (!strcmp(readNotification.identifier, mongoId)) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Mongo Ids are the same!!!!!");
    return true;
  } else {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "To be inserted ID: %s", (mongoId));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Current Id in place: %s", (readNotification.identifier));

      APP_LOG(APP_LOG_LEVEL_DEBUG, "Mongo Ids are different!!!!");
    return false;
  }  
};
static const uint32_t segments[] = { 400, 100, 100, 100, 100, 100, 400 };
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};

//handler for wakeup api --> shows notification
static void wakeup_handler(WakeupId id, int32_t reason) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Wakeup Handler reason: %d",(int)reason);
  
  static struct NOTIFICATION readNotification;
//  persist_read_data((int)id, &readNotification, sizeof(readNotification));
  persist_read_data(reason, &readNotification, sizeof(readNotification));

  
  //push notification window and populate it
  window_stack_push(notification_window, true);
  
  //set title layer
  text_layer_set_text(notification_title_text_layer, readNotification.title);
  
  //set message layer text
  text_layer_set_text(notification_message_text_layer, readNotification.message);
  
  //resize scroll layer based on content
  resize_scroll_area(notification_window);
  
  vibes_enqueue_custom_pattern(pat);
  
  printNotification(&readNotification);
  
  //Assume coorect data is cleared properly  somhow
  persist_delete(reason);
  
  decrease_counter();
}

// Called when a message is received from PebbleKitJS
static void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *tuple;

  struct NOTIFICATION newNotification;
  
  tuple = dict_find(received, TIMESTAMP_KEY);
  time_t myTime = tuple->value->uint32;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Wakeup id: %d", (int)s_wakeup_id); 
  
  tuple = dict_find(received, MESSAGE_KEY);
	if(tuple) {
    strcpy(newNotification.message, tuple->value->cstring);
	}
  
  tuple = dict_find(received, ID_KEY);
	if(tuple) {
      strcpy((newNotification.identifier), tuple->value->cstring);
	}
  
  tuple = dict_find(received, TITLE_KEY);
	if(tuple) {
    strcpy(newNotification.title, tuple->value->cstring);
	}
  
  
  do{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting wakeup with Timestamp: %ld", myTime); 
    s_wakeup_id = wakeup_schedule(myTime, myTime, true); //(time, reason, notify_if_missed)
    if (s_wakeup_id == -4){
      //manage past timestamps
    APP_LOG(APP_LOG_LEVEL_DEBUG, "past timeframe"); 
      myTime = time(NULL) + 5;
    }else if (s_wakeup_id == -8){
          APP_LOG(APP_LOG_LEVEL_DEBUG, "time taken");
      
      if ((check_notification_existence(myTime, newNotification.identifier))) {
        // this notification with same mongoId currently exists
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Repeat Notification!!!!!"); 
        return;
      }
      // different notification. Check if free in two minutes
            
      //manage timestamps that have already been used
      myTime += 120;
    }else if (s_wakeup_id < 0){
          APP_LOG(APP_LOG_LEVEL_DEBUG, "ERROR making wake up"); 
          return;
    }
    //todo handle systems errors
  }while ((int)s_wakeup_id < 0);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Recieved Timestamp: %ld %ld", myTime, time(NULL)); 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Wakeup time relative to Now: %ld", (myTime - time(NULL))); 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Wakeup Id %d size of %d and mytime is %d", (int)s_wakeup_id, sizeof(s_wakeup_id), (int)myTime); 
	

  printNotification(&newNotification);
  
  persist_write_data(myTime, &newNotification, sizeof(newNotification));
  
  increase_counter();
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, EVENT_KEY, "notificationAck");
  dict_write_cstring(iter, ID_KEY, newNotification.identifier);
  app_message_outbox_send();
}


// Called when an incoming message from PebbleKitJS is dropped
static void in_dropped_handler(AppMessageResult reason, void *context) {	
}

// Called when PebbleKitJS does not acknowledge receipt of a message
static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
}

////////////////////////////////////////////////////////////////////////////
//main window set up function
////////////////////////////////////////////////////////////////////////////
//function to update da time
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    //Use 2h hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    //Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(g_time_layer, buffer);
}

//handler for ticks
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

// handler for bluetooth status change
void bt_handler(bool connected) {
  //hide when connected
  layer_set_hidden(bitmap_layer_get_layer(g_bitmap_layer), connected);
}


//disable back button for exits
static void main_button_back_handler(ClickRecognizerRef recognizer, void *context) {
}

//add button handlers to notification window
static void main_click_config_provider(void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_BACK, main_button_back_handler);
}

//load function for main window with time
void window_load(Window *window)
{  
  Layer *window_layer = window_get_root_layer(window); 
  
  g_time_layer = text_layer_create(GRect(5, 52, 139, 50));
  text_layer_set_background_color(g_time_layer, GColorClear);
  text_layer_set_text_color(g_time_layer, GColorBlack);
  text_layer_set_text(g_time_layer, "00:00");
  
  text_layer_set_font(g_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(g_time_layer, GTextAlignmentCenter);
 
  layer_add_child(window_layer, text_layer_get_layer(g_time_layer));
  
  //add counter text layer
  g_counter_layer = text_layer_create(GRect(5, 120, 50, 50));
  text_layer_set_background_color(g_counter_layer, GColorClear);
  text_layer_set_text_color(g_counter_layer, GColorBlack);
  text_layer_set_text(g_counter_layer, "0");
  
  text_layer_set_font(g_counter_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(g_counter_layer, GTextAlignmentCenter);
 
  layer_add_child(window_layer, text_layer_get_layer(g_counter_layer));
  
  //add image for phone disconnect
  g_disconnect = gbitmap_create_with_resource(RESOURCE_ID_PHONE_DISCONNECT);

  g_bitmap_layer = bitmap_layer_create(GRect(100, 120, 26, 30));
  bitmap_layer_set_bitmap(g_bitmap_layer, g_disconnect);
  #if defined(PBL_BW)
  bitmap_layer_set_compositing_mode(g_bitmap_layer, GCompOpAssignInverted);
  #elif defined(PBL_COLOR)
  bitmap_layer_set_compositing_mode(g_bitmap_layer, GCompOpSet);
  #endif
    
  layer_add_child(window_layer, bitmap_layer_get_layer(g_bitmap_layer));

  // Show current connection state
  #ifdef PBL_SDK_2
    bt_handler(bluetooth_connection_service_peek());
  #elif PBL_SDK_3
    bt_handler(connection_service_peek_pebble_app_connection());
  #endif
  
  update_time(); 
  update_counter();
  
  window_set_click_config_provider(window, main_click_config_provider);
}
 
//main window unload function
void window_unload(Window *window)
{
  //We will safely destroy the Window's elements here!
  text_layer_destroy(g_time_layer);
  bitmap_layer_destroy(g_bitmap_layer);
  gbitmap_destroy(g_disconnect);
}

////////////////////////////////////////////////////////////////////////////
//notification window set up functions
////////////////////////////////////////////////////////////////////////////
//function to pop notification window and ack message
static void button_exit_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(true); 
  update_time();
}

//add button handlers to notification window
static void notification_click_config_provider(void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_BACK, button_exit_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, button_exit_handler);
}

void notification_load(Window *window)
{  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  // Initialize the scroll layer
  notification_scroll_layer = scroll_layer_create(bounds);

  // This binds the scroll layer to the window so that up and down map to scrolling
  // You may use scroll_layer_set_callbacks to add or override interactivity
  scroll_layer_set_click_config_onto_window(notification_scroll_layer, window);

  // Initialize the title layers
  notification_title_text_layer = text_layer_create(GRect(5, 0, 144, 200));    
  text_layer_set_text(notification_title_text_layer, "Title");
  text_layer_set_font(notification_title_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  
  //add title to scroll layer
  scroll_layer_add_child(notification_scroll_layer, text_layer_get_layer(notification_title_text_layer));
  
  // Initiailize the message layer
  //top should be based on size of title layer
  notification_message_text_layer = text_layer_create(GRect(5, 75, 144, 2000));
  text_layer_set_text(notification_message_text_layer, "message");
  text_layer_set_font(notification_message_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  
  //add message layer to scroll layer
  scroll_layer_add_child(notification_scroll_layer, text_layer_get_layer(notification_message_text_layer));

  // Trim text layer and scroll content to fit text box
  resize_scroll_area(window);

  //register button set-up handler
//   window_set_click_config_provider(window_layer, notification_click_config_provider);
  
  //add scroll layer to window
  layer_add_child(window_layer, scroll_layer_get_layer(notification_scroll_layer));  
  
//   ScrollLayerCallbacks cbacks;
//   cbacks.click_config_provider = notification_click_config_provider;
//   scroll_layer_set_callbacks(notification_scroll_layer, cbacks);
  
    scroll_layer_set_callbacks(notification_scroll_layer, (ScrollLayerCallbacks){.click_config_provider=notification_click_config_provider});

}
 
void notification_unload(Window *window)
{
  //We will safely destroy the Window's elements here!
  text_layer_destroy(notification_title_text_layer);
  text_layer_destroy(notification_message_text_layer);
  scroll_layer_destroy(notification_scroll_layer);
}

void init(void) {
  wakeupCount = persist_exists(STORAGE_KEY) ? persist_read_int(STORAGE_KEY) : 0;
  //create main time window
  g_window = window_create();
  window_set_window_handlers(g_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
 
  window_stack_push(g_window, true);
  
  //create notification display window
  notification_window = window_create();
  window_set_window_handlers(notification_window, (WindowHandlers) {
    .load = notification_load,
    .unload = notification_unload,
  });
  
	// Register AppMessage handlers
	app_message_register_inbox_received(in_received_handler); 
	app_message_register_inbox_dropped(in_dropped_handler); 
	app_message_register_outbox_failed(out_failed_handler);
		
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  bluetooth_connection_service_subscribe(bt_handler);

  wakeup_service_subscribe(wakeup_handler);

  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // The app was started by a wakeup
    WakeupId id = 0;
    int32_t reason = 0;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Wakeup has been called");

    
    // Get details and handle the wakeup
    wakeup_get_launch_event(&id, &reason);
    wakeup_handler(id, reason);
  }
}

void deinit(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "count %d", wakeupCount);
  persist_write_int(STORAGE_KEY, wakeupCount);
	app_message_deregister_callbacks();
  window_destroy(g_window);
}

int main( void ) {
	init();
	app_event_loop();
	deinit();
}