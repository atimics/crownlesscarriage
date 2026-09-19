/* Historical PLAYER_AMBUSH messages retained for accounts in old saves.
 * Copied from src/sim/cc_sim.c at commit
 * 89742c20853ffa44a61e5538dee5d74e8c05e55e (before 52fbecea).
 * Current journeys use encounter events. This fixture is source evidence,
 * not compiled simulation code.
 */
const char *legacy_player_ambush_formats[] = {
    "Roadside attackers take %d %s before disappearing into the route traffic.",
    "Roadside attackers take %d crowns before disappearing into the route traffic."
};
