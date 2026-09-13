/* Spoken forms of world roots; see docs/design/character-name-roots.md.
 * Keep each form authored so names remain short and easy to say. */
typedef struct CcNameRoot {
    uint32_t places;
    const char *forms[4];
} CcNameRoot;

#define NAME_FARM (UINT32_C(1) << CC_SETTLEMENT_FARMING)
#define NAME_MINE (UINT32_C(1) << CC_SETTLEMENT_MINING)
#define NAME_MARKET (UINT32_C(1) << CC_SETTLEMENT_MARKET)
#define NAME_WATCH (UINT32_C(1) << CC_SETTLEMENT_FORTRESS)
#define NAME_COURT (UINT32_C(1) << CC_SETTLEMENT_CAPITAL)
#define NAME_FRONTIER (UINT32_C(1) << CC_SETTLEMENT_DUNGEON_TOWN)

static const CcNameRoot GIVEN_NAME_ROOTS[] = {
    /* alder: shelter */
    {NAME_FARM | NAME_WATCH, {"Alda", "Aldis", "Aldren", "Aldwyn"}},
    /* thorn: endurance */
    {NAME_FARM | NAME_WATCH, {"Thora", "Thoren", "Thorna", "Thoris"}},
    /* reed: bending through hardship */
    {NAME_FARM | NAME_MARKET, {"Reida", "Reiden", "Reidel", "Reidwen"}},
    /* rill: running water */
    {NAME_FARM | NAME_MARKET, {"Rilla", "Rillen", "Rilsa", "Rilwen"}},
    /* willow: recovery */
    {NAME_FARM, {"Willa", "Willen", "Willet", "Wilna"}},
    /* rowan: a fruitful home */
    {NAME_FARM | NAME_COURT, {"Rowan", "Rowena", "Rowen", "Roweth"}},
    /* grain: enough to eat */
    {NAME_FARM, {"Grena", "Grenna", "Grenel", "Grenwin"}},
    /* ford: a safe crossing */
    {NAME_FARM | NAME_WATCH, {"Forda", "Forden", "Fordis", "Foryn"}},
    /* ferry: a welcome on the far bank */
    {NAME_FARM | NAME_MARKET, {"Ferra", "Ferren", "Ferris", "Ferwen"}},
    /* road: an open way */
    {NAME_MARKET | NAME_FRONTIER, {"Roda", "Roden", "Rodric", "Rodwin"}},
    /* tally: fair dealing */
    {NAME_MARKET | NAME_COURT, {"Talla", "Tallen", "Tallis", "Talwen"}},
    /* quill: remembrance */
    {NAME_MARKET | NAME_COURT, {"Quilla", "Quillen", "Quillan", "Quill"}},
    /* ink: a lasting mark */
    {NAME_MARKET | NAME_COURT, {"Inka", "Inken", "Inga", "Ingel"}},
    /* gate: hospitality */
    {NAME_MARKET | NAME_WATCH, {"Gatha", "Gathel", "Gatten", "Gatwin"}},
    /* oath: fidelity */
    {NAME_COURT | NAME_WATCH, {"Otha", "Othen", "Othric", "Othwen"}},
    /* ward: care of others */
    {NAME_WATCH | NAME_FRONTIER, {"Warda", "Wardis", "Waryn", "Warrin"}},
    /* chain: strength together */
    {NAME_WATCH | NAME_MINE, {"Chena", "Chenet", "Chenna", "Chenric"}},
    /* shield: protection */
    {NAME_WATCH, {"Skella", "Skellen", "Skeld", "Skelda"}},
    /* hearth: warmth shared */
    {NAME_FARM | NAME_MINE | NAME_FRONTIER, {"Harth", "Hartha", "Harthwin", "Harthild"}},
    /* ember: life through winter */
    {NAME_MINE | NAME_COURT, {"Embra", "Embren", "Emren", "Emberis"}},
    /* flint: a spark in the dark */
    {NAME_MINE | NAME_WATCH, {"Flinta", "Flinten", "Flinn", "Flinna"}},
    /* stone: steadiness */
    {NAME_MINE | NAME_FRONTIER, {"Stenna", "Sten", "Stellan", "Stenric"}},
    /* silver: brightness underground */
    {NAME_MINE | NAME_COURT, {"Silva", "Silven", "Silra", "Silwyn"}},
    /* forge: skill of hand */
    {NAME_MINE, {"Forga", "Forgen", "Forren", "Forra"}},
    /* cinder: a fresh start */
    {NAME_MINE | NAME_FRONTIER, {"Cinda", "Cindel", "Cindren", "Cindra"}},
    /* lamp: guidance */
    {NAME_MINE | NAME_FRONTIER, {"Lamma", "Lammen", "Lamra", "Lamwyn"}},
    /* rose: grace */
    {NAME_COURT, {"Rosa", "Rosel", "Roslin", "Roswyn"}},
    /* spire: high hopes */
    {NAME_COURT, {"Spira", "Spiren", "Spirel", "Spiro"}},
    /* star: finding the way home */
    {NAME_COURT | NAME_FRONTIER, {"Estra", "Estren", "Estel", "Estwyn"}},
    /* bell: a gathering */
    {NAME_COURT | NAME_MARKET, {"Bela", "Bellen", "Bellis", "Belva"}},
    /* barrow: kin remembered */
    {NAME_FRONTIER, {"Barra", "Barret", "Barwen", "Barin"}},
    /* hollow: shelter in the earth */
    {NAME_FRONTIER, {"Holla", "Hollen", "Holra", "Hollis"}}
};

