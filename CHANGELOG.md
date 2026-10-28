# Aeola changelog

## 1.13.1
- Ableton Live: the VST3 no longer shows extra automatable parameters (Bypass and a Program list), so Live's parameter list fills in again. Presets are still in Aeola's own menu; the Audio Unit keeps the host preset list.
- Fixed: when live data was unavailable, the saved-snapshot fallback could return nothing because of a stray comment in the code.

## 1.13.0
- Prettier map: land is drawn as soft grey shapes on a dark sea, with lakes and a faint, smooth shoreline, in both the Sky map and the Earth view. Finer land data when zoomed in, lighter data for wide views.
- Modulation, easier to see and use:
  - The same right-click menu on every knob: Type a value, Modulate by (or "Can't be modulated"), and the knob's connections, each with Remove, plus Show in the Matrix.
  - A knob with a connection always shows it: a thin orange arc for the range each connection covers, and a dot (with the number when there are several). Hover to see which sources move it.
  - Drag the orange ring around a knob up or down to change the amount (double-click it for 25 %).
  - The Motion tab is now called Modulation, and the connection list is the Matrix: each row has a remove button, and the arrow takes you to the control it moves.
  - 33 more targets: Attack, Release, Glide, Drift rate, Low cut, Warmth, Filter mix, Key track, Reverb size and damp, Range, Ghost level, Loop stretch, LFO 1 and 2 rate, and Swell time and Chop for each class.
- Mixer: full class names on the strips, and each strip shows the class's Gain. The class Level on the Aircraft tab is now called Gain: it is part of the class's voice, and the Mix fader comes after it. The short codes are now FRT (Freight) and TBP (Turboprop).

