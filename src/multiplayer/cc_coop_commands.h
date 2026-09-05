#ifndef CROWNLESS_COOP_COMMANDS_H
#define CROWNLESS_COOP_COMMANDS_H
#include "sim/cc_sim.h"

static inline const char *CcCoopActionName(CcCommandKind kind)
{
    static const char *const names[] = {
        "",
        "trade",
        "travel",
        "repair",
        "change_dungeon",
        "buy_map",
        "sell_map",
        "accept",
        "abandon",
        "fight",
        "negotiate",
        "refuse",
        "steal_hoard",
        "return_treasure",
        "buy_treasure",
        "sell_treasure",
        "archive_map",
        "retrieve_map",
        "steal_named_treasure",
        "return_named_treasure",
        "provisions",
        "withdraw",
        "breed_horses",
        "assign_horse",
        "intercept_tribute",
        "goblin_trade",
        "goblin_warn",
        "goblin_intercept",
        "talk",
        "pace",
        "goblin_tunnel",
        "enter_dungeon",
        "move_dungeon",
        "search_dungeon",
        "open_shortcut",
        "dungeon_encounter",
        "leave_dungeon",
        "break",
        "press_on",
        "camp",
        "lodge",
        "camp_road_site",
        "pass_road_site",
        "meet_pony",
        "help_pony",
        "swap_pony",
        "leave_pony",
        "party_wipe",
    };
    return kind > CC_COMMAND_NONE && kind <= CC_COMMAND_PARTY_WIPE ? names[(int)kind] : "";
}
#endif
