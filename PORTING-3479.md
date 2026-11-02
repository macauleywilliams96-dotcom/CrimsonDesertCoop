# Port to build 1.0.0.3479

| | Old (supported) | New (target) |
|---|---|---|
| FileVersion | 1.0.0.2976 | 1.0.0.3479 |
| PE timestamp | `0x6AB28F00` | `0x6AE6E3E8` |
| Steam build | 25477059 | 26361232 |

Old build source: app 3321460, depot 3321461, manifest 8201393788450828800.

## Method

1. Get the old `CrimsonDesert.exe` (only that file) next to the new one.
2. For every address in the old exe, take a long, unique byte window around it
   (wildcarding rip-relative displacements and call targets) and search the new
   exe. Rip-relative globals are re-resolved from the matched instruction.
3. Vtables and Troc opcodes: re-derive from RTTI names (`tools/scripts/rtti_dump.py`),
   which survive rebuilds.
4. Turn each address into a startup signature scan, so the next patch is a
   re-scan instead of a re-port.
5. Struct offsets and payload layouts: verify live with `cdmp_probe` in game.
6. Regenerate `src/common/enemy_tables_2976.h` from the new game data.

Note: Denuvo code lives in the RWX `.xtls` section; some functions may only be
readable in live memory, not on disk.

## Inventory

- `src/mod/game/offsets.h`: ~30 RVAs (globals, vtables, hook targets with
  16-byte prologue checks), struct offsets, ~30 Troc opcodes, `kRelayedAcks`.
- Other RVAs: `actor_lookup.cpp`, `game_errors.cpp`, `mod.cpp`,
  `net/aggro_test.cpp`, `net/coop.cpp`, `net/learnt_store.cpp`.
- `src/common/enemy_tables_2976.h`, `field_time.h`, `keys.h`, `enemy_load.h`.
- Reference AOBs for some globals: `docs/research/cd-companion-justskip.md`
  (sigs block), `docs/research/cheat-tables-versions.md`, and the actively
  maintained tkhquang/CrimsonDesertTools (MIT).

## SOLVED: invisible copy and jumpy movement (2026-11-02)

- **Visibility**: ClientStatusActorComponent vf79 (0x6E90A0) sets the model
  opacity ([[CC+0xE8]+0x100]) to 0 whenever any hide bit is set in
  Status +0x3B8 | +0x3C0 (2976: +0x390 / +0x394, moved +0x28), then pushes it
  to the meshes (0xB628D0). A loaded copy gets bit 20 (0x100000) on every
  load. `keep_copies_visible` clears the bits every 250 ms and calls vf79
  (old mask -> 0). Writing the opacity alone does nothing.
- **Movement**: the copy's position buffer moved to mc+0x430 (rotation
  +0x4E8; 2976 +0x3D0 / +0x488; same structure). 3479 sends Moves at
  40-80/s, so the 30-sample buffer was always full and no longer covered
  now - 200 ms: freeze, then jump (S4). Moves are thinned to one per 40 ms of
  game time in `puppet_event`.

- **Climbing**: while attached to a climbed object (parent = a world
  actor), MoveData's local Transform is relative; `detach_to_world` copies the
  world Transform in (Moves and StartActions). Ledge climb-ups: movement
  method index/array moved to mc+0x13C/+0x140 (WallUp = methods[5] at
  +0x168); the wall data moved (+0x180 / +0x184, see read_player_wallup);
  UpdateWallUpDataAck is 135 bytes in 3479 (the vault block gained a u32 and
  lost a flag) and the handler rejected the old 132.
- Known gaps: some vault-overs still hide the copy; the horse copy is off
  ([coop] horse=0, as in the original release), so a riding copy sits in mid
  air and is reloaded on dismount.

## Status (2026-11-02)

Static port applied; builds and unit tests pass. **Not tested in game yet.**
Old exe: `C:\Users\Mac\Desktop\CDCoop\old\bin64\CrimsonDesert.exe` (verified 2976).
Raw matcher output: `docs/research/data/port_3479.json` (`tools/scripts/port_match.py`).

### Addresses (all 36 mapped)

| How | Items |
|---|---|
| Unique masked body match | ClientPostPacket, ClientOnRecvPacket, MinimapIconDecide, ActionLayerAdvance, IsTargetable, SetCurrentTarget |
| Body match + vtable slot 7 check | PointClimbEnter, RideOffEnter |
| Vtable slot (body match was wrong) | RideOnEnter `0x74F270` (vf7), CheckAIBattleType `0x24BE2F0` (vf1) |
| Call-site match | ErrorLogger, ClientClock (head bytes verified), ActionLayersAdvance, ServerDockingWriter |
| 1 of 3 clones, picked by the opcode immediate it writes | ServerOtherEquipWriter `0x2C6E880` |
| rip-relative references, all agree | WorldSystem, ChunkOrigin, StaticPlayerPos, clocks, FieldTime, camera, minimap roots |
| RTTI class name | all 7 vtables (WallUp and Status needed RTTI; the ref match was wrong for WallUp) |
| Scan of static Troc instances | TrocInstances `0x6E87D10`..`0x6EBB370` (1,175 messages, was 1,115) |

