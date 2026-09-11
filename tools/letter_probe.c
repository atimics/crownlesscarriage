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
#include "sim/cc_gossip_topics.h"
#include "sim/cc_occupations.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Candidate {
    int32_t score;
    int32_t offset;
} Candidate;

#define CC_PROBE_TOPIC_COUNT (CC_GOSSIP_TOPIC_COUNT - 1)

static const char *TopicName(int32_t topic)
{
    return CcGossipTopicName((CcGossipTopic)topic);
}

static int32_t TopicId(const char *name)
{
    CcGossipTopic topic = CC_GOSSIP_TOPIC_ALL;
    (void)CcGossipTopicParse(name, &topic);
    return (int32_t)topic;
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
static bool HasComposingSeal(const CcSim *sim, const CcCharacter *writer);

static const CcCharacter *BestHeldWriter(const CcSim *sim, bool diary, bool sealed)
{
    const CcCharacter *best = NULL;
    int32_t best_held = -1;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (CcCharacterAgeYears(sim, person) < 16 ||
            person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING) continue;
        if (diary && HasComposingSeal(sim, person)) continue;
        if (sealed && !HasComposingSeal(sim, person)) continue;
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
        if (topic != 0 && !CcGossipTopicMatches((CcGossipTopic)topic, story->kind)) continue;
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

/* ------------------------------------------------------------------ */
/* Research-mission mode: baseline book, pages, scout collection       */
/* ------------------------------------------------------------------ */

/* One notable topic per character, derived from their role. This is the
   simplest form of the model: a herdsman holds herd gossip, a guard holds
   road/bandit gossip, an official holds throne gossip. The mission topic
   and the teller's notable topic together give the "by craft" flag — a
   herdsman telling a herds fact vouches for it better than an official
   would. */
static int32_t RoleNotableTopic(CcCharacterRole role)
{
    switch (role) {
        case CC_CHARACTER_OFFICIAL: return 4;  /* throne */
        case CC_CHARACTER_LABORER:  return 6;  /* herds */
        case CC_CHARACTER_SCOUT:    return 8;  /* road */
        case CC_CHARACTER_TRAVELLER:return 8;  /* road */
        case CC_CHARACTER_REFUGEE:  return 5;  /* wheat */
        case CC_CHARACTER_COURIER:  return 3;  /* war */
        default:                    return 1;  /* dragon (unmapped roles default to the grand subject) */
    }
}

/* The adult non-travelling character at one settlement who holds the most
   gossip accounts. The mission visits each town and asks its most-informed
   resident for a page. */
static const CcCharacter *BestHeldWriterAt(const CcSim *sim,
                                           CcId settlement_id, int32_t topic)
{
    const CcCharacter *best = NULL;
    int32_t best_held = -1;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (person->current_settlement_id != settlement_id) continue;
        if (CcCharacterAgeYears(sim, person) < 16 ||
            person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING) continue;
        int32_t held = HeldCount(sim, person->id);
        if (sim->schema_version >= 79U) {
            held = 0;
            for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
                const CcGossip *story = CcSimPersonalGossip(sim, person->id, offset, NULL);
                if (story != NULL && CcGossipTopicMatches((CcGossipTopic)topic, story->kind))
                    ++held;
            }
        }
        if (held > best_held) {
            best_held = held;
            best = person;
        }
    }
    return best;
}

#define CC_MISSION_MAX_PAGES 10
#define CC_MISSION_PAGE_FACTS 3

