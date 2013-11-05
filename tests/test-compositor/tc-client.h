#ifndef __TC_CLIENT_H__
#define __TC_CLIENT_H__

#include "tc-events.h"

/*
 * Allow saving object along with some helper data and object's listener
 */
struct client_object {
	struct wl_proxy *proxy;
	const void *listener;

	void *data;
	void (*data_destr)(void *); /* data destructor */

	/* here we can save last event catched for the object */
	struct event last_event;
};

/**
 * Data (which can be) used by client.
 *
 * This structure can be filled using function client_populate(). In that
 * case default listeners (can be found in wit-client-protocol.c) are used.
 * However, this structure can be created and handled manually with user's
 * listeners.
 * NOTE: some functions rely on data in this structure. When client_populate()
 * is not used, use at least client_init() which fills in the minimum data needed.
 */
struct client {
	struct wl_display *display;

	struct client_object registry;
	struct client_object compositor;
	struct client_object seat;
	struct client_object pointer;
	struct client_object keyboard;
	struct client_object touch;
	struct client_object shm;

	int sock;

	/* here we can store events if we need (no need to pass more arguments
	 * or create global variables */
	struct eventarray *events;

	/* set value 1 here, when client asked for emitting events */
	int emitting;

	/* data for user's arbitrary use */
	void *data;
};

/**
 * Fill in struct client minimum data needed for
 * another functions (i.e. socket and display).
 *
 * NOTE: This function must not be used altogether with client_populate().
 *
 * Usual usage is:
 *
 * struct client c;
 * client_init(&c, sock);
 *
 * c.registry.proxy = wl_display_get_registry(c.display);
 * client_add_listener(&c, "wl_registry", registry_listener);
 *
 * ... another user's listeners
 * ...
 *
 * @param c    pointer to client's struct
 * @param s    socket passed to client's main function
 * @return nothing (aborts on error)
 */
void
client_init(struct client *c, int s);

/**
 * Create and populate structure client.
 *
 * Default listeners are used and this function blocks until display
 * creates all objects.
 *
 * Usual usage is:
 *
 * struct client *c = client_populate(sock);
 * // now we have all objects (display, registry, ...) filled
 * struct wl_surface *surf =
 * 			wl_compositor_create_surface(c->compositor.proxy);
 * ...
 *
 * @param sock  socket passed to client's main function
 * @return      filled struct client
 */
struct client *
client_populate(int sock);

/**
 * Free all allocated memory and destroy proxies.
 * Does a roundtrip before and check for errors before freeing
 *
 * @param c   client's struct
 */
void
client_free(struct client *c);

/**
 * Ask display to run predefined function
 *
 * The function must be defined on display side and added using
 * display_add_user_func() function.
 * Display must call display_run_user_func() to process this request.
 *
 * Usual usage is:
 *
 * // DISPLAY SIDE -----------------------
 * void user_function(void *data)
 * {
 * 	.. do something
 * }
 *
 * TEST(test_name)
 * {
 * 	... init etc. ..
 *
 * 	display_add_user_func(display, user_function, func_data);
 * 	display_run_user_func(display); // wait for client to ask for func
 * }
 *
 * // CLIENT SIDE -----------------------
 * int client_main(int sock)
 * {
 *	... init etc ...
 *
 *	client_call_user_func(client);
 *
 *	return EXIT_SUCCESS;
 * }
 *
 * @param cl    struct client
 */
void
client_call_user_func(struct client *cl);

/**
 * Ask display to emit events
 *
 * Events must be stored in display's events field. There are more ways how
 * to achieve this:
 * 1) Events can be created on display side and stored there using
 *    display_add_events() (or manualy assigned to display->events)
 * 2) Events can be created on client side and send to display using
 *    client_send_eventarray() (display must call
 *    display_recieve_eventarray() accordingly). In this case events are
 *    assigned into display->events automatically.
 *
 * Display must call display_emit_events() to process this request.
 *
 * @param cl    pointer to the client's struct
 * @param n     how many events emit (0 means all)
 * @return      number of events that display emitted
 *
 * NOTE: this function doesn't block until the events are actually emitted.
 * It only tells display to emit and ends.
 */
int
client_ask_for_events(struct client *cl, int n);

/**
 * Send bytestream to the display.
 *
 * Display must call display_recieve_data() to process this request.
 * Data are stored into data filed of struct display (can overwrite
 * user data, if he stored any)
 *
 * @param cl    client's struct
 * @param src   pointer to the data to be send
 * @param size  size of data
 */
void
client_send_data(struct client *cl, void *src, size_t size);

/**
 * Send eventarray to the display
 *
 * Display must call display_recieve_eventarray() to accept this request.
 * Eventarray is stored into display.events field (thus can overwrite events
 * stored there before by user)
 *
 * @param cl    client structure
 * @param ea    eventarray
 */
void
client_send_eventarray(struct client *cl, struct eventarray *ea);

/**
 * Ask display to emit single event
 *
 * Display must call display_emit_event()
 *
 * Usual usage is:
 * == CLIENT ==
 * ...
 *
 * WIT_EVENT_DEFINE(e, &wl_touch_interface, WL_TOUCH_FRAME);
 * client_trigger_event(client, e);
 * ...
 *
 * == DISPLAY ==
 * ...
 *
 * display_run(display);
 * display_emit_event(display); // accept request
 * ...
 *
 * @param cl    client
 * @param e     event to be emited
 * @param ...   arguments of the event
 */
void
client_trigger_event(struct client *cl, const struct event *e, ...);

/**
 * Sync client with display
 *
 * Display must call display_barrier() to accept this request. This request
 * (like any other request) stops display and lets it process the code before
 * the request. After the barrier is done on both sides, both display and client
 * continue from line behind barrier at the same time.
 *
 * Usual usage is:
 * == CLIENT ==
 * ...
 * client_barrier(client);
 * client_trigger_event(client, e, arg);
 * ...
 *
 * == DISPLAY ==
 * ...
 * display_run(display);
 *
 * // display_run() blocks, following code will take effect after
 * // client calls client_barrier() (or any other request)
 * wl_display_create_resource(...); // create resource for upcoming event
 * display_barrier(); // sync with client and run wl_display againg
 * ...
 */
void
client_barrier(struct client *cl);

/**
 * Add listener to client's object
 *
 * This is a way how to conviniently add listeners to objects in client
 * structure. Since client structure is public nothing keep us from doing
 * it manually. However, we can use this function.
 *
 * @param cl         client
 * @param interface  string representing object (e.g. "wl_registry", "wl_pointer", etc.)
 * @param listener   pointer to the listener
 *
 * NOTE: this function is not defined for all interfaces at the moment.
 * It aborts when unsupported interface is given.
 */
void
client_add_listener(struct client *cl, const char *interface,
			const void *listener);

/**
 * Print debug information about client state
 *
 * Print what objects, proxies, listeners etc. are present in client structure
 *
 * @param cl    client structure
 */
void
client_state(struct client *cl);

#endif /* __TC_CLIENT_H__ */
