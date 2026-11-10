/*
JOIN4_PLAYER1.C (test)

The lobby's ADD PLAYER screen (port/linux/game/menu_functions.c's join4):
which controllers its quarters let join, whom a quarter's A brings into the
lobby, and whose B backs out of the screen. Controller 1 (port 1, the
keyboard's and the first gamepad's) normally plays already; when its player
left a game that the machine's other players stayed in, it joins the next
game's lobby through the screen as the other controllers do, except by the
keyboard's SPACE, which is an A of its port. test_join4_player1.py takes
the enum, the screen's state and the code from menu_functions.c
(config.inc, under_test.inc).
*/

#include <wchar.h>
#include "harness.h"

#define MAXIMUM_LOCAL_PLAYERS 4
#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))
#define ustrncpy(to, from, count) wcsncpy(to, from, count)
#define platform_log(...) ((void)0)
#define QUARTER_TAG 7
#define tag_loaded(group, name) QUARTER_TAG

#include "config.inc"

struct widget_instance
{
	struct widget_instance *child, *next;
	long definition_tag_index;
	short local_player_index;
};
struct player_profile { int unused; };

/* the machine's controllers: their players joined (wanting to play) or in
the network game, the gamepads on the ports, their active profiles */
static boolean wants[MAXIMUM_LOCAL_PLAYERS];
static int in_game[MAXIMUM_LOCAL_PLAYERS];
static boolean gamepads[MAXIMUM_LOCAL_PLAYERS];
static long profiles[MAXIMUM_LOCAL_PLAYERS];
/* the profile each controller joined with (ui_widget_port_multiplayer_player) */
static long joined[MAXIMUM_LOCAL_PLAYERS];
/* the event join4_open sent each quarter */
static int dispatched[MAXIMUM_LOCAL_PLAYERS];
static boolean space_held;

static unsigned long system_milliseconds(void) { return 1000; }
static boolean player_ui_local_player_wants_to_play_multiplayer(short controller) { return wants[controller]; }
static void *lobby_local_player(short controller) { return in_game[controller] ? &in_game[controller] : NULL; }
static boolean input_has_gamepad(short controller) { return gamepads[controller]; }
static unsigned char halo_keyboard_join_held(void) { return space_held; }
static void halo_virtual_gamepads_start(void) { }
static long player_ui_get_active_player_profile_index(short controller) { return profiles[controller]; }
static long player_ui_get_player1_last_used_profile_index(void) { return 3; }
static boolean player_profile_get(long index, struct player_profile *profile) { return TRUE; }
static void player_ui_set_active_player_profile(short controller, long index, struct player_profile *profile)
{
	profiles[controller] = index;
}
static boolean ui_widget_port_multiplayer_player(short controller, long index)
{
	joined[controller] = index;
	profiles[controller] = index;
	wants[controller] = TRUE;
	return TRUE;
}
static void ui_widget_port_dispatch_event(struct widget_instance *widget, int event, short controller,
	boolean *deleted)
{
	dispatched[controller] = event;
}

#include "under_test.inc"

/* the screen: its four quarters, controller 1's first */
static struct widget_instance quarter_widgets[MAXIMUM_LOCAL_PLAYERS];
static struct widget_instance screen;

/* two gamepads, on ports 1 and 2; nobody playing */
static void setup(void)
{
	short index;

	memset(&join4, 0, sizeof(join4));
	lobby_join.adding = FALSE;
	lobby_join.controller = NONE;
	for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
	{
		wants[index] = FALSE;
		in_game[index] = 0;
		gamepads[index] = index < 2;
		profiles[index] = NONE;
		joined[index] = NONE;
		dispatched[index] = NONE;
		quarter_widgets[index].definition_tag_index = QUARTER_TAG;
		quarter_widgets[index].local_player_index = index;
		quarter_widgets[index].next = index + 1 < MAXIMUM_LOCAL_PLAYERS ? &quarter_widgets[index + 1] : NULL;
	}
	screen.child = &quarter_widgets[0];
	space_held = FALSE;
}

/* controller 1's player left a game that controller 2's stayed in: the next
game's lobby has controller 2's player, and controller 1's START opened the
screen (lobby_join_start: lobby_join.controller) */
static void player1_left(void)
{
	setup();
	in_game[1] = 1;
	profiles[1] = 2;
	lobby_join.controller = 0;
	lobby_join.adding = TRUE;
}

