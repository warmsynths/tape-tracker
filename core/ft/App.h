#ifndef FT_APP_H_
#define FT_APP_H_

#include "Canvas.h"
#include "FourTrackSession.h"
#include "Gfx.h"
#include "Hal.h"
#include "Mask.h"
#include "Chords.h"
#include "Roles.h"
#include "Latency.h"
#include "View.h"
#include "loop_bars.h"

namespace ft
{
// What the firmware is called on its own screens (Config, About). Change it here.
constexpr const char* kProductName = "Four-Track Recorder";
// The longest first take (and so the longest loop) in seconds. The memory behind each track is 2 s more.
constexpr uint32_t kMaxTakeSeconds = 16;
// Buttons and keys of the Tracker that the application reacts to. The eight buttons under the
// screen are B1 to B8. Shift is held, not pressed: see App::setShift.
enum Key
{
    kB1, kB2, kB3, kB4, kB5, kB6, kB7, kB8,
    kUp, kDown, kLeft, kRight,
    kRec, kPlay, kStop,
    kJogPress,
    kSampleEdit,   // opens the Part Editor
    kPerformance,  // opens Glitch and live effects
    kMaster,       // opens the lo-fi master
    kFile,         // opens Project
    kSong,         // not used (the click is Shift + Play)
    kPattern,      // back to the main screen from any page
    kParams,       // the track mixer
    kSamplePlay,   // settings of the pad sample
    kSampleLoad,   // load samples from the SD card
    kSampleRec,    // resample the mix into a pad slot
    kConfig,       // device settings
    kCopy,         // copy a Part to another track
    kDelete,       // clear a Part (Shift: all)
    kNote,         // chords and scale notes on the pads
    kInstr,        // the sound of the chord pads: patches and tweaks
    kFx1,          // reverb and delay sends
    kFx2,          // live filter
    kKeyCount
};

enum class Screen : uint8_t
{
    Main, Tempo, Editor, Glitch, Master, Input, Project, NameEntry, Settings, Export, Pads, Memory, Mixer, PadSample, Load, Resample, Device, Chords, Sound, SoundAdv, Calibrate, Sets, Play, Fx, Filter, Role, RoleSound, ArmLevel
};

enum class Popup : uint8_t
{
    None, Replace, NoSound, NotArmed, SdMissing, Unsaved, Message, ReplaceSlot, Copy, Clear, Busy, Bounce
};

struct Settings
{
    bool loopOnce = false;     // project: Loop Mode
    bool click = false;        // project: click while playing
    uint8_t windowIdx = 2;     // device: alignment window 0, 30, 60, 100, 150 ms
    uint8_t preRollIdx = 2;    // device: pre-roll 0, 60, 120, 250, 500 ms
    uint8_t roundIdx = 0;      // device: tempo rounding off, 0.1, 1
    uint16_t tempoTenths = 0;  // starting tempo in tenths of a BPM, 0: Free
    uint8_t countIdx = 3;      // count-in: 0 Off, 1 beat, 2 beats, 4 beats, 8 beats
    uint8_t bars = 0;          // project: bars of the first take at a set tempo, 0: asked afterwards
    bool tailWrap = true;      // device: a note's tail past the loop end is folded onto the start
    bool roleFlow = false;     // Rec on an empty track asks what to record (Chords, Bass, Melody, Drums, Audio); real machines only
    bool layer = true;         // device: a later take on a track that has a Part is laid over it (false: replaces it)
    uint8_t fadePct = 100;     // device: how much of the older layers stays under a new pass, 40 to 100
    bool waitSound = false;    // project: the first take waits for the input to get loud
    int8_t waitDb = -36;       // device: how loud, in dB
    int countBeats() const { static const int b[5] = { 0, 1, 2, 4, 8 }; return b[countIdx > 4 ? 4 : countIdx]; }
    bool monitorLine = true;   // device: monitoring default for line
    bool monitorMic = false;   // device: monitoring default for the mics
    uint8_t gridIdx = 4;       // project: move and snap grid: 0 Off (1 ms), 1 Bar, 2 1/4, 3 1/8, 4 1/16
    bool snapZero = true;      // project: cut points land on a zero crossing
    bool waveBitmap = false;   // device: waveforms as bitmaps in the display chip (plan D); false: columns (plan A)
    uint8_t homeGrid = 2;      // device: lines over the lanes: 0 off, 1 bars, 2 bars and beats, 3 also sixteenths
};

// The Four-Track application: every screen and everything behind them. No hardware in here: it
// talks to the machine only through Hal and draws only through Gfx. The Tracker firmware and
// the web emulator both run exactly this code.
class App
{
  public:
    App(Hal& hal, const Palette& palette);

