/* letter_probe: what letters would emerge from a given world?

   A probe for the Letters, Maps and Books design. It runs a real world
   forward and, at each checkpoint, picks the adult character who holds the
   most gossip accounts and composes candidate letters from ONLY the
   accounts that character actually holds (CcSimPersonalGossip), as
   realized by their own held version (CcGossipText). The purpose vector
   (report / petition / claim / dispatch) changes which held accounts are
   selected and how the letter frames them — but never invents accounts.

   This is a prototype harness, not shipped game logic. It writes nothing
   to the save, mutates no sim state, and intentionally stays out of
   cc_sim.c so the knowledge boundary is exercised from the outside.

   Usage:
     crownless_letter_probe [--seed N | --seeds N] [--years Y]
                            [--purpose report|petition|claim|dispatch]
                            [--max-accounts K] [--census]
*/

#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Candidate {
    int32_t score;
    int32_t offset;
} Candidate;

/* A topic is a research mandate: the account kinds a patron (Scriptorium,
   court, cult, guild) would commission a scout to collect. The probe's
   topic filter is the stand-in for the future research-commission system:
   a researcher targeting a topic searches only the accounts that match its
   kinds, never the rest of a writer's holdings. */
#define CC_PROBE_TOPIC_COUNT 10

static const char *TopicNames[CC_PROBE_TOPIC_COUNT + 1] = {
    "all",
    "dragon",   /* 1 */
    "goblin",   /* 2 */
    "war",      /* 3 */
    "throne",   /* 4 */
    "wheat",    /* 5 */
    "herds",    /* 6 */
    "ponies",   /* 7 */
    "road",     /* 8 */
    "bandit",   /* 9 */
    "treasure"  /* 10 */
};

