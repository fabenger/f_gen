# f_gen – Rhythm Sampler (VST3)

Lädt One-Shots (Chord Stabs, Percussion, Noten) in 4 Slots und würfelt daraus House-, Underground- und Minimal/Detroit-Rhythmen in der gewählten Tonart.

## Installation (Windows 11 / FL Studio)
1. Unter **Releases** die neueste `f_gen_Setup_x.y.z.exe` herunterladen und ausführen.
2. Das Plugin landet in `C:\Program Files\Common Files\VST3\f_gen.vst3`.
3. FL Studio: *Options → Manage plugins → Find installed plugins*, dann f_gen als Generator in den Channel Rack laden.
4. Play drücken: f_gen läuft synchron zum FL-Tempo.

## Bedienung
| Bereich | Funktion |
|---|---|
| **Slots A–D** | Sample per Drag & Drop oder Doppelklick laden. MUTE, LOCK (von „Randomize All“ ausnehmen), RND (neuer Rhythmus nur für diesen Slot). |
| **Header** | KEY + SCALE (Tonart des Loops), LOOP (2/4/8 Takte), PLAY (Host Sync oder MIDI Trigger), SWING, MASTER, RANDOMIZE ALL. |
| **Sample** | Wellenform, LOAD / PREVIEW / EJECT, KEY TUNE (Sample wird vom erkannten Grundton auf die Tonart gestimmt, bei Drums aus), REVERSE, ROOT (Auto oder manuell). |
| **Pattern** | Klick = Step an/aus, ziehen = malen, Mausrad = Velocity, Rechtsklick = Note (Skalenstufe), Velocity, Ratchet. |
| **Generator** | Stil: House, Deep/UG, Minimal Detroit (polymetrische Zyklen), Dub Chords, Euclid, Wild. DENSITY, VARIATION (Abwandlungen + Fills am Phrasenende), MELODY (Töne aus der Skala, Akkordwechsel). RANDOMIZE / UNDO / CLEAR. |
| **Sound** | Pitch, Fine, Start · Attack, Decay, Sustain, Release, Gate (Notenlänge in Steps) · Filter (LP/HP/BP) Cutoff + Reso, Drive (Sättigung), Crush (Bit/Sample-Rate-Reduktion) · Volume, Pan. |

**MIDI Trigger:** Pattern läuft nur, solange eine Taste gehalten wird; C5 (MIDI 60) = Originaltonhöhe, andere Tasten transponieren.

Hinweis: Projekte speichern den Dateipfad der Samples. Wenn du Samples verschiebst, lade sie neu.

## Build
Wird automatisch per GitHub Actions gebaut (`.github/workflows/build.yml`): JUCE 8 + CMake, Installer mit Inno Setup.
Lokal: `cmake -B build && cmake --build build --config Release`.