    // The screen was opened or closed.
    void enter();
    void leave();

    // Input
    void key(int key);
    void keyUp(int key); // releases matter for the held effect buttons on the Glitch screen
    void jog(int delta);
    void setShift(bool held) { shift = held; }
    const ChordCfg& chordSettings() const { return chords.cfg; }
    int32_t inputLatencyFrames() const { return latencyFrames; }
    void pad(int pad, bool down, int velocity = 0); // velocity 1 to 127 if the pads send one, 0 if not

    // One pass of the main loop.
    void tick();

    // Drawing. Layers 0 to 3 are the main screen (View.h); layer 4 is the page of any other
    // screen, layer 5 a popup. A layer is drawn in stages. takeRedraw returns true if the layer
    // has to be redrawn since the last call and clears the flag.
    bool takeRedraw(int layer);
    bool layerVisible(int layer) const;
    int stagesOf(int layer) const;
    void draw(int layer, int stage, Gfx& gfx) const;
    void setMeasure(Gfx* g) { page.setMeasure(g); popupPage.setMeasure(g); }
    const View& view() const { return model; }
    void setPalette(const Palette& palette) { pal = palette; dirtyAll(); }

    const FourTrackSession& state() const { return session; }
    Screen screen() const { return current; }
    Popup popup() const { return popupKind; }
    const Settings& settings() const { return set; }
    Settings& settingsForTest() { return set; }
    int chordEventCount() const { return chordEvCount; }
    uint32_t chordEventFrame(int i) const { return chordEv[i].frame; }
    bool waiting() const { return waitingSound; }
    bool pageOverflowed() const { return page.overflowed() || popupPage.overflowed(); }
    int pageStages() const { return page.stageCount(); }

  private:
    // ---- main screen
    void pressRecord();
    void startRecording(); // after the replace question and the count-in
    void pressStop();
    void pressPlay();
    void togglePlay();
    void selectTrack(int track);
    void moveSelection(int delta);
    void promptMove(int delta);
    void promptConfirm();
    void promptCancel();
    void togglePadMode();
    void muteTrack(int track);
    void undoTake();
    void applySource(Source source);
    bool monitorFor(Source source) const { return source == Source::Line ? set.monitorLine : (source == Source::Pads ? false : set.monitorMic); }
    void startPlayer();
    void syncPlayer();
    void onFirstTakeFinished();
    void serviceFirstTake();
    void serviceArmed();
    void servicePartRecording();
    void finishPart(int track, uint32_t firstSample, bool early);
    void cancelRecording();
    void serviceClick();
    void serviceCountIn();
    bool updateTrackMeters();
    void computeWave(int track);
    void fillView();
    void refresh(bool force);
    void applyAlignment(int track, uint32_t firstSample);
    void pushBank();
    void syncTrackCfg(int track);
    uint32_t windowFrames() const;
    uint32_t preRollFrames() const;
    static const char* sourceName(Source source);

