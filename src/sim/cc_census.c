#include "sim/cc_census.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const int32_t THORNFORD_WEIGHTS[CC_CENSUS_DISTRICTS_PER_TOWN] =
    {64, 240, 240, 440, 392, 87};
static const char *const THORNFORD_NAMES[CC_CENSUS_DISTRICTS_PER_TOWN] =
    {"Market Centre", "Mill Street", "River Row", "West Crofts",
     "North Fields", "Road Hamlets"};
static const char *const OTHER_NAMES[CC_CENSUS_DISTRICTS_PER_TOWN] =
    {"Town Centre", "East Ward", "West Ward", "South Fields",
     "North Fields", "Road Hamlets"};

static CcId NewId(CcSim *sim, CcEntityKind kind)
{
    CcId result = CcMakeId(kind, sim->next_entity_serial);
    sim->next_entity_serial++;
    return result;
}

static int32_t SettlementSlot(const CcSim *sim, CcId id)
{
    for (int32_t slot = 0; slot < sim->settlement_count; ++slot)
        if (sim->settlements[slot].id == id) return slot;
    return -1;
}

static int32_t DistrictTarget(int32_t population, int32_t district)
{
    int32_t allocated = 0;
    for (int32_t i = 0; i < district; ++i)
        allocated += (int32_t)((int64_t)population * THORNFORD_WEIGHTS[i] / 1463);
    if (district == CC_CENSUS_DISTRICTS_PER_TOWN - 1)
        return population - allocated;
    return (int32_t)((int64_t)population * THORNFORD_WEIGHTS[district] / 1463);
}

const CcCensusResident *CcCensusResidentById(const CcSim *sim, CcId id)
{
    if (sim == NULL || id == 0U) return NULL;
    int32_t low = 0, high = sim->census.resident_count;
    while (low < high) {
        int32_t middle = low + (high - low) / 2;
        CcId candidate = sim->census.residents[middle].id;
        if (candidate == id) return &sim->census.residents[middle];
        if (candidate < id) low = middle + 1;
        else high = middle;
    }
    return NULL;
}

static int CompareResidents(const void *left, const void *right)
{
    CcId a = ((const CcCensusResident *)left)->id;
    CcId b = ((const CcCensusResident *)right)->id;
    return a < b ? -1 : a > b ? 1 : 0;
}

static bool HasRichRecord(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->character_count; ++i)
        if (sim->characters[i].id == id) return true;
    return false;
}

static bool AssignHome(CcCensus *census, int32_t town,
                       int32_t *district_slot, int32_t *dwelling_slot)
{
    int32_t first = town * CC_CENSUS_DISTRICTS_PER_TOWN;
    int32_t offsets[CC_CENSUS_DISTRICT_CAP + 1] = {0};
    for (int32_t i = 0; i < census->district_count; ++i)
        offsets[i + 1] = offsets[i] + census->districts[i].dwelling_count;
    uint8_t *occupancy = calloc((size_t)offsets[census->district_count] + 1U, 1U);
    if (occupancy == NULL) return false;
    for (int32_t i = 0; i < census->resident_count; ++i) {
        const CcCensusResident *person = &census->residents[i];
        if (person->left_day == 0 && person->district_slot >= 0 &&
            person->district_slot < census->district_count &&
            person->dwelling_slot >= 0 &&
            person->dwelling_slot < census->districts[
                person->district_slot].dwelling_count)
            ++occupancy[offsets[person->district_slot] + person->dwelling_slot];
    }
    for (int32_t district = first;
         district < first + CC_CENSUS_DISTRICTS_PER_TOWN; ++district) {
        const CcCensusDistrict *place = &census->districts[district];
        for (int32_t dwelling = 0; dwelling < place->dwelling_count; ++dwelling) {
            if (occupancy[offsets[district] + dwelling] >= 4U) continue;
            *district_slot = district;
            *dwelling_slot = dwelling;
            free(occupancy);
            return true;
        }
    }
    free(occupancy);
    /* Growth adds a placed address to the edge of the last housing district. */
    int32_t last = first + CC_CENSUS_DISTRICTS_PER_TOWN - 1;
    CcCensusDistrict *place = &census->districts[last];
    if (place->dwelling_count >= CC_CENSUS_PERSON_CAP) return false;
    *district_slot = last;
    *dwelling_slot = place->dwelling_count++;
    return true;
}