Hook prologue check arrays (`k*Head`) were refreshed from the new exe.

### Troc opcodes (all 60 used by the mod remapped)

Almost every opcode moved (e.g. MoveActorReq 2556 -> 2833). Mapped by message
class: RTTI vtable -> the static initializer that stores it -> instance opcode.
The same method reproduced every old opcode on the old exe. Patched in
`offsets.h` (incl. `kRelayedAcks`) and 12 files under `src/mod/net/`, `mod.cpp`.

### Live test 1-2 (2026-11-02, solo, F9 clone)

- Working: build check, sniffer hooks (1,174 types), game clock, spawn
  templates, injection of ActorLoadDataAck / RemoveActorAck, overlay.
- Game error logger: in 3479 it is a plain function, not a jmp into .xtls;
  `game_errors.cpp` accepts both.
- **MoveData changed** (the movement block in MoveActor and StartAction):
  reordered and 218 -> 222 bytes, so MoveActorReq 514 -> 518 and
  StartActionReq 725 -> 729. Every Move was dropped at the size check, so the
  clone never got a position. Readers confirm Req = key | MoveData | tail and
  Ack = key | MoveData | u8 | tail, as before. New layout and the old->new
  field map are in `offsets.h` (`kMv*`, `kSaMoveData`, `sa_md`); all users in
  `remote_puppet.cpp`, `local_capture.cpp`, `enemy_relay.cpp` go through it.
  The recorded 3,816 Moves / 171 StartActions all pass the converters now.
- Every other message seen in the capture kept its 2976 size.
### Live test 3 (2026-11-02, with horse riding)

- Moves now reach the copy (3,500 replayed), but the copy sat ~11 km away, at
  the world origin: **ActorLoadDataAck's Transform moved 4 bytes**, position
  158 from the end (was 154), rotation 146 (was 142). Fixed in `offsets.h`.
