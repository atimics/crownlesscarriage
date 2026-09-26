/* The Return, milestone 3: a resident at the gate says what changed
   (docs/design/the-return.md). The words come from src/story/cc_gate_voice.c;
   this file only shows them and offers one follow-up. Marking a story as told
   is a journalled command, applied when the line is spoken. */

/* How near the player must be to the staged speaker spot for the speaker to
   stand there (world units). */
#define GATE_VOICE_SPOT_REACH 6.0f

/* Stand the speaker in the scene. This runs in the update path; the renderer
   only draws what the course holds. */
static void StageGateSpeaker(const CcSim *sim, LocalState *local)
{
    const CcCharacter *person = CcSimCharacter(sim, local->gate_voice.speaker_id);
    if (!local->gate_voice_open || person == NULL) {
        CcLocalCourseClearGateSpeaker(&local->course);
        return;
    }
    CcNpcRole role = person->role == CC_CHARACTER_OFFICIAL ? CC_NPC_ROLE_GUARD :
        person->role == CC_CHARACTER_SCOUT ? CC_NPC_ROLE_SCOUT :
        person->role == CC_CHARACTER_TRAVELLER || person->role == CC_CHARACTER_COURIER ?
            CC_NPC_ROLE_TRAVELLER :
        person->role == CC_CHARACTER_REFUGEE ? CC_NPC_ROLE_REFUGEE :
        person->occupation == CC_OCCUPATION_INNKEEPER ||
        person->occupation == CC_OCCUPATION_BAKER ? CC_NPC_ROLE_MERCHANT :
            CC_NPC_ROLE_LABORER;
    /* Stand at the spot the arrival staging (milestone 2) reserved by the
       gate, when the player is there to hear it. A player already parked
       deep in town is spoken to where they stand instead: the conversation
       camera frames both people, and a speaker back at the gate would put
       buildings between them. */
    Vector2 spot = {local->return_scene.speaker.x, local->return_scene.speaker.z};
    bool at_gate = local->return_scene.placed &&
        hypotf(spot.x - local->agent.position.x,
               spot.y - local->agent.position.z) <
            GATE_VOICE_SPOT_REACH;
    CcLocalCourseStageGateSpeaker(&local->course, &local->agent,
                                  at_gate ? &spot : NULL,
                                  person->id, person->appearance_seed, role);
}

static void GateVoiceSpeak(CcJournal *journal, CcSim *sim, LocalState *local,
                           bool audio)
{
    CcCommand told;
    char note[192];
    if (CcGateVoiceToldCommand(&local->gate_voice, &told))
        (void)ApplyCommand(journal, sim, told, note, sizeof(note));
    local->gate_voice.more = CcGateVoiceHasMore(sim, &local->gate_voice);
    StageGateSpeaker(sim, local);
    CcSpeech speech;
    if (audio && CcGateVoiceSpeech(sim, &local->gate_voice, &speech)) {
        /* The voice seam: the same turn feeds subtitles and audio. */
        CcAudioClearSpeech();
        ClientSaySpeech(&speech);
    }
}

/* Open the gate voice when the company parks in a town it has seen before. */
static void BeginGateVoice(CcJournal *journal, CcSim *sim, LocalState *local,
                           bool audio)
{
    local->gate_voice_open = !sim->journey.active &&
        CcGateVoiceBegin(sim, sim->player.location_id, &local->gate_voice);
    if (local->gate_voice_open) GateVoiceSpeak(journal, sim, local, audio);
}

static void GateVoiceName(const LocalState *local, char *name, size_t capacity)
{
    const CcGateVoice *voice = &local->gate_voice;
    (void)snprintf(name, capacity, "%s%s%s", voice->speaker,
                   voice->speaker_label[0] != '\0' ? ", " : "", voice->speaker_label);
}

static Rectangle GateVoiceBubble(const LocalState *local)
{
    float width = fminf(720.0f, (float)GetScreenWidth() - 80.0f);
    float text = (float)AdventureText(local->gate_voice.line, 0, 0, (int)width - 44,
                                      AdventureTextSize(20), INK, false);
    float height = 68.0f + text + 58.0f;
    return (Rectangle){((float)GetScreenWidth() - width) * 0.5f,
        (float)GetScreenHeight() - 108.0f - height, width, height};
}

/* Option 0 is "Tell me more" when there is more; the last is "Thank you". */
static Rectangle GateVoiceOption(Rectangle bubble, int32_t index)
{
    return (Rectangle){bubble.x + 22.0f + (float)index * 212.0f,
        bubble.y + bubble.height - 52.0f, 200.0f, 38.0f};
}

static bool HandleGateVoiceInput(CcJournal *journal, CcSim *sim,
                                 LocalState *local, ClientView view)
{
    if (!local->gate_voice_open) {
        if (local->course.gate_speaker_active) CcLocalCourseClearGateSpeaker(&local->course);
        return false;
    }
    if (view != VIEW_LOCAL || sim->journey.active ||
        sim->player.location_id != local->gate_voice.settlement_id) {
        local->gate_voice_open = false;
        CcLocalCourseClearGateSpeaker(&local->course);
        return false;
    }
    StageGateSpeaker(sim, local);
    Rectangle bubble = GateVoiceBubble(local);
    bool more = local->gate_voice.more;
    int32_t thanks_index = more ? 1 : 0;
    if (more && (ClientKeyPressed(KEY_ONE) || AdventureHit(GateVoiceOption(bubble, 0)))) {
        if (CcGateVoiceNext(sim, &local->gate_voice))
            GateVoiceSpeak(journal, sim, local, true);
        else
            local->gate_voice_open = false;
        StageGateSpeaker(sim, local);
        return true;
    }
    if (ClientKeyPressed(more ? KEY_TWO : KEY_ONE) || ClientKeyPressed(KEY_ENTER) ||
        ClientKeyPressed(KEY_ESCAPE) || ClientKeyPressed(KEY_BACKSPACE) ||
        AdventureHit(GateVoiceOption(bubble, thanks_index))) {
        CcAudioClearSpeech();
        local->gate_voice_open = false;
        CcLocalCourseClearGateSpeaker(&local->course);
        return true;
    }
    /* A short exchange at the gate: it holds the input until it ends. */
    return true;
}

static void DrawGateVoice(const LocalState *local)
{
    if (!local->gate_voice_open) return;
    const CcGateVoice *voice = &local->gate_voice;
    char name[2 * CC_NAME_CAPACITY + 4];
    GateVoiceName(local, name, sizeof(name));
    ClientTouchHeading(name, voice->line);
    Rectangle bubble = GateVoiceBubble(local);
    DrawPanel(bubble, Fade(PANEL_DEEP, 0.95f));
    CcOverlayDrawText(name, (int)bubble.x + 22, (int)bubble.y + 16,
                      AdventureTextSize(20), CC_GOLD);
    (void)AdventureWrap(voice->line, (int)bubble.x + 22, (int)bubble.y + 50,
                        (int)bubble.width - 44, AdventureTextSize(20), INK);
    int32_t index = 0;
    if (voice->more)
        AdventureButton(GateVoiceOption(bubble, index++), "1  Tell me more", true, false);
    AdventureButton(GateVoiceOption(bubble, index),
                    voice->more ? "2  Thank you" : "1  Thank you", true, false);
}