    // ---- other screens
    void open(Screen s);
    void back();
    void keyScreen(int k);
    void jogScreen(int delta);
    void buildPage();
    void drawTempo(class Pen& p);
    void drawEditor(class Pen& p);
    void drawGlitch(class Pen& p);
    void drawMaster(class Pen& p);
    void drawInput(class Pen& p);
    void drawProject(class Pen& p);
    void drawNameEntry(class Pen& p);
    void drawSettings(class Pen& p);
    void drawExport(class Pen& p);
    void drawPads(class Pen& p);
    void drawMemory(class Pen& p);
    void drawMixer(class Pen& p);
    void drawPadSample(class Pen& p);
    // Chords page and chord pads
    void drawChords(class Pen& p);
    void drawPlayView(class Pen& p);
    // Track roles (Roles.h): what you record decides what the pads are
    TrackRole trackRole[4] = {};
    bool roleChosen[4] = {};
    TrackRole lastRole = TrackRole::Chords;
    uint8_t roleSound[kRoleCount] = { 0, 8, 10, 0, 0 }; // the patch of Chords, Bass and Melody and the kit of Drums
    int roleList = 0;                    // the row in focus on the sound list
    int roleOct[kRoleCount] = {};        // Oct - and Oct + for Bass and Melody, in octaves
    uint8_t roleMode = 0;                // Melody: 0 scale, 1 pentatonic, 2 blues
    bool roleGlide = false, roleMono = true, roleHold = false, roleChordLock = false, roleRepeat = false;
    uint8_t roleMods = 0;                // held: bit 0 Accent, 1 Ghost, 2 Short, 3 Soft
    int8_t roleSounding[48] = {};        // the note a role pad is sounding, 0 none
    uint32_t roleOffAt[48] = {};         // Short: when a note ends by itself
    bool roleLatched[48] = {};
    int drumRate = -1, drumRateHeld = -1; // note repeat rate of the drum pads: latched, or held now
    bool drumLatch = false, drumSoft = false;
    uint8_t drumSwing = 50;              // percent, 50 is straight
    int repPad = -1, repStep = 0;        // the pad that is repeating
    uint32_t repNext = 0;
    void applyRole(int track, TrackRole role);
    void roleSoundApply(TrackRole role);
    int roleSoundCount(TrackRole role) const;
    int roleSoundId(TrackRole role, int listIndex) const;
    const char* roleSoundName(TrackRole role, int id) const;
    void roleKey(int k);
    void roleSoundKey(int k);
    void roleSoundJog(int delta);
    void rolePadPress(int p, bool down, int velocity);
    void roleAllOff();
    void serviceRoleRepeat(uint32_t now);
    int roleLight(int p) const;
    // The chord changes recorded with a Chords take (the chord track): Bass and Melody follow them
    static constexpr int kMaxChordEvents = 64;
    struct ChordEv { uint32_t frame; uint8_t count; uint8_t notes[kMaxChordNotes]; };
    ChordEv chordEv[kMaxChordEvents] = {};
    int chordEvCount = 0;
    int roleCurrentChord(uint8_t* out) const;
    void chordEventsWrite(const char* name);
    void chordEventsRead(const char* name);
    void drawRole(class Pen& p);
    void drawRoleSound(class Pen& p);
    void drawRolePad(class Pen& p, int pad, int px, int py, int pw, int ph);
    TrackRole selectedRole() const { return trackRole[session.selected()]; }
    void drawArmLevel(class Pen& p);
    void armLevelOpen();
    void drawFx(class Pen& p);
    void drawFilter(class Pen& p);
    void fxKey(int k);
    void fxJog(int delta);
    void filterKey(int k);
    void filterJog(int delta);
    void fxPush();                // the delay time from the tempo, and the settings to the audio side
    int fxBlock = 0, fxRow = 0;   // FX1 page: 0 reverb, 1 delay; the row in the block (4 settings, then the 4 sends)
    // New projects start at 80 BPM, 4 bars and a count-in of one bar (the owner's default); Free stays one button away
    void newProjectDefaults();
    uint32_t autoFrames() const;          // length of a first take with the bars set up front, 0 when it is open
    void finishTail(bool all);            // stop recording the tail past the loop end and fold it onto the start
    void serviceTail();
    void scrub(int steps, bool bars);   // move the playhead: a sixteenth a step, or a bar
    void stepBeat(int dir);
    uint32_t scrubPos = 0;              // the playhead while stopped
    // Punch: only part of the loop is re-recorded (set in move mode, B5 and B6), and "Double" repeats the loop
    uint32_t punchIn = 0, punchOut = 0;
    bool punchOn() const { return punchOut > punchIn; }
    bool autoMuteOn[4] = {}, autoMuteWas[4] = {};
    void doubleLoop();
    bool barsMode = false;      // changing how many bars the loop is
    int barsPending = 0;
    void barsKey(int k);
    void setLoopBars(int bars);
    void punchSet(bool in);
    void endAutoMute();
    uint32_t sixteenth() const { return session.bars() > 0 ? session.loopFrames() / ((uint32_t)session.bars() * 16) : 1; }
    void finishLayer(int track, uint32_t firstSample);
    void mixLayer(int track, uint32_t firstSample, uint32_t recorded);
    int layerScratchTrack(int track) const;
    int layerScratch[4] = { -1, -1, -1, -1 }; // the empty track whose memory a layer is recorded into
    uint32_t layerMixed = 0;                  // frames of the take already added to the Part
    void loopCfgSave();
    void loopCfgLoad();
    bool layerPass[4] = {};     // the take being recorded is laid over the Part that was there
    int barsFit(int tempoTenths) const;   // the most bars that fit one Part at this tempo
    int barsStep(int from, int dir) const;