static bool TopicMatches(int32_t topic, CcEventKind kind)
{
    switch (topic) {
        case 1: /* dragon: the ember and its shadow */
            switch (kind) {
                case CC_EVENT_DRAGON_HOARD_STOLEN:
                case CC_EVENT_DRAGON_OMEN:
                case CC_EVENT_DRAGON_TREASURE_RETURNED:
                case CC_EVENT_DRAGON_RETALIATION:
                case CC_EVENT_DRAGON_MUSTERED:
                case CC_EVENT_DRAGON_BATTLE:
                case CC_EVENT_DRAGON_SLAIN:
                case CC_EVENT_DRAGON_HOARD_RECOVERED:
                case CC_EVENT_DRAGON_HUNT:
                case CC_EVENT_DRAGON_CROWNED:
                case CC_EVENT_DRAGON_UNCROWNED:
                case CC_EVENT_DRAGON_BROOD:
                case CC_EVENT_DRAGON_WHELP_DISPERSED:
                case CC_EVENT_DRAGON_AFTERSHOCK:
                case CC_EVENT_DRAGON_SUCCESSOR:
                case CC_EVENT_DRAGON_PATRON_NAMED:
                case CC_EVENT_DRAGON_TERRITORY_LOST:
                case CC_EVENT_GOBLIN_CULT_RALLIED:
                case CC_EVENT_GOBLIN_DRAGON_SEED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED:
                    return true;
                default:
                    return false;
            }
        case 2: /* goblin: the Cinder Tithe and the underroad */
            switch (kind) {
                case CC_EVENT_GOBLIN_TRIBUTE_DEPARTED:
                case CC_EVENT_GOBLIN_TRIBUTE_TAKEN:
                case CC_EVENT_GOBLIN_TRIBUTE_DELIVERED:
                case CC_EVENT_GOBLIN_RAID_DEPARTED:
                case CC_EVENT_GOBLIN_RAIDED:
                case CC_EVENT_GOBLIN_RAID_RETURNED:
                case CC_EVENT_GOBLIN_HOARD_DEFENDED:
                case CC_EVENT_GOBLIN_RAID_PREPARED:
                case CC_EVENT_GOBLIN_TARGET_WARNED:
                case CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED:
                case CC_EVENT_GOBLIN_TRADE:
                case CC_EVENT_GOBLIN_TUNNEL_TRAVERSED:
                case CC_EVENT_GOBLIN_CULT_RALLIED:
                case CC_EVENT_GOBLIN_DRAGON_SEED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED:
                case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED:
                    return true;
                default:
                    return false;
            }
        case 3: /* war: courts, couriers, and supply lines */
            switch (kind) {
                case CC_EVENT_WAR_PRESSURE:
                case CC_EVENT_WAR_DECLARED:
                case CC_EVENT_PEACE_DECLARED:
                case CC_EVENT_ALLIANCE_DECLARED:
                case CC_EVENT_WAR_CHEST_FUNDED:
                case CC_EVENT_WAR_SUPPLY_BOUGHT:
                case CC_EVENT_WAR_SUPPLY_SHORTAGE:
                case CC_EVENT_WAR_MATERIEL_LOST:
                case CC_EVENT_COURIER_DEPARTED:
                case CC_EVENT_COURIER_ARRIVED:
                case CC_EVENT_COURIER_LOST:
                case CC_EVENT_COURIER_DISTORTED:
                case CC_EVENT_DRAGON_MUSTERED:
                    return true;
                default:
                    return false;
            }
        case 4: /* throne: succession, pretenders, and the court's legitimacy */
            switch (kind) {
                case CC_EVENT_KINGDOM_ACTION:
                case CC_EVENT_PRETENDER_CRISIS:
                case CC_EVENT_ROYAL_SUCCESSION:
                case CC_EVENT_MONASTIC_SUCCESSION:
                case CC_EVENT_KING_ANOINTED:
                case CC_EVENT_FACTION_SHIFT:
                    return true;
                default:
                    return false;
            }
        case 5: /* wheat: the food supply that keeps hunger from the wall */
            switch (kind) {
                case CC_EVENT_HARVEST_FAILED:
                case CC_EVENT_SHORTAGE:
                case CC_EVENT_RELIEF:
                case CC_EVENT_BAKERY_PRODUCTION:
                case CC_EVENT_PAPER_MILLED:
                    return true;
                default:
                    return false;
            }
        case 6: /* herds: cows, sheep, and the carriage team */
            switch (kind) {
                case CC_EVENT_COW_CALVING:
                case CC_EVENT_COW_SLAUGHTERED:
                case CC_EVENT_HORSE_BRED:
                case CC_EVENT_FOAL_BORN:
                case CC_EVENT_SHEEP_BRED:
                case CC_EVENT_SHEEP_SHEARED:
                case CC_EVENT_SHEEP_SLAUGHTERED:
                case CC_EVENT_HORSE_TEAM_CHANGED:
                    return true;
                default:
                    return false;
            }
        case 7: /* ponies: the seven rainbow companions. WIP: no gossip events
                   exist for them yet, so a researcher finds nothing — and the
                   probe reports that honestly instead of inventing holdings. */
            return false;
        case 8: /* road: routes, carriage, and the working sites */
            switch (kind) {
                case CC_EVENT_ROUTE_CLOSED:
                case CC_EVENT_ROUTE_REPAIRED:
                case CC_EVENT_ROUTE_DECAY:
                case CC_EVENT_ROYAL_CARRIAGE_BLOCKED:
                case CC_EVENT_ROYAL_CARRIAGE_REROUTED:
                case CC_EVENT_ROAD_HOUSE_LODGING:
                case CC_EVENT_JOURNEY_WARNING:
                case CC_EVENT_WOODLOT_HARVEST:
                case CC_EVENT_QUARRY_OUTPUT:
                case CC_EVENT_MASONRY_REPAIR:
                    return true;
                default:
                    return false;
            }
        case 9: /* bandit: raiders and the armed road */
            switch (kind) {
                case CC_EVENT_BANDIT_PRESSURE:
                case CC_EVENT_BANDIT_RAID_DEPARTED:
                case CC_EVENT_SETTLEMENT_RAIDED:
                case CC_EVENT_BANDIT_RAID_RETURNED:
                case CC_EVENT_ENCOUNTER_LOOT:
                case CC_EVENT_AMBUSH_EVADED:
                case CC_EVENT_ENCOUNTER_WITHDRAWN:
                case CC_EVENT_HOARD_HEIST_DEPARTED:
                case CC_EVENT_HOARD_HEIST_RETURNED:
                    return true;
                default:
                    return false;
            }
        case 10: /* treasure: the hoard, the ledger, and wealth's movement */
            switch (kind) {
                case CC_EVENT_TREASURE_CRAFTED:
                case CC_EVENT_DRAGON_HOARD_STOLEN:
                case CC_EVENT_DRAGON_TREASURE_RETURNED:
                case CC_EVENT_DRAGON_HOARD_RECOVERED:
                case CC_EVENT_IRON_LEDGER_LOAN:
                case CC_EVENT_IRON_LEDGER_REPAID:
                case CC_EVENT_INEQUALITY_PRESSURE:
                case CC_EVENT_GOBLIN_TRADE:
                    return true;
                default:
                    return false;
            }
        default: /* all / none */
            return true;
    }
}

