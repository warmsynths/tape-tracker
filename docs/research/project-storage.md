# Project storage on the SD card: four-track recorder on the Original Tracker (MK66)

Ticket question: how could a four-track project be stored on the SD card? Compare reusing the stock project format against a new simple one, and cover the Open and Save As lists, naming, save time for four 30 s mono Parts, and the cost of one level of undo kept on the SD card.

Method: read the firmware source and docs only (read-only, nothing built, nothing run, no hardware). All paths are relative to the repo root. **Every timing number below is my own estimate from byte counts and driver reading; the repo contains no measured SD throughput.** Guesses are marked "Guess".

## 1. Short answer

| Question | Answer | Confidence |
|---|---|---|
| Reuse the stock `.mt` + `.pti` + patterns format? | No. It cannot carry our per-project fields without changing frozen structs, and its save/open code is built around 48 instrument slots and 255 patterns we do not use. | High |
| Recommended format | A new project folder per project: one small settings file (`project.ft`) plus four mono `.wav` files (`part1.wav` to `part4.wav`), plus an `undo/` folder. | Medium-High |
| Open and Save As lists | Reuse `GlobalFileBrowser` in folder-package mode with our own marker file. It is the same mechanism stock Open and Save As already use. Almost no new UI code. | High |
| Naming | The browser's built-in on-screen keyboard. Project name limit 32 characters (stock value). | High |
| Save time, four 30 s Parts (10.6 MB) | Guess: about 0.7 s to 3.5 s for all four, about 0.2 s to 0.9 s for one Part. Wide range because the card is unknown. | Low |
| Undo on SD | Cheap if the previous take is already on the card. Cost is then one rename at record time and one read (about 0.2 to 0.9 s, Guess) on undo. Needs a "dirty" rule (section 7). | Medium (design), Low (timings) |
| Device settings | Append one new section to `mtConfig` (stored in the chip's EEPROM, not the SD card). | Medium |

## 2. How stock stores a project

### 2.1 Two places: `/Workspace` and `/Projects/<path>/<name>`

- The live project is kept in `/Workspace`, not in the project's own folder. Paths: `Workspace/project.mt`, `Workspace/patterns/pattern_NN.mtp`, `Workspace/instruments/instrument_NN.pti`, `Workspace/patterns/patternsMetadata` (`core/FileManager/fileManagerDefs.h:63-72`).
- A saved project lives at `Projects/<path>/<name>/` with `project.mt`, `patterns/`, `instruments/<n> <name>.pti`, optional `samples/` (old format) and `export/` (`core/FileManager/fileManagerDefs.h:74-86`).
- The marker that makes a folder count as a project is the file `project.mt` (`core/FileManager/fileManagerDefs.h:96`, `core/FileManager/fileManager.cpp:661-671` `projectExist`).

### 2.2 Copy-on-write model (`docs/developer_guide.md:22-48`)

- Open copies only the 392-byte instrument header of each `.pti` into `/Workspace`; audio stays in the project folder and is streamed from there (`docs/developer_guide.md:27-30`).
- A full `.pti` (header + PCM) is written to `/Workspace` only when an instrument's audio is new or edited (`docs/developer_guide.md:31-33`).
- Save into the same project is incremental: it deletes deactivated slots, copies full files only for edited slots, and rewrites only the 392-byte header for unchanged slots (`docs/developer_guide.md:34-35`, `core/FileManager/ProjectSave.cpp:235-420`). Save As to a new name copies everything so the new folder is self-contained (`docs/developer_guide.md:36`).
- Both operations run as step machines driven from the main loop: `ProjectSave::update()` steps init, project file, up to 255 pattern files, 48 instrument slots, finish (`core/FileManager/ProjectSave.cpp:17-71`, `core/FileManager/ProjectSave.h:19-24`, `core/source/mtStructs.h:126,107`). Open is the mirror image (`core/FileManager/ProjectOpen.cpp:17-60`).
- Pattern copying loops over all 255 pattern numbers and probes each file (`core/FileManager/ProjectSave.cpp:192-233`). Unused, but it costs directory lookups on every save in stock.

### 2.3 The `.mt` file

- `strProjectFile` = 16-byte header (`id_file "MT"`, type, fw version, structure version, size) + `strMtProjectRemote` + CRC32 (`core/FileManager/fileManagerDefs.h:12-19,34-42`).
- `strMtProjectRemote` = song (256 B) + `strMtValues` (fixed 1792 B) + project name (33 B) + project path (222 B) (`core/source/mtStructs.h:995-1002`, `139,147`).
- Version number `PROJECT_FILE_VERSION = 18` (`core/source/mtStructs.h:43`). On load, an older version goes through `versionRecovery.translateProject()`; a newer one is accepted with a to-do for a popup (`core/FileManager/fileManagerProject.cpp:90-105`). A size mismatch resets the name to "Unsaved Project" (`core/FileManager/fileManagerProject.cpp:107-116`).
- CRC is verified but a mismatch is only logged under a debug flag; zero CRC is tolerated (`core/FileManager/fileManagerProject.cpp:70-88`).
- Writing fills the header, copies song and values, name and path, and a CRC (`core/FileManager/fileManagerProject.cpp:170-204`).

### 2.4 Backward-compatibility rules that apply

- "Never change the integer values or ordering of existing serialized enums... or shift existing fields in project/patch/config structs" (`CONTRIBUTING.md:58-59`).
- `strMtValues` is pinned by `static_assert(sizeof(strMtValues) == 1792)` (`core/source/mtStructs.h:985`) and `strInstrument` by `static_assert(sizeof(strInstrument) == 372)` (`docs/developer_guide.md:48`). Both leave no free bytes for loop length, bars, per-Part offsets or alignment data. The only `reserved` bytes I found belong to MIDI instrument slots (`core/source/mtStructs.h:967`), which are the wrong place to hide recorder data.
- This fork's own rule is separate from stock's: the owner wants a stock `.ptf` kept on the card for recovery (`HANDOVER.md`). A project format that stock firmware cannot misread helps that (section 4).

## 3. Option A: reuse the stock format

Map each Part to an instrument slot 1 to 4 (a full `.pti`), keep `project.mt`, `patterns/`, and fit the recorder fields somewhere.

What it gives: the stock Open/Save As/recent list/overwrite handling/`Save` progress bar work unchanged (`core/interface/projectEditor/fProjEdit.cpp:750-919`); stock firmware could open the project and play the four Parts as samples.

What it costs:
1. No room for our fields (section 2.4). Options are adding a side file anyway (so it is not "reusing the format" any more), or adding a new field to `strMtValues` and `PROJECT_FILE_VERSION` 19 plus a `versionRecovery` entry, which is the riskiest change to the highest-compatibility struct in the firmware, for an owner who cannot read the diff.
2. Parts are not `.pti` natively. A `.pti` is 392-byte header + raw PCM; the header carries instrument parameters (`core/FileManager/fileManagerDefs.h:21-30`). Our four Parts would have to carry 372 bytes of meaningless instrument data each.
3. The save machinery is built around things we do not have: 48 slots, 255 patterns, synth patches, patterns metadata (`core/FileManager/ProjectSave.cpp:518-548`). Trimming it is a larger change than writing a 200-line saver.
4. The `/Workspace` copy-on-write model exists to avoid copying 13 to 45 MB on open (`docs/developer_guide.md:25`). Our whole project is 10.6 MB, which must be in RAM to play anyway (`docs/research/cpu-ram-budget.md:99`), so there is nothing for copy-on-write to save. The stock open path would stream Parts from SD (`docs/developer_guide.md:30`), which is the opposite of what we need (Parts must be resident; see `docs/research/cpu-ram-budget.md:48-50`).
5. Stock Save also copies the whole `/Workspace` model into the project, so a save of one changed Part still walks 255 pattern probes and 48 slots (Guess: tens of directory lookups, small next to the audio write; unmeasured).

## 4. Option B: new simple format (recommended)

```
/FourTrack/                      (own root, not /Projects)
  <Project Name>/
    project.ft                   settings, ~200 bytes, marker file
    part1.wav ... part4.wav      mono, 16-bit, written by existing helper
    undo/part1.wav ... part4.wav one previous take per Part (section 7)
```

Why a separate root and marker: stock `Projects/` browsing treats a folder as a project only if `project.mt` exists (`core/source/FileBrowser.cpp:98-118`). With a different root and marker, stock firmware shows our folders as plain folders and never tries to open them. Stock's browser, run on the same card, cannot mis-open or overwrite them. (This is an inference from the marker check, not a test.)

### 4.1 Contents of `project.ft`

From the ticket: loop length in samples, bars, tempo, Loop Mode, click, project name, and per Part: offset, alignment data, name, length, has-audio flag. Roughly 200 bytes.

Format rules, borrowed from stock because they work and the owner cannot debug a corrupt file by hand:
- Header: 2-byte magic, version, total size, CRC32, mirroring `strProjectFileHeader` (`core/FileManager/fileManagerDefs.h:12-19`).
- Tolerant reading, as `readConfig` does for device config: read `min(stored size, known size)` bytes, leave unknown tail alone, default any field the file lacks (`core/source/mtConfig.cpp:176-186`). A later firmware can append fields without breaking older files.
- Never reorder fields; append only (same rule as `CONTRIBUTING.md:58-59`, applied to our own file).
- Treat a bad CRC as "ask the user" or fall back to defaults rather than crash. Stock only logs it (`core/FileManager/fileManagerProject.cpp:70-88`); our file is small enough to do better.
- Write via a temp name then `rename` (the platform file system has `rename`, `core/include/Platform.h:358`) so a power cut during Save does not destroy the previous `project.ft`. Stock's `saveMemoryToFile` deletes the old file first and then writes (`core/FileManager/fileTransfer.cpp:787-797`), which is not power-cut safe. Guess: a power cut is rare but real for a device used with a guitar and cables.

### 4.2 Part audio as `.wav`

- A helper already exists: `cFileTransfer::saveMemoryToWav(L, L, samples, path)` writes a mono 16-bit WAV when the two channel pointers are equal (`core/FileManager/fileTransfer.cpp:1329-1421`). It writes 32,640 bytes per main-loop call and fills the 44-byte header at the end (`:1356-1357,1406-1415`, buffer size `core/FileManager/fileTransfer.h:31`).
- Loading: `loadInstrumentRawSampleDataToMemory(file, fileOffset, memory, limit, outCount)` reads raw PCM from a byte offset, 32 KB per call (`core/FileManager/fileTransfer.cpp:875-950`). Use offset 44. Alternatively `loadSampleToMemory` (`:210`) validates the WAV header and converts; slower and more general.
- The header always says 44,100 Hz (`core/FileManager/fileTransfer.cpp:864`) while the hardware really runs at 44,117.6 Hz (`docs/research/record-path-and-latency.md:section 2`). That is a 0.04% label error. It matters only if a file is played elsewhere (about 0.4 s drift over 15 minutes, arithmetic). Because Parts are one loop each, it does not affect alignment inside the device. Worth knowing, not worth fixing.
- Stock's recorder-to-SD path (`core/FileManager/RecordingSave.cpp:63-130`) writes 512 samples (1 KB mono) per update tick. For a 30 s Part that is about 2,600 ticks. Do not reuse it for Part saves; use `saveMemoryToWav`.
- Plain WAVs are a feature for this owner: the card can be put in a computer and the Parts dragged into any DAW. Stock `.pti` files need a tool to open.

### 4.3 Costs of Option B

- New code to write: settings struct and reader/writer, a Part load/save stepper, a project list hook. Guess: a few hundred lines. All of it is I/O plumbing on helpers that exist.
- Not interoperable with the stock Open dialog (by design, see above). Stock could still load the individual WAVs through its Sample Loader.
- Does not use stock's `versionRecovery` machinery; our file has its own version byte.

## 5. Open and Save As lists via `GlobalFileBrowser`

Stock Open and Save As already use it: `kLoadFolderPackage` for Open and `kSaveFolderPackage` for Save As, with `packageMarkerFile = "project.mt"` (`core/interface/projectEditor/fProjEdit.cpp:801-806,853-858`; modes in `docs/global_file_browser.md:34-35`).

For us the change is the config values only:
- `packageMarkerFile = "project.ft"`, `initialDirectory = "/FourTrack"`, own `contextKey` (for example `"ft_projects"`; the browser remembers 10 contexts, `docs/global_file_browser.md:38`).
- Open: `onConfirm` receives the folder path; split it as `fProjEdit.cpp:824-830` does.
- Save As: `defaultFileName` = current project name; `maxNameLength = 32` to match `PROJECT_NAME_SIZE - 1` (`core/source/mtStructs.h:139`, `core/interface/projectEditor/fProjEdit.cpp:857-858`). `confirmOverwrite` is on by default (`core/interface/fileBrowser/GlobalFileBrowser.h:38`) and asks before overwriting an existing project (`core/interface/fileBrowser/GlobalFileBrowser.cpp:2010`).
- Inspector text: `onDescribeItem` can show tempo, bars and how many Parts hold audio, by reading `project.ft` (about 200 bytes). The docs require closing the file handle immediately on return because the list calls this on every highlight (`docs/global_file_browser.md`, note "FatFs File-Handle Lifetime" in section 5).
- Recent list: `secondaryListItems`, up to 32 entries (`docs/global_file_browser.md:40`), as stock does at `fProjEdit.cpp:808-835`. Stock keeps 10 recents in `Projects/recents` (`core/FileManager/fileManagerDefs.h:98`, `core/source/mtStructs.h:365`). We need our own small file; Guess: not needed for v1.
- Delete: the browser can delete a project folder; copy stock's `canDelete` guard that refuses to delete the open project (`fProjEdit.cpp:757-783`).
- RAM: the browser uses a 24 KB slice of the shared 768 KB scratch buffer, not the sample pool (`docs/global_file_browser.md:174`, `core/interface/fileBrowser/GlobalFileBrowser.cpp:44`, `docs/memory_map.md` section 3), so it does not compete with the 10.1 MiB of Parts. **I did not verify that the scratch buffer survives a trimmed build that drops the `.ptfx` loader; that is a build-time check.** The file is compiled unconditionally in stock (no `#if` guard found in `GlobalFileBrowser.cpp`).
- Browser behaviours worth knowing: opening a project while audio plays needs care (stock stops the SD player first, `fProjEdit.cpp:746-748`, we would stop the Parts); screen buttons cancel the browser (`docs/global_file_browser.md:57`), which matters if our UI has a Mixer or Record screen button.

### Naming (text entry)

- Save As opens the on-screen keyboard (`openSaveFileKeyboard`, `core/interface/fileBrowser/GlobalFileBrowser.cpp:1747-1796`), prefilled with the current name, length-limited by `maxNameLength` clamped to 1..60 (`:1793`), with a "random name" button (`handleKeyboardButtonPress`, `:1876-1890`). Same widget stock uses; no new UI.
- For Part names (not a file), call `mtKeyboardManager` directly (`core/source/mtKeyboardManager.h:74-86`: `fillName`, `activateKeyboard(limit)`). Stock's own track-name limit is 7 characters (`TRACK_NAME_LENGTH`, `core/source/mtStructs.h:142`); choose our own for Parts. Guess: 12 fits a small screen cell; that is a layout decision for the design brief.
- I did not check which characters the keyboard permits. If it permits characters FAT forbids (for example `/` or `:`), project names could fail to create. **Needs checking on the device (or a longer source read).**

## 6. What a save costs on an SD card

Data: one 30 s mono Part = 30 x 44,100 x 2 = 2,646,000 B (2.52 MiB); four = 10,584,000 B (10.09 MiB) (`docs/research/cpu-ram-budget.md:90,99`). At 32,640 B per call: 82 calls per Part, 325 for four.

What I could establish from the source:
- Card interface: SD host with a 4-bit data bus when the card allows it, up to 50 MHz (`platforms/mk66/3rdparty/SD/sdmmc/src/fsl_sd.c:2056-2065`, `:1161-1162`). Theoretical raw bus rate about 25 MB/s; real sustained write is far lower and depends on the card.
- File system: FatFs, FAT32 (exFAT off: `platforms/mk66/3rdparty/SD/ffconf.h` `FF_FS_EXFAT 0`), long file names on (`FF_USE_LFN 1`), 512-byte sectors (`FF_MAX_SS 512`).
- `SdFile::write` is a direct `f_write` (`platforms/mk66/3rdparty/SD/SdFile.cpp:156-167`). Whether it uses DMA, and how long it blocks the main loop per call, I did not determine.
- The stock helpers write in 32,640-byte pieces (`core/FileManager/fileTransfer.h:31`). 32,640 is not a multiple of 512 (it is 63.75 sectors), so every call after the first starts mid-sector. Stock's newer aligned path trims chunks to 512 multiples and uses 256 KB chunks (`core/FileManager/fileTransfer.cpp:1572,1880-1900`). Reading that, I infer someone found alignment to matter, but no comment says so. Guess: a 32,768 or larger multiple-of-512 chunk would be faster for our save; test it.
- Main-loop pacing: the stepper does one write per call and returns "in progress" (`core/FileManager/fileTransfer.cpp:1399-1401`). Total time is therefore writes plus however long the main loop takes between calls. The main loop also redraws the display and, per `docs/research/cpu-ram-budget.md:84`, is starved when the audio interrupt runs hot (four never-sleeping loops plus a reverb).

Estimate table. **All Guess.** Pick the row by what the card actually sustains; I cannot see which card the owner has.

| Sustained write | One Part (2.52 MiB) | Four Parts (10.09 MiB) |
|---|---|---|
| 3 MB/s (slow or fragmented card) | 0.9 s | 3.5 s |
| 8 MB/s (typical class 10) | 0.33 s | 1.3 s |
| 15 MB/s (good UHS-I card, ideal conditions) | 0.18 s | 0.7 s |

Plus overhead per file (open, create, FAT chain allocation, close, small): Guess tens of milliseconds each. A first save of a new project also creates the folder and four files.

Reading back on Open: same arithmetic with read speeds, which are usually higher than write speeds. Guess 0.5 to 1.5 s for four Parts. Stock already shows a progress bar for open and save (`core/FileManager/ProjectSave.cpp:73-112`, `getProgress`), so a short blocking "Saving..." screen is an established pattern.

Cost reducers, in order of value:
1. Save only Parts changed since the last save (a dirty flag per Part). Stock does the same in spirit (`docs/developer_guide.md:34-35`). A typical save after one take is then one Part: 0.2 to 0.9 s.
2. Never save during recording or during the first moments of playback; the main loop is busy and an SD write can starve the display.
3. Use 512-aligned, larger chunks (Guess, test).
4. Write `project.ft` last, via temp file and rename, so a partly written Part never looks complete.

## 7. Undo for one take with the previous take on SD

Why SD at all: RAM has no room. Four Parts take 10.09 MiB of the 11.25 MiB pool, leaving 1.15 MiB, and a second 2.52 MiB copy of any Part does not fit (`docs/research/cpu-ram-budget.md:99,103`). Also, the recorder writes into the Part's own region, so the old take is overwritten the moment recording starts. Stock's undo is RAM-only and for patterns only (`core/FileManager/fileManagerUndo.cpp:9-14,40-46`), so there is nothing to reuse.

Three ways to do it:

| Way | Cost when you record again | Cost when you press Undo | Notes |
|---|---|---|---|
| U1. Write old take to `undo/partN.wav` at record time | One Part write, 0.2 to 0.9 s (Guess) before or during recording. Delays arming or competes with the recording interrupt. | One read, same size | Simple. Bad timing: the delay lands exactly when the player is about to play. |
| U2. Write every take to SD when it finishes, and keep the old file by rename | No write at record time. One rename (a directory entry change, Guess: milliseconds). | One read, 0.2 to 0.9 s (Guess) | Needs takes to be saved right after recording. The write happens at the end of the take, when the user is listening, with the same SD-versus-audio risk as any save. |
| U3. Write the old take to `undo/` only if RAM differs from the saved `partN.wav` | Zero if the Part was saved and untouched; one write otherwise | One read | Least SD traffic, but two code paths and a dirty flag to get right. |

Recommendation: **U3 with U2's rename trick.** Rules:
- Each Part has `dirty` (RAM differs from `partN.wav`).
- Before a re-record of Part N: if not dirty, `rename partN.wav -> undo/partN.wav` (cheap). If dirty, write RAM to `undo/partN.wav` first. In both cases the old take now exists on the card.
- Undo: load `undo/partN.wav` back into the Part's RAM region and mark dirty. One level only: a second record replaces the undo file.
- Space: one extra 2.52 MiB file per Part, so at most 10.09 MiB extra per project (Guess: acceptable on any card; stock projects reach 45 MB).

Costs, summarised: the extra SD write is zero in the common case and one Part in the dirty case; undo is one read of one Part; undo state survives a power cut (a plus over RAM undo). Open question I could not answer from source: whether reloading a Part while the other three keep playing glitches. Reads are done in the main loop and the audio interrupt is higher priority (`docs/research/cpu-ram-budget.md:31`), so audio should keep running, but the reload also overwrites the Part's RAM while it is playing. Stopping that Part, or all playback, during Undo is the safe choice.

## 8. Device settings (`ConfigManager`, `mtConfig`)

- Device-wide settings live in the chip's EEPROM, not on the SD card. `ConfigManager` is a two-function interface, `readData`/`writeData` (`core/include/ConfigManager.h`); on MK66 it calls the Teensy `EEPROM` library at offset 10 (`platforms/mk66/source/ConfigManagerImpl.cpp:7-19`, `platforms/mk66/config/config.h:12`). The underlying store is at most 4096 bytes (`platforms/mk66/3rdparty/teensy_core/eeprom.c:44`; a per-chip size macro appears at `:71-77`, I did not resolve which applies).
- `mtConfig` is sections (audio codec, firmware, midi, values, general, interface, metronome, debug, common), each starting with its own size field, listed in two arrays and written one by one (`core/source/mtConfig.cpp:30-46,79-136`, struct at `core/source/mtStructs.h:1042-1194`). Reading takes the smaller of stored and known size per section, so new sections can be appended without breaking old EEPROMs (`core/source/mtConfig.cpp:176-186`). Version constant `CONFIG_STRUCT_VER = 10` (`core/source/mtStructs.h:47`); its change hook `eepromStructureChange()` does nothing today (`core/source/mtConfig.cpp:657-661`).
- Input source (`inSelect`), input gain (`inputGain`) and monitoring-related bytes already exist in `audioCodecConfig` (`core/source/mtStructs.h:1056-1068`). Alignment window, pre-roll and tempo rounding do not.
- Recommended: add one new section `recorder` at the end of `strMtConfig`, with a leading `size` field, add it to the two arrays and to `forceSaveConfig()`, and range-check it in `checkConfig()` (`core/source/mtConfig.cpp:237`). Do not touch existing sections (`CONTRIBUTING.md:58-59`).
- Guess, not verified: if an older EEPROM lacks the new section, the loop breaks before reading it (`core/source/mtConfig.cpp:194-196`), so the new fields keep whatever RAM held. That is only safe if RAM holds defaults at that point. Needs a read of `resetConfig()` ordering or a test with an old config. Marked as the main risk of this part.
- Alternative: put device settings in a small `/FourTrack/device.cfg` on the SD card. Upside: no EEPROM change at all, survives firmware flashes. Downside: settings follow the card, not the device, and a missing or write-protected card loses them. I recommend EEPROM for device settings because that is where the stock device settings are and the section mechanism is built for it, but either works.
- Project-level settings (Loop Mode, click) go in `project.ft`, not in config, per the ticket split.

## 9. Recommendation

Use Option B: a new folder-per-project format.

1. `/FourTrack/<name>/project.ft` (header, version, size, CRC; append-only fields; tolerant reader) + `part1..4.wav` (mono, written with `saveMemoryToWav`) + `undo/`.
2. Open and Save As: `GlobalFileBrowser` folder-package modes with marker `project.ft`, own `contextKey`, an inspector line from the settings file. Naming by the built-in keyboard, 32-character limit.
3. Save: write only dirty Parts, then `project.ft` via temp file and rename. Show a short "Saving..." screen.
4. Undo: U3 with the rename trick.
5. Device settings: append a `recorder` section to `mtConfig`.

Why not stock: it needs a change to a frozen, size-asserted struct for the benefit of stock interoperability the owner has not asked for, and drags 48-slot, 255-pattern machinery along. The new format is smaller, readable in any computer, cannot be mis-opened by stock firmware, and reuses stock's list, naming and WAV helpers.

When this recommendation would be wrong: if the owner wants a project saved by the recorder to open in stock Tracker with its sound and patterns intact. The ticket does not ask for that. Then Option A, with a side file for the recorder fields, would be the fallback.

**Confidence: Medium-High** on the format choice and the browser reuse (read directly from code that already does the same job). **Low** on all timings. **Medium** on the undo design.

## 10. What only the device can confirm

1. Real sustained SD write and read speed for the owner's card with the stock `saveMemoryToWav` path, 32,640-byte chunks versus 512-aligned larger chunks. Time one Part and four Parts. (Section 6 numbers are guesses.)
2. How long the main loop blocks per `f_write` call, and whether the display or pads stutter during a save while four Parts loop.
3. Whether an SD write during playback causes audio dropouts (audio interrupt priority 5, SDHC 6, `docs/research/cpu-ram-budget.md:31`), and how Undo behaves while other Parts play.
4. Whether `GlobalFileBrowser` fits and survives in a trimmed build, and whether its folder listing is quick with many projects.
5. Which characters the on-screen keyboard allows versus what FAT accepts.
6. Behaviour of an old EEPROM config after a new `mtConfig` section is appended (section 8 risk).
7. Power-cut behaviour of the temp-file-then-rename save. Pull the card or power during a save and check the project still opens.
8. Whether the 44-byte WAV header and the stated 44,100 Hz label round-trip cleanly through the stock Sample Loader and a computer.

## 11. Where I am guessing, plainly

- All timings in sections 6 and 7 (SD throughput, per-file overhead, rename cost, main-loop pacing).
- That 512-aligned chunks help; I only inferred it from the stock aligned path.
- That a few hundred lines of new code suffice.
- That stock firmware leaves non-`project.mt` folders alone; read from the marker check, not tested.
- That the `GlobalFileBrowser` scratch slice is available in a trimmed build.
- That a missing `mtConfig` section leaves defaults in RAM.
- The extra SD space for undo files being acceptable.
