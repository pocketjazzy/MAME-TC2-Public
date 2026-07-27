// license:BSD-3-Clause
// copyright-holders:Angelo Salese
/***************************************************************************

    Namco C139 - Serial I/F Controller

***************************************************************************/
#ifndef MAME_NAMCO_NAMCO_C139_H
#define MAME_NAMCO_NAMCO_C139_H

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// P068 (branch patch/linkplay-menu): the per-machine cfg handlers below take
// the configuration-manager types by value / pointer only.
enum class config_type : int;
enum class config_level : int;
namespace util::xml { class data_node; }


//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> namco_c139_device

class namco_c139_device : public device_t,
						  public device_memory_interface
{
public:
	// construction/destruction
	namco_c139_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	~namco_c139_device();   // out-of-line: m_context (unique_ptr<context>) needs class context to be complete

	// configuration
	auto irq_handler() { return m_irq_cb.bind(); }

	// status: true once the asio thread has accepted a peer or completed a connect
	bool comm_connected() const;

	// I/O operations
	void regs_map(address_map &map) ATTR_COLD;

	uint16_t status_r();

	uint16_t ram_r(offs_t offset);
	void ram_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	void data_map(address_map &map) ATTR_COLD;
protected:
	// inner class hosting the asio io_context, sockets, and worker thread
	class context;

	// device-level overrides
//  virtual void device_validity_check(validity_checker &valid) const;
	virtual void device_start() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual space_config_vector memory_space_config() const override;
public:
	// Phase 9d (permanent): host driver pushes our cabinet's state-machine call counter
	// (gp+0x75B8) here every vblank so the heartbeat timer can stamp it into
	// bytes 0-1 of replayed packets - that's what the partner's dispatcher
	// reads as "partner_counter" when computing the timeout delta.
	void set_local_counter(uint16_t counter) { m_local_counter = counter; }

	// Driver-pushed staging-phase signal (P060-era plumbing, retained as
	// shared infrastructure) - the host driver pushes it here every vblank:
	// mode2 = (staging mode word 0x802F3FD0 == 2, linked gameplay/cutscene),
	// mode_word = the raw word (transition-log context only).  Only the driver
	// can read the mode word (it maps m_mainram); the device debounces the
	// signal (HBPA_DEBOUNCE_VBLANKS consecutive mode-2 vblanks before the
	// in-game state arms, dropped immediately on loss).  Same emulation-thread
	// setter idiom as set_local_counter (written at vblank, read on the
	// emulation thread - no atomics needed).  MODEL PROVENANCE: Fable 5.
	// Consumer: the P063 TX-complete release v2 (m_txc2_armed, see the P063
	// member block).
	void set_ingame(bool mode2, uint32_t mode_word);

	// P002 (H2 "drift lockstep", branch patch/vblank-lockstep):
	// per-vblank frame-token barrier.  The host driver calls vblank_tick()
	// once per frame (vblank rising edge); when armed via
	// NAMCOS23_PATCH_VBLANK_LOCKSTEP the device sends a frame-token control
	// frame to the peer and stalls (bounded, wall-clock) whenever the local
	// frame count runs more than LOCKSTEP_MAX_LEAD frames ahead of the last
	// token received from the peer.  Inert (early-return) when not armed.
	void vblank_tick();

private:
	uint16_t reg_r(offs_t offset);
	void reg_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	void start_comm() ATTR_COLD;
	void send_pending_tx_frame();
	void deliver_rx_frames(int32_t param);

	// P010 (branch patch/chunk-reassembly): TX-side chunked
	// bulk-frame reassembly, env-gated by NAMCOS23_PATCH_CHUNK_REASSEMBLY
	// (inert when unset).  The ROM's TX chunk emitter (0x8000BB6C) splits
	// any message larger than 0xFF halfwords into successive TXSIZE=0xFF
	// chunks + remainder, writing TXOFFSET only for the FIRST chunk and
	// relying on the chip auto-advancing its DMA pointer for the rest.
	// We model that pointer, hold the chunks, and put the whole message on
	// the wire as ONE TCP frame.
	void chunk_drop(const char *reason);
	// P038 (branch patch/latch-v3-dedupe): record a wire completion of a
	// latched bulk class into the dedupe ring (called at the consumed_ok
	// retirement and at a latch dispatch; the definition documents why).
	void al_dedupe_record(uint16_t offset, uint32_t expected_hw);
	// P026 (branch patch/reasm-chunk-passthru): bulk_chunk=true marks a chunk of
	// an in-progress >255hw message forwarded individually under
	// NAMCOS23_PATCH_CHUNK_PASSTHRU - excluded from heartbeat capture (a chunk is
	// a message FRAGMENT; replaying or restamping one would interleave garbage
	// mid-message) and from the P021 op55 wire-flag injection (cell-walk
	// association is meaningless on a fragment).  Default false = existing
	// behavior for every other caller.
	void emit_tx_frame(std::vector<uint8_t> payload, bool bulk_chunk = false);

	TIMER_CALLBACK_MEMBER(irq_pulse_off);
	TIMER_CALLBACK_MEMBER(heartbeat_tick);

	const address_space_config m_space_config;
	devcb_write_line m_irq_cb;
	uint16_t* m_ram = nullptr;
	uint16_t m_regs[8];
	std::unique_ptr<context> m_context;
	emu_timer *m_irq_pulse_timer = nullptr;

	// Phase 9d synthetic-heartbeat state (permanent - the HB_CADENCE-paced
	// replay is the link keepalive/pacing token).
	emu_timer *m_heartbeat_timer = nullptr;
	std::vector<uint8_t> m_last_tx_payload;   // last real TX, replayed periodically
	uint16_t m_local_counter = 0;             // pushed in by host vblank

	// P002 (branch patch/vblank-lockstep) state.
	// Ownership: everything here is emulation-thread-only EXCEPT
	// m_lockstep_peer_token / m_lockstep_tokens_rx, which the asio network
	// thread writes (via lockstep_token_received()) and the emulation
	// thread reads - hence the atomics.
	void lockstep_token_received(uint32_t token)   // network thread
	{
		m_lockstep_peer_token.store(token, std::memory_order_release);
		m_lockstep_tokens_rx.fetch_add(1, std::memory_order_release);
	}

	bool m_lockstep_armed = false;                 // env gate, read in device_start()
	uint32_t m_lockstep_local_frame = 0;           // our vblank count = token payload
	std::atomic<uint32_t> m_lockstep_peer_token{0};// last token from peer
	std::atomic<uint32_t> m_lockstep_tokens_rx{0}; // count of tokens from peer
	bool m_lockstep_have_baseline = false;         // launch-stagger offset captured
	int32_t m_lockstep_offset = 0;                 // local - peer at link-up
	bool m_lockstep_suspended = false;             // free-running (peer token flow died)
	uint32_t m_lockstep_peer_at_suspend = 0;       // peer token when we suspended
	uint32_t m_lockstep_peer_at_streak = 0;        // peer token at start of timeout streak
	uint32_t m_lockstep_consec_timeouts = 0;       // consecutive full-timeout stalls

	// P010/P011 (branch patch/chunk-sequencing, stacked on
	// patch/chunk-reassembly).  All emulation-thread only.
	//
	// P011 changes the chunk-to-message ASSOCIATION from P010's "is the
	// bulk-announce flag still asserted on this saturated chunk" test (which
	// the ROM's TXSIZE-staged-after-trigger timing never lined up with - so
	// P010 held only the FIRST 255hw chunk and stale-swept it) to TX
	// SLOT/POINTER CONTINUITY: a held bulk message records the slot pointer it
	// began from (m_chunk_msg_start_ptr) and the advanced DMA pointer the next
	// chunk must resume from (m_chunk_resume_ptr).  ANY later chunk whose read
	// pointer resumes m_chunk_resume_ptr is the continuation and is appended -
	// regardless of the announce flag's timing - until accum reaches expected.
	// A chunk that starts at a slot base (a fresh TXOFFSET write) is a NEW
	// message, NOT a continuation, even if it reuses the same slot.
	bool m_chunk_armed = false;                    // env gate, read in device_start()
	uint16_t m_chunk_tx_ptr = 0;                   // auto-advancing TX DMA pointer (halfword index)
	bool m_chunk_tx_ptr_valid = false;             // latched at least once (host wrote TXOFFSET)
	uint32_t m_chunk_expected_hw = 0;              // total message halfwords from the 2 size cells before TXOFFSET (LAST announced)
	uint32_t m_chunk_held_expected_hw = 0;         // P011: expected total of the HELD message, snapshotted at hold (an interleaved TXOFFSET write would otherwise clobber m_chunk_expected_hw)
	bool m_chunk_bulk_pending = false;             // expected > 0xFF announced, message not yet emitted/dropped
	std::vector<uint8_t> m_chunk_accum;            // held chunk payload bytes awaiting the message tail
	attotime m_chunk_accum_since;                  // emulated time of the first held chunk (staleness)
	uint16_t m_chunk_msg_start_ptr = 0;            // P011: slot pointer (TXOFFSET) the held bulk message began at
	uint16_t m_chunk_resume_ptr = 0;               // P011: advanced DMA pointer the next continuation chunk must resume from
	uint32_t m_chunk_msg_chunks = 0;               // P011: chunks (first + continuations) accumulated for the CURRENT held message
	bool m_chunk_saw_txoffset = false;             // P011: a TXOFFSET write happened since the last send (PATH C reprogram); a send with this clear is a PATH B/B' continuation

	// P026 (branch patch/reasm-chunk-passthru): CHUNK PASS-THROUGH
	// emit mode, env-gated by NAMCOS23_PATCH_CHUNK_PASSTHRU (inert when unset;
	// setting it IMPLIES m_chunk_armed - the P010/P011/P014 machinery is the
	// association engine it rides on).  P025+P024 showed 2/2 runs: the slave
	// cab's link-session freshness dies within ~0.4 s of the FIRST coalesced
	// >255-halfword reassembled mega-frame arriving (329 hw t=45.94 / 383 hw
	// t=33.55) - the ROM's own chunks saturate at 255 hw because that is its
	// per-frame ceiling, and the receiving ROM's RX ring machinery (end-marker
	// back-scan + forward drain from its own cursor) is BUILT to reassemble
	// chunk sequences in the ring itself.  Pass-through keeps the ENTIRE P011
	// pointer-continuity association and the P014 txsize_commit dispatch
	// unchanged (m_chunk_accum still accumulates as the association/progress
	// TRACKER, so every is_continuation / commit-trigger predicate is
	// bit-identical) but forwards each chunk to the wire AS ITS OWN <=255-hw
	// frame the moment it is associated; on completion nothing extra is sent
	// (the chunks already went out) and the tracker is cleared.  Retired in
	// this mode: the rx_clear suppression (the mega-frame's held-chunk race it
	// protected is gone - continuations dispatch synchronously at the TXSIZE
	// write since P014 - and P025 showed the suppression itself wedging the
	// device against the ROM's buffer management for 104 s, 832 suppressions,
	// on a bulk announce whose chunks never came) and heartbeat replays while
	// a chunk sequence is open (a replay must never interleave a device-
	// invented frame between the ROM's chunks on the wire).  All emulation-
	// thread only.
	bool m_chunk_passthru = false;                 // env gate (NAMCOS23_PATCH_CHUNK_PASSTHRU); implies m_chunk_armed

	// P027 (branch patch/hb-cadence-wipe-restore, off
	// patch/reasm-chunk-passthru): three independently env-gated transport-
	// cadence changes targeting the P026 run-1 blue jitter (40 freshness-gate
	// dips = 75BC 0->2->0 with gate3502 cleared 0.23-0.72 s each, 38 of them
	// in the cutscene at ~1.1/s, 1:1 with starved inter-bulk gaps: the
	// cutscene exchange is snapshots ~1.4/s + 250 ms heartbeat replays
	// against the ROM's reset-to-2 / ceiling-17 drift budget = 0-2 frames of
	// margin; aggravated by 116 rxclear wipe races killing ~54% of red's
	// staged chunk sends PRE-WIRE - chkfail=0 both cabs proved the loss is
	// cadence, not integrity).  All three are transport-cadence only - no
	// ROM/game state is forced.
	// (a) NAMCOS23_PATCH_HB_CADENCE_MS=<ms>: heartbeat replay re-arm interval
	//     250 -> N ms.  The clock already re-arms on every captured real TX
	//     (emit_tx_frame), so the replay naturally fires "N ms after the last
	//     real TX"; when the override is armed, forwarded bulk CHUNKS also
	//     re-arm the clock (real wire traffic previously excluded from
	//     capture) without ever being captured as the replay payload.
	// (The P027 (b) one-shot wipe-restore gate - preserve a staged+announced
	// bulk-chunk TXSIZE through the peer rx_clear that wipes it in the head's
	// stage->START-edge race - was refuted by the P028 A/B (the restore gate
	// alone was the regression: round-start exchange decimated both
	// directions) and removed in P072; the race knowledge is retained at the
	// rx_clear wipe site in the .cpp, and the kept P031 announce-latch
	// repairs the race without fighting the ROM's green light.)
	// (The P027 (c) heartbeat-restamp reserve knob - stamp replayed heartbeats
	// from the peer's aged last-seen counter - was refuted (its stamps were
	// broken) and removed in P072; the P026 drift measurement it targeted is
	// retained at the heartbeat_tick stamp site in the .cpp.)
	// All emulation-thread only.
	bool m_hb_cadence_override = false;            // (a) env gate (NAMCOS23_PATCH_HB_CADENCE_MS parsed valid)
	uint32_t m_hb_cadence_ms = 250;                // (a) heartbeat re-arm interval, ms (250 = stock, bit-identical when unset)

	// Driver-pushed staging-mode-2 debounce state (P060-era plumbing, retained
	// as shared infrastructure; see set_ingame).  Retained P059/P060 knowledge:
	// the heartbeat replay is the pacing TOKEN that clears the PEER's reg5
	// stop-and-wait, so under stop-and-wait the peer's distinct fresh-emit
	// rate ~= 1000/cadence; flat HB=16 (P052) broke op55 ESTABLISHMENT
	// (attract/handshake floods) and the P060 phase-aware fast in-game cadence
	// failed catastrophically (every extra token pairs a STALE replay with a
	// fresh emit; the ROM wedged at ~120 deliveries/s) - the token lever is
	// DEAD; the adopted release is the P063 TX_COMPLETE_V2 self-release below.
	bool m_hbpa_ingame = false;                    // debounced phase: true = stable staging mode-2 (gates the P063 TX-complete release)
	uint32_t m_hbpa_mode2_streak = 0;              // consecutive mode-2 vblanks seen (debounce counter)
	uint32_t m_hbpa_transitions = 0;               // cumulative debounced in-game flips (transition-log context)

	// Retained P061-era knowledge (the STOCK reg5 stop-and-wait, P052 RE):
	// unpatched, the device models NO TX-complete - a staged TXSIZE (reg5)
	// survives until an rx_clear (a delivered PEER frame) wipes it, so the
	// ROM's TX pump 0x8000BB6C bails at its 0x8000BB80 busy-gate and one
	// distinct TX per direction is in flight, paced by the peer's heartbeat
	// replay token.  The chip's real release is gold txsize_commit (a non-zero
	// TXSIZE written while START is armed transmits; TXSIZE->0 + TX-done IRQ
	// follow from the serialization) - modeled by the adopted P063
	// TX_COMPLETE_V2 below.  The staged bytes are provably pristine only at
	// the stage instant (P035).

	// P031 (branch patch/announce-latch, off
	// patch/hb-cadence-wipe-restore): ANNOUNCE-LATCH DISPATCHER, env-gated by
	// NAMCOS23_PATCH_ANNOUNCE_LATCH (inert when unset; requires
	// NAMCOS23_PATCH_CHUNK_PASSTHRU - a NOTE is logged and nothing arms
	// without it).  The P029/P030 cross-run analysis showed EVERY room/area
	// skip (P026/P028/P029/P030, 3-for-3 run-kills) is one signature: a
	// round-start/boundary bulk announce loses the stage->START rx_clear race
	// (deliver_rx_frames runs at the top of the very reg_w that carries the
	// START edge and wipes the staged m_regs[5] first), the ROM re-announces
	// 3-6x across both ring buffers, exhausts, and ABANDONS the class
	// ROM-finally (the content recomposes only at the next scripted event -
	// 62 s late in P029).  Loss probability RISES with the peer arrival rate,
	// so no cadence/restamp recipe can fix it.  The wipe itself is an
	// emulation compromise - on real C139 hardware an RX never clears a
	// staged TX; rx_clear exists for the link-up busy-poll green light - so
	// remembering the stage across the wipe is legitimate emulation repair,
	// not game-state forcing.
	//
	// Mechanism (all sites in namco_c139.cpp, emulation-thread only):
	//  - ARM/REFRESH/SUPERSEDE: at each bulk TXOFFSET announce (the reg_w
	//    PATH C event where m_chunk_bulk_pending latches true) the latch
	//    records (offset, expected_hw, announce time).  A re-announce of the
	//    SAME class (same offset+expected) refreshes it; any OTHER announce
	//    supersedes it (the ROM moved on - never dispatch a stale offset).
	//  - WIPE-CAPTURE: when the peer rx_clear wipes a staged bulk TXSIZE (the
	//    existing rxclear-wiped-staged-txsize diagnostic site), the latch
	//    remembers the wiped size.  The wipe itself proceeds EXACTLY as
	//    before - m_regs[5] is cleared, the ROM's PATH A green light is never
	//    fought (the P025/P027 lesson: the retired P027 one-shot restore
	//    gate kept the register non-zero and starved the ROM's own emitter;
	//    this latch NEVER writes the register file).
	//  - DISPATCH: the ROM's START rising edge still fires
	//    send_pending_tx_frame(), which today aborts at its zero-size
	//    early-out (the verified abort site).  When that abort is reached
	//    with a live+wiped latch inside its TTL and the DMA pointer still at
	//    the latched offset, the send proceeds with the latched size instead
	//    of returning: the payload bytes are read from the ROM-staged C422
	//    RAM exactly as the un-wiped send would have read them
	//    (byte-identical wire content, chkfail stays 0), and the downstream
	//    P011/P014 tracker state is populated identically so the remainder's
	//    txsize_commit trigger fires as normal.  One-shot: the latch is
	//    consumed at dispatch (and retired when a head sends normally), so
	//    the same announce can never dispatch twice.
	//  - TTL: a latch older than AL_TTL_MS (150) logs expired and drops -
	//    stale offsets are never dispatched.
	bool m_al_armed = false;                       // env gate (NAMCOS23_PATCH_ANNOUNCE_LATCH)
	bool m_al_valid = false;                       // a live latch exists (announce -> head-send window)
	bool m_al_wiped = false;                       // the staged TXSIZE was rx_clear-wiped since the announce
	uint16_t m_al_offset = 0;                      // TXOFFSET of the latched bulk announce (head read pointer)
	uint32_t m_al_expected_hw = 0;                 // total announced halfwords (from the size cells)
	uint16_t m_al_wiped_hw = 0;                    // the staged TXSIZE value the wipe destroyed (dispatch size)
	attotime m_al_time;                            // announce/refresh emulated time (TTL anchor)
	uint32_t m_al_latched = 0;                     // cumulative event counters (printed on their per-event lines)
	uint32_t m_al_refreshed = 0;                   //   same-class re-announces that re-timed the latch
	uint32_t m_al_wipes_captured = 0;              //   staged-TXSIZE wipes remembered by a live latch
	uint32_t m_al_dispatched = 0;                  //   sends reconstructed from the latch (== wiped-then-dispatched)
	uint32_t m_al_expired = 0;                     //   latches dropped past TTL (never dispatched)
	uint32_t m_al_superseded = 0;                  //   latches dropped by a newer/other ROM stage

	// P035 (branch patch/latch-v2-snapshot, off patch/
	// roundend-trace): ANNOUNCE-LATCH v2 - WIPE-TIME PAYLOAD SNAPSHOT,
	// selected by NAMCOS23_PATCH_ANNOUNCE_LATCH=2 ("1"/other truthy values =
	// the P031 v1 behavior above, byte-identical; unset/"0" = fully inert).
	//
	// The P033 boss window ran the ROM's bulk compose at 60/60 frames for
	// ~43 s and the v1 dispatch-read race scaled with it: the START-edge
	// dispatch re-reads the C422 ring slot up to AL_TTL_MS after the wipe,
	// by which time the ROM may be recomposing that slot for its NEXT frame
	// - 127 of red's 133 marker=MISMATCH completes landed in the flood
	// window (~32% of its dispatches TORN), and most torn frames PASS
	// checksum and deliver mixed entity payloads (the visible enemy
	// glitching; blue's first 2 chkfails of the latch era are the detected
	// tail).  v2 closes the race by copying the staged head payload at
	// WIPE-capture time - the one instant the bytes are provably pristine
	// (the ROM staged them; the wipe hits only the register) - and having
	// the dispatch transmit that snapshot verbatim.  The bulk REMAINDER
	// needs no snapshot: its P014 txsize_commit trigger sends synchronously
	// inside the very reg_w that staged its TXSIZE (deliver_rx_frames runs
	// at the top of reg_w BEFORE the store), so no stage->send re-read
	// window exists for it.
	//
	// Invariants (why a stale snapshot can never transmit): the snapshot is
	// consultable only at a dispatch, which requires m_al_valid+m_al_wiped;
	// m_al_wiped can only become true at a wipe-capture, which under v2
	// always rewrites the snapshot (valid flag first cleared, re-set only
	// after a full copy); arm/refresh/supersede clear m_al_wiped, forcing a
	// fresh capture before any later dispatch; and the dispatch checks
	// (offset,size) equality against the latch it just consumed, falling
	// back to the v1 RAM re-read (counted + logged dispatched-reread,
	// expected 0) on any mismatch.  Drop/retire sites therefore do not need
	// to touch the snapshot fields (minimal diff).  Not save-state
	// registered, matching v1 (no m_al_* field is).  Everything else - TTL,
	// one-shot, supersede rails, rx_clear path, register file - is v1
	// verbatim.
	bool m_al_snapshot = false;                    // v2 selected (env value == 2): dispatch transmits the wipe-time snapshot
	bool m_al_snap_valid = false;                  // snapshot below was captured for the CURRENT wiped latch
	uint16_t m_al_snap_offset = 0;                 // C422 word offset the snapshot was copied from (== m_al_offset at capture)
	std::vector<uint8_t> m_al_snap;                // wipe-time payload copy, wire byte order (reserved once, reused)
	uint32_t m_al_snap_copied = 0;                 // cumulative event counters (printed on their per-event lines): captures at wipe
	uint32_t m_al_snap_dispatched = 0;             //   dispatches that transmitted the snapshot (snap_tx)
	uint32_t m_al_snap_fallback = 0;               //   v2 dispatches that had to re-read RAM (defensive rail; expect 0)

	// P037 (branch patch/latch-genstamp, off patch/round-arm):
	// GENERATION-STAMP diagnosis rider on the v2 snapshot.  LOG-ONLY - rides
	// NAMCOS23_PATCH_ANNOUNCE_LATCH=2 (no new env var), ZERO behavior change:
	// no timing, no sends, no suppression; the transmitted bytes remain the
	// wipe-time snapshot untouched, and every new statement below is behind
	// m_al_snapshot / al_snap_use gates (=1 and unset are byte-identical).
	//
	// The P035 run-1 residual this diagnoses: with v2 the dispatch-read race
	// is dead (all 375 dispatches across both cabs sent snapshots, zero
	// fallbacks) yet ~10% of boss-flood completes still arrived
	// marker=MISMATCH (29 in t 230-250; dispatch ages bimodal, 154/334 over
	// 100 ms, max = the 150 ms TTL) plus red's first-ever chkfail.
	// Hypothesis: head/remainder GENERATION SKEW - at >100 ms dispatch ages
	// the ROM recomposes the ring slot between the wipe (head snapshot =
	// generation N) and the remainder stage (generation N+1), so the two
	// halves of one message disagree with NO device re-read involved.  One
	// read-only compare proves or kills it: at each dispatched-snap, compare
	// the ring slot's CURRENT first 2 halfwords (what a re-read would fetch)
	// against the snapshot's and tag the line gen=same|differ; at the P014
	// remainder commit of a latch-dispatched message, repeat the compare at
	// the remainder's stage instant (+ the head's age, same announce anchor
	// as the dispatch age_ms).  If gen=differ events time-join the MISMATCH
	// completes (and the chkfail instants), the skew is proven and latch-v3
	// (dispatch-time supersede) gets designed from measured data; if
	// gen=same everywhere while MISMATCH persists, the residual is upstream
	// in the ROM's own emitter and v3 is dead before it's born.
	//
	// Scratch lifetime (so a later message's remainder can never be compared
	// against a stale head): armed only at an al_snap_use dispatch; retired
	// by any NEW message head send that is not such a dispatch (a
	// remainder/continuation never retires it); rewritten by the next
	// dispatch.  A remainder commit can only fire while the tracked message
	// is the held one, whose head provably passed that arm/retire site.
	// Not save-state registered, matching every other m_al_* field.
	uint32_t m_al_gen_differ = 0;                  // gen=differ compare events, dispatch + remainder (printed on the per-event lines)
	bool m_al_gen_head_live = false;               // currently-open tracked message's head was a v2 snapshot dispatch
	uint16_t m_al_gen_head_offset = 0;             // that head's C422 ring offset (m_al_offset at dispatch)
	uint16_t m_al_gen_head_hw0 = 0;                // head snapshot's first halfword (recomposed from wire order)
	uint16_t m_al_gen_head_hw1 = 0;                // head snapshot's second halfword (meaningful when m_al_gen_head_hws == 2)
	uint8_t m_al_gen_head_hws = 0;                 // halfwords held in the scratch (1 when the snapshot is a single hw, else 2)
	attotime m_al_gen_head_time;                   // the head's announce anchor (m_al_time at dispatch) - remainder head_age_ms shares the dispatch age_ms anchor

	// P038 (branch patch/latch-v3-dedupe, off patch/latch-genstamp):
	// ANNOUNCE-LATCH v3 - DISPATCH DEDUPE, selected by
	// NAMCOS23_PATCH_ANNOUNCE_LATCH=3 ("2" = the P035 v2 snapshot behavior
	// above, byte-identical; "1"/other truthy = P031 v1; unset/"0" = fully
	// inert).  v3 is the FULL v2 behavior PLUS the dedupe below.
	//
	// The P036-measured defect this kills: in compose floods the ROM
	// re-announces each slot at heartbeat cadence; the FIRST copy sends
	// naturally and completes; the peer's rx_clear wipes the RE-announce (as
	// flow control - the content is already delivered); the latch then
	// "rescues" the wiped re-announce 117-134 ms later as a STALE DUPLICATE
	// (head = wipe-time snapshot of gen N, end-marker region = gen N+k) that
	// passes the byte-sum checksum and is INGESTED.  ALL 56 torn
	// (marker=MISMATCH) completes in the P036 run were such dispatches
	// (56/56 timestamp join; natural sends tore 0/239) and ~180 of red's 185
	// dispatches were +150 ms duplicates of already-completed content - the
	// leading candidate for the room-1 instant-resolve, the stage-1 skips
	// and the boss-window double-ingest.
	//
	// Mechanism: every wire completion of a latched class is recorded in the
	// small (offset, expected_hw, t) ring below - at the consumed_ok
	// retirement (the NATURAL head send) and at a latch dispatch (so one
	// announce chain can never double-dispatch); deduped consumptions are
	// NOT recorded (nothing went on the wire).  At the START-edge dispatch
	// site, after every v1/v2 rail has passed, a latch whose class has a
	// recorded completion within AL_DEDUPE_LOOKBACK_MS (.cpp) of its
	// announce anchor is CONSUMED WITHOUT SENDING (counted deduped,
	// per-event line).  The genuine rescue class - announces with NO recent
	// same-class completion, the P031 room-1 save - dispatches exactly as
	// v2.  A deduped class is never abandoned: the flood re-announces the
	// slot ~one cadence hop later, so a suppression costs at most one hop of
	// staleness, while the P028/P029 abandon-forever class (announced, never
	// naturally sent) has no recent same-key completion and is never
	// suppressed.
	//
	// Key choice (offset, expected_hw), not offset alone: the ROM's TX ring
	// reuses 4 slots, so offset alone would false-match a DIFFERENT class
	// that rotated into the slot; expected_hw separates the classes.
	// Residual false-match (same slot, same size, genuinely NEW content
	// announced inside the lookback) is bounded by the one-hop argument
	// above; residual false-miss (a stale re-send whose size cells changed
	// between generations) still dispatches, but the P036 duplicate mass was
	// same-(total_hw,start_ptr) pairs, which this key covers.
	// Not save-state registered, matching every other m_al_* field.
	static constexpr unsigned AL_DEDUPE_RING = 16; // recent-completion entries (peak measured flood ~10 msg/s -> >1.5 s of history)
	struct al_complete_rec
	{
		uint16_t offset = 0;                       // C422 word offset (TXOFFSET) of the completed head
		uint32_t expected_hw = 0;                  // announced total halfwords (the class key's second half)
		attotime t;                                // emulated time the head went on the wire
		bool valid = false;                        // entry holds a real completion
	};
	bool m_al_dedupe = false;                      // v3 selected (env value == 3): v2 snapshot + dispatch dedupe
	al_complete_rec m_al_completes[AL_DEDUPE_RING];// completion ring (overwrite-oldest, fixed size, no allocation)
	uint8_t m_al_comp_idx = 0;                     // next ring write slot
	uint32_t m_al_deduped = 0;                     // dispatches suppressed: class completed within the lookback (per-event line)
	uint32_t m_al_refresh_retired = 0;             // same-class re-announces that retired a PENDING WIPED dispatch (expect 0 in floods)

	// Retained P013 register-map knowledge (the TXEMIT register-programming
	// trace was removed in P072): the C139 register byte offsets (gold
	// mame/10-c139-register-map.md) map to device m_regs[] indices as
	// REG[0x06] START/trigger = m_regs[3] (reg_idx 3); REG[0x0A] TXSIZE =
	// m_regs[5] (reg_idx 5); REG[0x0E] TXOFFSET = m_regs[7] (reg_idx 7).
	// The ROM's gp+0x75CA 4-slot 0x400-stride TX DMA cursor lives in MIPS
	// main RAM (not reachable from this device) but is FULLY reflected at the
	// register layer: PATH C writes REG[0x0E] = gp[0x75CA]+2, so m_regs[7] is
	// the device-observable image of the cursor and the slot index is
	// (m_regs[7]-2)/0x400 (slot bases 0x0000/0x0400/0x0800/0x0C00; +2 offset
	// = the 2 size cells).

	// P050 (branch patch/single-burst-pump, off patch/reaper-patience):
	// the TRANSPORT fix for the remaining top defects (red hit-reg lag, stage-3
	// choppy movers, cross-visibility gaps, blue's dead MG - all confirmed
	// LINK-INDUCED by the 2026-07-08 solo control run).  Two INDEPENDENT env
	// gates; both inert when unset.  MODEL PROVENANCE: Opus 4.8.
	//
	// GATE (a) COMPANION - m_bq_armed (env NAMCOS23_PATCH_BURST_QUANTUM, read
	// here in device_start AND by the driver, which owns the RAM-code poke
	// @0x8000BC78 slti 0x100 -> slti 0x401 that makes the ROM's TX pump emit
	// every VM frame as ONE burst up to 0x400 hw instead of a <=255-hw chunk
	// train).  The device needs NO change to CROSS a large single burst: the
	// CHUNK_PASSTHRU saturation heuristic keys on frame_size == EXACTLY 0xFF hw
	// AND expected_hw > 0xFF, so a single burst of size N (== its own
	// expected_hw, != 0xFF) falls straight through the self-contained
	// passthrough in send_pending_tx_frame as ONE complete wire frame, and
	// deliver_rx_frames writes any size <= the 0x4000-byte network cap into the
	// RX ring with the faithful 0x0FFF wrap (the ROM's own drain validator
	// @0x8000BF90 accepts <= 0x400 hw = the single-burst ceiling).  Two SMALL
	// companions, both gated on m_bq_armed AND self-guarding (they fire only
	// when a >255-hw single burst actually reaches the send/emit path, which
	// requires the poke to have taken - if the poke is DRC-stale the ROM still
	// chunks and neither companion ever runs): (1) at the self-contained send of
	// a >255-hw whole frame, retire any announce-latch its own PATH C bulk
	// announce armed (bulk_pending latches on expected_hw > 0xFF, but the
	// first-saturated consumed_ok site never runs for a single burst) and record
	// the dedupe key - so the latch attribution + v3 dedupe stay correct; (2) a
	// >255-hw single burst re-arms the heartbeat clock (as a bulk chunk does)
	// so the keepalive replay stays suppressed while real large-frame traffic
	// flows - which also stops a stale small replay from out-arriving a fresh
	// fight frame.
	bool m_bq_armed = false;                       // env NAMCOS23_PATCH_BURST_QUANTUM (device-side companion arm)

	// Retained P049/P050 knowledge (the P050b newest-wins delivery cap itself
	// was dropped after P051/P052 and removed in P072): frame supersession is
	// the ROM's own compose-side semantics - F1: the pump only ever sends the
	// newest 0x400-hw slot; F3: state re-serializes every frame and the
	// 32-slot reliable backlog re-publishes until 0xFD-acked, so a dropped
	// older frame instance is re-sent by design.  The "complete-standalone"
	// test is the ROM's strict trailer invariant (bit-8 end marker in the
	// final halfword AND the trailer's claimed length == the frame's own
	// halfword count), applied inline by the TX_COMPLETE_V2 admission gate in
	// reg_w (the nw_frame_complete helper went with the FIFO-JOIN stamps in
	// P072 phase C) - a test a multi-chunk message's
	// fragments NEVER pass (an intermediate chunk has no trailer; a last
	// chunk's claimed length is the whole-message total, not the chunk's own
	// size).  The op6F play-clock beacon rides only 1/256 frames, so any
	// single dropped frame delays clock resync up to ~4.3 s.

	// (The P050 FIFO-JOIN falsifier instrumentation - per-complete-frame
	// tx-join/rx-join stamps + 1/s fps status, keyed on the cellseq drift
	// sequence - was removed in P072 phase C; the seq/latency-join method
	// knowledge is retained at the former helper site in namco_c139.cpp.)

public:
	// Link-role accessor, consumed by the P068 frontend "Link Play Config"
	// menu (src/frontend/mame/ui/linkplay.cpp).  (The P055-origin
	// FRESH/REPLAY ingest bridge that once shared this public block - its
	// classifier, rx-apply generation and counters - was removed in P072 with
	// its last trace consumer, POSCORR.)
	bool comm_is_connector() const { return m_comm_is_connector; } // this instance connected OUT (blue/follower = the ingest side) vs listened (red/leader)

	// P068 (branch patch/linkplay-menu): native-UI link-play configuration.
	// The four values below are the per-machine cfg\<system>.cfg <linkplay>
	// settings edited by the frontend "Link Play Config" menu
	// (src/frontend/mame/ui/linkplay.cpp).  Resolution priority at comm
	// bring-up: (1) explicit -comm_* CLI options (any of the four changed from
	// MAME defaults - the pre-P068 path, launch scripts unchanged); (2) these
	// cfg-stored values; (3) the built-in loopback defaults below (so two bare
	// instances on one PC link out-of-the-box).  Under (2)/(3) the ROLE comes
	// from the "Link ID" machine configuration (Left/Red = listener binds
	// listen_host:listen_port, Right/Blue = connector dials
	// connect_host:connect_port) and the link DIP (DIP:5) gates whether any
	// comm starts at all.  Values are consumed ONCE at machine start (config
	// FINAL) - menu edits apply on the NEXT launch; no live socket rebinding.
	// Setters/getters are emulation/UI-thread only (the UI menu runs on the
	// emulation thread; the network thread never touches these).
	// MODEL PROVENANCE: Fable 5.
	static constexpr uint16_t     LP_DEFAULT_PORT         = 9876;
	static constexpr char const  *LP_DEFAULT_LISTEN_HOST  = "0.0.0.0";
	static constexpr char const  *LP_DEFAULT_CONNECT_HOST = "127.0.0.1";

	std::string const &lp_listen_host() const  { return m_lp_listen_host; }
	uint16_t           lp_listen_port() const  { return m_lp_listen_port; }
	std::string const &lp_connect_host() const { return m_lp_connect_host; }
	uint16_t           lp_connect_port() const { return m_lp_connect_port; }
	void set_lp_listen_host(std::string v)     { m_lp_listen_host = std::move(v); }
	void set_lp_listen_port(uint16_t v)        { m_lp_listen_port = v; }
	void set_lp_connect_host(std::string v)    { m_lp_connect_host = std::move(v); }
	void set_lp_connect_port(uint16_t v)       { m_lp_connect_port = v; }
	void lp_reset_defaults()
	{
		m_lp_listen_host  = LP_DEFAULT_LISTEN_HOST;
		m_lp_listen_port  = LP_DEFAULT_PORT;
		m_lp_connect_host = LP_DEFAULT_CONNECT_HOST;
		m_lp_connect_port = LP_DEFAULT_PORT;
	}

	// session status for the menu's info lines
	bool comm_started() const      { return bool(m_context); }      // a socket context exists this session
	bool comm_cli_override() const { return m_lp_cli_active; }      // CLI -comm_* differed from MAME defaults => CLI won
	bool lp_role_is_connector() const;                              // "Link ID" machine config: Right/Blue = connector
	bool lp_role_available() const { return lp_link_id_field() != nullptr; } // machine exposes the Link ID config at all

private:
	// P068 internals (see the public block above).
	void linkplay_config_load(config_type cfg_type, config_level cfg_level, util::xml::data_node const *parentnode);
	void linkplay_config_save(config_type cfg_type, util::xml::data_node *parentnode);
	void start_comm_cfg() ATTR_COLD;               // deferred cfg/default comm bring-up (config FINAL)
	ioport_field *lp_link_id_field() const;        // the timecrs2 "Link ID" PORT_CONFNAME, or nullptr

	std::string m_lp_listen_host  = LP_DEFAULT_LISTEN_HOST;   // red/left: bind address
	uint16_t    m_lp_listen_port  = LP_DEFAULT_PORT;          // red/left: bind port
	std::string m_lp_connect_host = LP_DEFAULT_CONNECT_HOST;  // blue/right: target address
	uint16_t    m_lp_connect_port = LP_DEFAULT_PORT;          // blue/right: target port
	bool        m_lp_cli_active   = false;         // CLI -comm_* override took the pre-P068 path in start_comm
	bool        m_lp_comm_deferred = false;        // start_comm saw all-MAME-defaults => resolve at config FINAL
	bool m_comm_is_connector = false;              // set in start_comm: remote configured, no local = the connector (blue)

	// P062-origin shared record type (the TXSTAGE per-stage content-hash
	// trace that introduced it was removed in P072): a (hash, len) pair for a
	// staged/dispatched TX content - the KEPT P063 TX_COMPLETE_V2 admission
	// gate uses it for its m_txc2_hist belt-and-braces ring below.
	struct ts_hist_rec
	{
		uint32_t hash = 0;                         // FNV-1a 32 of the staged wire bytes
		uint16_t len = 0;                          // staged TXSIZE (halfwords)
		bool valid = false;
	};

	// P063 (branch patch/tx-complete-v2, off patch/txstage-trace):
	// TX-COMPLETE RELEASE v2 - the P062-measured retry of the P061 concept,
	// env-gated by NAMCOS23_PATCH_TX_COMPLETE_V2 (boolean idiom; inert/
	// byte-identical when unset).
	// ACTIVE only while the P060 set_ingame debounced mode-2 state
	// (m_hbpa_ingame) holds; outside mode-2 (attract / op55 establishment /
	// mode-select) the stock reg5 stop-and-wait is byte-identical.  ARM ON
	// BOTH CABS.
	//
	// WHY (P062 run analysis, the measured spec): under stock stop-and-wait
	// the ROM's fresh-compose rate EQUALS wire consumption in every 30 s
	// bucket (~30/s at HB=33) - compose is consumption-THROTTLED, with a
	// measured compose ceiling of ~54-58 fresh/s when the wire drains faster.
	// P061's release concept was right but its (seq,len) dedupe key was
	// unsound (seq is a shared slow PHASE counter) and its instant-0 busy-poll
	// removed the pump's back-pressure (4.7-6.5k passages/s spin).  P062
	// measured the corrected gate: dups are STRICTLY ADJACENT re-stages
	// (dup==prev in ~99.9% of stages; a fresh stage's hash recurred 0 times in
	// 38k), so a previous-STAGED-content-hash gate suffices; and the ROM lives
	// at polls ~99% busy with 70-100 START passages/s, tolerating >=16.5 ms
	// busy spans - the texture a modeled TX-busy interval must reproduce.
	//
	// MECHANISM (three parts, all emulation-thread only):
	//  1. ADMISSION GATE (reg_w, the txsize_commit site): a non-zero TXSIZE
	//     stage in stable mode-2 that is a fresh standalone frame (the P061
	//     structural checks: fresh TXOFFSET this passage, no held bulk, size
	//     4..0x400, ROM trailer invariant on the staged image; the
	//     BURST_QUANTUM >255-hw whole-frame class included) is hashed
	//     (FNV-1a-32 over the staged wire image, exactly ts_hash_ram) and
	//     dispatched SYNCHRONOUSLY via send_pending_tx_frame() IFF its hash
	//     differs from the previous staged/dispatched content (prev-hash key +
	//     a free TXC2_HIST=4 ring covering the 0.07/s A-B-A transients).
	//     TXSIZE=0 writes are ignored entirely (idle pump passages, bursting
	//     to 5.9k/s at transitions - they neither dispatch nor perturb gate/
	//     window/park state nor count as stages).  A hash-identical re-stage
	//     is the pump re-offering already-crossed content: no dispatch, PARKED.
	//  2. MODELED TX-BUSY (reg_r, the pump's 0x8000BB80 busy-poll): after each
	//     dispatch - and for a duplicate re-stage that lands with the modeled
	//     serializer idle - reg5 reads return a synthesized BUSY value (the
	//     in-flight TXSIZE, low byte forced non-zero) for m_txc2_busy_ms
	//     (default 12 ms ~ one vblank, env NAMCOS23_PATCH_TXC2_BUSY_MS,
	//     range 8..20), WITHOUT writing the register file.  The first poll
	//     AFTER the window closes releases a parked duplicate (m_regs[5] -> 0,
	//     the one deliberate reg5 write - same sanctioned release class as
	//     P061's) and the pump starts its next passage: passages are paced at
	//     ~1/busy_ms = 70-100/s = the stock texture, dispatches are throttled
	//     only by content freshness (compose ~55/s) - NOT P061's 4.7-6.5k/s
	//     spin (a duplicate landing on an idle serializer re-opens the window,
	//     so a poll can never instantly release the passage that just staged).
	//     A dup landing INSIDE an open window is a pure no-op (the +199 us
	//     re-stage beat rides through; the window is never extended).
	//  3. STALE-REPLAY AGE-OUT (heartbeat_tick): when ACTIVE (armed + stable
	//     mode-2) the heartbeat replay is SUPPRESSED (re-arm and return, like
	//     the chunk-passthru hold) when the captured payload is older than
	//     m_txc2_stale_ms (default 500 ms, env NAMCOS23_PATCH_TXC2_STALE_MS).
	//     P062 Q5: red's >255hw scene-table stretches are capture-EXCLUDED
	//     (P010/P026, correctly - restamping chunked/bulk frames corrupts
	//     headers), so its replay payload ages up to 66 s while ~20 replays/s
	//     fire - and under V2 the peer no longer needs the token (self-release)
	//     so a stale replay is pure poison with no pacing value.  The capture
	//     logic itself is UNTOUCHED (proven newest-at-capture, 19129/19129);
	//     outside mode-2 (or unarmed) the heartbeat is untouched - it still
	//     paces establishment.
	//
	// MODE-2 BOUNDARY: INACTIVE (mode-2 lost / device_reset) clears the gate
	// key, ring, park and busy window IMMEDIATELY - no synthesized busy read
	// or stale park survives into the stock-owned phase (a parked stage left
	// in reg5 is released by the stock rx_clear, exactly as stock).  The next
	// ACTIVE stretch's first fresh stage always dispatches (prev invalidated).
	// Not save-state registered, matching every other patch member.
	// MODEL PROVENANCE: Fable 5.
	static constexpr unsigned TXC2_HIST = 4;       // dispatched-content (hash,len) ring depth (A-B-A transient cover; P062: 0.07/s)
	bool m_txc2_armed = false;                     // env gate (NAMCOS23_PATCH_TX_COMPLETE_V2)
	uint32_t m_txc2_busy_ms = 12;                  // modeled TX serialization interval, ms (env-tunable 8..20)
	uint32_t m_txc2_stale_ms = 500;                // heartbeat replay age-out threshold, ms (env-tunable)
	bool m_txc2_prev_valid = false;                // prev hash/len below hold real dispatched content
	uint32_t m_txc2_prev_hash = 0;                 // FNV-1a-32 of the last dispatched staged wire image (the admission key)
	uint16_t m_txc2_prev_len = 0;                  // its staged length (halfwords)
	ts_hist_rec m_txc2_hist[TXC2_HIST];            // last TXC2_HIST dispatched (hash,len) - belt-and-braces ring
	uint8_t m_txc2_hist_idx = 0;                   // next ring write slot
	attotime m_txc2_busy_until;                    // modeled busy window end (dispatch/park instant + busy_ms)
	uint16_t m_txc2_busy_hw = 0;                   // TXSIZE of the modeled in-flight frame (the synthesized busy read value)
	bool m_txc2_parked_dup = false;                // a hash-verified duplicate re-stage sits in reg5 (cleared at the first post-window poll)
	attotime m_txc2_cap_time;                      // emulated time of the last heartbeat capture (age-out anchor)
	bool m_txc2_first_dispatch_logged = false;     // one-shot first-dispatch line
	bool m_txc2_first_release_logged = false;      // one-shot first parked-dup release line
	bool m_txc2_first_stale_logged = false;        // one-shot first stale-replay suppression line
};


// device type definition
DECLARE_DEVICE_TYPE(NAMCO_C139, namco_c139_device)



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************



#endif // MAME_NAMCO_NAMCO_C139_H