static const char *TopicName(int32_t topic)
{
    if (topic >= 0 && topic <= CC_PROBE_TOPIC_COUNT) {
        return TopicNames[topic];
    }
    return "all";
}

static int32_t TopicId(const char *name)
{
    if (name == NULL) return 0;
    for (int32_t topic = 1; topic <= CC_PROBE_TOPIC_COUNT; ++topic) {
        if (strcmp(name, TopicNames[topic]) == 0) return topic;
    }
    return 0;
}

static const char *PurposeName(int purpose)
{
    switch (purpose) {
        case 0: return "report";
        case 1: return "petition";
        case 2: return "claim";
        case 3: return "dispatch";
        default: return "report";
    }
}

static bool IsKingdomEvent(CcEventKind kind)
{
    switch (kind) {
        case CC_EVENT_KINGDOM_ACTION:
        case CC_EVENT_WAR_DECLARED:
        case CC_EVENT_PEACE_DECLARED:
        case CC_EVENT_ALLIANCE_DECLARED:
        case CC_EVENT_DRAGON_MUSTERED:
        case CC_EVENT_DRAGON_BATTLE:
        case CC_EVENT_DRAGON_SLAIN:
        case CC_EVENT_DRAGON_CROWNED:
        case CC_EVENT_DRAGON_HOARD_RECOVERED:
        case CC_EVENT_DRAGON_HUNT:
        case CC_EVENT_DRAGON_BROOD:
        case CC_EVENT_PRETENDER_CRISIS:
        case CC_EVENT_ROYAL_SUCCESSION:
            return true;
        default:
            return false;
    }
}

static bool IsLocalTrouble(CcEventKind kind)
{
    switch (kind) {
        case CC_EVENT_HARVEST_FAILED:
        case CC_EVENT_ROUTE_CLOSED:
        case CC_EVENT_SHORTAGE:
        case CC_EVENT_BANDIT_PRESSURE:
        case CC_EVENT_MONSTER_PRESSURE:
        case CC_EVENT_SETTLEMENT_RAIDED:
        case CC_EVENT_GOBLIN_RAIDED:
        case CC_EVENT_WAR_SUPPLY_SHORTAGE:
        case CC_EVENT_INEQUALITY_PRESSURE:
        case CC_EVENT_DRAGON_OMEN:
        case CC_EVENT_DRAGON_RETALIATION:
            return true;
        default:
            return false;
    }
}

static int32_t SettlementSlotById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id == id) return i;
    }
    return -1;
}

static const CcKingdom *KingdomForSettlement(const CcSim *sim,
                                             CcId settlement_id)
{
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (place == NULL) return NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == place->kingdom_id) {
            return &sim->kingdoms[i];
        }
    }
    return NULL;
}

/* The adult character at a settlement who holds the most gossip accounts.
   Ties break on story count, then on character slot. */
static const CcCharacter *BestHeldWriter(const CcSim *sim)
{
    const CcCharacter *best = NULL;
    int32_t best_held = -1;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (CcCharacterAgeYears(sim, person) < 16 ||
            person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING) continue;
        int32_t held = 0;
        for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
            const CcGossipVersion *version = NULL;
            if (CcSimPersonalGossip(sim, person->id, offset, &version) == NULL) break;
            held += 1;
        }
        if (held > best_held) {
            best_held = held;
            best = person;
        }
    }
    return best;
}

/* May a writer compose on stamped scriptorium paper with a registered seal?
   Only appointed officials do (#435: "authorized paper carries a central
   issuance mark; appointed scribes apply individually tracked personal
   seals"). Everyone else writes an unstamped diary: locally useful, never
   admitted to central intake. */