static CcCensusResident *AddResident(CcSim *sim, CcId id, int32_t birth_day,
                                     int32_t town, bool rich)
{
    CcCensus *census = &sim->census;
    if (census->resident_count >= CC_CENSUS_PERSON_CAP || town < 0) return NULL;
    int32_t district = 0, dwelling = 0;
    if (!AssignHome(census, town, &district, &dwelling)) return NULL;
    int32_t slot = census->resident_count;
    while (slot > 0 && census->residents[slot - 1].id > id) --slot;
    if (slot < census->resident_count)
        memmove(&census->residents[slot + 1], &census->residents[slot],
                (size_t)(census->resident_count - slot) * sizeof(census->residents[0]));
    ++census->resident_count;
    CcCensusResident *person = &census->residents[slot];
    *person = (CcCensusResident){.id = id, .birth_day = birth_day,
        .district_slot = district, .dwelling_slot = dwelling,
        .rich_identity = rich ? 1 : 0};
    return person;
}

void CcCensusInit(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 114U) return;
    CcCensus *census = &sim->census;
    *census = (CcCensus){0};
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        const CcSettlement *settlement = &sim->settlements[town];
        const char *const *names = town == 0 ? THORNFORD_NAMES : OTHER_NAMES;
        for (int32_t district = 0;
             district < CC_CENSUS_DISTRICTS_PER_TOWN; ++district) {
            int32_t target = DistrictTarget(settlement->population, district);
            CcCensusDistrict *place = &census->districts[census->district_count++];
            place->id = NewId(sim, CC_ENTITY_DISTRICT);
            place->settlement_id = settlement->id;
            size_t length = strlen(names[district]);
            if (length >= sizeof(place->name)) length = sizeof(place->name) - 1U;
            memcpy(place->name, names[district], length);
            place->name[length] = '\0';
            place->dwelling_count = (target + 3) / 4 +
                (district == CC_CENSUS_DISTRICTS_PER_TOWN - 1 ? 2 : 0);
        }
        /* Existing characters take addresses first and keep their lifetime IDs. */
        const CcCharacter *named[CC_MAX_CHARACTER_RECORDS];
        int32_t named_count = 0;
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *character = &sim->characters[i];
            if (character->home_settlement_id != settlement->id ||
                character->birth_day > sim->current_day ||
                character->death_day <= sim->current_day) continue;
            named[named_count++] = character;
        }
        int32_t named_cursor = 0;
        for (int32_t district = 0;
             district < CC_CENSUS_DISTRICTS_PER_TOWN; ++district) {
            int32_t target = DistrictTarget(settlement->population, district);
            int32_t district_slot = town * CC_CENSUS_DISTRICTS_PER_TOWN + district;
            for (int32_t ordinal = 0; ordinal < target &&
                 census->resident_count < CC_CENSUS_PERSON_CAP; ++ordinal) {
                CcCensusResident *person =
                    &census->residents[census->resident_count++];
                bool rich = named_cursor < named_count;
                CcId id = rich ? named[named_cursor]->id :
                    NewId(sim, CC_ENTITY_CHARACTER);
                int32_t birth_day = rich ? named[named_cursor++]->birth_day :
                    sim->current_day -
                    (16 + (int32_t)((id ^ sim->world_seed) % 58U)) * 365;
                *person = (CcCensusResident){.id = id,
                    .birth_day = birth_day, .district_slot = district_slot,
                    .dwelling_slot = ordinal / 4,
                    .rich_identity = rich ? 1 : 0};
            }
        }
        /* A legacy town can report fewer residents than living named people.
           Keep those people in recorded shelter beside the old town. */
        while (named_cursor < named_count) {
            const CcCharacter *character = named[named_cursor++];
            CcCensusResident *person = AddResident(
                sim, character->id, character->birth_day, town, true);
            if (person == NULL) break;
            person->sheltered = 1;
        }
    }
    qsort(census->residents, (size_t)census->resident_count,
          sizeof(census->residents[0]), CompareResidents);
}

CcId CcCensusClaimResident(CcSim *sim, CcId settlement_id)
{
    if (sim == NULL || sim->schema_version < 114U ||
        sim->census.district_count == 0) return 0U;
    for (int32_t i = 0; i < sim->census.resident_count; ++i) {
        CcCensusResident *person = &sim->census.residents[i];
        if (person->left_day != 0 || person->rich_identity != 0 ||
            sim->census.districts[person->district_slot].settlement_id !=
                settlement_id) continue;
        if (HasRichRecord(sim, person->id)) continue;
        person->rich_identity = 1;
        return person->id;
    }
    return 0U;
}