    bool onHome() const { return current == Screen::Main || current == Screen::Play; }
    void chordsKey(int k);
    void chordsJog(int delta);
    void chordPad(int p, bool down, int velocity = 0);
    void chordsOff();
    void chordsReleaseAll();
    void chordNote(int pad, int index, int note, int velocity, bool on);
    Chords chords;
    bool padsChords = false;      // the pads play chords and scale notes
    int chField = 0;              // chords page: 0 key, 1 scale, 2 style, 3 sound, 4 voicing, 5 octave
    int chSound = 0;              // 0 .. kSynthPresets - 1 the patches, then kSynthPresets: the pad sample
    // Sound page (Instr): a patch, eight tweak knobs on top, and every raw setting on the Advanced page
    // Input latency: how late line and mic recordings arrive, measured with a cable from the output to the line input
    void drawCalibrate(class Pen& p);
    void calibrateOpen();
    void calibrateKey(int k);
    void calibrateStart();
    void calibrateAbort();
    void calibrateRestore();
    void serviceCalibrate();
    bool calibrateAllowed() const;
    void latencySave();
    void latencyLoad();
    void latencyMark() { latencyDirty = true; latencyDirtyAt = hal.millis(); }
    enum class CalState : uint8_t { Ready, Running, Done, Failed };
    CalState calState = CalState::Ready;
    int calWhy = 0;
    uint32_t calStart = 0, calDrawAt = 0;
    int calPercent = 0;
    LatencyResult calResult;
    TrackCfg calSaveCfg;
    bool calSaveBypass = false;
    int32_t latencyFrames = 0;    // line and mic Parts are moved this much earlier
    bool latencyDirty = false;
    uint32_t latencyDirtyAt = 0;
    void drawSound(class Pen& p);
    void drawSoundAdv(class Pen& p);
    void soundArtSync();          // uploads the Sound screen's pictures to the display chip when they are out of date
    bool artStatic = false;       // dial ring and patch shapes are in the chip
    uint32_t artSig = 0;          // which patch the note picture shows
    void soundKey(int k);
    void soundJog(int delta);
    void soundAdvKey(int k);
    void soundAdvJog(int delta);
    void soundApply();
    void synthLevelStep(int d);   // the synth level from Left and Right on the home screen, and the Chords page
    int synthDb10 = 0;            // sends the patch with the knobs applied to the synth
    void soundPick(int preset);
    void soundHear(bool on);
    bool soundEdited() const;     // the raw settings differ from the patch
    void soundMark();             // something changed that is worth saving
    // Chord sets: 8 slots on the card
    void drawSets(class Pen& p);
    void drawChordPad(class Pen& p, int pad, int px, int py, int pw, int ph);
    void setsKey(int k);
    void setsJog(int delta);
    void setsFileRead();
    void setsFileWrite();
    void setsSummary(const uint8_t* blob, char* out, int size) const;
    int setSel = 0;
    int setLoaded = -1;
    bool setUsed[8] = {};
    char setNames[8][14] = {};
    uint8_t setData[8][138] = {};
    char lastChordName[16] = {};
    int lastFamily = -1;
    int packSet(uint8_t* b) const;
    void unpackSet(const uint8_t* b);
    void setsOpen();
    void projectChordsWrite(const char* name);
    void projectChordsRead(const char* name);
    void soundSave();
    void soundLoad();
    int sndPreset = 0;
    SynthPatch sndBase;
    SynthMacros sndMacros;
    int sndField = 0;             // 0 the patch list, 1 to 8 the knobs
    int advField = 0;             // the raw setting in focus
    int hearPad = -1;
    bool soundDirty = false;
    uint32_t soundDirtyAt = 0;
    void serviceLights();
    int beatIndex();
    uint8_t lightSent[48];        // what each pad's light was last set to (255: not yet)
    uint32_t lightTimer = 0;
    uint32_t hitAt[48] = {};      // when each chord pad was let go, for the fade
    uint32_t beatAt = 0;          // when the last beat started, for the pulse
    int lightBeat = -1;
    bool lightsOn = false;
    // Playing the chords: block, strum and arpeggio, with humanise (see Chords.h)
    void serviceChordNotes();
    void startNote(int p, int index, int note, int velocity, uint32_t delayMs);
    void startArp(int p, const uint8_t* chord, int count, int velocity);
    void stopArp();
    void stepArp(uint32_t now);
    double currentBpm();
    int humanVelocity(int base);
    int rnd(int n);
    struct Pending { uint32_t at; uint8_t pad, index, note, velocity; };
    Pending pend[64];
    int pendCount = 0;
    uint8_t soundMask[48] = {};   // bit i: note i of the pad's chord is sounding
    uint32_t rng = 2463534242u;
    int arpPad = -1;              // the pad whose chord is arpeggiating, -1: none
    uint8_t arpSeq[24] = {};
    int arpLen = 0, arpStep = 0, arpVoice = 0, arpNote = -1, arpVel = 100;
    double arpDue = 0, arpPeriod = 0;
    uint32_t arpOffAt = 0;
    void releasePad(int p);
    void releaseLatched();
    bool chHold = false;          // the Hold pad: chords keep sounding until the next one
    uint8_t heldMods = 0, latchedMods = 0; // modifiers held right now, and latched by a double tap
    uint32_t modTapAt[kMods] = {};
    void updateMods();
    bool latched[48] = {};
    uint8_t heldCount[48] = {};
    int8_t tapeSpeed = 0;         // varispeed in percent, -20 to +20 (Config > Four-Track)
    uint8_t liveWave[120] = {};   // the level heard so far in this take, by place in the loop (Play view)
    uint64_t usedPads[4] = {};   // per track: the pads played while that track was recording (shown as dots)
    void markUsed(int pad)
    {
        const FourTrackSession::Mode m = session.mode();
        if (m == FourTrackSession::Mode::RecordingFirst || m == FourTrackSession::Mode::RecordingPart) usedPads[session.selected() & 3] |= 1ull << pad;
    }
    uint8_t heldNote[48][kMaxChordNotes] = {};
    int psField = 0;              // pad sample page: 0 tune, 1 volume, 2 pan, 3 play mode, 4 start, 5 end
    bool psAuditioning = false;
    void psChange(int delta);
    // C2 Load samples: the SD card's folders and WAV files, a preview, a destination slot
    static constexpr int kLdMax = 48;
    static constexpr int kLdRows = 9;
    void drawLoad(class Pen& p);
    void loadOpen();
    void loadList();
    void loadSelect(int i);
    void loadKey(int k);
    void loadJog(int delta);
    void loadMeta(int i);
    void loadPath(int i, char* out, int size) const;
    void loadDo(bool advance);
    void loadStopPreview();
    BrowseEntry ldEntry[kLdMax];
    uint32_t ldMs[kLdMax] = {};
    uint8_t ldChannels[kLdMax] = {};
    bool ldHasMeta[kLdMax] = {};
    int ldCount = 0, ldSel = 0, ldTop = 0, ldSlot = 0;
    char ldDir[96] = "/Samples";
    bool ldPreviewing = false, ldCard = true;
    uint8_t ldWave[58] = {};
    bool ldWaveOk = false;
    uint32_t ldPreviewAt = 0;
    bool ldConfirmed = false;
    int ldAdvance = 0;
    // C4 Resample: the finished mix, rendered like an export, into a pad slot
    enum class Rs : uint8_t { Ready, Recording, Saved, Full };
    void drawResample(class Pen& p);
    void resampleOpen();
    void resampleKey(int k);
    void resampleJog(int delta);
    void resampleStart();
    void resampleFinish();
    void resamplePickName();
    uint32_t rsFreeBytes() const;
    Rs rsState = Rs::Ready;
    int rsLoops = 1, rsSlot = 0;
    bool rsJogSlot = false, rsNameAuto = true, nameForResample = false;
    char rsName[17] = "Resample 01";
    char rsPath[64] = {};
    int rsLevelL = -60, rsLevelR = -60;
    uint32_t rsBytesSaved = 0;
    // C5 Device settings: groups on the left, the rows of the chosen group on the right
    void drawDevice(class Pen& p);
    void drawFirmwareUpdate(class Pen& p);
    void deviceOpen();
    void deviceKey(int k);
    void deviceJog(int delta);
    int deviceRowCount(int group) const;
    void deviceRow(int group, int row, char* name, char* value, char* help, int size);
    void deviceChange(int group, int row, int delta);
    void firmwareGo();
    void serviceFirmware();
    int synthPeak = 0;            // the synth load shown in About
    int dvGroup = 0, dvRow = 0;
    bool dvRows = true;           // focus is on the rows (false: on the list of groups)
    char fwNames[8][48] = {};
    int fwCount = 0, fwSel = 0;
    bool fwUpdating = false, fwInstalled = false, fwSavedFirst = false;
    uint32_t fwStartMs = 0;
    int mxTrack = 0, mxField = 0; // mixer: the column and the control in focus (0 level, 1 pan, 2 tone)
    void drawPopup(class Pen& p);
    void showPopup(Popup kind, int arg = 0);
    void popupKey(int k);
    void dirtyAll();

