# OBW Nexus page — FAQ + description improvements

Built 2026-07-12 from the real posts on the mod page (mod 182452, 68 comments read). The BBCode blocks
below are ready to paste into the Nexus description editor. Grouped by the actual recurring confusions.

---

## PART 1 — Expanded FAQ (BBCode, paste over the current FAQ section)

```bbcode
==================================================================
FAQ
==================================================================

[size=4][b]Understanding the mod[/b][/size]

[b]Does this just assign OBody presets, like OBody NG does? My NPCs still get a preset (often "unknown") - is it even working?[/b]
It depends on the body mode, and yes it's working:
[list]
[*][b]Procedural[/b] (recommended) - OBW builds each body FROM SCRATCH and ignores preset files. The "preset" you see it clear/replace is just OBW taking over; that's expected. This is the mode with the most variety and it needs no presets at all.
[*][b]OBody Sim Weight[/b] and [b]Procedural Oriented[/b] - these DO use OBody's assigned presets on purpose, then vary them per NPC. So you'll still see presets assigned; the point is that two NPCs sharing one preset no longer look identical.
[/list]
If you want it to stop using RaceMenu/OBody presets entirely, pick [b]Procedural[/b].

[b]I already have a big preset collection (e.g. 1000+ CBBE 3BA presets). What does OBW add?[/b]
Far more variety, and no more clones. In Procedural mode it replaces the preset look with fully generated bodies. In [b]Sim Weight[/b] / [b]Procedural Oriented[/b] it KEEPS your curated presets but gives each NPC her own size/proportions, so NPCs sharing a preset stop being copy-paste twins. Pick the mode by whether you want to keep your presets' look (Oriented / Sim Weight) or maximum variety (Procedural).

[b]Which of the three modes should I pick?[/b]
[list]
[*]Want the world to just fill with varied bodies, no presets needed -> [b]Procedural[/b].
[*]Love a specific preset collection but hate the clones -> [b]OBody Sim Weight[/b] (keeps the preset, varies the fullness).
[*]Want your presets' flavour but still some generated variety -> [b]Procedural Oriented[/b] (set the blend slider).
[/list]

[size=4][b]Requirements & setup[/b][/size]

[b]Do I need to collect BodySlide presets?[/b]
No. In Procedural mode OBW builds every body itself. The other two modes reuse whatever OBody already distributes.

[b]Do I still need armor/body BodySlide patches?[/b]
Yes. OBW reshapes the body you built; it does NOT replace body/armor meshes. You still need your CBBE/3BA + HIMBO bodies AND your armor patches built in BodySlide with [b]"Build Morphs" checked[/b] (zeroed sliders, per OBody's install instructions). Without the morphs/.tri, there's nothing for OBW to move.

[b]My bodies look broken - everyone is either zero-sliders or fully cranked, cleavage pokes through clothes. Help?[/b]
Two common causes:
[list]
[*][b]CBBE base instead of CBBE 3BA.[/b] OBW drives the full 3BA slider set. On a plain CBBE body many of those sliders don't exist, which can look wrong. Use [b]CBBE 3BA[/b] (Female) for the intended result.
[*][b]OBody's "ORefit" option.[/b] A few setups (often VR / older RaceMenu) get broken distributions with ORefit ON. If your bodies break, try turning [b]ORefit OFF[/b] in OBody, then reprocess. 
[/list]

[size=4][b]Body types & compatibility[/b][/size]

[b]Does it support BHUNP?[/b]
OBW is built and calibrated for [b]CBBE / CBBE 3BA[/b]. Because 3BA and BHUNP share most BodySlide slider names, the core shape does carry over, but it is not officially tuned for BHUNP and some BHUNP-only sliders won't be driven. If you run BHUNP, use the [b]Base body[/b] setting in the menu so the realism toggles match your body (see the next question).

[b]What are the "Natural" and "Curvy" sliders?[/b]
Two optional realism axes:
[list]
[*][b]Natural women[/b] - gives a share of women a more moderate, natural bust and waist (a BHUNP-style realistic look) on top of the curvier default. Aimed at CBBE/3BA users who want less exaggeration.
[*][b]Curvy women[/b] - the opposite pole: a fuller, perkier, more cinched 3BA-style look. Aimed at BHUNP users who want some of that curvier shape.
[/list]
The menu shows the one that fits your [b]Base body[/b] (Auto-detect, or set CBBE / BHUNP manually).

[b]My player character's body keeps changing. How do I set my own body?[/b]
Assign your body with [b]OBody NG's own key ('O' by default)[/b] - OBW leaves OBody's direct assignments alone. Only OBW's [b]re-roll key[/b] generates a random body for whoever you aim at (including you). If you don't want to touch your PC, just don't re-roll yourself. Tip: set OBody's own selection hotkey and OBW's re-roll key to different keys.

[b]Will my followers / custom NPCs get changed?[/b]
Only if you let them. Open the [b]Exclusions[/b] list and tick their mod, or exclude a single NPC by aiming at them and pressing the exclude key (or add a [b]Plugin.esp|0xFormID[/b] line). Your choices persist across saves.

[b]Does it conflict with Racial Body Morphs / height mods? Is there height variation?[/b]
OBW controls body shape/size and will [b]overwrite[/b] weight-based body mods like Racial Body Morphs. It does [b]not[/b] change height (yet) - height touches combat and animation, so it's a bigger job; it's on the maybe-later list. Height mods can run alongside since OBW doesn't touch height.

[b]I use a CBPC physics config (e.g. Petite to Plenty). Will OBW's physics conflict?[/b]
OBW ships a tame per-body physics profile (soft/heavy jiggle more, toned/lean stay firm) that's only applied if CBPC is installed. If you prefer your own config, delete OBW's CBPC files in its folder and yours will be used. Without CBPC, the physics part is simply skipped.

[b]Armor/body flickers invisible for a moment when I get near an NPC.[/b]
That's the body applying as the NPC streams in. Update to the [b]latest version[/b] (the body-pop was reduced), and if it persists on Procedural, [b]OBody Sim Weight[/b] applies more quietly. Make sure your bodies/armors are built with morphs so there's no rebuild hitch.

[size=4][b]Controls[/b][/size]

[b]How do I exclude just one NPC instead of a whole mod?[/b]
Aim at them and press the [b]exclude-target key[/b] (bind it in the menu), or add a [b]Plugin.esp|0xLocalFormID[/b] line to an exclusions file. Per-FormID exclusion is supported.

[b]The Exclusions list stops listing plugins partway through the alphabet.[/b]
The list is paginated (Papyrus caps a page at ~120 entries). Use the page nav at the bottom of the Exclusions page; anything still missing can be added by hand to [b]OBodyNGWeight_Exclusions_MCM.txt[/b].

[b]How do I reset / rebind the hotkey?[/b]
Rebind it in the menu (Re-roll key / Exclude key). The default re-roll key is the [ key; you can set it in the INI ([b]ReRollKey[/b] scancode) if the menu rebind gets stuck (e.g. on Steam Deck).

[b]Men too?[/b]
Yes - women and men are both supported and can be toggled independently, with a build dial for men.

[b]Does it change faces or the NPC's weight number?[/b]
No. It only shapes the body (via SKEE morphs), and keeps faces and outfits matching - no neck seams, no real-weight edits.
```