int32_t CcCensusPopulation(const CcSim *sim, CcId settlement_id)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->census.resident_count; ++i) {
        const CcCensusResident *person = &sim->census.residents[i];
        if (person->left_day == 0 && !person->sheltered &&
            sim->census.districts[person->district_slot].settlement_id ==
                settlement_id) ++count;
    }
    return count;
}

int32_t CcCensusDistrictPopulation(const CcSim *sim, CcId district_id)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->census.resident_count; ++i) {
        const CcCensusResident *person = &sim->census.residents[i];
        if (person->left_day == 0 && !person->sheltered &&
            sim->census.districts[person->district_slot].id == district_id)
            ++count;
    }
    return count;
}

void CcCensusReconcile(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 114U ||
        sim->census.district_count == 0) return;
    CcCensus *census = &sim->census;
    for (int32_t i = 0; i < census->resident_count; ++i) {
        CcCensusResident *person = &census->residents[i];
        if (person->left_day != 0 || !person->rich_identity) continue;
        const CcCharacter *rich = CcSimCharacter(sim, person->id);
        if (rich == NULL || rich->death_day <= sim->current_day) {
            person->left_day = sim->current_day;
            continue;
        }
        person->birth_day = rich->birth_day;
        CcId home = census->districts[person->district_slot].settlement_id;
        if (home != rich->home_settlement_id) {
            int32_t town = SettlementSlot(sim, rich->home_settlement_id);
            if (town >= 0)
                (void)AssignHome(census, town, &person->district_slot,
                                 &person->dwelling_slot);
        }
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *rich = &sim->characters[i];
        if (rich->birth_day > sim->current_day ||
            rich->death_day <= sim->current_day ||
            CcCensusResidentById(sim, rich->id) != NULL) continue;
        (void)AddResident(sim, rich->id, rich->birth_day,
                          SettlementSlot(sim, rich->home_settlement_id), true);
    }
    for (int32_t town = 0; town < sim->settlement_count; ++town) {
        const CcSettlement *settlement = &sim->settlements[town];
        int32_t current = CcCensusPopulation(sim, settlement->id);
        for (int32_t i = 0; i < census->resident_count &&
             current < settlement->population; ++i) {
            CcCensusResident *person = &census->residents[i];
            if (!person->sheltered || person->left_day != 0 ||
                census->districts[person->district_slot].settlement_id !=
                    settlement->id) continue;
            person->sheltered = 0;
            ++current;
        }
        while (current < settlement->population &&
               census->resident_count < CC_CENSUS_PERSON_CAP) {
            CcId id = NewId(sim, CC_ENTITY_CHARACTER);
            if (AddResident(sim, id, sim->current_day - 20 * 365,
                            town, false) == NULL) break;
            ++current;
        }
        for (int32_t i = census->resident_count - 1;
             i >= 0 && current > settlement->population; --i) {
            CcCensusResident *person = &census->residents[i];
            if (person->left_day != 0 || person->rich_identity ||
                person->sheltered ||
                census->districts[person->district_slot].settlement_id !=
                    settlement->id) continue;
            person->left_day = sim->current_day;
            --current;
        }
        for (int32_t i = census->resident_count - 1;
             i >= 0 && current > settlement->population; --i) {
            CcCensusResident *person = &census->residents[i];
            if (person->left_day != 0 || !person->rich_identity ||
                person->sheltered ||
                census->districts[person->district_slot].settlement_id !=
                    settlement->id) continue;
            person->sheltered = 1;
            --current;
        }
    }
    /* The live census owns current addresses. The historical cast keeps
       named life records; departed anonymous IDs remain spent in the serial. */
    int32_t living = 0;
    for (int32_t i = 0; i < census->resident_count; ++i)
        if (census->residents[i].left_day == 0)
            census->residents[living++] = census->residents[i];
    census->resident_count = living;
}