/* the usual lobby: player 1 in, ADD PLAYER pressed (lobby_add_player) */
static void player1_playing(void)
{
	setup();
	wants[0] = TRUE;
	profiles[0] = 1;
	lobby_join.adding = TRUE;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	CASE("player1-opener")
	{
		player1_left();
		join4_open(&screen);
		CHECK(dispatched[0] == JOIN4_EVENT_OPENER, "controller 1's quarter got event %d, not its spinner",
			dispatched[0]);
		CHECK(dispatched[1] == JOIN4_EVENT_PLAYING, "controller 2's quarter got event %d, not its player's",
			dispatched[1]);
		CHECK(join4.opener == NONE && lobby_join.controller == NONE && lobby_join.adding,
			"the screen's state after opening");
		return 0;
	}
	CASE("player2-opener")
	{
		player1_playing();
		lobby_join.controller = 1;
		join4_open(&screen);
		CHECK(dispatched[0] == JOIN4_EVENT_PLAYING, "controller 1's quarter got event %d, not its player's",
			dispatched[0]);
		CHECK(dispatched[1] == JOIN4_EVENT_OPENER, "controller 2's quarter got event %d, not its spinner",
			dispatched[1]);
		CHECK(dispatched[2] == NONE, "controller 3's quarter got event %d", dispatched[2]);
		return 0;
	}
	CASE("player1-rejoins")
	{
		player1_left();
		join4_open(&screen);
		CHECK(join4_join(0), "controller 1's A did not join");
		CHECK(!join4_join(1), "controller 2, playing, joined again");
		join4_set(0, _join4_choosing);
		CHECK(!join4_done(1), "controller 2's A brought the players in while controller 1 was choosing");
		CHECK(lobby_join.adding, "the screen closed while controller 1 was choosing");
		profiles[0] = 1;
		join4_set(0, _join4_selected);
		CHECK(join4_done(1), "controller 2's A did not bring the players in");
		CHECK(joined[0] == 1, "controller 1 joined with profile %ld, not 1", joined[0]);
		CHECK(joined[1] == NONE, "controller 2, playing, joined again with profile %ld", joined[1]);
		CHECK(!lobby_join.adding && lobby_join.controller == NONE, "the screen's state after done");
		return 0;
	}
	CASE("player1-playing")
	{
		player1_playing();
		join4_open(&screen);
		CHECK(!join4_join(0), "controller 1, playing, joined");
		CHECK(join4_join(1), "controller 2 did not join");
		CHECK(!join4_join(2), "controller 3, with no gamepad, joined");
		CHECK(join4_done(0), "controller 1's A did not bring the players in");
		CHECK(joined[0] == NONE && joined[1] == NONE, "a controller joined: %ld, %ld", joined[0], joined[1]);
		player1_playing();
		join4_open(&screen);
		CHECK(join4_back(0), "controller 1's B did not back out");
		CHECK(!lobby_join.adding, "the screen stayed after controller 1's B");
		return 0;
	}
	CASE("player1-back-steps")
	{
		player1_left();
		join4_open(&screen);
		join4_set(0, _join4_choosing);
		CHECK(!join4_back(0), "controller 1's B backed out of the screen while it was choosing");
		CHECK(lobby_join.adding, "the screen closed while controller 1 was choosing");
		join4_set(0, _join4_selected);
		CHECK(!join4_back(0), "controller 1's B backed out of the screen while it had selected");
		join4_set(0, _join4_away);
		CHECK(!join4_back(1), "controller 2's B backed out of the screen");
		CHECK(join4_back(0), "controller 1's B did not back out once its quarter was left");
		CHECK(!lobby_join.adding, "the screen stayed after controller 1's B");
		return 0;
	}
	CASE("space-not-a-join")
	{
		player1_left();
		join4_open(&screen);
		space_held = TRUE;
		CHECK(!join4_join(0), "the keyboard's SPACE joined controller 1");
		CHECK(!join4_done(0), "the keyboard's SPACE brought the players in");
		space_held = FALSE;
		CHECK(join4_join(0), "controller 1's A did not join once SPACE was up");
		return 0;
	}
	fprintf(stderr, "unknown case: %s\n", case_name);
	return 2;
}