    // Moving a Part on the main screen (Shift + Insert) and the beat grid
    uint32_t gridFrames() const;
    static const char* gridName(int idx);
    int32_t noteFrame(int track);             // where the first note sits in the Part, -1 if none
    void shiftTrack(int track, int32_t delta, bool all);
    void moveStep(int track, int dir, bool fine);
    void moveTo(int track, bool toStart);
    void moveKey(int k);
    void enterMove();
    void actionKey(int k);
    bool moveMode = false, moveAll = false;
    bool actionMenu = false; // Insert on a selected track: Move, Edit, Glitch, Mute, Source, Undo
    int32_t moveEntry[FourTrackSession::kTracks]{};
    int32_t noteAt[FourTrackSession::kTracks] = { -2, -2, -2, -2 }; // -2: not worked out yet

    // Part Editor
    int editZoom = 0; // 0 whole loop, 1 a quarter, 2 a sixteenth, 3 a sixty-fourth
    void editTrim();
    uint32_t zoomStart() const;
    uint32_t zoomSpan() const;
    struct EditParams
    {
        uint32_t trimStart, trimEnd, fadeIn, fadeOut;
        float gain;
        int32_t userOffset;
    };
    void editEnter();
    void editApply(const EditParams& e);
    void editPush();
    EditParams editNow() const;
    void editField(int delta);
    int editTrack = 0, editFieldIdx = 0, editHist = 0, editHistLen = 0;
    EditParams editStack[16];
    EditParams editEntry{};
    bool auditioning = false;
    uint8_t editWave[760];
    uint8_t padWave[760];
    int padWaveFor = -1;          // the pad slot whose picture is in the page bitmap, -1 none
    void uploadPageMask(const uint8_t* peaks, int maxHalf);
    bool editWaveDirty = true;
    void computeEditWave();