## 1.12.0
- Mix tab (Advanced): one strip per aircraft class with a fader, pan, solo, mute and a meter. The fader and pan are separate from the class's own Level, so a live mix never changes the sound design. Presets and A/B leave the mix alone.
- Separate outputs: each class can go to its own stereo output (nine extra outputs, dry, without Aeola's reverb). Turn it on in the Mix tab or Settings, then switch the outputs on in the host (Live: an audio track, Audio From = the Aeola track, choose the class). Classes whose output is off stay in the main output.
- Automation: exactly 64 parameters are automatable (macros, main sound and filter controls, reverb, North, Glide, Consonance, Evolve, Radius, Aircraft, Hold, and the mixer: fader, pan, solo, mute per class), so Ableton Live lists them all from the start. The rest are still saved with the project but cannot be automated; automation drawn on them in older projects no longer plays.
- Sky map: a real map that fills the view, with zoom and pan of its own (scroll, + and -, drag; double-click = back to the circle). The listening radius is one dashed circle you can drag. Aircraft beyond it are shown dim and silent with a short trail and "in N min" when they are heading in; hover for where they are going. A ring marks every aircraft entering (green) or leaving (grey). The aircraft card says when an aircraft will leave the circle. Live data now reaches about twice the radius.
- Modulation: eight connections (was four), six new targets (Inharmonic, Odd/Even, Breath, North, Consonance, Evolve), and right-click any knob > Modulate by to connect a source directly.
- Controls for four settings that had none: Mod wheel amount and Bend range (Sound tab, More controls), Ghost level (Sky tab), Sim seed (Sky tab, when Simulated).
- Earth view sits higher, so it looks centred.
- Daily data limit: Aeolas share one request per place for as long as the request budget allows (was 12 seconds), a copy saved on disk a moment ago by another program or before a restart is used instead of asking again, and Details for a bug report shows how many requests this Mac made today.
- A preset no longer switches MIDI out off. Projects from before 1.12 open with the mixer at 0 dB.

## 1.11.0
- New name: the plug-in is now just "Aeola" (it was "Aeola VST"); the browser version is "Aeola web". Live projects and presets made with earlier versions still load: the plug-in IDs did not change. The installer removes old "Aeola VST" bundles from /Library/Audio/Plug-Ins; copies you placed in your own ~/Library folders must be deleted by hand, or the host may list two Aeolas.
- Modulation per aircraft class: new sources "<class> count" (how many of that class are in range; four or more = full) and "<class> altitude" (average height), and new targets "<class> level" and "<class> octave". Example: Helicopter count -> Helicopter octave at +25 % moves helicopters up one octave when four or more are around. In the Motion tab they sit in sub-menus, so the lists stay short.
- MIDI out: "Channel per class" now says it is for hardware.

## 1.10.0
- MIDI out (VST3 in Live): each sounding aircraft sends a note. In Live, set another MIDI track's "MIDI From" to the Aeola track. Settings > MIDI out: On, Mute sound, Channel per class (channels 1-9, for hardware and multi-timbral synths), Class.
- One class per track: one Aeola set to Class "All" is the source. Other Aeolas set to a single class copy that class from it, so every track records the same sky. They fetch no data and make no sound of their own. Without a source, a single-class Aeola uses its own sky.
- MIDI out is not an AU feature in Live; use the VST3.

## 1.9.1
- Settings: Send feedback (opens an email with the technical details filled in), Details for a bug report (copyable box: version, system, host, place kind, source, aircraft, data age, quota, last problem; no coordinates or address), Support (Ko-fi), website, source code, credits for adsb.lol, adsb.fi, Natural Earth and JUCE.
- Update notice: once a day at most, the plugin fetches one small file (version.json on the project page). A newer version shows a dot on the Settings button and a download item in the menu. It can be switched off in Settings and sends nothing but a user-agent with the version.
- Project web page (index.html at the repository root, served by GitHub Pages)..

## 1.9.0
- Fixed: the number above each knob in the Advanced drawer (and Radius / Aircraft) could show an old value. It is now part of the knob and follows every change, including presets, automation and modulation. (Reverb showed 58 % while set to 50 %.)
- The Aircraft setting is exact now. Evolve used to thin the sky silently (Aircraft 8 gave about 7 sounding, 12 gave about 10) and the anchor drone used up one or two of the Voices; now Aircraft 12 with Voices 12 plays 12 aircraft, and the anchor is extra. The sky view footer reads "7 / 8 sounding".
- Live data budget: the feed reads adsb.lol's rate-limit headers and spreads the remaining daily quota (500 a day) over the time left (typically one request every 1.5 to 3 minutes, never faster than 20 s, never slower than 5 min). Aircraft are moved on between snapshots as before.
- No automatic simulated traffic. When live data stops, Aeola keeps the last real snapshot, labelled "Saved snapshot from N min ago. Not live.", and with nothing to show it plays the drone bed alone and says why: "Daily data limit reached at adsb.lol, resets 01:00". Simulated traffic only when you pick it in Sky source.
- Optional backup source in Settings: adsb.fi, used only while adsb.lol's limit is reached. Off by default: adsb.fi allows personal, non-commercial use only and requires attribution.
- Airports show IATA codes (ARN Stockholm Arlanda); projects and presets still store the ICAO list index.

## 1.8.2
- Fixed: the response graph in the Filter tab did not update when settings changed. It now redraws as you turn Cutoff, Resonance, Low cut, Drive or Mix, or change the model.
- The graph now follows the chosen model (Clean, Ladder, Ladder 12 dB) and the Mix, and shows a cutoff marker and a faint fill.

## 1.8.1
- Simple mode now has a Consonance knob in its own "Harmony" section (between Place and Sound), with a one-line explanation.
- The intro tour has a fifth step that spotlights Consonance, as it does the macros.

## 1.8.0
- New Earth view (third tab next to Sound and Sky, key 3): the place seen from above at an angle, with the coastline, range rings, an altitude scale at the listener, and horizon haze. Aircraft hang at their altitude on a stem with a shadow on the ground, leave a trail at altitude, and send ripples across the ground while they sound. Drag to tilt and rotate, scroll to zoom, double-click to reset.
- The ground is drawn once into an image and only redrawn when the camera, zoom or place changes, so the per-frame cost is about 5 ms more than the Sky tab on a 2012 Mac mini (about 18 ms more while dragging).
- Not done yet: land fill (the coastline data has no land polygons) and a satellite/terrain basemap.

## 1.7.2
- App icon: the sky map in miniature (radar rings, freight triangle with its Consonance move, heavy square, business-jet hexagon). Used by the Standalone app.

## 1.7.1
- "The Guard" is now called Consonance everywhere: aircraft cards say "moved off D3 by Consonance (it would clash)", the count under the sky view says "moved by Consonance", and the Sky tab explains the knob.
- Sky map in Advanced mode: much larger (it uses the full height of the view) and centred instead of hugging the top.
- Intro tour redone: dimmed background with a spotlight on the part being explained (sky, place, macros), a proper card with step count, Next / Skip / Start, and a mention of Consonance.

## 1.7.0
- Advanced mode rebuilt: the same sky view as Simple on top, the same Place / Radius / Aircraft / Macros column on the right, and one drawer with six tabs (Aircraft, Sound, Tune & Space, Filter, Motion, Sky). Only one tab is visible at a time, with 4-12 controls, values shown above the knobs.
- Aircraft tab: class list with live counts and S / M; the selected class in detail (what it is, its voice, Level / Register / Swell time / Chop, who is in the sky now, "Solo and listen"); the selected class lights up in the sky view.
- Sound tab: two sources with a blend bar and the spectrum; Colour / Movement / Shape groups; rarer controls behind "More controls".
- Filter tab: model buttons and a response curve. Motion tab: two LFO cards and only the connections in use, plus "Add a connection". Sky tab: key and scale, the Guard, drone bed (with Hold), life of the sky, data (source, Why?, Freeze, Drag MIDI).
- Output level moved to the top bar.

## 1.6.0
- Pitch: altitude now spans 0-42,000 ft (was clamped at 30,000), so cruise traffic is spread over the range instead of collapsing onto the top note; aircraft avoid doubling another aircraft's note (unisons only at low Consonance). In a simulated busy overflight sky, distinct notes per voice went from 44 % to 88 %.
- Distance: the rim of the radius is about 18 dB below an aircraft overhead (was about 10 dB), and brightness follows distance more strongly.
- Class voices with their own timbre are that timbre (they used to carry about 30 % of the base sound).
- Presets are sounds: they no longer change the place, radius, Sky switch, output level, per-class voices or solo / mute. A/B scenes don't morph those either.
- Data honesty: a failing feed is shown (reason and retry time) instead of "Live, 0 s ago"; data age is measured on the real clock, so offline bounces don't drop to simulated traffic; old cached snapshots are no longer flown on for minutes; aircraft with only GPS altitude are kept; aircraft taking off and landing (below 200 ft, fast) are kept; parked traffic is limited to the radius; the count excludes the virtual audition aircraft and says "nearest" when capped at 64.
- Network: instances share one request per place and 12 s; 429/503 "slow down" and Retry-After are respected for all instances; the disk cache is written once a minute and old cache files are deleted after 14 days; User-Agent says AeolaVST.
- Robustness: the cached frame is double buffered (no race on place change); the mapper never orders more voices than the engine holds; a project with "Custom place" stays a custom place if airports are added later; voice ids can no longer collide with the anchor / fill voices; more helicopter types and cargo airlines are recognised; per-block work skips unchanged reverb and spectrum settings; the coastline outline is cached; the sky view repaints at 24 fps.
- Interface: one place button in Simple mode; the source list is Advanced-only; Hold appears in Simple mode while it is on; Simple mode says when the preset blends in a second sound ("+ 25 % Bowl"); class colours no longer reuse the state colours (green / amber / red); map glyphs keep the class shapes; the Sound view caption is accurate and its columns cover every note in play; the beating arc is Advanced-only; right-click a knob to type a value; the tour describes only what Simple mode shows; About shows the real version.
- Repository: compiled test programs and dev probes removed, all seven test programs run by tools/release.sh, docs moved to docs/.

## 1.5.0 and earlier
See docs/SPEC.md.