static void RunMission(const CcSim *sim, int topic, int32_t baseline,
                       int facts_per_page)
{
    if (sim == NULL || topic < 1 || topic > CC_PROBE_TOPIC_COUNT) return;
    /* The archive pre-fills the book with what it already knew on this
       subject by the baseline day (facts it has heard). */
    CcId prefill[CC_MAX_GOSSIP];
    int32_t prefill_count = 0;
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        const CcGossip *story = CcSimGossipStory(sim, i);
        if (story == NULL || !CcGossipTopicMatches((CcGossipTopic)topic, story->kind)) continue;
        if (story->heard_day > 0 && story->heard_day <= baseline) {
            if (prefill_count < CC_MAX_GOSSIP) {
                prefill[prefill_count++] = story->event_id;
            }
        }
    }

    printf("== RESEARCH MISSION: %s — seed %" PRIu32 ", day %d ==\n",
           TopicName(topic), sim->world_seed, sim->current_day);
    printf("   Book: Scriptorium research copy on \"%s\" — baseline day %d "
           "(archive knew %d facts before the scout left)\n\n",
           TopicName(topic), baseline, prefill_count);

    /* The scout rides the towns in order and takes one page per town from
       its most-informed adult: on-topic facts held AFTER the baseline, the
       newest first, at most facts_per_page, one lineage per kind+origin. */
    int32_t pages_written = 0;
    int32_t facts_written = 0;
    int32_t novel_count = 0;
    int32_t repeat_count = 0;
    int32_t further_count = 0;
    int32_t craft_count = 0;
    printf("   Scout route: ");
    for (int32_t s = 0; s < sim->settlement_count; ++s) {
        printf("%s%s", s > 0 ? " → " : "", sim->settlements[s].name);
    }
    printf("\n\n");

    /* Every fact the book ends up holding, so a later town can be checked
       for contradiction against any earlier page. */
    typedef struct BookFact {
        CcEventKind kind;
        CcId origin_id;
        CcId event_id;
        int32_t page;
    } BookFact;
    BookFact book[CC_MAX_GOSSIP];
    int32_t book_count = 0;

    for (int32_t s = 0; s < sim->settlement_count && pages_written < CC_MISSION_MAX_PAGES; ++s) {
        const CcSettlement *place = &sim->settlements[s];
        const CcCharacter *teller = BestHeldWriterAt(sim, place->id, topic);
        if (teller == NULL) continue;
        bool by_craft = sim->schema_version >= 79U ?
            CcOccupationTopic(teller->occupation) == (CcGossipTopic)topic :
            RoleNotableTopic(teller->role) == topic;

        typedef struct CandidateFact {
            const CcGossip *story;
            const CcGossipVersion *version;
        } CandidateFact;
        CandidateFact facts[CC_MAX_GOSSIP];
        int32_t fact_count = 0;
        for (int32_t offset = 0; offset < CC_MAX_GOSSIP; ++offset) {
            const CcGossipVersion *version = NULL;
            const CcGossip *story = CcSimPersonalGossip(sim, teller->id, offset, &version);
            if (story == NULL || version == NULL) break;
            if (!CcGossipTopicMatches((CcGossipTopic)topic, story->kind)) continue;
            int32_t held_day = story->heard_day > 0 ? story->heard_day : story->day;
            if (held_day <= baseline) continue;
            bool dup = false;
            for (int32_t k = 0; k < fact_count; ++k) {
                if (facts[k].story->kind == story->kind &&
                    facts[k].story->origin_id == story->origin_id) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
            facts[fact_count].story = story;
            facts[fact_count].version = version;
            fact_count += 1;
            if (fact_count >= facts_per_page) break;
        }
        if (fact_count == 0) continue;

        printf("  --- page %d: told by %s (%s, %s), at %s%s ---\n",
               pages_written + 1, teller->name, CcCharacterRoleName(teller->role),
               CcOccupationName(teller->occupation),
               place->name, by_craft ? "   [by craft]" : "");
        for (int32_t f = 0; f < fact_count; ++f) {
            const CcGossip *story = facts[f].story;
            const CcGossipVersion *version = facts[f].version;
            char account[CC_EVENT_TEXT_CAPACITY];
            CcGossipText(sim, story, version, account, sizeof(account));
            bool novel = true;
            for (int32_t p = 0; p < prefill_count; ++p) {
                if (prefill[p] == story->event_id) { novel = false; break; }
            }
            /* Is this the first telling of this exact event in the book? */
            bool repeat = false;
            for (int32_t b = 0; b < book_count; ++b) {
                if (book[b].event_id == story->event_id) { repeat = true; break; }
            }
            /* Or does a DIFFERENT event on the same kind+origin already sit
               in the book? Ten copies of one story are one lineage; two
               events on one subject are a separate occurrence — not a
               contradiction, which needs a conflicting claim. */
            int32_t further_of = 0;
            for (int32_t b = 0; b < book_count; ++b) {
                if (book[b].kind == story->kind &&
                    book[b].origin_id == story->origin_id &&
                    book[b].event_id != story->event_id) {
                    further_of = book[b].page;
                    break;
                }
            }
            if (novel) novel_count += 1;
            printf("    %d. %s\n", f + 1, account);
            printf("       cited: %s, event %" PRIu64 " (%s), held day %d, retellings %d, %s\n",
                   CcSimSettlement(sim, story->origin_id) != NULL ?
                       CcSimSettlement(sim, story->origin_id)->name : "the road",
                   story->event_id, CcEventKindName(story->kind),
                   story->heard_day > 0 ? story->heard_day : story->day,
                   version->retellings,
                   repeat ? "[repeat — one lineage, not copied]" :
                   novel ? "[NOVEL since baseline]" : "[known since baseline]");
            if (repeat) {
                repeat_count += 1;
                continue; /* do not add a second page for the same event */
            }
            if (further_of != 0) {
                further_count += 1;
                printf("       [further account — a different telling on the same subject as page %d]\n",
                       further_of);
            }
            book[book_count].kind = story->kind;
            book[book_count].origin_id = story->origin_id;
            book[book_count].event_id = story->event_id;
            book[book_count].page = pages_written + 1;
            book_count += 1;
        }
        printf("\n");
        if (by_craft) craft_count += 1;
        facts_written += fact_count;
        pages_written += 1;
    }

    printf("  Book summary: %d pages / %d facts — %d novel since baseline, %d repeat%s, "
           "%d further account%s, %d page%s by a craft-witness\n\n",
           pages_written, facts_written, novel_count, repeat_count,
           repeat_count == 1 ? "" : "s", further_count,
           further_count == 1 ? "" : "s", craft_count,
           craft_count == 1 ? "" : "s");
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
    bool diary = false;
    bool sealed = false;
    bool scan = false;
    bool mission = false;
    int32_t mission_topic = 0;
    int32_t baseline = 1;
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
        } else if (strcmp(argv[argument], "--sealed") == 0) {
            sealed = true;
        } else if (strcmp(argv[argument], "--diary") == 0) {
            diary = true;
        } else if (strcmp(argv[argument], "--compare") == 0) {
            compare = true;
        } else if (strcmp(argv[argument], "--scan") == 0) {
            scan = true;
        } else if (strcmp(argv[argument], "--mission") == 0 && argument + 1 < argc) {
            mission = true;
            mission_topic = TopicId(argv[++argument]);
        } else if (strcmp(argv[argument], "--baseline") == 0 && argument + 1 < argc) {
            baseline = (int32_t)strtol(argv[++argument], NULL, 10);
        } else {
            (void)fprintf(stderr, "Usage: %s [--seed N] [--seeds N] [--years Y] "
                          "[--purpose report|petition|claim|dispatch|auto] "
                          "[--topic dragon|goblin|war|throne|wheat|herds|ponies|road|bandit|treasure|all] "
                          "[--mission TOPIC] [--baseline DAY] [--max-accounts K] "
                          "[--census] [--compare] [--scan] [--diary | --sealed]\n", argv[0]);
            return 1;
        }
    }
    if (seeds < 1 || years < 1 || max_accounts < 1) return 1;
    if (diary && sealed) {
        (void)fprintf(stderr, "Choose one writer filter: --diary or --sealed.\n");
        return 2;
    }
    if (mission && (mission_topic < 1 || mission_topic > CC_PROBE_TOPIC_COUNT)) {
        (void)fprintf(stderr, "--mission needs a real topic (dragon|goblin|war|throne|wheat|herds|ponies|road|bandit|treasure)\n");
        return 1;
    }
    if (baseline < 1) baseline = 1;

    printf("letter probe — world seeds %d..%d, %d years, purpose \"%s\", topic \"%s\"%s\n\n",
           seed_number, seed_number + seeds - 1, years,
           purpose < 0 ? "auto (writer goal)" : PurposeName(purpose),
           TopicName(topic), scan ? ", scan all topics" : "");

    for (int32_t s = 0; s < seeds; ++s) {
        CcSim sim;
        CcSimInit(&sim, (uint32_t)(seed_number + s) * UINT32_C(0x9e3779b9));
        if (mission) {
            /* A research mission runs the world forward, then sends the scout
               out once — the book is composed at the end from everything the
               scout gathered after the baseline day. */
            for (int32_t year = 1; year <= years; ++year) {
                for (int32_t day = 0; day < 365; ++day) {
                    CcSimAdvanceDays(&sim, 1);
                }
            }
            RunMission(&sim, mission_topic, baseline, max_accounts);
            continue;
        }
        for (int32_t year = 1; year <= years; ++year) {
            for (int32_t day = 0; day < 365; ++day) {
                CcSimAdvanceDays(&sim, 1);
            }
            PrintWorldContext(&sim);
            if (census) PrintCensus(&sim);
            const CcCharacter *writer = BestHeldWriter(&sim, diary, sealed);
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