static const CcNameRoot FAMILY_NAME_ROOTS[] = {
    /* Thornford: crossing, mill, fields, carts */
    {NAME_FARM, {"Fordward", "Thornbank", "Shallowford", "Miller"}},
    {NAME_FARM, {"Millward", "Millrace", "Rillbank", "Underwheel"}},
    {NAME_FARM, {"Sheafbinder", "Barleycroft", "Hedgerow", "Longmead"}},
    {NAME_FARM, {"Drover", "Cartwright", "Oxley", "Wainward"}},
    /* Silverwick: pit, furnace, stone, lamps */
    {NAME_MINE, {"Deepwell", "Shaftward", "Underwick", "Silvervein"}},
    {NAME_MINE, {"Furnace", "Cinderhand", "Bellows", "Emberwright"}},
    {NAME_MINE, {"Stonecutter", "Flintbeck", "Stonehewer", "Slagfield"}},
    {NAME_MINE, {"Lampwright", "Wickman", "Windlass", "Orehauler"}},
    /* Gloamgate: road, market, archive, customs */
    {NAME_MARKET, {"Wayhouse", "Roadward", "Wheelwright", "Coachman"}},
    {NAME_MARKET, {"Tallyman", "Fairweight", "Bondkeeper", "Longreckon"}},
    {NAME_MARKET, {"Quill", "Inkwell", "Bookbinder", "Parchman"}},
    {NAME_MARKET, {"Gateward", "Tollman", "Gloamward", "Sealwright"}},
    /* Alderwatch: bridge, watch, muster, arms */
    {NAME_WATCH, {"Bridgeward", "Chainward", "Eastspan", "Alderbridge"}},
    {NAME_WATCH, {"Watchman", "Alderward", "Highwall", "Lanternwatch"}},
    {NAME_WATCH, {"Muster", "Rollkeeper", "Bannerward", "Oathkeeper"}},
    {NAME_WATCH, {"Shieldwright", "Bowyer", "Fletcher", "Mailmender"}},
    /* Rosespire: gardens, procession, petitions, treasury */
    {NAME_COURT, {"Rosewell", "Rosethorn", "Rosebower", "Spirebank"}},
    {NAME_COURT, {"Bellward", "Highspire", "Starward", "Herald"}},
    {NAME_COURT, {"Petitioner", "Sealkeeper", "Faircopy", "Courtward"}},
    {NAME_COURT, {"Crownweigh", "Goldweight", "Coffer", "Purseward"}},
    /* Hollowbarrow: old tombs, expeditions, return, salvage */
    {NAME_FRONTIER, {"Barrowward", "Holloway", "Oldstone", "Graveward"}},
    {NAME_FRONTIER, {"Lanternward", "Ropeman", "Longroad", "Wayfinder"}},
    {NAME_FRONTIER, {"Returnbond", "Surety", "Threshold", "Homeward"}},
    {NAME_FRONTIER, {"Salvager", "Relicward", "Oldiron", "Ashfinder"}}
};

static const char *RootedNameForm(const CcNameRoot *roots, size_t count,
                                  int32_t place_function, uint32_t seed)
{
    uint32_t place = place_function >= CC_SETTLEMENT_FARMING &&
        place_function <= CC_SETTLEMENT_DUNGEON_TOWN ?
        UINT32_C(1) << (uint32_t)place_function : 0U;
    /* One in four rooted names has travelled from the wider world. */
    if ((seed & 3U) == 0U) place = 0U;
    size_t eligible = 0U;
    for (size_t i = 0U; i < count; ++i) {
        if (place == 0U || (roots[i].places & place) != 0U) ++eligible;
    }
    if (eligible == 0U) {
        place = 0U;
        eligible = count;
    }
    size_t choice = MixCharacterSeed(seed ^ UINT32_C(0x64e7a91d)) % eligible;
    for (size_t i = 0U; i < count; ++i) {
        if (place != 0U && (roots[i].places & place) == 0U) continue;
        if (choice-- == 0U) return roots[i].forms[(seed >> 2U) & 3U];
    }
    return roots[0].forms[0];
}

#undef NAME_FARM
#undef NAME_MINE
#undef NAME_MARKET
#undef NAME_WATCH
#undef NAME_COURT
#undef NAME_FRONTIER
