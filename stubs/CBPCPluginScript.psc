scriptName CBPCPluginScript hidden
{ Compile stub - real script ships with CBPC (cbp.dll). Only the functions we call.
  CBPC re-evaluates the per-actor physics config conditions (IsInFaction etc.) when
  RefreshActorBounceSettings is called, so OBW assigns the tier faction then refreshes. }

function ReloadConfig() global native
function RefreshActorBounceSettings(Actor npc) global native
function RefreshActorCollisionSettings(Actor npc) global native
; Scale an actor's physics by interpolating a named config (UniqueName) at a percentage (0-100).
; ADDITIVE on top of the actor's existing physics - does not replace the config.
function ApplyBounceInterpolation(Actor npc, String uniqueName, int percentage) global native
function ApplyCollisionInterpolation(Actor npc, String uniqueName, int percentage) global native