static bool HasComposingSeal(const CcSim *sim, const CcCharacter *writer)
{
    if (sim == NULL || writer == NULL) return false;
    if (writer->id == sim->archives.abbot_character_id) return true;
    return writer->role == CC_CHARACTER_OFFICIAL;
}

/* The writer's goal supplies the default purpose vector; the CLI purpose
   overrides it. KEEP_ORDER leads reports and dispatches, SECURE_LIVELIHOOD
   petitions, SURVIVE_CRISIS reports, CARRY_NEWS spreads word. */
static int32_t PurposeForGoal(CcCharacterGoal goal)
{
    switch (goal) {
        case CC_CHARACTER_GOAL_KEEP_ORDER: return 0; /* report */
        case CC_CHARACTER_GOAL_SECURE_LIVELIHOOD: return 1; /* petition */
        case CC_CHARACTER_GOAL_SURVIVE_CRISIS: return 0; /* report */
        case CC_CHARACTER_GOAL_CARRY_NEWS: return 3; /* dispatch */
        default: return 0;
    }
}

static int32_t HeldCount(const CcSim *sim, CcId character_id)
{
    int32_t count = 0;
    for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
        const CcGossipVersion *version = NULL;
        if (CcSimPersonalGossip(sim, character_id, offset, &version) == NULL) break;
        count += 1;
    }
    return count;
}

static void SortCandidates(Candidate *candidates, int32_t count)
{
    for (int32_t i = 1; i < count; ++i) {
        Candidate key = candidates[i];
        int32_t j = i - 1;
        while (j >= 0 && candidates[j].score < key.score) {
            candidates[j + 1] = candidates[j];
            j -= 1;
        }
        candidates[j + 1] = key;
    }
}

static bool AccountHasDigits(const char *text)
{
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] >= '0' && text[i] <= '9') return true;
    }
    return false;
}

static const char *Salutation(int purpose, const CcKingdom *kingdom)
{
    switch (purpose) {
        case 0:
            return "To the Scriptorium Intake, in witness of these accounts, I report as follows:";
        case 1:
            return kingdom != NULL ?
                "To the Court of %s, respecting the town, I make this petition:" :
                "To the Court, respecting the town, I make this petition:";
        case 2:
            return "To the Abbot and the Court, concerning the succession, I set down this claim:";
        case 3:
            return "By order of the crown, to the garrison and gatewards, this dispatch is carried sealed:";
        default:
            return "";
    }
}

static const char *Closing(int purpose)
{
    switch (purpose) {
        case 0:
            return "Recorded in truth as I hold it.";
        case 1:
            return "Consider the need of this town before the season turns.";
        case 2:
            return "I sign this in the presence of my witnesses.";
        case 3:
            return "Break the seal only at the destination.";
        default:
            return "";
    }
}

static const char *Audience(int purpose)
{
    switch (purpose) {
        case 0: return "the Scriptorium Intake";
        case 1: return "the court of the writer's kingdom";
        case 2: return "the Abbot and the court";
        case 3: return "the garrison and gatewards";
        default: return "an unnamed recipient";
    }
}

/* Purpose scoring over held accounts. This is the probe's stand-in for the
   future document-mode of the discourse scorer (#277). It selects accounts
   the writer holds; it can never fetch one they do not. The topic filter is
   applied before this (PrintLetter skips irrelevant accounts), so scoring
   here only ranks within the researched subject. */