- Riding capture (1,861 mounted Moves) settled the MoveData semantics: +0x04
  is the LOCAL Transform (saddle offset when riding), +0x4C the parent's
  (horse's) world Transform, +0xC4/+0xB4 the rider's world pos/rot. The
  `kMv*` names now follow that; all recorded Moves (foot and riding) pass.
- Still unverified: `kMvKeyField` (0 in every captured Move).

### Live test 4 + memory probe (2026-11-02)

- The copy now spawns next to the player and replays moves and attacks (its
  attack VFX are visible), but **its body is not drawn**.
- Missed by the first port: 19 inline RVAs (`base + 0x...` in
  `remote_puppet.cpp`, rope/tree code and the actor-update hook; `Site`
  table in `horse_diag.cpp`). All matched (unique bodies or 3/3 callers,
  ScopeAttacherBase by RTTI) and patched; the hook-site bytes are identical.
- Probe (copy actor `ClientChildOnlyInGameActor`, key 0xA010F001):
  - same 56 components as the player, in the same order;
  - actor bytes +0xC8/+0xC9/+0xCD: player 2/2/0, own horse 1/1/0, copy
    **0/0/0xFF**;
  - ClientHierarchyActorComponent: player has 5 children (keys
    0xB01000C2..C6, likely attached gear), the copy has none.
- Next: find what sets actor +0xC8/+0xC9 (load state?) for a remote
  actor, and whether the body record (type 7) is accepted. The game error
  hook logged nothing for the copy.

### Invisible copy: ruled out (2026-11-02)

- Login: +0x5E 0x10 and +0xCF 1 on the copy (login loop 0x9A0620 uses the
  same offsets as 2976).
- +0xC8: an ownership category set by 0x5AE490 (2 local, 1 owned, 0 another
  user's); same rule in 2976, so 0 is expected for the copy.
- +0xCD: varies per session on the player too (Shift+F9 poke changed nothing).
- Body record: the copy's body 0xB010F001 exists (ClientNormalInGameActor),
  logged in, at (0, 100, 0) exactly like the player's.
- Character number: Ctrl+F9 spawn with the sender's number, still invisible.
- Same 56 components. Differences: Hierarchy has no children and no hknp
  simulation pointer; the 5 children of the player are its gear (type 7 loads
  right after the player load: weapon, shield, lantern, ...), not the body.

### Echo test (cdmp_peer relay) and crash

- Echoed player copy: invisible as well.
- First Ctrl+F6: Bandit copy 0xB01F0001 loaded and logged in.
- Second Ctrl+F6: **game crash** 0.2 s after injecting a Wandering Freesword
  copy. Dump: access violation reading 0x116 at exe+0x7F5AA6 (function
  ~0x7F58D0): `[[cc+0x88 + idx*8]+0x10]` is null, an action layer without
  playback, the same family as the 2976 crashes kActionLayersAdvance guards.
  Enemy copies only; not yet fixed.
- The Bandit copy showed its red minimap icon but **no body**: injected NPC
  copies are invisible too. The cause is common to every injected actor.

### More ruled out (2026-11-02)

- Injection call: OnRecvPacket's prologue and argument use are identical
  in 2976 and 3479.
- Server follow-ups: after 400 real world-actor loads the server sends
  nothing that would "show" an actor; follow-ups are client Reqs.
- Key range: a test build with copy keys inside the server pool (index
  0x3C01) was still invisible. Reverted to 0xF001.
- No server reply mentions the copy (all S>C traffic for its keys is ours).

### Lead: the client never builds the copy's equipment

- 2976 (experimento-4c.md): after the clone loaded, the client sent
  `OnDemandEquipDockingGimmickSpawnReq` x3 (it builds the carried gear).
- 3479: the client sends it for the player (`01 00 10 A0 ...`) and for NPCs
  (`... 10 B0 0E 00`), but **never for an injected copy**. So a client-side
  gate on building a character's equipment and appearance now excludes
  injected actors.
- Message instance: 2976 0x6A6F450 (op 2249), 3479 0x6EA6AF0 (op 2288).
  Referenced by the registration (0x254DA0), the server reader (0x2BD1980,
  called from 0x2BD08F9), and 0x50C40D0 / 0x54AA5C0 (no direct callers or
  calls found statically: virtual or obfuscated). 2976 pairs: 0x221910,
  0x2963890, 0x4D4D140, 0x510EA00.
- Next: find the client function that builds an actor's equipment and
  decides to send this Req (probably a ClientEquipSlotActorComponent or
  CharacterControl vfunc), diff its conditions between 2976 and 3479, and
  see which one an injected actor fails. A live trace (hook the Req's send
  and log the call stack for the player) would find it fastest.

### Gear trace (troc_sniffer `log_gear_trace`, 2026-11-02)

- 4 traces (3 player, 1 NPC). Shared path: 0xAB3100 / 0xAB6D00 ->
  0xAB26E0 -> 0xAB2D10 (sends the Req). 2976: 0x9AF360 / 0x9B2F70 ->
  0x9AE960 -> 0x9AEF70.
- 0xAB2D10 (builder): same 22 checks as 2976.
- 0xAB26E0 is not a decider: it is a job-system batch runner (scheduler in
  rdi, +0x429F8 / +0x42A00, dispatch 0x15E7A60) that runs the builder for a
  list of work items. The real gate is where an actor gets queued for that
  work, before the job system, so the stack traces cannot reach it.
- Next: find who fills the work list (writes to the list passed in r9 /
  rsi, or the callers of 0xAB3100 / 0xAB6D00) and diff that against 2976.

### The gear work comes from a world grid query

- 0xBDE730 (per-frame world update, sole caller) gets one actor through
  0x99D660 (the focus: the local player) and calls 0xAB3100(grid, actor).
- 0xAB3100: exits unless grid+0x322; reads the actor's TransformSync
  world position; radius from 0x1928770(actor); 0xAAFB40 = query of a
  2D cell grid around it (cells within radius, then 0xAB0180 gathers each
  cell: dense array grid+0x288/+0x290 or hash grid+0x268/+0x278/+0x280,
  plus the global list grid+0x260; per cell 0x22920C0); then 0xAB26E0
  (job runner) -> 0xAB2D10 builds gear and sends the Req; grid+0x320 = 1.
- So an actor gets gear only if it is registered in this grid near the
  player. Injected copies apparently are not (and their Hierarchy has no
  hknp simulation pointer, unlike the player's).
- vfunc diff of ClientHierarchy / ClientTransformSync components: new 3479
  checks in Hierarchy vf42 and TransformSync vf24 read flags +0x305 / +0x306
  of static character data (0x3DD960 by u16 id), but only for world actors
  (load type 7). The player copy is type 1, so they do not explain it.
- Next (live): `tools/scripts/refs_diff.py` lists the objects that point to
  the player actor but not to the copy, by class: the missing registration.

### Reference diff and the decisive reinject test (2026-11-02)

- refs_diff: player 583 references, copy 173. The copy is inside several
  actor-pointer arrays; the player's extras are NPC schedule data and
  untyped entries. No single clearly missing registration.
- **Alt+F9 test** (`enemy_debug_reinject`): RemoveActorAck for a nearby NPC
  (works, it vanishes), then its **original, byte-identical**
  ActorLoadDataAck injected 1.5 s later: delivered, no game error, and the
  NPC does **not** come back (2 of 2, NPCs 4-5 m away).
- ~~So on 3479 an injected ActorLoadDataAck alone no longer yields a visible
  actor~~ **Retracted**: snapshots (`cdmp_snap_<key>_before/removed/after`)
  show the re-injected NPC 0xB0100143 exists but stays at +0x5E 0x08,
  +0xCF 0: it is a group member (+0xAC parent 0xB0100141), the documented
  "member never logs in while its leader exists" case. The test now picks
  solo NPCs only (+0xAC == 0). The player copy, by contrast, does log in
  (+0x5E 0x10, +0xCF 1), so its invisibility is a different cause.
- No specific S>C message precedes real NPC loads (they come in batches with
  other loads and routine updates).
- The ActorLoadDataAck handler (vf2 0xDE7E70; 0xCB25A0 in 2976) is
  Denuvo-virtualised in both builds, so a static diff of it is not possible.
- **Solo NPC reinject works**: with +0xAC == 0 the re-injected original load
  comes back visible. Injection itself is fine on 3479; the problem is
  specific to player-type copies.
- Player copy, ruled out in this round:
  - ClientField grid: the copy is registered (TransformSync+0x7D1 = 1, set
    by the grid add 0xAAECC0, called only from TransformSync vf24 0xAFB350).
  - Grid gather mode 0x0A takes every actor in a cell, unfiltered.
  - Gear builder 0xAB2D10: for a non-local player-type actor the "visible"
    decision is the camera-frustum test 0xB2C0E0 (same as 2976 0xA255E0).
    Forcing it to "visible" for mod keys (hook at 0xAB2EC8 -> 0xAB2FC1,
    `install_copy_visible`) fired, but the copy stayed invisible and no
    gear Req was sent for it. Hook left in the code, not installed.
- 0xB2C0E0 is not a frustum test but the gear/appearance builder (the gear Req
  is sent from inside it, via obfuscated code). Vehicle+0xEB and
  [CC+0xB8]+0x19 are 0 for both the player and the copy: same path. Its
  return value is 0 for the (visible) player too, so it does not decide
  visibility.
- Login state: the server sends CharacterCustomizationAck (2101),
  EquipmentListOnLoginAck (2317), MercenaryEquipmentListAck (2856) and
  SubLevelInfoListAck (2970) for the player at login. Replaying them for the
  copy (keys rewritten, `replay_login_state`) changed nothing. Note: the
  observer re-keeps the injected copies, so later replays send puppet-keyed
  data; fix before relying on it.
- The copy's model exists: the skeleton chain of 0x5B4FD0 (2976 0x500730,
  identical) reaches a SceneObjectClient for both, and the copy's root
  rotation changes as it moves. Ownership +0xC8 set to 1 on the copy: no
  effect.
- **Skeleton children**: SceneObjectClient vf21 (0x15659F0) adds a child:
  +0x20 parent, +0x58 prev, +0x60 next sibling, parent +0x38/+0x40 first/
  last child. The player's skeleton nodes 2, 4, 5 have siblings; all the
  copy's nodes have none. The copy has no mesh parts attached.
  2976 copies loaded "with the equipment of the load moment"; in 3479 the
  player's equipment arrives separately (EquipmentListOnLoginAck), which is
  self-only. Next: dress the copy through OtherEquipItemAck (the MMO's
  other-player path), starting with a live equip change.
- Open options: trace the client's work after a real vs an injected load at
  runtime (hooks on the functions it reaches, e.g. actor creation and the
  world-grid insert), or ask the server side to create the actor (only NPCs
  have a cheat Req for that, SpawnCharacterCheatReq).

### Not verified (needs the game running)

- Struct field offsets (actor, component, physics, stats, camera, layers).
- Payload layouts and sizes, e.g. `kRelayedAcks` sizes, `kLoadPositionFromEnd`,
  if a message gained fields.
- `enemy_tables_2976.h` (generated from game data), `keys.h` key ranges,
  minimap icon ids.
- Comments still cite old addresses/opcodes; namespace is still `build_2976`.

### Next: in-game check (single PC)

1. `.\scripts\deploy.ps1`, launch, load a save.
2. Log should show the new timestamp and the reader enabled; F8 panel shows own
   HP/stamina; F9 clone + mirror (milestone 4c/4d) exercises the Troc hooks.
3. `cdmp_peer --mode relay` for the full loop before testing with a friend.