    // Glitch
    void glitchKey(int k);
    int gTrack = 0, gSlot = 0;
    bool shiftChoosesEffect = false;

    // Master
    void masterKey(int k);
    void masterJog(int delta);
    int mRow = 0, mSub = 0;

    // Settings
    int sRow = 0;
    void settingsJog(int delta);

    // Project
    void projectKey(int k);
    void refreshProjectList();
    void newProject(bool everything = false);
    void startSave();
    void startLoad(const char* name);
    void startExport();
    void serviceJob();
    uint32_t partOffset(int track) const;
    void nameKey(int k);
    void nameJog(int delta);
    bool loadHeader(const char* path);
    char list[16][24] = {};
    int listCount = 0, listSel = 0;
    uint8_t listBars[16] = {};
    uint16_t listTempo[16] = {};
    int inputPeakDb = -48;
    uint16_t tempoEntry = 0;
    uint8_t countEntry = 3;
    bool waitEntry = false;
    int8_t waitDbEntry = -36;
    char projectName[20] = "Untitled";
    bool unsaved = false;
    char nameEdit[17] = {};
    int nameCursor = 0, nameChar = 0;
    bool nameForSaveAs = false;
    int pendingAction = 0; // what to do after the "Save your changes?" question
    char pendingName[24] = {};

    // jobs: saving and loading a project, writing a WAV
    enum class Job : uint8_t { None, Save, Load, Export, Done, Resample, Backup, MixRec, MixFinish };
    // Record mix: the live output goes to the card as it plays (a raw file first, then wrapped as a stereo WAV)
    uint32_t mixFrames = 0, mixCopied = 0;
    char mixWav[56] = {};
    void startMixRec();
    void stopMixRec();
    Job job = Job::None;
    char jobPath[48] = {};
    uint32_t jobDone = 0, jobTotal = 0, jobStage = 0;
    int jobTrack = 0;
    int exportRepeats = 1;
    char exportFile[40] = {};
    int exportPercent = 0;
    TrackBank exportBank;
    Lofi exportLofi;

    // Pads
    int padsSel = 0;
    // Input
    int inputRow = 0;