static int32_t ScoreForPurpose(const CcSim *sim, const CcCharacter *writer,
                               const CcGossip *story,
                               const CcGossipVersion *version,
                               int purpose)
{
    int32_t told_day = story->heard_day > 0 ? story->heard_day : story->day;
    int32_t recency = told_day;
    int32_t score = recency * 2;
    score -= version->retellings * 4;
    score += version->alarm / 8;
    int32_t slot = SettlementSlotById(sim, writer->current_settlement_id);
    bool local = slot >= 0 &&
        (story->origin_id == writer->current_settlement_id ||
         (story->settlement_mask & (UINT32_C(1) << (uint32_t)slot)) != 0U);
    switch (purpose) {
        case 0: /* report: recency and alarm, local trouble is useful context. */
            if (local) score += 8;
            if (IsLocalTrouble(story->kind)) score += 12;
            break;
        case 1: /* petition: the town's own troubles, and losses, lead. */
            if (local) score += 22;
            if (IsLocalTrouble(story->kind)) score += 10;
            break;
        case 2: /* claim: kingdom-level accounts and alarmed tellings lead. */
            if (IsKingdomEvent(story->kind)) score += 28;
            score += version->court_bias / 4;
            break;
        case 3: /* dispatch: kingdom accounts, heard near the court-leaning side. */
            if (IsKingdomEvent(story->kind)) score += 24;
            if (version->court_bias > 0) score += 10;
            break;
        default:
            break;
    }
    return score;
}

static void PrintLetter(const CcSim *sim, const CcCharacter *writer,
                        int purpose, int32_t topic, int max_accounts)
{
    const CcSettlement *place = CcSimSettlement(sim, writer->current_settlement_id);
    const CcKingdom *kingdom = KingdomForSettlement(sim, writer->current_settlement_id);
    char name[CC_NAME_CAPACITY];
    (void)snprintf(name, sizeof(name), "%s", writer->name);

    Candidate candidates[CC_MAX_GOSSIP];
    int32_t candidate_count = 0;
    int32_t held = 0;
    int32_t relevant = 0;
    for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
        const CcGossipVersion *version = NULL;
        const CcGossip *story = CcSimPersonalGossip(sim, writer->id, offset, &version);
        if (story == NULL || version == NULL) break;
        held += 1;
        if (topic != 0 && !TopicMatches(topic, story->kind)) continue;
        relevant += 1;
        if (candidate_count < CC_MAX_GOSSIP) {
            candidates[candidate_count].score =
                ScoreForPurpose(sim, writer, story, version, purpose);
            candidates[candidate_count].offset = offset;
            candidate_count += 1;
        }
    }
    SortCandidates(candidates, candidate_count);

    /* One evidentiary lineage per letter: drop a candidate whose kind and
       origin repeat an earlier cited account. Ten copies of one story are one
       line of evidence (#437); selection honors the same rule. */
    const CcGossip *lineage[CC_MAX_GOSSIP];
    const CcGossipVersion *lineage_version[CC_MAX_GOSSIP];
    int32_t selected_count = 0;
    for (int32_t i = 0; i < candidate_count && selected_count < max_accounts; ++i) {
        const CcGossipVersion *version = NULL;
        const CcGossip *story = CcSimPersonalGossip(
            sim, writer->id, candidates[i].offset, &version);
        if (story == NULL || version == NULL) continue;
        bool duplicate_lineage = false;
        for (int32_t j = 0; j < selected_count; ++j) {
            if (lineage[j]->kind == story->kind &&
                lineage[j]->origin_id == story->origin_id) {
                duplicate_lineage = true;
                break;
            }
        }
        if (duplicate_lineage) continue;
        lineage[selected_count] = story;
        lineage_version[selected_count] = version;
        selected_count += 1;
    }

    const bool stamped = HasComposingSeal(sim, writer);

    char salutation[512];
    const char *salutation_template = Salutation(purpose, kingdom);
    if (place == NULL || strchr(salutation_template, '%') == NULL) {
        (void)snprintf(salutation, sizeof(salutation), "%s",
                       salutation_template);
    } else {
        (void)snprintf(salutation, sizeof(salutation), salutation_template,
                       kingdom != NULL ? kingdom->name : "the Kingdom");
    }

    printf("  --- purpose: %s\n", PurposeName(purpose));
    if (topic != 0) {
        printf("  ---- topic: %s\n", TopicName(topic));
    }
    printf("  From: %s (%s), at %s\n",
           name, CcCharacterRoleName(writer->role),
           place != NULL ? place->name : "the road");
    printf("  To:   %s\n", Audience(purpose));
    if (stamped) {
        printf("  Paper: stamped scriptorium paper  ·  Seal: %s's registered scribe seal  ·  Composed day %d\n",
               name, sim->current_day);
    } else {
        printf("  Paper: unmarked personal diary (not admitted to intake)  ·  No seal registered  ·  Composed day %d\n",
               sim->current_day);
        printf("  %s carries no seal; this writing cannot enter the Scriptorium corpus as it stands.\n",
               name);
    }
    printf("  %s\n", salutation);
    if (held == 0) {
        printf("    <no held accounts — this writer composes nothing>\n");
    } else if (relevant == 0) {
        printf("    <no held account touches \"%s\" — the researcher finds nothing on this subject here>\n",
               TopicName(topic));
    }
    int32_t used = 0;
    for (int32_t i = 0; i < selected_count; ++i) {
        /* The writer's own town is vouched: quantities stand. A distant or
           retold account stays suspicious, so its particulars are redacted
           from the letter even though the writer may hold them. */
        const CcGossip *story = lineage[i];
        const CcGossipVersion *version = lineage_version[i];
        char account[CC_EVENT_TEXT_CAPACITY];
        CcGossipText(sim, story, version, account, sizeof(account));
        if (account[0] == '\0') continue;
        const CcSettlement *origin = CcSimSettlement(sim, story->origin_id);
        bool vouched = origin != NULL &&
                       origin->id == writer->current_settlement_id;
        printf("    %d. %s\n", used + 1, account);
        if (AccountHasDigits(account) && !vouched) {
            printf("       (particulars redacted — the writer vouches only for %s, not for distant quantities)\n",
                   place != NULL ? place->name : "this town");
        }
        printf("       cited: %s, event %" PRIu64 " (%s), held day %d, retellings %d, "
               "court bias %+d, alarm %d%s\n",
               origin != NULL ? origin->name : "the road",
               story->event_id, CcEventKindName(story->kind),
               story->heard_day > 0 ? story->heard_day : story->day,
               version->retellings, version->court_bias, version->alarm,
               vouched ? ", vouched (writer's town)" : "");
        used += 1;
    }
    printf("  %s — %s\n", Closing(purpose), name);
    if (topic != 0) {
        printf("  ( %d of %d held accounts relevant to \"%s\"; %d cited in this letter )\n\n",
               relevant, held, TopicName(topic), used);
    } else {
        printf("  ( %d held accounts; %d cited in this letter )\n\n", held, used);
    }
}

