#pragma once
// Addresses and struct offsets for ONE game build. Every value here was
// verified live with cdmp_probe; see docs/research/hallazgos-en-vivo.md.
// On any other build the mod refuses to read game memory.

#include <cstdint>

namespace cdmp::game::build_2976 {

// CrimsonDesert.exe 1.0.0.2976 = public patch 2.03.02 (Steam build 25477059).
inline constexpr uint32_t kPeTimestamp = 0x6AE6E3E8;
inline constexpr uintptr_t kImageBase = 0x140000000;  // fixed, no ASLR

// Static globals (RVAs).
inline constexpr uintptr_t kWorldSystemSlot = 0x71B6C50;  // -> WorldSystem*
inline constexpr uintptr_t kChunkOrigin = 0x71A2EB0;      // vec3: world = physics local + this
inline constexpr uintptr_t kStaticPlayerPos = 0x71C5DF0;  // vec3 world pos (cross-check)
// The client's game clock (game_clock.h): exe+0x1416B80 returns, on client
// threads, [kClientFrameClockMs] + [kClientFrameClockUs] / 1000 (cached per
// frame; checked live against our Req timestamps); handlers add the server
// offset [kClientClockBias].
inline constexpr uintptr_t kClientClock = 0x15F8610;
inline constexpr uint8_t kClientClockHead[] = {0x80, 0x3D, 0x5E, 0xA7, 0xAD, 0x05, 0x00, 0x75,
                                               0x17};
inline constexpr uintptr_t kClientFrameClockMs = 0x70D2DA0;  // u64
inline constexpr uintptr_t kClientFrameClockUs = 0x70D2DB0;  // u64
inline constexpr uintptr_t kClientClockBias = 0x71B6EF8;     // i64, set by ServerTimeInfoAck
// The client's copy of the time of day, pa::FieldTime (32 bytes; field_time.h,
// hora-del-dia.md section 8). The server's copy is at 0x6A6D238.
inline constexpr uintptr_t kFieldTimeClient = 0x6EA4998;
inline constexpr uint16_t kOpSetFieldTimeReq = 2227;  // FieldTime(31) | u8 flag | u32 key
inline constexpr uint16_t kOpSetFieldTimeAck = 2952;
// Our client's weather, sent when it changes (hora-del-dia.md section 3): f32 rain,
// snow, temperature (C), cloud?, fog?, wind? (24 bytes). Build 2976: 2560.
inline constexpr uint16_t kOpSyncWeatherStateReq = 2689;  // u32 key | FieldTime(31)

// vtables (RVAs), used to validate every hop of the chain.
inline constexpr uintptr_t kVtClientActorManager = 0x593ECF0;
inline constexpr uintptr_t kVtControlledActor = 0x590CCA0;  // ClientChildOnlyInGameActor
inline constexpr uintptr_t kVtCharacterControl = 0x593D8E0;
inline constexpr uintptr_t kVtStatusComponent = 0x59146E0;
inline constexpr uintptr_t kVtPhysicsObject = 0x60C52E0;

// WorldSystem
inline constexpr uint32_t kWorldSystem_ActorManager = 0x30;
// ClientActorManager
inline constexpr uint32_t kActorManager_ControlledActor = 0x50;
// ClientChildOnlyInGameActor
inline constexpr uint32_t kActor_ComponentTable = 0x68;
// Component table
inline constexpr uint32_t kComponents_Status = 0x20;
inline constexpr uint32_t kComponents_CharacterControl = 0x40;
// ClientCharacterControlActorComponent -> physics object
inline constexpr uint32_t kControl_Inner = 0x140;
inline constexpr uint32_t kControlInner_Physics = 0x2C0;
// Physics object: rows are the local X/Y/Z axes, then the chunk-local position.
inline constexpr uint32_t kPhysics_AxisX = 0x60;
inline constexpr uint32_t kPhysics_AxisY = 0x70;
inline constexpr uint32_t kPhysics_AxisZ = 0x80;
inline constexpr uint32_t kPhysics_Position = 0x90;

// Stats: entries = [[Status+0x18]+0x58], count = u32 at container+0x60,
// each entry 0x90 bytes: +0x00 id, +0x08 current, +0x10 regen, +0x18 max
// (int64, value * 1000).
inline constexpr uint32_t kStatus_StatContainer = 0x18;
inline constexpr uint32_t kStatContainer_Entries = 0x58;
inline constexpr uint32_t kStatContainer_Count = 0x60;
inline constexpr uint32_t kStatEntry_Size = 0x90;
inline constexpr uint32_t kStatEntry_Id = 0x00;
inline constexpr uint32_t kStatEntry_Current = 0x08;
inline constexpr uint32_t kStatEntry_Max = 0x18;
inline constexpr uint64_t kStatId_Health = 0;    // verified: regenerates, max 300
inline constexpr uint64_t kStatId_Stamina = 22;  // verified: drains while sprinting, max 120
inline constexpr uint32_t kMaxStatEntries = 64;  // sanity bound

// Camera (CrimsonDesertTelemetry layout; vtable and sentinel checked live).
inline constexpr uintptr_t kCameraSlot = 0x70D2F68;
inline constexpr uintptr_t kVtCamera = 0x60E5150;
inline constexpr uint32_t kCamera_Block = 0x428;
inline constexpr uint32_t kCameraBlock_Position = 0x80;
inline constexpr uint32_t kCameraBlock_Forward = 0x90;
inline constexpr uint32_t kCameraBlock_View = 0x3E0;
inline constexpr uint32_t kCameraBlock_Projection = 0x4E0;
inline constexpr uint32_t kCameraBlock_Sentinel = 0xAC0;
inline constexpr float kCameraBlockSentinelValue = 6360000.0f;

// Internal client/server messages (docs/research/troc-sintesis.md). Wire
// packet: u16 opcode | u8 slot | u16 payload_len | payload.
inline constexpr uintptr_t kClientPostPacket = 0x3A5A30;    // client -> server bytes
inline constexpr uintptr_t kClientOnRecvPacket = 0x3A5DC0;  // server -> client bytes
// First bytes of both functions, checked before hooking (from the AOBs).
inline constexpr uint8_t kClientPostPacketHead[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x4C, 0x89, 0x4C,
                                                    0x24, 0x20, 0x55, 0x56, 0x57, 0x41};
inline constexpr uint8_t kClientOnRecvPacketHead[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
                                                      0x24, 0x10, 0x48, 0x89, 0x7C, 0x24};
// At both entries: r9 = packet, [rsp+0x28] = u16 size; PostPacket r8d = session.
inline constexpr uint32_t kHookArgSizeStackOffset = 0x28;
// Static Troc handler instances (0x90 bytes each) live in this range.
inline constexpr uintptr_t kTrocInstancesBegin = 0x6E87D10;
inline constexpr uintptr_t kTrocInstancesEnd = 0x6EBB370;
inline constexpr uint32_t kTroc_Kind = 0x08;    // u32: 4 Req, 1 Ack/Nak, 2 auth, 0x7FFFFFFE internal
inline constexpr uint32_t kTroc_Opcode = 0x0C;  // u16
// Opcodes of this build (verified against the Troc table / RTTI at runtime).
inline constexpr uint16_t kOpActorLoadDataAck = 2005;
inline constexpr uint16_t kOpRemoveActorAck = 3067;
inline constexpr uint16_t kOpMoveActorReq = 2833;
inline constexpr uint16_t kOpMoveActorAck = 3094;
inline constexpr uint16_t kOpStartActionReq = 2202;
inline constexpr uint16_t kOpStartActionAck = 2552;
inline constexpr uint16_t kOpRotateActorReq = 2852;
inline constexpr uint16_t kOpRotateActorAck = 3078;
inline constexpr uint16_t kOpUpdateWallUpDataAck = 3024;
inline constexpr uint16_t kOpUnequipItemAck = 2615;
inline constexpr uint16_t kOpOtherEquipItemAck = 3136;
inline constexpr uint16_t kOpOtherUnequipItemAck = 2651;  // u32 key | u16 slot | u32 serial

// Map pins (docs/research/mapa-minimapa.md §1; capture cdmp_packets_20261003-211630):
//   CreateOrChangePinMarkerAck (6 + 22 N B): 04 | u32 owner | u8 group |
//                       N x {u64 id | f32 x, y, z (world) | u8 shape | u8 colour}
//   RemovePinMarkerAck (13 B): u32 owner | u64 id | u8 group
// The owner must be the LOCAL player (anything else is ignored). The group is
// not range-checked by the client: only 0 (normal) or 1 (trace), anything
// else writes past its two lists. The server accepts shape < 14, colour < 15
// and hands out ids from 0 (lowest free); ids are only compared, so ours sits
// far outside its range. Re-sending an id moves the icon but not the stored
// position (the UI rebuilds from it), so a move is Remove + Create. Pins
// created by an Ack alone never reach the server, nor the save.
inline constexpr uint16_t kOpCreateOrChangePinMarkerAck = 2647;
inline constexpr uint16_t kOpRemovePinMarkerAck = 2376;
inline constexpr uint64_t kCoopPinId = 0x7FDD00000000F001ull;
inline constexpr uint8_t kPinGroupNormal = 0;
inline constexpr uint8_t kPinShapeCount = 14, kPinColourCount = 15;

// Minimap icon of an actor (mapa-minimapa.md §2, §4): 0x9E89C0(minimap
// component, u16* icon, u16* sub icon, u8* a, u8* b), void; component +0x08 =
// actor. Icon keys come from a table of u16 at [[[[exe+root]]+8]+0x270].
inline constexpr uintptr_t kMinimapIconDecide = 0xAED090;
inline constexpr uint8_t kMinimapIconDecideHead[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
                                                     0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18, 0x55};
inline constexpr uintptr_t kMinimapIconRoot = 0x71B6FE8;
inline constexpr uintptr_t kMinimapIconRootAlt = 0x71B6FF0;  // used under a TLS flag
inline constexpr int kMinimapIconPlayerArrow = 0x126;       // the local player's arrow
inline constexpr int kMinimapIconCompanion = 0x100;         // a mercenary in our clan
inline constexpr int kMinimapIconOutlinedArrow = 0x124;     // key 23, the co-op default
inline constexpr uint32_t kActor_Key = 0x60;                 // u32 actor key

// Server-side writer of OtherEquipItemAck (what the MMO sent to OTHER players
// when someone changed gear; docs/research/apariencia-estado-coop.md §1). It
// runs on every equip path even in single player (then it has no recipients),
// so its arguments tell us the item ids, which no packet to our own client
// carries. rcx = server actor (u32 key at +0x60), r8 = u32* serial,
// r9 = vector {item* data, u32 count at +8}, items {u32 itemKey, u16 slot, pad} (8 B).
inline constexpr uintptr_t kServerOtherEquipWriter = 0x2C6E880;
inline constexpr uint8_t kServerOtherEquipWriterHead[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x4C, 0x89, 0x4C,
                                                          0x24, 0x20, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x55};
inline constexpr uint32_t kServerActor_Key = 0x60;

// Server-side writer of ChangeDockingSocketByFrameEventAck (2747): the MMO's
// relay of "weapon to hand / to back" frame events to other players
// (combate-coop.md §3 and annex). rcx = server actor, r8 = FrameEventAccessor:
// u8 type +0x08 (0 = action), u8 slot +0x10, u32 action hash +0x14,
// u16 id +0x18, u32 frame-event index +0x1C.
inline constexpr uint16_t kOpChangeDockingSocketByFrameEventAck = 2907;

// Messages the MMO server sent to every nearby player ("ToNear"), the subject
// included, so the sender's own client receives exactly the bytes the others
// got (apariencia-estado-coop.md §2.1). The subject actor's key is at +0.
// Any other actor key left in a relayed message would name an unrelated actor
// of the receiver's world, so such messages are dropped. Unless the layout is
// known (`fields_known`), every byte offset is checked; that also hits floats,
// hashes and ids that happen to look like a key (the tree hook's scene id and
// quaternion: both hooks of the 6 Oct Shadow test were dropped).
struct RelayedAck {
    uint16_t op;
    bool reliable;      // state that must not get lost (sent in order)
    uint16_t size;      // exact payload size seen in the captures; 0 = not measured
    uint16_t other[3];  // fields that may hold ANOTHER actor's key (source, caster,
                        // killer); when they do, they are set to the copy. 0 = unused
    bool fields_known = false;  // +0 and other[] / none[] are the only key fields
    uint16_t none[2] = {};      // key fields that must name no actor (else dropped). 0 = unused
};
inline constexpr RelayedAck kRelayedAcks[] = {
    {2740, false, 92, {0x54}},              // VaryHPAck: HP; +0x54 = who caused it
    {2305, false, 90, {0x54}},              // DataDefinedRegenerateValueAck; +0x54 = source
    {2362, false, 41, {}},                  // VaryDataDefinedStaticStatAck
    {2677, true, 77, {0x18, 0x1C, 0x21}},   // ApplyBuffAck: caster, target, tagged source (0x22 = actor)
    {2521, true, 77, {0x18, 0x1C, 0x21}},   // UnapplyBuffAck
    {2386, false, 42, {}},                  // UpdateChangeBuffLevelAck
    {3143, false, 118, {0x28}},             // UpdateElementalAck (wet, burning...); +0x28 = source
    {2985, true, 19, {0x04}},               // NotifyHandleDeadAck: +0 dead (the sender), +4 killer
    {2225, true, 0, {}},                    // NotifyHandleFatalAck
    {3043, true, 0, {}},                    // NotifyHandleReviveAck
    {3053, true, 18, {}},                   // ReviveAck
    {2101, true, 279, {}},                  // CharacterCustomizationAck
    // World interactions (interacciones-mundo.md): hooking a tree with the
    // rope. Carries a scene-object id (not an actor key), same on every PC.
    // +0 who hooks | +0x04 u8 slot | +0x05 u8 type | +0x09 u8 target class (2 =
    // scene object) | +0x0A target actor (0 for a tree) | +0x0E scene id 16 B |
    // +0x1E u64 piece (made by the hooking client: local to its PC) | +0x26
    // vec3 hook point | +0x32 vec3 | +0x3E quat | +0x4E string ... (reader
    // 0xB5B710; arbol-cuerda-fallo.md).
    {2395, true, 97, {}, true, {0x0A}},     // RemoteCatchResultAck
};
inline constexpr uint16_t kOpRemoteCatchResultAck = 2395;
inline constexpr size_t kRemoteCatchSceneId = 0x0E;  // 16 B
// UpdateOnDemandGimmickAck: 04 | u32 player | u8 1 spawned / 0 unspawned |
// scene id 16 B. A tree becomes a gimmick (an actor) only near each player,
// on each PC on its own (interacciones-mundo.md §1.1).
inline constexpr uint16_t kOpUpdateOnDemandGimmickAck = 2923;
inline constexpr uintptr_t kServerDockingWriter = 0x2BD7D20;
inline constexpr uint8_t kServerDockingWriterHead[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57,
                                                       0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
// MoveData: the movement block shared by MoveActor (payload +0x04) and
// StartAction (+0x8E). Build 3479 reordered it and grew it from 218 to 222
// bytes (PORTING-3479.md). Offsets below are MoveActor payload offsets;
// StartAction's are kSaMoveData - kMvMoveData + these.
//   +0x04 local Transform (pos, quat, scale) | +0x2C vec3 velocity | +0x38 vec3 |
//   +0x44 u64 parent/mount key | +0x4C parent's world Transform | +0x74 vec3 |
//   +0x80 vec3 | +0x8C Transform | +0xB4 world quat | +0xC4 world pos | +0xD0 u32 |
//   +0xD4 u64 ts | ...
// On foot local == world and the parent Transform is identity. Riding (3479
// capture, 1,861 Moves): local = offset in the saddle (0.03, 1.37, 0.51), the
// parent Transform = the horse's world position, world = horse + offset.
// Req = key | MoveData | f32 f32 | f32 speed | 10 x 28 B history.
// Ack = key | MoveData | u8 0 | f32 f32 | f32 speed.
inline constexpr size_t kMvMoveData = 0x04;
inline constexpr size_t kMvPos = 0xC4;          // world (2976: 0x04)
inline constexpr size_t kMvRot = 0xB4;          // world (2976: 0x10)
inline constexpr size_t kMvLocalPos = 0x04;     // relative to the parent (2976: 0x4C)
inline constexpr size_t kMvLocalRot = 0x10;     // (2976: 0x58)
inline constexpr size_t kMvLocalScale = 0x20;
inline constexpr size_t kMvParent = 0x44;       // 0xFFFFFFFFFFFFFFFF on foot (2976: 0x8C)
inline constexpr size_t kMvParentPos = 0x4C;    // the horse while riding (2976: 0x94)
inline constexpr size_t kMvParentRot = 0x58;    // (2976: 0xA0)
// UNVERIFIED: the u32 that sometimes held an actor key in 2976 (+0x20); 0 in
// every 3479 Move captured so far.
inline constexpr size_t kMvKeyField = 0xD0;
inline constexpr size_t kMvTs = 0xD4;          // u64 client ms (2976: 0xD5)
inline constexpr size_t kMvAckSplit = 0xE2;    // end of MoveData: the Ack's u8 goes here
inline constexpr size_t kMvWireSize = 0xEE;    // without the history (2976: 0xEA)
inline constexpr size_t kMvReqSize = 518;      // 2976: 514
inline constexpr size_t kMvAckSize = kMvWireSize + 1;
// StartAction: key | A[0] 69 B | A[1] 69 B | B (MoveData first) | u8 flag.
// Ack: key | A[0] | A[1] | u8 flag | B.
inline constexpr size_t kSaMoveData = 0x8E;
inline constexpr size_t kSaReqSize = 729;  // 2976: 725
inline constexpr size_t kSaFlag = kSaReqSize - 1;
inline constexpr size_t sa_md(size_t mv_off) { return kSaMoveData + mv_off - kMvMoveData; }
inline constexpr size_t kSaAfterMd = kSaMoveData + (kMvAckSplit - kMvMoveData);  // two u32 follow

// ActorLoadDataAck: Transform at the end (position, rotation quaternion, scale).
inline constexpr size_t kLoadRotationFromEnd = 146;  // 2976: 142

// Ledge climb-up ("WallUp") path of a character, which a remote copy needs to
// place its climb-up animation (broadcast-servidor-movimiento.md §10).
inline constexpr uint32_t kControl_MoveController = 0xC8;  // X = [CC+0xC8]
// 3479: the method index and the method array moved (+0x4C / +0x48; 14 code
// sites read byte [mc+0x13C] then [mc+idx*8+0x140]); 2976: +0xF0 / +0xF8.
inline constexpr uint32_t kMoveController_Method = 0x13C;  // u8 active movement method
inline constexpr uint32_t kMoveController_WallUp = 0x168;  // methods[5]: WallUpMovementMethod* (2976 0x120)
inline constexpr uintptr_t kVtWallUpMovementMethod = 0x591DF78;
inline constexpr uint8_t kMoveMethodWallUp = 5;
inline constexpr uint32_t kControl_WallData = 0xD8;        // D = [CC+0xD8]

// PointClimbMovementMethod::Enter (vf7). Fails on a remote copy (its climb
// sensor only runs for the local character), which freezes the remote copy for
// the whole wall climb (build/research/pointclimb/informe-pointclimb.md).
inline constexpr uintptr_t kPointClimbEnter = 0x75FFC0;
inline constexpr uint8_t kPointClimbEnterHead[] = {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x10, 0x49,
                                                   0x89, 0x6B, 0x18, 0x49, 0x89, 0x73, 0x20, 0x57};
// RideOn / RideOff movement methods (6 / 7): their Enter needs the rider<->
// mount link in the vehicle component, which a remote copy never gets
// (docs/research/caballo-coop.md §3.4). Same prologue for both.
// Advances the play time of one action layer of a character's control
// component (caballo-coop.md §9.1). rcx = &layer+0x18 (the playback of the
// layer's current action), xmm1-3 times, [rsp+0x28] float* rest; returns the
// time used in xmm0. Its caller 0x2329700 does not check for a null playback,
// which a network-driven horse copy has until its first action: crash at
// +0x76 reading 0x10. Nothing to advance -> return 0 (as the caller does when
// the rate is <= 0).
inline constexpr uintptr_t kActionLayerAdvance = 0x214C070;
inline constexpr uint8_t kActionLayerAdvanceHead[] = {0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0x80,
                                                      0x00, 0x00, 0x00, 0xC5, 0xF8, 0x29, 0x78, 0xD8};
// Advances both action layers of a character-control component
// (rcx = component; +0x08 actor; layers at +0x88 / +0x90, each with the
// playback at +0x18 and its own action chart at +0x110). Called once per
// frame from 0x863E80 at 0x8641C6, which checks every layer itself
// afterwards and ignores the return value; the out floats at rdx keep the
// caller's defaults when it is skipped. A layer with a chart and no playback
// crashes more than one function below it (live test 2026-10-03: exe+0x1F63FE6
// and exe+0x232D5FF), so the puppet's update is skipped in that state.
// Inside 0x7F58D0: `mov rdx,[rax+0x10]; cmp byte [rdx+0x116],r15b; jge +skip`.
// A copy's layer with no playback ([layer+0x10] = 0) crashed here (build 3479).
inline constexpr uintptr_t kLayerPlaybackRead = 0x7F5AA2;
inline constexpr uintptr_t kLayerPlaybackSkip = 0x7F5EF3;  // the jge's own target
inline constexpr uint8_t kLayerPlaybackReadHead[] = {0x48, 0x8B, 0x50, 0x10, 0x44, 0x38,
                                                     0xBA, 0x16, 0x01, 0x00, 0x00};
inline constexpr uintptr_t kActionLayersAdvance = 0x2516190;
inline constexpr uint8_t kActionLayersAdvanceHead[] = {0x48, 0x8B, 0xC4, 0x41, 0x55, 0x41, 0x56, 0x48,
                                                       0x83, 0xEC, 0x68, 0xC5, 0xF8, 0x29, 0x70, 0xC8};
inline constexpr uint32_t kControl_Layer0 = 0x88;
inline constexpr uint32_t kLayer_Playback = 0x18;
inline constexpr uint32_t kLayer_Chart = 0x110;
inline constexpr uintptr_t kRideOnEnter = 0x74F270;
inline constexpr uintptr_t kRideOffEnter = 0x750240;
inline constexpr uint8_t kRideEnterHead[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48,
                                             0x89, 0x70, 0x18, 0x55, 0x57, 0x41, 0x54, 0x41};
inline constexpr uint8_t kLoadKindMount = 5;  // ActorLoadDataAck +0x0D: 1 player, 5 mount
inline constexpr uint32_t kMethod_Actor = 0x20;       // movement method -> its actor
inline constexpr uint32_t kActor_OwnerUser = 0x90;    // u32 owning user key (0 = none)
// ActorLoadDataAck (both the user-owned player actor and its world body):
// world position (3 floats) sits this many bytes before the end of the payload.
inline constexpr size_t kLoadPositionFromEnd = 158;  // 2976: 154 (verified in a 3479 capture)

// While a save loads the actor sits at (0, 1000, 0) before being placed.
inline constexpr float kLoadingParkHeight = 1000.0f;
inline constexpr float kLoadingParkRadius = 5.0f;

}  // namespace cdmp::game::build_2976
