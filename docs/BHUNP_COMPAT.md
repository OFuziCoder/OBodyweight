# BHUNP compatibility for OBW — study & plan

**Study date 2026-07-11.** Reference: BHUNP mod installed (`C:\MO2\mods\BHUNP Bodyslider Modder's
Resources` + `Baka Haeun UNP` SliderGroups) + 384 curated BHUNP body presets at
`D:\Skyrim Mods\Bodyslide Presets BHUNP\Extracted` (475 BHUNP-set presets analyzed).

## What BHUNP is (vs CBBE 3BA)
BHUNP (BHUNP = *UUNP Next Generation*) is a **UNP-lineage** body; CBBE 3BA is CBBE-lineage. Different mesh
topology / UV / `.tri` morph data, so a preset is body-specific and armor is built per-body. BUT both
descend from the same BodySlide slider-naming tradition, so their slider **vocabularies overlap heavily**.

- BHUNP presets use ~**297-321 sliders**; sets are `BHUNP 3BBB Advanced Ver 4` (dominant), `Ver 3/2`,
  `BHUNP 3BBB`, `BHUNP TBBP Advanced`, `Hyren Nyr BHUNP Body`.
- **BHUNP signature sliders (UNP heritage, no 3BA equivalent):** the `7B*` family (`7B BCup/Bombshell/
  Natural/Cleavage High/Low`, `7BUNP`, `CHSBHC`), and the **`Maximum Curvy/Slim × Heavy/Light`** master
  sliders (BHUNP's built-in body-archetype blends), `Breast Inflate`, `Manga`, `Feminine`. These are how
  BHUNP natively dials a body type.

## Slider overlap with CBBE 3BA (the key finding)
**202 slider names are SHARED** (apply to both bodies) — the entire core: `Breasts, Butt, Hips, Thighs,
Waist, Belly, Arms, CalfSize, ShoulderWidth, BigButt, RoundAss, AppleCheeks, Muscle{Abs,Arms,Legs,Butt},
Nipple*, BreastGravity2, BreastTopSlope, BreastsTogether, BreastCleavage, BreastHeight, BreastWidth,
BreastPerkiness, ChestWidth, WideWaistLine, HipBone, HipForward, SlimThighs, ChubbyLegs/Arms/Waist/Butt,
ButtShape2/Classic/Dimples, RibsProminance, BigTorso...` (full list in the analysis run). 95 are BHUNP-only,
the rest 3BA-only.

## OBW → BHUNP compatibility: **40 of 49 OBW female sliders work AS-IS** (82%)
OBW's `FemaleSliders()` (OBW_Quest.psc) applies 49 sliders. Cross-checked vs BHUNP's set:
- **SUPPORTED directly (40):** Breasts, Butt, Belly, Hips, Thighs, BigButt, Arms, ForearmSize, ChubbyArms,
  Waist, BreastsGone, BreastPerkiness, BreastGravity2, BreastWidth, HipBone, SlimThighs, MuscleAbs/Arms/Legs,
  CalfSize, ChubbyLegs, BigBelly, ShoulderWidth, RoundAss, AppleCheeks, ButtClassic/Shape2/Dimples,
  MuscleButt, BreastsTogether, BreastCleavage, BreastHeight, WideWaistLine, ChubbyWaist, BigTorso,
  ChestWidth, RibsProminance, HipForward, BreastTopSlope, BreastSideShape.
- **MISSING (9), with fix:** simple RENAME for 4 (BHUNP name in parens):
  `ThighInsideThicc_v2`→`ThighInnerThicker`, `ThighOutsideThicc_v2`→`ThighOuter`,
  `ThighFBThicc_v2`→`ThighFBThicker`, `MuscleMoreArms_v2/Legs_v2`→base `MuscleArms/Legs`. No BHUNP
  equivalent (skip, minor): `WristSize`, `VeraMuscleTones`, `MuscleMoreAbs_v2`, `LegShapeClassic`.

### Recommended OBW implementation (low-risk, NO body detection)
`SetBodyMorph` on a slider not present in an actor's `.tri` is a **harmless no-op**. So OBW can:
1. Keep setting the 40 shared sliders (already works on both bodies today).
2. For the 4 renamed thigh/muscle sliders, ALSO set the BHUNP name (set both the `_v2` 3BA name AND the
   BHUNP name) — each no-ops on the body that lacks it. => one slider list drives BOTH bodies, zero
   per-actor detection, zero regression on 3BA.
3. (Optional, richer) detect BHUNP actors and additionally drive BHUNP-native `Maximum Curvy/Slim
   Heavy/Light` for more authentic BHUNP variety — but not required; the shared 40 already produce the
   archetype shapes. Detection would key off the body ARMO/skin (BHUNP keyword via SPID, like the
   [[project_obw_dualbody_facade]] plan) — OBW is already the "brain" there.

## Can shapes be imitated across bodies? **YES, ~80-90% via the shared sliders.**
Because the proportional core (bust/waist/hips/thighs/butt/belly + muscle + nipple) shares slider names,
copying the shared-slider values reproduces a shape's PROPORTIONS on the other body. Caveats:
- Not pixel-identical: different base mesh + the body-specific sliders (`7B*`/`Maximum` on BHUNP;
  `*_v2`/`VeraMuscleTones` on 3BA) don't transfer, so fine detail/silhouette differs slightly.
- Practical path: a **name-map table** (shared = identity; the ~4 renames above; drop the ~5 unmappable),
  then apply the source preset's values to the target body's matching sliders. Good enough for "same look,
  other body" — which is exactly what the dual-body facade wants (OBW generates once, applies to whichever
  body the NPC carries).

## Verdict
BHUNP support in OBW is **cheap and safe**: the current 3BA slider set already covers BHUNP at 82%; adding
~4 BHUNP-name aliases (set-both, no-op-safe) pushes it near-complete with zero body detection and zero 3BA
regression. Shape cross-imitation is feasible via the shared-slider name map. Ties directly into the
[[project_obw_dualbody_facade]] (CBBE 3BA + BHUNP in one game) — OBW as the shared brain.
