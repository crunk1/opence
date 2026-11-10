"""The lobby's ADD PLAYER screen (port/linux/game/menu_functions.c's join4): controller 1 joins through it too, once
its player left a game that the machine's other players stayed in, as the other controllers do; not by the keyboard's
SPACE (an A of its port); its B steps back while it is choosing; and the usual lobby, player 1 in, is as before."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run  # noqa: E402

CASES = ['player1-opener', 'player2-opener', 'player1-rejoins', 'player1-playing', 'player1-back-steps',
         'space-not-a-join']
FUNCTIONS = ('lobby_join_reset', 'lobby_controller_playing', 'join4_notice', 'join4_open', 'join4_join',
             'join4_set', 'join4_back', 'join4_done')

# a fault in menu_functions.c, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    'opener-port-1-skipped': (
        ('else if (controller == join4.opener && input_has_gamepad(controller))',
         'else if (controller > 0 && controller == join4.opener && input_has_gamepad(controller))'),
        'join4_open', 'player1-opener'),
    'join-port-1-refused': (
        ('return controller >= 0 && controller < MAXIMUM_LOCAL_PLAYERS',
         'return controller > 0 && controller < MAXIMUM_LOCAL_PLAYERS'),
        'join4_join', 'player1-rejoins'),
    'join-by-space': (
        (' && !(controller == 0 && halo_keyboard_join_held());', ';'),
        'join4_join', 'space-not-a-join'),
    'done-port-1-not-waited-for': (
        ('for (controller = 0; controller < MAXIMUM_LOCAL_PLAYERS; controller++)\n\t{\n'
         '\t\tif (join4.state[controller] == _join4_choosing)',
         'for (controller = 1; controller < MAXIMUM_LOCAL_PLAYERS; controller++)\n\t{\n'
         '\t\tif (join4.state[controller] == _join4_choosing)'),
        'join4_done', 'player1-rejoins'),
    'done-port-1-not-joined': (
        ('long profile = player_ui_get_active_player_profile_index(controller);',
         'long profile = controller ? player_ui_get_active_player_profile_index(controller) : NONE;'),
        'join4_done', 'player1-rejoins'),
    'back-while-choosing': (
        ('if (controller != 0 || join4.state[0] != _join4_away)', 'if (controller != 0)'),
        'join4_back', 'player1-back-steps'),
}


def generated(fault=None, faulty_function=None):
    source = read('port/linux/game/menu_functions.c')
    config = enum_with(source, '_join4_away') + '\n'
    config += '\n'.join(re.findall(r'^#define JOIN4_EVENT_\w+ \d+$', source, re.M)) + '\n'
    # (the lobby's join and the screen's state: static anonymous structures)
    config += re.search(r'^static struct\n\{\n\t/\* ADD PLAYER chosen.*?^\} lobby_join = \{ FALSE, NONE \};\n',
                        source, re.M | re.S).group(0)
    config += re.search(r'^static struct\n\{\n\tunsigned char state\[MAXIMUM_LOCAL_PLAYERS\];.*?^\} join4;\n',
                        source, re.M | re.S).group(0)
    text = ''
    for name in FUNCTIONS:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        # (the quarter's tag group: a multi-character constant the harness's compiler refuses)
        text += code.replace("tag_loaded('DeLa', ", 'tag_loaded(0, ') + '\n'
    return (('config.inc', config), ('under_test.inc', text))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('join4_player1', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('control', NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build('join4_player1', generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"menu_functions.c with a fault ({control}) passed '{case}': the test cannot see it"