bool CcCensusValidate(const CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 114U) return sim != NULL;
    const CcCensus *census = &sim->census;
    if (sim->settlement_count < 1 ||
        sim->settlement_count > CC_MAX_SETTLEMENTS ||
        census->district_count < 0 ||
        census->district_count > CC_CENSUS_DISTRICT_CAP ||
        census->district_count != sim->settlement_count *
            CC_CENSUS_DISTRICTS_PER_TOWN ||
        census->resident_count < 0 ||
        census->resident_count > CC_CENSUS_PERSON_CAP) return false;
    int32_t offsets[CC_CENSUS_DISTRICT_CAP + 1] = {0};
    for (int32_t i = 0; i < census->district_count; ++i) {
        const CcCensusDistrict *district = &census->districts[i];
        if (CcIdKind(district->id) != CC_ENTITY_DISTRICT ||
            district->settlement_id != sim->settlements[
                i / CC_CENSUS_DISTRICTS_PER_TOWN].id ||
            district->name[0] == '\0' ||
            memchr(district->name, '\0', sizeof(district->name)) == NULL ||
            district->dwelling_count < 0 ||
            district->dwelling_count > CC_CENSUS_PERSON_CAP) return false;
        offsets[i + 1] = offsets[i] + district->dwelling_count;
    }
    uint8_t *occupancy = calloc((size_t)offsets[census->district_count], 1U);
    if (occupancy == NULL) return false;
    bool valid = true;
    for (int32_t i = 0; i < census->resident_count && valid; ++i) {
        const CcCensusResident *person = &census->residents[i];
        if ((i > 0 && person->id <= census->residents[i - 1].id) ||
            CcIdKind(person->id) != CC_ENTITY_CHARACTER ||
            person->birth_day > sim->current_day ||
            person->district_slot < 0 ||
            person->district_slot >= census->district_count ||
            person->dwelling_slot < 0 ||
            person->dwelling_slot >= census->districts[
                person->district_slot].dwelling_count ||
            person->left_day < 0 || person->left_day > sim->current_day ||
            (person->left_day != 0 && person->left_day < person->birth_day) ||
            (person->rich_identity != 0 && person->rich_identity != 1) ||
            (person->sheltered != 0 && person->sheltered != 1) ||
            (person->sheltered && !person->rich_identity)) {
            valid = false;
            break;
        }
        if (person->left_day == 0) {
            int32_t slot = offsets[person->district_slot] + person->dwelling_slot;
            if (++occupancy[slot] > 4U) valid = false;
        }
    }
    free(occupancy);
    if (!valid) return false;
    for (int32_t town = 0; town < sim->settlement_count; ++town)
        if (CcCensusPopulation(sim, sim->settlements[town].id) !=
            sim->settlements[town].population) return false;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *rich = &sim->characters[i];
        if (rich->birth_day > sim->current_day ||
            rich->death_day <= sim->current_day) continue;
        const CcCensusResident *person = CcCensusResidentById(sim, rich->id);
        if (person == NULL || person->left_day != 0 ||
            person->rich_identity != 1 ||
            person->birth_day != rich->birth_day ||
            census->districts[person->district_slot].settlement_id !=
                rich->home_settlement_id) return false;
    }
    return true;
}

