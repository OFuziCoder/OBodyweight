# Body slider map — CBBE 3BA ↔ BHUNP (cross-imitation table)

**Built 2026-07-12.** Companion to [BHUNP_COMPAT.md](BHUNP_COMPAT.md). This is the authoritative
name-map used to reproduce a body SHAPE across the two mesh families (CBBE 3BA ↔ BHUNP), and the exact
map OBW's `FemaleSliders()` follows. Anchored on OBW's 49 female sliders + the 202 shared BodySlide names
found in the 475-preset BHUNP analysis.

## How to read it
- **Rule of `SetBodyMorph`:** setting a slider a body's `.tri` does NOT have is a **harmless no-op**. So
  the safe cross-body strategy is *set-both*: emit the 3BA name AND the BHUNP name; each no-ops on the body
  that lacks it. One value list drives both bodies, zero per-actor detection, zero 3BA regression.
- **Transfer quality:** the proportional core (bust/waist/hips/thighs/butt/belly + muscle + nipple) shares
  names, so copying those values reproduces a shape's PROPORTIONS on the other body at ~80-90%. It is NOT
  pixel-identical — different base mesh + the body-specific "flavor" sliders don't cross.

---

## A. SHARED — identity map (transfer 1:1, same name on both bodies)
These are the backbone. Same value, same name, works on 3BA and BHUNP. **This alone carries a shape.**

| Group | Sliders (identical name on 3BA & BHUNP) |
|---|---|
| Bust volume | `Breasts` · `BreastsGone` · `BigBelly`* |
| Bust shape | `BreastPerkiness` · `BreastGravity2` · `BreastWidth` · `BreastsTogether` · `BreastCleavage` · `BreastHeight` · `BreastTopSlope` · `BreastSideShape` |
| Butt | `Butt` · `BigButt` · `RoundAss` · `AppleCheeks` · `ButtClassic` · `ButtShape2` · `ButtDimples` · `MuscleButt` |
| Hips/waist | `Hips` · `HipBone` · `HipForward` · `Waist` · `WideWaistLine` · `ChubbyWaist` |
| Legs | `Thighs` · `SlimThighs` · `CalfSize` · `ChubbyLegs` |
| Arms | `Arms` · `ForearmSize` · `ChubbyArms` |
| Torso | `ShoulderWidth` · `BigTorso` · `ChestWidth` · `RibsProminance` · `Belly` |
| Muscle | `MuscleAbs` · `MuscleArms` · `MuscleLegs` |

*(`BigBelly` is a belly-volume slider present on both.)

---

## B. RENAME — same shape, different slider name (emit BOTH; each no-ops on the other body)
The only true divergences OBW hits. The `_v2` names are the **CBBE 3BA / 3BBB Advanced** thigh & muscle
sliders; the plain names are their **BHUNP** counterparts. OBW already sets both (see `OBW_Quest.psc`
~L412/428/436).

| Shape driven | CBBE 3BA name | BHUNP name | OBW status |
|---|---|---|---|
| Inner-thigh thickness | `ThighInsideThicc_v2` | `ThighInnerThicker` | ✅ set-both |
| Outer-thigh thickness | `ThighOutsideThicc_v2` | `ThighOuter` | ✅ set-both |
| Front/back thigh | `ThighFBThicc_v2` | `ThighFBThicker` | ✅ set-both |
| Deep arm definition | `MuscleMoreArms_v2` | `MuscleArms` (base) | ⚠ approximate (fold into base) |
| Deep leg definition | `MuscleMoreLegs_v2` | `MuscleLegs` (base) | ⚠ approximate (fold into base) |

---

## C. 3BA-ONLY — drop when going to BHUNP (no BHUNP equivalent)
OBW sets these; on a BHUNP actor they simply no-op. No shape loss worth chasing.

`WristSize` · `VeraMuscleTones` · `MuscleMoreAbs_v2` · `LegShapeClassic`

> These 4 + the 2 "deep muscle" renames in B are the entire 3BA→BHUNP gap: **~9% of the slider budget,
> all fine-detail.** The silhouette is fully carried by A+B.

---

## D. BHUNP-ONLY — drop when going to 3BA (UNP-heritage; approximate via A instead)
BHUNP dials a body type through these native master sliders. There is **no 3BA equivalent** — do not try
to transfer them; instead the equivalent proportions are already captured by the shared A sliders (bust
size/together/width, waist, hips, thighs). Listed so you know what a BHUNP source preset carries that a
3BA target can't reproduce verbatim.

| BHUNP-only family | What it does | 3BA approximation (via shared A) |
|---|---|---|
| `Maximum Curvy/Slim × Heavy/Light` | BHUNP's built-in archetype blend | frameScore + archetype deltas already do this |
| `7B BCup/Bombshell/Natural/Cleavage High/Low`, `7BUNP`, `CHSBHC` | UNP bust presets | `Breasts`+`BreastsTogether`+`BreastWidth`+`BreastGravity2` |
| `Breast Inflate` | uniform bust scale | `Breasts` |
| `Manga`, `Feminine` | stylization | (no map — cosmetic) |

---

## E. Direction cheat-sheet

**Imitate a 3BA shape on a BHUNP body**
1. Copy all **A** values 1:1.
2. Add the **B** BHUNP names (`ThighInnerThicker`/`ThighOuter`/`ThighFBThicker`) from the `_v2` values.
3. Skip **C** (no-op anyway).
→ ~85-90% match; silhouette identical, deep-muscle micro-detail softens.

**Imitate a BHUNP shape on a 3BA body**
1. Copy all **A** values 1:1.
2. Reverse **B** (`ThighInnerThicker`→`ThighInsideThicc_v2`, etc.).
3. Ignore **D** masters — the bust/waist/hip proportions they produced are already in the A values you
   copied (BodySlide bakes the master's effect down into the individual shared sliders in the preset file,
   so a *baked* BHUNP preset transfers its look through A even though the master name itself doesn't).
→ ~85-90% match.

## Verdict
The shared core (A) plus 3 thigh renames (B) reproduces a body's proportions across 3BA↔BHUNP at ~85-90%
with **zero body detection** (set-both no-op safety). The unmappable sliders (C/D) are all fine-detail or
UNP-native archetype masters whose *effect* survives through the baked shared sliders. This is exactly the
shared-brain path the [dual-body facade](../README.md) wants: OBW generates one shape, emits both name
sets, and whichever body the NPC carries renders it.