    Hal& hal;
    Palette pal;
    FourTrackSession session;
    View model{};
    Settings set;
    Screen current = Screen::Main;
    Popup popupKind = Popup::None;
    int popupArg = 0;
    bool shift = false;

    Source trackSource[FourTrackSession::kTracks] = { Source::Line, Source::Line, Source::Line, Source::Line };
    const int16_t* partData[FourTrackSession::kTracks]{};
    int32_t alignShift[FourTrackSession::kTracks]{};
    int32_t userOffset[FourTrackSession::kTracks]{};
    bool hasUndoTake[FourTrackSession::kTracks]{};
    bool glitchLatched[FourTrackSession::kTracks]{};
    int appliedSource = -1;
    bool focusUser = false;       // the selected track is open in focus (pressed twice)
    uint8_t live600[600] = {};
    int liveDone = 0;             // columns of the take being recorded that are final
    // Copy and Clear (C8, C9), with one undo step for the last of either (Shift + Copy)
    struct LastOp
    {
        uint8_t kind = 0;      // 0 none, 1 copy, 2 clear, 3 clear all
        uint8_t mask = 0;      // tracks it changed
        uint8_t prevHad = 0;   // which of them had a Part before
        uint8_t fromFile = 0;  // which of those were kept on the card (a copy over a Part); a clear leaves the audio where it is
        TrackCfg cfg[4];
        int32_t align[4] = {}, user[4] = {};
    } lastOp;
    int copyTarget = 0;
    bool clearAll = false;
    char noteHint[72] = {};
    // Bounce: mix several tracks onto an empty one (level, mute, trims, fades and Offset baked in), as on a four-track
    uint8_t bounceSel = 0;
    bool bounceClear = true;
    int bounceTarget = -1;
    void bounceOpen();
    void doBounce();
    void copyOpen();
    void clearOpen(bool all);
    void doCopy();
    void doClear();
    void undoOp();
    void snapshotForUndo(int track);
    void finishCopy();
    void resetAll();
    int bkDst = 0;
    uint32_t bkDone = 0;
    void computeLive(int track);
    void uploadMasks(int track);
    uint8_t wave600[4][600] = {};
    bool maskOk[4] = {};
    uint32_t blinkAt = 0;
    bool blinkOn = false;
    bool padsKeysMode = true;
    bool padModeKeys[48];

    bool promptShown = false;
    int promptBars[kBarChoiceMax]{};
    int promptCount = 0;
    int promptSelected = 0;

    // count-in before a first take at a set tempo
    // Waiting for sound: the take records from the start, and begins (less the pre-roll) at the first
    // sound above the level. takeOffset is how much of the recording to leave out at its start.
    bool waitingSound = false;
    uint32_t takeOffset = 0, waitScanned = 0;
    int waitLevelDb = -60;
    int waitPeakDb = -60;
    uint32_t waitPeakAt = 0;
    bool levelCheck = false;       // waiting for sound, but only showing the level: nothing starts until Rec is pressed
    void serviceWait();
    void cancelWait();
    bool countingIn = false;
    int tailTrack = -1;          // a first take whose tail is still being recorded
    uint32_t tailAt = 0;         // frame where the loop ends, counted from the loop start
    uint32_t tailBase = 0;       // where the loop starts in the take
    bool tailAwait = false;      // the first take was stopped by hand: the tail waits for the loop length
    uint32_t tailScanned = 0, tailQuiet = 0, tailUsable = 0xFFFFFFFFu; // looking for the silence after the loop end
    bool autoSnapped = false;    // the start of the first take has been moved onto its first sound
    uint32_t tailWant = 0;       // how many frames of tail to record
    uint32_t countInStart = 0;
    int countInBeats = 0;
    bool replaceConfirmed = false;
    int lastClickBeat = -1;

    uint32_t staticSignature = 0;
    uint32_t pageSignature = 0;
    bool waveDirty[FourTrackSession::kTracks] = { true, true, true, true };
    uint8_t waveVersion[FourTrackSession::kTracks]{};
    uint32_t meterTimer = 0;
    uint32_t pageTimer = 0;
    int trackDb[FourTrackSession::kTracks] = { -60, -60, -60, -60 };
    bool redraw[6] = { true, true, true, false, false, false };
    bool visibleNow = false;

    Canvas page, popupPage;
};
} // namespace ft

#endif /* FT_APP_H_ */