static uint64_t HashValue(uint64_t hash, uint64_t value)
{
    for (int i = 0; i < 8; ++i) {
        hash = (hash ^ (value & UINT64_C(0xff))) * UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

uint64_t CcCensusHash(const CcCensus *census)
{
    if (census == NULL) return 0U;
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = HashValue(hash, (uint32_t)census->district_count);
    hash = HashValue(hash, (uint32_t)census->resident_count);
    for (int32_t i = 0; i < census->district_count; ++i) {
        const CcCensusDistrict *place = &census->districts[i];
        hash = HashValue(hash, place->id);
        hash = HashValue(hash, place->settlement_id);
        hash = HashValue(hash, (uint32_t)place->dwelling_count);
        for (size_t j = 0; j < sizeof(place->name); ++j)
            hash = HashValue(hash, (uint8_t)place->name[j]);
    }
    for (int32_t i = 0; i < census->resident_count; ++i) {
        const CcCensusResident *person = &census->residents[i];
        hash = HashValue(hash, person->id);
        hash = HashValue(hash, (uint32_t)person->birth_day);
        hash = HashValue(hash, (uint32_t)person->district_slot);
        hash = HashValue(hash, (uint32_t)person->dwelling_slot);
        hash = HashValue(hash, (uint32_t)person->left_day);
        hash = HashValue(hash, (uint32_t)person->rich_identity);
        hash = HashValue(hash, (uint32_t)person->sheltered);
    }
    return hash;
}

uint64_t CcCensusIssuedIdCount(const CcCensus *census)
{
    if (census == NULL) return 0U;
    uint64_t issued = (uint64_t)census->district_count;
    for (int32_t i = 0; i < census->resident_count; ++i)
        if (!census->residents[i].rich_identity) ++issued;
    return issued;
}

size_t CcCensusEncodedSize(const CcCensus *census)
{
    if (census == NULL || census->district_count < 0 ||
        census->district_count > CC_CENSUS_DISTRICT_CAP ||
        census->resident_count < 0 ||
        census->resident_count > CC_CENSUS_PERSON_CAP) return 0U;
    return 12U + (size_t)census->district_count * 52U +
        (size_t)census->resident_count * 32U;
}

static void Write32(uint8_t **at, uint32_t value)
{
    for (int i = 0; i < 4; ++i) *(*at)++ = (uint8_t)(value >> (8 * i));
}

static void Write64(uint8_t **at, uint64_t value)
{
    for (int i = 0; i < 8; ++i) *(*at)++ = (uint8_t)(value >> (8 * i));
}

static uint32_t Read32(const uint8_t **at)
{
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= (uint32_t)*(*at)++ << (8 * i);
    return value;
}

static uint64_t Read64(const uint8_t **at)
{
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) value |= (uint64_t)*(*at)++ << (8 * i);
    return value;
}

size_t CcCensusEncode(const CcCensus *census, uint8_t *bytes, size_t capacity)
{
    size_t needed = CcCensusEncodedSize(census);
    if (needed == 0U || bytes == NULL || capacity < needed) return 0U;
    uint8_t *at = bytes;
    Write32(&at, 2U);
    Write32(&at, (uint32_t)census->district_count);
    Write32(&at, (uint32_t)census->resident_count);
    for (int32_t i = 0; i < census->district_count; ++i) {
        const CcCensusDistrict *place = &census->districts[i];
        Write64(&at, place->id);
        Write64(&at, place->settlement_id);
        memcpy(at, place->name, sizeof(place->name)); at += sizeof(place->name);
        Write32(&at, (uint32_t)place->dwelling_count);
    }
    for (int32_t i = 0; i < census->resident_count; ++i) {
        const CcCensusResident *person = &census->residents[i];
        Write64(&at, person->id);
        Write32(&at, (uint32_t)person->birth_day);
        Write32(&at, (uint32_t)person->district_slot);
        Write32(&at, (uint32_t)person->dwelling_slot);
        Write32(&at, (uint32_t)person->left_day);
        Write32(&at, (uint32_t)person->rich_identity);
        Write32(&at, (uint32_t)person->sheltered);
    }
    return (size_t)(at - bytes);
}

bool CcCensusDecode(CcCensus *census, const uint8_t *bytes, size_t length)
{
    if (census == NULL || bytes == NULL || length < 12U) return false;
    const uint8_t *at = bytes;
    uint32_t version = Read32(&at);
    uint32_t districts = Read32(&at);
    uint32_t residents = Read32(&at);
    if (version != 2U || districts > CC_CENSUS_DISTRICT_CAP ||
        residents > CC_CENSUS_PERSON_CAP ||
        length != 12U + (size_t)districts * 52U +
                  (size_t)residents * 32U) return false;
    *census = (CcCensus){0};
    census->district_count = (int32_t)districts;
    census->resident_count = (int32_t)residents;
    for (uint32_t i = 0; i < districts; ++i) {
        CcCensusDistrict *place = &census->districts[i];
        place->id = Read64(&at);
        place->settlement_id = Read64(&at);
        memcpy(place->name, at, sizeof(place->name)); at += sizeof(place->name);
        place->dwelling_count = (int32_t)Read32(&at);
    }
    for (uint32_t i = 0; i < residents; ++i) {
        CcCensusResident *person = &census->residents[i];
        person->id = Read64(&at);
        person->birth_day = (int32_t)Read32(&at);
        person->district_slot = (int32_t)Read32(&at);
        person->dwelling_slot = (int32_t)Read32(&at);
        person->left_day = (int32_t)Read32(&at);
        person->rich_identity = (int32_t)Read32(&at);
        person->sheltered = (int32_t)Read32(&at);
    }
    return (size_t)(at - bytes) == length;
}