static void PrintWorldContext(const CcSim *sim)
{
    int32_t max_hunger = 0;
    int32_t total_hunger = 0;
    int32_t abandoned = 0;
    int32_t wars = 0;
    int32_t crises = 0;
    int32_t anointed = 0;
    int32_t closed_routes = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (CcSettlementIsAbandoned(&sim->settlements[i])) abandoned += 1;
        total_hunger += sim->settlements[i].hunger;
        if (sim->settlements[i].hunger > max_hunger) {
            max_hunger = sim->settlements[i].hunger;
        }
    }
    for (int32_t both = 0; both < sim->kingdom_count; ++both) {
        for (int32_t other = 0; other < sim->kingdom_count; ++other) {
            if (sim->diplomacy[both][other] == CC_DIPLOMACY_WAR &&
                other > both) wars += 1;
        }
        crises += sim->kingdoms[both].pretender_crises;
        if (sim->kingdoms[both].anointed) anointed += 1;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].closed) closed_routes += 1;
    }
    printf("== world seed %" PRIu32 " — year %d, day %d ==\n",
           sim->world_seed, sim->current_day / 365 + 1, sim->current_day);
    printf("   hunger mean %.1f / max %d, abandoned settlements %d, closed routes %d, "
           "wars %d, pretender crises %d, anointed %d, dragon stage %s, hoard %" PRIu64 "\n",
           (double)total_hunger / (double)sim->settlement_count, max_hunger,
           abandoned, closed_routes, wars, crises, anointed,
           CcDragonLifeStageName(sim->dragon.life_stage),
           (uint64_t)sim->dragon.hoard);
}

static void PrintCensus(const CcSim *sim)
{
    printf("   -- knowledge census --\n");
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        int32_t residents = 0;
        int32_t resident_accounts = 0;
        for (int32_t c = 0; c < sim->character_count; ++c) {
            const CcCharacter *person = &sim->characters[c];
            if (person->current_settlement_id != place->id) continue;
            int32_t held = HeldCount(sim, person->id);
            residents += 1;
            resident_accounts += held;
            printf("      %-10.10s %-10.10s: %d held accounts, %d knowledge records\n",
                   place->name, person->name, held, person->knowledge_count);
        }
        (void)residents;
        (void)resident_accounts;
    }
}

