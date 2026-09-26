/* The Return, milestone 4: news met on the road (docs/design/the-return.md).
   The sim records what the company meets as the carriage moves
   (sim/cc_road_news.h); the words come from src/story/cc_road_voice.c. This
   file keeps the current line on the update path and shows it on the travel
   view. It changes nothing in the world. */

/* Keep the newest piece of road news of this leg ready for the travel view.
   Built only when a new piece appears; a traveller's line is also spoken. */
static void UpdateRoadNews(const CcSim *sim, LocalState *local, bool audio)
{
    CcId town = 0U;
    const CcRoadNews *news = CcRoadNewsLatest(sim, &town);
    if (news == NULL) {
        local->road_news_ready = false;
        return;
    }
    if (local->road_news_ready && local->road_news.town == town &&
        local->road_news.news.tick == news->tick &&
        local->road_news.news.channel == news->channel) return;
    local->road_news_ready = CcRoadVoiceBuild(sim, town, news, &local->road_news);
    CcSpeech speech;
    if (local->road_news_ready && audio &&
        CcRoadVoiceSpeech(sim, &local->road_news, &speech)) ClientSaySpeech(&speech);
}

/* A card low on the travel view: who or what, then the line. */
static void DrawRoadNews(const CcSim *sim, const LocalState *local)
{
    if (!local->road_news_ready || !sim->journey.active) return;
    const CcRoadVoice *voice = &local->road_news;
    char name[2 * CC_NAME_CAPACITY + 4];
    (void)snprintf(name, sizeof(name), "%s%s%s", voice->speaker,
                   voice->speaker_label[0] != '\0' ? ", " : "", voice->speaker_label);
    ClientTouchHeading(name, voice->line);
    float width = fminf(680.0f, (float)GetScreenWidth() - 80.0f);
    int text_size = AdventureTextSize(20);
    float text = (float)AdventureText(voice->line, 0, 0, (int)width - 44,
                                      text_size, INK, false);
    float height = 58.0f + text + 20.0f;
    Rectangle card = {((float)GetScreenWidth() - width) * 0.5f,
                      (float)GetScreenHeight() - 128.0f - height, width, height};
    DrawPanel(card, Fade(PANEL_DEEP, 0.93f));
    Color heading = voice->channel == CC_ROAD_NEWS_WITNESSED ? CC_STYLE_PARCHMENT : CC_GOLD;
    CcOverlayDrawText(name, (int)card.x + 22, (int)card.y + 16,
                      AdventureTextSize(18), heading);
    (void)AdventureWrap(voice->line, (int)card.x + 22, (int)card.y + 46,
                        (int)card.width - 44, text_size, INK);
}