---

## PART 2 — Description improvements (edits to make in the existing sections)

1. **Requirements** — make the 3BA point unmissable (several "broken body" reports were plain-CBBE users):
   change the body line to:
   `[*]A [b]CBBE 3BA[/b] female body (the full 3BA slider set - plain CBBE will look wrong) and a [b]HIMBO[/b] male body, both built in BodySlide with "Build Morphs" checked.[/*]`

2. **New "Compatibility" section** (add after Requirements) — this is the single biggest source of posts:
```bbcode
==================================================================
COMPATIBILITY
==================================================================
[list]
[*][b]Body mods that set weight (e.g. Racial Body Morphs)[/b] - OBW controls body size/shape and will overwrite them. Height mods are fine (OBW doesn't touch height).
[*][b]Preset collections[/b] - fully compatible. Procedural ignores them; Sim Weight / Oriented reuse and vary them.
[*][b]Your own body[/b] - assign it with OBody NG's own key ('O'); OBW leaves direct OBody assignments alone.
[*][b]CBPC configs[/b] - OBW's per-body physics only applies with CBPC; delete OBW's CBPC files to use your own config.
[*][b]CBBE 3BA required[/b] - plain CBBE lacks the 3BA sliders OBW drives. BHUNP is not officially supported (shared sliders carry the core shape; set 'Base body' in the menu).
[*][b]OBody 'ORefit'[/b] - if bodies come out broken (all min/max), try turning ORefit OFF in OBody.
[/list]
```

3. **HOW TO USE** — add one line so PC-body confusion stops:
   `[*]To set YOUR OWN body (or a specific NPC's), use OBody NG's own key ('O') - not OBW's re-roll key. OBW never overrides an OBody direct assignment.[/*]`

4. **MAIN FEATURES** — add the two realism axes (once shipped):
   `[*][b]Realism axes[/b] - optional 'Natural' (moderate, BHUNP-style) and 'Curvy' (fuller, 3BA-style) dials, gated to your base body, for a slider between realistic and curvy crowds.[/*]`

5. **Add a short "KNOWN / FIXED" note or lean on the changelog** for the two most-reported early issues, both addressed:
   - thick thighs + flat butt (now correlated via regional parity),
   - body/clothing flicker on approach (body-pop reduced in 1.4.7+).