int main(int argc, char **argv)
{
    int32_t seed_number = 1;
    int32_t seeds = 4;
    int32_t years = 5;
    int32_t purpose = 0;
    int32_t max_accounts = 3;
    bool census = false;
    bool compare = false;
    bool scan = false;
    int32_t topic = 0;

    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            seed_number = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--seeds") == 0 && argument + 1 < argc) {
            seeds = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--years") == 0 && argument + 1 < argc) {
            years = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--max-accounts") == 0 && argument + 1 < argc) {
            max_accounts = (int32_t)strtol(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--topic") == 0 && argument + 1 < argc) {
            topic = TopicId(argv[++argument]);
        } else if (strcmp(argv[argument], "--purpose") == 0 && argument + 1 < argc) {
            const char *name = argv[++argument];
            if (strcmp(name, "auto") == 0) purpose = -1;
            else purpose = strcmp(name, "petition") == 0 ? 1 :
                           strcmp(name, "claim") == 0 ? 2 :
                           strcmp(name, "dispatch") == 0 ? 3 : 0;
        } else if (strcmp(argv[argument], "--census") == 0) {
            census = true;
        } else if (strcmp(argv[argument], "--compare") == 0) {
            compare = true;
        } else if (strcmp(argv[argument], "--scan") == 0) {
            scan = true;
        } else {
            (void)fprintf(stderr, "Usage: %s [--seed N] [--seeds N] [--years Y] "
                          "[--purpose report|petition|claim|dispatch|auto] "
                          "[--topic dragon|goblin|war|throne|wheat|herds|ponies|road|bandit|treasure|all] "
                          "[--max-accounts K] [--census] [--compare] [--scan]\n", argv[0]);
            return 1;
        }
    }
    if (seeds < 1 || years < 1 || max_accounts < 1) return 1;

    printf("letter probe — world seeds %d..%d, %d years, purpose \"%s\", topic \"%s\"%s\n\n",
           seed_number, seed_number + seeds - 1, years,
           purpose < 0 ? "auto (writer goal)" : PurposeName(purpose),
           TopicName(topic), scan ? ", scan all topics" : "");

    for (int32_t s = 0; s < seeds; ++s) {
        CcSim sim;
        CcSimInit(&sim, (uint32_t)(seed_number + s) * UINT32_C(0x9e3779b9));
        for (int32_t year = 1; year <= years; ++year) {
            for (int32_t day = 0; day < 365; ++day) {
                CcSimAdvanceDays(&sim, 1);
            }
            PrintWorldContext(&sim);
            if (census) PrintCensus(&sim);
            const CcCharacter *writer = BestHeldWriter(&sim);
            if (writer == NULL) {
                printf("   <no adult non-travelling character holds accounts>\n\n");
                continue;
            }
            printf("   writer: %s (%s) at %s — %d held accounts\n",
                   writer->name, CcCharacterRoleName(writer->role),
                   CcSimSettlement(&sim, writer->current_settlement_id) != NULL ?
                       CcSimSettlement(&sim, writer->current_settlement_id)->name : "road",
                   HeldCount(&sim, writer->id));
            if (compare) {
                for (int32_t p = 0; p < 4; ++p) {
                    PrintLetter(&sim, writer, p, topic, max_accounts);
                }
            } else if (scan) {
                /* A researcher searches every subject in turn, composing one
                   research report per topic from the same writer's holdings.
                   Reports go to the Scriptorium Intake (purpose 0) because
                   research letters are intake documents. */
                int32_t used_purpose = purpose >= 0 ? purpose : 0;
                for (int32_t t = 1; t <= CC_PROBE_TOPIC_COUNT; ++t) {
                    PrintLetter(&sim, writer, used_purpose, t, max_accounts);
                }
            } else {
                int32_t used_purpose = purpose >= 0 ? purpose :
                    PurposeForGoal(writer->goal);
                PrintLetter(&sim, writer, used_purpose, topic, max_accounts);
            }
        }
        printf("\n");
    }
    return 0;
}
