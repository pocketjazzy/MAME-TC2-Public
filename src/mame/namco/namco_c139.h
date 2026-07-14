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
	// PHASE 9D: host driver pushes our cabinet's state-machine call counter
	// (gp+0x75B8) here every vblank so the heartbeat timer can stamp it into
	// bytes 0-1 of replayed packets - that's what the partner's dispatcher
	// reads as "partner_counter" when computing the timeout delta.
	void set_local_counter(uint16_t counter) { m_local_counter = counter; }

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off
	// patch/linked-gate-supply): the host driver pushes the WIRE-ONLY 0x6000
	// injection gate here every vblank.  Only the driver can read the staging
	// mode word (0x802F3FD0) and the env arm, so it computes both and hands
	// them to the device; the device performs the OR-only op55 wire-flag
	// injection in emit_tx_frame.  Same emulation-thread setter idiom as
	// set_local_counter (both members are written here at vblank and read in
	// emit_tx_frame, all on the emulation thread - no atomics needed).
	// Experiment branch only - never merge to milestone.
	void set_linked_gate_txonly(bool armed, bool mode2)
	{
		m_lg_txonly_armed = armed;
		m_lg_txonly_mode2 = mode2;
	}

	// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
	// the host driver pushes the staging-phase signal here every vblank -
	// mode2 = (staging mode word 0x802F3FD0 == 2, linked gameplay/cutscene),
	// mode_word = the raw word (transition-log context only).  Only the driver
	// can read the mode word (it maps m_mainram); the device owns the cadence:
	// it debounces the signal (HBPA_DEBOUNCE_VBLANKS consecutive mode-2 vblanks
	// before FAST, back to SAFE immediately on loss) and the heartbeat re-arm
	// sites pick m_hbpa_fast_ms vs m_hb_cadence_ms via hb_cadence_effective_ms().
	// Same emulation-thread setter idiom as set_local_counter /
	// set_linked_gate_txonly (written at vblank, read at the heartbeat timer
	// re-arms, all on the emulation thread - no atomics needed).  Defined in the
	// .cpp (it logs the FAST<->SAFE transitions).  MODEL PROVENANCE: Fable 5.
	// P061 (branch patch/tx-complete-irq): the debounced state now has a SECOND
	// consumer - the C422 TX-complete dispatch model (m_txci_armed, see the P061
	// member block) is active only while this debounced mode-2 state holds, so
	// the driver pushes here when EITHER env is armed and set_ingame processes
	// for either; NAMCOS23_PATCH_HB_PHASE_AWARE itself stays independent (the
	// cadence choice still requires m_hbpa_armed).
	// P063 (branch patch/tx-complete-v2): a THIRD consumer - the TX-complete
	// release v2 (m_txc2_armed, see the P063 member block) shares the same
	// debounced state; the driver pushes when ANY of the three envs is armed.
	// Experiment branch only - never merge to milestone.
	void set_ingame(bool mode2, uint32_t mode_word);

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace, off
	// patch/op55-carrier-repeat): running counts of REAL op6F play-clock cells
	// seen by the cell-walk (op6f_tx = emitted by this cab, op6f_rx = delivered
	// from the peer), read by the driver's 1/s PLAYCLOCK: status line so the
	// "op6f tx/rx this second" deltas sit next to the +0x390/+0x394 clock values.
	// Emulation-thread only (bumped in emit_tx_frame / deliver_rx_frames, read at
	// vblank - same thread, no atomics needed).  Experiment branch only - never
	// merge to milestone.
	uint32_t op6f_tx_count() const { return m_pc6f_tx_index; }
	uint32_t op6f_rx_count() const { return m_pc6f_rx_index; }

	// P002 EXPERIMENT (H2 "drift lockstep", branch patch/vblank-lockstep):
	// per-vblank frame-token barrier.  The host driver calls vblank_tick()
	// once per frame (vblank rising edge); when armed via
	// NAMCOS23_PATCH_VBLANK_LOCKSTEP the device sends a frame-token control
	// frame to the peer and stalls (bounded, wall-clock) whenever the local
	// frame count runs more than LOCKSTEP_MAX_LEAD frames ahead of the last
	// token received from the peer.  Inert (early-return) when not armed.
	// Experiment branch only - never merge to milestone.
	void vblank_tick();

	// Snapshot of the lockstep counters for the driver-side 1/s status tap.
	// Safe to call from the emulation thread at any time.
	struct lockstep_stats
	{
		uint32_t tokens_tx;     // tokens we sent
		uint32_t tokens_rx;     // tokens received from the peer
		uint32_t local_frame;   // our vblank counter (token payload)
		uint32_t peer_token;    // last token value received from the peer
		uint32_t stall_events;  // vblanks on which we stalled at all
		uint32_t stall_timeouts;// stalls that hit the wall-clock cap
		uint64_t stall_us;      // total wall-clock time spent stalled
		bool     suspended;     // barrier auto-suspended (peer token flow died)
	};
	lockstep_stats get_lockstep_stats() const
	{
		return lockstep_stats{
				m_lockstep_tokens_tx,
				m_lockstep_tokens_rx.load(std::memory_order_relaxed),
				m_lockstep_local_frame,
				m_lockstep_peer_token.load(std::memory_order_relaxed),
				m_lockstep_stall_events,
				m_lockstep_stall_timeouts,
				m_lockstep_stall_us,
				m_lockstep_suspended };
	}

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): READ-ONLY
	// peek of the C139 RX RAM (m_ram) word at the given word offset, with NO
	// side effects (unlike ram_r(), which drains the RX queue).  The driver's
	// NAMCOS23_TRACE_ADOPT vblank tap uses this to watch the bulk ingest
	// landing region (P014 delivered the 718B batch "to word=0x01c6", i.e.
	// rx_base 0x1000 + fifo_ptr 0x01c6 = m_ram word 0x11c6).  Emulation-thread
	// only (same thread that owns m_ram during a delivered frame).  Experiment
	// branch only - never merge to milestone.
	uint16_t adopt_peek_ram(offs_t word_offset) const { return m_ram ? m_ram[word_offset & 0x3fff] : 0; }

private:
	uint16_t reg_r(offs_t offset);
	void reg_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);

	void start_comm() ATTR_COLD;
	void send_pending_tx_frame();
	void deliver_rx_frames(int32_t param);

	// P009 EXPERIMENT (branch patch/qual-trace): compact per-frame
	// qualification fingerprint, logged for every delivered RX frame and
	// every (real / heartbeat) TX so qualifying vs non-qualifying frames
	// can be diffed offline.  Read-only.  Experiment branch only - never
	// merge to milestone.
	void log_qual_fingerprint(const char *kind, const uint8_t *payload, std::size_t bytes);

	// P018 EXPERIMENT (branch patch/op70-real-detector): REAL (non-heuristic)
	// op-70 cell-walk.  Walks the gate-4 VM token stream (the cell stream = low
	// byte of each received/sent halfword) the way the ROM dispatcher 0x800AA840
	// does - advancing the cursor by each op's true on-wire length - and flags a
	// cell ONLY when the cursor lands on a real opcode byte 0x70 (handler
	// 0x800B2544, value cursor[0..1] big-endian masked 0x7fff).  This replaces
	// the P016 heuristic byte-scan, which over-reported mid-payload 0x70 operand
	// bytes (0x11c6 / 0x4ea8) as op-70 hits.  Called for delivered RX frames
	// (dir="rx") and emitted TX frames (dir="tx").  Read-only; logs OP70R: lines
	// only when m_trace_op70.  P025 (branch patch/playclock-humangate-trace):
	// the SAME walk now also reports op6F play-clock cells (handler 0x800B2448,
	// 6 operand bytes = two sign-extended 24-bit values, the +0x390/+0x394 pair)
	// as PLAYCLOCK: op6f-tx / op6f-rx lines, gated independently on
	// m_trace_playclock - same clean-run discipline, zero heuristics.
	// Experiment branch only - never merge to milestone.
	void op70_cell_walk(const char *dir, const uint8_t *frame_bytes, std::size_t bytes, bool bulk, unsigned cellseq, uint32_t &index);

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): READ-ONLY op55
	// WIRE-FLAG tap.  P018 proved gp+0x7074 never reaches 2 because the PARTNER
	// record +0x370 0x6000 bits are never set.  The ROM derives the partner
	// 0x6000 verbatim from the op55 24-bit wire flags (RX 0x800B058C parses
	// operand bytes 4/5/6 -> $s2; the call to 0x80013D44 stores $s2|0xC0000000 to
	// rec1 +0x370 at 0x80013E10).  So whether 0x6000 is ON THE WIRE is read off
	// the op55 frame: a cell-stream byte 0x55 (op55 is gate-4 table entry
	// 0x8023F4D0[0x55]=0x800B058C) followed by the 24-bit flags at operand bytes
	// 4/5/6 = cells[op55+5..+7]; 0x6000 lives in cell[op55+6] & 0x60.  Walking
	// every 0x55 candidate and decoding those flags tells us, on TX (dir="tx",
	// candidate (Y)) and RX (dir="rx", candidate (Z)), whether the local cab
	// SENDS 0x6000 and whether the peer's frame DELIVERS it.  Read-only; logs
	// LINKBITS: txflags / rxflags only when m_trace_linkbits.  Experiment branch
	// only - never merge to milestone.
	void linkbits_op55_scan(const char *dir, const uint8_t *cells, std::size_t n);

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off
	// patch/linked-gate-supply): WIRE-ONLY 0x6000 "fully-linked" injection.
	// P020 proved that supplying 0x6000 to the LOCAL +0x370 record drives the
	// full handshake to gp+0x7074==2 on the PEER (genuine cross-cab sync,
	// 0x118002 freeze broken), but it ALSO poisons the local live record: the
	// gun-actor (0x80013644) / own-record tick (0x80014CC0) read bit 0x2000 on
	// the SAME word and treat the local player as remote-driven, derailing it
	// into the synchronized continue/death screen.  The fix is to advertise
	// 0x6000 to the PEER WITHOUT setting it in this cab's live RAM record: the
	// peer derives its partner-record 0x6000 verbatim from the op55 24-bit wire
	// flags (RX 0x800B058C -> 0x80013E10), so OR-ing 0x6000 into those wire-flag
	// bytes IN THE OUTGOING FRAME COPY makes the peer's gate7074 reach 2 while
	// this cab's game RAM +0x370 is never touched (gun-actor stays human).  The
	// outgoing buffer is a SEPARATE copy: emit_tx_frame's payload was read out of
	// the C422 link RAM (m_ram) into a fresh std::vector upstream (see the FIFO
	// readout loop), so modifying it does NOT touch m_ram or MIPS main RAM.  In
	// the device cell stream (low byte of each halfword) the flags follow the
	// 0x55 opcode at cells[op55+5..+7]; 0x6000 lives in cells[op55+6] & 0x60 =
	// payload[(op55+6)*2 + 1] & 0x60.  OR-only (idempotent) and gated by the
	// driver-pushed arm + mode==2.  Experiment branch only - never merge.
	void linkbits_op55_inject_6000(std::vector<uint8_t> &payload);

	// P010 EXPERIMENT (branch patch/chunk-reassembly): TX-side chunked
	// bulk-frame reassembly, env-gated by NAMCOS23_PATCH_CHUNK_REASSEMBLY
	// (inert when unset).  The ROM's TX chunk emitter (0x8000BB6C) splits
	// any message larger than 0xFF halfwords into successive TXSIZE=0xFF
	// chunks + remainder, writing TXOFFSET only for the FIRST chunk and
	// relying on the chip auto-advancing its DMA pointer for the rest.
	// We model that pointer, hold the chunks, and put the whole message on
	// the wire as ONE TCP frame.  Experiment branch only - never merge to
	// milestone.
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

	// PHASE 9D: synthetic-heartbeat state.  REMOVE BEFORE COMMIT.
	emu_timer *m_heartbeat_timer = nullptr;
	std::vector<uint8_t> m_last_tx_payload;   // last real TX, replayed periodically
	uint16_t m_local_counter = 0;             // pushed in by host vblank

	// P002 EXPERIMENT (branch patch/vblank-lockstep) state.
	// Ownership: everything here is emulation-thread-only EXCEPT
	// m_lockstep_peer_token / m_lockstep_tokens_rx, which the asio network
	// thread writes (via lockstep_token_received()) and the emulation
	// thread reads - hence the atomics.  Experiment branch only.
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
	uint32_t m_lockstep_tokens_tx = 0;             // diagnostics for the 1/s tap
	uint32_t m_lockstep_stall_events = 0;
	uint32_t m_lockstep_stall_timeouts = 0;
	uint64_t m_lockstep_stall_us = 0;

	// P009 EXPERIMENT (branch patch/qual-trace) state.  Read-only trace,
	// env-gated by NAMCOS23_TRACE_QUAL in device_start().  Emulation-thread
	// only.  Experiment branch only - never merge to milestone.
	bool m_trace_qual = false;                     // env gate
	uint32_t m_qual_rx_index = 0;                  // running RX frame index (pairs lines across logs)
	uint32_t m_qual_tx_index = 0;                  // running TX frame index (real + heartbeat)

	// P010/P011 EXPERIMENT (branch patch/chunk-sequencing, stacked on
	// patch/chunk-reassembly).  All emulation-thread only.  Experiment branch
	// only - never merge to milestone.
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
	bool m_chunk_assoc_pointer = true;             // option (a) [default] vs (b) announce-flag (NAMCOS23_CHUNK_ASSOC_ANNOUNCE=1 -> (b))
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
	uint32_t m_chunk_reassembled = 0;              // 1/s status counters
	uint32_t m_chunk_dropped = 0;
	uint32_t m_chunk_continued = 0;                // P011: continuation chunks appended by pointer continuity
	uint32_t m_chunk_passthru_during_bulk = 0;     // P011: non-resuming chunks emitted while a bulk message was held
	uint32_t m_chunk_rxclear_suppressed = 0;
	uint32_t m_chunk_status_div = 0;               // vblank divider for the 1/s status line

	// P026 EXPERIMENT (branch patch/reasm-chunk-passthru): CHUNK PASS-THROUGH
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
	// thread only.  Experiment branch only - never merge to milestone.
	bool m_chunk_passthru = false;                 // env gate (NAMCOS23_PATCH_CHUNK_PASSTHRU); implies m_chunk_armed
	uint32_t m_chunk_pt_msg_id = 0;                // running bulk-message id for CHUNK_PASSTHRU: fwd lines
	uint32_t m_chunk_pt_chunks_fwd = 0;            // cumulative chunks forwarded to the wire
	uint32_t m_chunk_pt_msgs_complete = 0;         // cumulative complete chunk sequences
	uint32_t m_chunk_pt_hb_held = 0;               // heartbeat replays held while a sequence was open
	uint32_t m_chunk_pt_rxclear_wiped = 0;         // rx_clear wiped a staged TXSIZE while a bulk was announced/open (diagnostic; NOT suppressed in this mode)

	// P027 EXPERIMENT (branch patch/hb-cadence-wipe-restore, off
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
	// (b) NAMCOS23_PATCH_WIPE_RESTORE=1: when a peer rx_clear wipes a
	//     staged+announced bulk-chunk TXSIZE (the head's stage->START-edge
	//     race), restore m_regs[5] ONCE per bulk announce; a SECOND wipe of
	//     the same stage logs and yields - NEVER the P025 infinite
	//     suppression fight (that suppression stays retired; the one-shot
	//     budget is re-armed only by the ROM's own next TXOFFSET bulk
	//     announce).
	// (c) NAMCOS23_PATCH_HB_RESTAMP=1 (reserve): stamp replayed heartbeats
	//     with the peer's last-seen counter + elapsed frames instead of
	//     m_local_counter, so the receiver's drift reset lands near transit
	//     latency (P026: red rode drift 13-14 through the whole cutscene on
	//     blue's heartbeats - a constant 3-frame margin).  Sampled only from
	//     COMPLETE delivered messages (trailer marker + claimed-length test)
	//     so a mid-message chunk fragment can never poison the sample.
	// All emulation-thread only.  Experiment branch only - never merge to
	// milestone.
	bool m_hb_cadence_override = false;            // (a) env gate (NAMCOS23_PATCH_HB_CADENCE_MS parsed valid)
	uint32_t m_hb_cadence_ms = 250;                // (a) heartbeat re-arm interval, ms (250 = stock, bit-identical when unset)
	bool m_wipe_restore_armed = false;             // (b) env gate (NAMCOS23_PATCH_WIPE_RESTORE)
	bool m_wipe_restore_budget = false;            // (b) one restore available for the current bulk announce
	uint32_t m_wipe_restored = 0;                  // (b) staged TXSIZEs preserved through an rx_clear (cumulative)
	uint32_t m_wipe_yielded = 0;                   // (b) repeat wipes of an already-restored stage - yielded (cumulative)
	bool m_hb_restamp_armed = false;               // (c) env gate (NAMCOS23_PATCH_HB_RESTAMP)
	bool m_hb_peer_ctr_valid = false;              // (c) a peer counter sample exists
	uint16_t m_hb_peer_ctr = 0;                    // (c) halfword 0 of the last COMPLETE delivered message
	attotime m_hb_peer_ctr_time;                   // (c) emulated arrival time of that sample
	uint32_t m_hb_restamped = 0;                   // (c) replays stamped from the peer sample (cumulative)
	uint32_t m_hb_restamp_fallback = 0;            // (c) replays that fell back to local_counter (no/stale sample)

	// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
	// PHASE-AWARE heartbeat token cadence, env-gated by
	// NAMCOS23_PATCH_HB_PHASE_AWARE=<fast_ms> (inert/byte-identical when unset;
	// value-checked [HBPA_FAST_MIN_MS..HBPA_FAST_MAX_MS], refused otherwise).
	// WHY (P059 run analysis): the heartbeat replay is the pacing TOKEN that
	// clears the PEER's reg5 stop-and-wait, so the peer's distinct fresh-emit
	// rate ~= 1000/cadence; P059 measured arrived==delivered for the WHOLE run
	// (zero device-side headroom) => the remote-entity correction supply is
	// bounded by this cadence, and the channel organically carried ~50 fresh/s
	// with zero drops => a faster cadence is transport-safe.  But flat HB=16
	// (P052) broke op55 ESTABLISHMENT (attract/handshake floods) - a
	// phase-specific breakage.  So: SAFE cadence (m_hb_cadence_ms, the recipe's
	// HB_CADENCE_MS=33) outside staging mode-2, FAST cadence (m_hbpa_fast_ms)
	// only after the driver-pushed mode-2 signal (set_ingame) has been stable
	// for HBPA_DEBOUNCE_VBLANKS consecutive vblanks (~1 s); back to SAFE
	// IMMEDIATELY when mode-2 is lost, so attract / handshake / re-establishment
	// / reset always sees the safe cadence even if the mode word flickers.
	// Arm on BOTH cabs (each cab's heartbeat releases the PEER's emits - a
	// one-sided arm only speeds one direction).  Changes ONLY WHEN the
	// heartbeat fires - never what it sends (still the restamped stale-replay
	// token), never regs[5].  All emulation-thread only (set at vblank, read at
	// the timer re-arms).  MODEL PROVENANCE: Fable 5.  Experiment branch only -
	// never merge to milestone.
	uint32_t hb_cadence_effective_ms() const
	{
		return (m_hbpa_armed && m_hbpa_ingame) ? m_hbpa_fast_ms : m_hb_cadence_ms;
	}
	bool m_hbpa_armed = false;                     // env gate (NAMCOS23_PATCH_HB_PHASE_AWARE parsed valid)
	uint32_t m_hbpa_fast_ms = 16;                  // FAST (in-game) heartbeat re-arm interval, ms (env value)
	bool m_hbpa_ingame = false;                    // debounced phase: true = FAST cadence live (P061: shared by the TX-complete gate)
	uint32_t m_hbpa_mode2_streak = 0;              // consecutive mode-2 vblanks seen (debounce counter)
	uint32_t m_hbpa_transitions = 0;               // cumulative FAST<->SAFE flips (transition-log context)

	// P061 EXPERIMENT (branch patch/tx-complete-irq, off patch/hb-phase-aware):
	// C422 TX-COMPLETE dispatch model, env-gated by
	// NAMCOS23_PATCH_TX_COMPLETE_IRQ (boolean idiom; inert/byte-identical when
	// unset), ACTIVE only while the P060 set_ingame debounced mode-2 state
	// (m_hbpa_ingame) holds - outside mode-2 (attract / op55 establishment /
	// mode-select) the stock reg5 stop-and-wait is untouched.
	//
	// WHY (P052 RE + P060 run analysis + the 2026-07-13 audit): our device
	// models NO TX-complete; a staged TXSIZE (reg5) survives until an rx_clear
	// (a delivered PEER frame) wipes it, so the ROM's TX pump 0x8000BB6C bails
	// at its 0x8000BB80 busy-gate and one distinct TX per direction is in
	// flight, paced by the peer's heartbeat replay token.  P060 proved raising
	// the token rate toxic (every extra token = an extra STALE replay + an
	// extra frame stacked per ROM RX service; the ROM wedged at ~120
	// deliveries/s).  P061 removes the peer-arrival dependence entirely.
	//
	// MECHANISM (empirically derived from the P060 red log's reg_w r3/r5 +
	// rx_clear interleave, full derivation in the agent log): the ROM writes
	// START(reg3=3) BEFORE TXOFFSET/TXSIZE in every pump passage, so our
	// reg3-edge send always fires one stage EARLY (the zero-size abort beat)
	// and a staged frame is actually transmitted only when a delivery lands
	// exactly at a pump gate read and the IRQ handler's nested pump passage
	// re-stages it for the interrupted outer passage's START edge.  A
	// timer-deferred reg5 wipe (the audit's sketch) cannot reproduce that
	// two-passage interleave and would deadlock into heartbeat pacing.  The
	// faithful release is the chip's OTHER documented trigger (gold
	// mame/10-c139-register-map.md "txsize_commit" - a non-zero TXSIZE written
	// while the chip is armed; already modeled for bulk remainders by P014):
	// dispatch the freshly staged standalone frame SYNCHRONOUSLY inside the
	// very reg_w that staged it, via send_pending_tx_frame() - whose OWN
	// existing sync TXSIZE clear (the "TX complete" the ROM busy-polls, P052's
	// clear site (i)) then releases the pump gate.  The dispatch path itself
	// never writes regs[5] from patch code (it only calls the device's own
	// send); the single deliberate reg5 write is the parked-dup release below.
	// rx_clear stays untouched; NO TX-done IRQ pulse is added (status_r
	// already returns bit2 - the corpus-verified TX cause the handler
	// 0x8000BEAC jals the pump on - and an extra pulse per tick would run
	// extra handler pump passages that stage blocking duplicates; see the
	// agent log).  The staged bytes are read at the stage instant - the one
	// moment they are provably pristine (P035).
	//
	// PACING SAFETY (the P060 lesson): the dispatch is deduped on the frame's
	// own ROM drift seq (cells 0-1) + staged length - the compose bumps the
	// seq once per composed frame (60 Hz), so re-stages of an already-sent
	// frame (the pump's post-send restage, or an IRQ-handler pump passage
	// staging the same slot again) are SKIPPED and dispatches are structurally
	// capped at one per compose = one distinct frame per vblank to the peer,
	// with a TXCI_MAX_PER_VBLANK=2 hard cap as belt-and-braces.
	//
	// THE DUP RELEASE (the TX-complete half of the model): a dup-skipped stage
	// is PARKED in reg5 and, on real hardware, would serialize and complete -
	// TXSIZE reading 0 again ~a frame later.  Left alone it would block the
	// next kick's pump passage at the 0x8000BB80 busy-gate (measured mechanics:
	// the mid-drain delivery's handler pump passage stages a pre-compose dup
	// BEFORE the same tick's kick).  So: at the ROM's next TXSIZE busy-poll
	// READ (reg_r reg5 - the exact site today's rx_clear release also acts
	// through), a parked stage that RE-VERIFIES as the last-dispatched frame
	// (same seq + length read back from the staged RAM image) is cleared and
	// the read returns 0 - the busy-poll observing the chip's TX completion.
	// This is the ONE deliberate reg5 write in the patch: mode-2 gated,
	// content-verified strictly sacrificial (those exact bytes already
	// crossed), mirroring the device's own rx_clear release - the
	// audit-sanctioned emulator-faithful exception to the "never touch
	// regs[5]" rule.  A stage that is NOT a verified dup (distinct frame,
	// bulk, cap-skip) is NEVER touched.  The heartbeat is untouched: it
	// re-arms on every capture, so at ~60 captures/s it organically never
	// fires (replays vanish by starvation, not by code).  Arm on BOTH cabs.
	// All emulation-thread only.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	bool m_txci_armed = false;                     // env gate (NAMCOS23_PATCH_TX_COMPLETE_IRQ)
	bool m_txci_last_valid = false;                // m_txci_last_seq/_len hold a real dispatch
	uint16_t m_txci_last_seq = 0;                  // ROM drift seq of the last dispatch (dedupe key)
	uint16_t m_txci_last_len = 0;                  // staged halfword count of the last dispatch (dedupe key)
	bool m_txci_parked_dup = false;                // a dup-skipped stage sits in reg5 (release candidate at the next busy-poll)
	uint32_t m_txci_vbl_dispatches = 0;            // dispatches this vblank (hard cap TXCI_MAX_PER_VBLANK; reset in vblank_tick)
	bool m_txci_first_logged = false;              // one-shot first-dispatch line emitted
	bool m_txci_first_release_logged = false;      // one-shot first dup-release line emitted
	uint32_t m_txci_dispatch_win = 0;              // 1/s window: dispatches
	uint32_t m_txci_dup_skips_win = 0;             // 1/s window: same-seq re-stages skipped (the burst guard working)
	uint32_t m_txci_release_win = 0;               // 1/s window: parked dups released at the busy-poll (the TX-complete read)
	uint32_t m_txci_cap_skips_win = 0;             // 1/s window: per-vblank hard-cap skips (expect 0)
	uint32_t m_txci_bulk_skips_win = 0;            // 1/s window: bulk/continuation-context stages left to the stock path
	uint32_t m_txci_incomplete_skips_win = 0;      // 1/s window: trailer-test failures left to the stock path
	uint32_t m_txci_dispatch_tot = 0;              // cumulative dispatches
	uint32_t m_txci_release_tot = 0;               // cumulative dup releases
	uint32_t m_txci_status_div = 0;                // 1/s status divider

	// P031 EXPERIMENT (branch patch/announce-latch, off
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
	//    fought (the P025/P027 lesson: WIPE_RESTORE kept the register
	//    non-zero and starved the ROM's own emitter; this latch NEVER writes
	//    the register file).
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
	// Experiment branch only - never merge to milestone.
	bool m_al_armed = false;                       // env gate (NAMCOS23_PATCH_ANNOUNCE_LATCH)
	bool m_al_valid = false;                       // a live latch exists (announce -> head-send window)
	bool m_al_wiped = false;                       // the staged TXSIZE was rx_clear-wiped since the announce
	uint16_t m_al_offset = 0;                      // TXOFFSET of the latched bulk announce (head read pointer)
	uint32_t m_al_expected_hw = 0;                 // total announced halfwords (from the size cells)
	uint16_t m_al_wiped_hw = 0;                    // the staged TXSIZE value the wipe destroyed (dispatch size)
	attotime m_al_time;                            // announce/refresh emulated time (TTL anchor)
	uint32_t m_al_latched = 0;                     // counters for the 1/s status line (cumulative)
	uint32_t m_al_refreshed = 0;                   //   same-class re-announces that re-timed the latch
	uint32_t m_al_wipes_captured = 0;              //   staged-TXSIZE wipes remembered by a live latch
	uint32_t m_al_dispatched = 0;                  //   sends reconstructed from the latch (== wiped-then-dispatched)
	uint32_t m_al_expired = 0;                     //   latches dropped past TTL (never dispatched)
	uint32_t m_al_superseded = 0;                  //   latches dropped by a newer/other ROM stage
	uint32_t m_al_consumed_ok = 0;                 //   latches retired because the head went out normally (no dispatch needed)

	// P035 EXPERIMENT (branch patch/latch-v2-snapshot, off patch/
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
	// verbatim.  Experiment branch only - never merge to milestone.
	bool m_al_snapshot = false;                    // v2 selected (env value == 2): dispatch transmits the wipe-time snapshot
	bool m_al_snap_valid = false;                  // snapshot below was captured for the CURRENT wiped latch
	uint16_t m_al_snap_offset = 0;                 // C422 word offset the snapshot was copied from (== m_al_offset at capture)
	std::vector<uint8_t> m_al_snap;                // wipe-time payload copy, wire byte order (reserved once, reused)
	uint32_t m_al_snap_copied = 0;                 // counters for the 1/s status line (cumulative): captures at wipe
	uint32_t m_al_snap_dispatched = 0;             //   dispatches that transmitted the snapshot (snap_tx)
	uint32_t m_al_snap_fallback = 0;               //   v2 dispatches that had to re-read RAM (defensive rail; expect 0)

	// P037 EXPERIMENT (branch patch/latch-genstamp, off patch/round-arm):
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
	// Experiment branch only - never merge to milestone.
	uint32_t m_al_gen_differ = 0;                  // gen=differ compare events, dispatch + remainder (1/s status field)
	bool m_al_gen_head_live = false;               // currently-open tracked message's head was a v2 snapshot dispatch
	uint16_t m_al_gen_head_offset = 0;             // that head's C422 ring offset (m_al_offset at dispatch)
	uint16_t m_al_gen_head_hw0 = 0;                // head snapshot's first halfword (recomposed from wire order)
	uint16_t m_al_gen_head_hw1 = 0;                // head snapshot's second halfword (meaningful when m_al_gen_head_hws == 2)
	uint8_t m_al_gen_head_hws = 0;                 // halfwords held in the scratch (1 when the snapshot is a single hw, else 2)
	attotime m_al_gen_head_time;                   // the head's announce anchor (m_al_time at dispatch) - remainder head_age_ms shares the dispatch age_ms anchor

	// P038 EXPERIMENT (branch patch/latch-v3-dedupe, off patch/latch-genstamp):
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
	// Experiment branch only - never merge to milestone.
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
	uint32_t m_al_deduped = 0;                     // dispatches suppressed: class completed within the lookback (1/s status)
	uint32_t m_al_refresh_retired = 0;             // same-class re-announces that retired a PENDING WIPED dispatch (expect 0 in floods)

	// P013 EXPERIMENT (branch patch/tx-emitter-trace, stacked on
	// patch/chunk-sequencing).  READ-ONLY instrumentation, env-gated by
	// NAMCOS23_TRACE_TXEMIT (inert when unset).  Locates where the ROM TX
	// emitter (0x8000BB6C PATH B/B') puts the bulk REMAINDER of a >255hw
	// message: P010 + P011 both assumed the ROM stages the remainder as a
	// SECOND chunk that resumes the advanced DMA pointer, but every staged
	// chunk in the P011 run came from a slot BASE (a fresh TXOFFSET), none
	// from a resume pointer.  This trace logs the RAW TX register-programming
	// SEQUENCE per send so the analyst can decide between:
	//   (i)   the remainder DMAs on a path our chunk hook does not observe,
	//   (ii)  the ROM re-DMAs the whole >255hw message into the SAME 255hw
	//         saturated chunk (device only ever sees the truncated head), or
	//   (iii) the remainder is gated on a chip TX-complete IRQ we do not model.
	// The C139 register byte offsets (gold mame/10-c139-register-map.md) map
	// to device m_regs[] indices as: REG[0x06] START/trigger = m_regs[3]
	// (reg_idx 3); REG[0x0A] TXSIZE = m_regs[5] (reg_idx 5); REG[0x0E]
	// TXOFFSET = m_regs[7] (reg_idx 7).  The ROM's gp+0x75CA 4-slot
	// 0x400-stride DMA cursor lives in MIPS main RAM (not reachable from this
	// device), but it is FULLY reflected at the register layer: PATH C writes
	// REG[0x0E] = gp[0x75CA]+2, so m_regs[7] is the device-observable image of
	// the cursor and the slot index is (m_regs[7]-2)/0x400 (slot bases
	// 0x0000/0x0400/0x0800/0x0C00; +2 offset = the 2 size cells).  All
	// emulation-thread only.  Experiment branch only - never merge to milestone.
	bool m_txemit_trace = false;                   // env gate (NAMCOS23_TRACE_TXEMIT)
	uint32_t m_txemit_frame = 0;                   // independent per-vblank frame counter for line tags
	uint32_t m_txemit_txsize_zero_reads = 0;       // count of TXSIZE polls that read 0 (the synchronous-done case = candidate (iii) signal)
	uint32_t m_txemit_sends = 0;                   // count of socket sends (emit_tx_frame) since arm
	uint32_t m_txemit_status_div = 0;              // vblank divider for the 1/s TXEMIT status line

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): READ-ONLY
	// device-side half of the adoption trace, env-gated by NAMCOS23_TRACE_ADOPT
	// (inert when unset).  Logs an ADOPT: ingest line for every delivered RX
	// frame - the landing word, op/cellseq, and the first few body words - so
	// the analyst can see WHERE the ingested op-0x17/0x71/0xfe batch lands and
	// pair it with the driver-side ADOPT: render-gate/cutscene/actor taps by t=.
	// Emulation-thread only.  Experiment branch only - never merge to milestone.
	bool m_trace_adopt = false;                    // env gate (NAMCOS23_TRACE_ADOPT)
	uint32_t m_adopt_rx_index = 0;                 // running delivered-frame index for ADOPT ingest lines

	// P016/P018 EXPERIMENT (branch patch/op70-real-detector, off
	// patch/op70-cutscene-timer-arm): READ-ONLY device-side op-70 cutscene-timer
	// DETECTION, env-gated by NAMCOS23_TRACE_OP70 (inert when unset; env var name
	// REUSED so the user's run command is unchanged).  op-70 is a SUB-op WITHIN
	// the gate-4 VM token stream (dispatch table 0x8023F4D0 entry 0x70 -> handler
	// 0x800B2544 -> adoption 0x80016E28; value = cursor[0..1] big-endian masked
	// 0x7fff).  P016 scanned the cell stream for ANY byte 0x70 and over-reported
	// mid-payload operand bytes (0x11c6 / 0x4ea8) as false op-70 hits.  P018
	// REPLACES that byte-scan with a REAL cell-walk (op70_cell_walk): it walks the
	// cell stream as the ROM dispatcher 0x800AA840 does, advancing the cursor by
	// each op's TRUE on-wire length (gate-4 op-length table verified from gold/
	// full.txt + the 0x00 terminator and 0xFD/0xFE/0xFF intrinsics), and flags a
	// 0x70 ONLY when the cursor legitimately lands on it AS an opcode - never as
	// an operand/data byte.  Walked on BOTH directions: delivered RX frames
	// (OP70R: rx) and emitted TX frames (OP70R: tx, answering "does THIS cab
	// emit op-70 at all").  The DECISIVE accept/reject stays the driver-side
	// gp+0x7054 watch.  Emulation-thread only.  Experiment branch only - never
	// merge to milestone.
	bool m_trace_op70 = false;                     // env gate (NAMCOS23_TRACE_OP70)
	uint32_t m_op70_rx_index = 0;                  // running real-op-70 index for OP70R: rx lines
	uint32_t m_op70_tx_index = 0;                  // running real-op-70 index for OP70R: tx lines

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): READ-ONLY op55
	// wire-flag tap state, env-gated by NAMCOS23_TRACE_LINKBITS (inert unset; the
	// driver reads the same var for the +0x370 0x6000 sample).  Walks the cell
	// stream for op55 (the carrier of the partner +0x370 0x6000 bits) and logs
	// LINKBITS: txflags (emit_tx_frame) / rxflags (deliver_rx_frames) with the
	// decoded 24-bit flags + whether 0x6000 is present.  Emulation-thread only.
	// Experiment branch only - never merge to milestone.
	bool m_trace_linkbits = false;                 // env gate (NAMCOS23_TRACE_LINKBITS)
	uint32_t m_lb_tx_index = 0;                    // running op55 TX-frame index for LINKBITS: txflags
	uint32_t m_lb_rx_index = 0;                    // running op55 RX-frame index for LINKBITS: rxflags

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace, off
	// patch/op55-carrier-repeat): READ-ONLY device-side REAL op6F PLAY-CLOCK
	// detection, env-gated by NAMCOS23_TRACE_PLAYCLOCK (inert when unset).  op6F
	// is the gate-4 play-clock sync sub-op (dispatch table 0x8023F4D0[0x6F] =
	// RX handler 0x800B2448): its 6 operand bytes are TWO sign-extended 24-bit
	// big-endian values = the SENDER's LOCAL record +0x390/+0x394 play-clock pair
	// (serialized by the TX emitter 0x800B22CC once per 256 frames, gates
	// gp+0x74F3==0 / keepalive>0 / role byte 0x802F3FFC bit7 SET; the RECEIVER
	// adopts both into its OWN record +0x390/+0x394 at 0x800B2504-08 when its
	// role bit7 is CLEAR - a master->slave absolute clock sync).  Detected with
	// the SAME non-heuristic gate-4 VM cell-walk as op-70 (op70_cell_walk; the
	// P018 discipline: a 0x6F is reported ONLY when the cursor lands on it as an
	// opcode inside a clean 0x00-terminated run of >=2 known ops), on BOTH
	// directions: PLAYCLOCK: op6f-tx (emitted) / op6f-rx (delivered).
	// Emulation-thread only.  Experiment branch only - never merge to milestone.
	bool m_trace_playclock = false;                // env gate (NAMCOS23_TRACE_PLAYCLOCK)
	uint32_t m_pc6f_tx_index = 0;                  // running real-op6F index for PLAYCLOCK: op6f-tx lines
	uint32_t m_pc6f_rx_index = 0;                  // running real-op6F index for PLAYCLOCK: op6f-rx lines

	// P044 EXPERIMENT (branch patch/anchor-lifecycle, off patch/wave-init-hold):
	// READ-ONLY device-side ENTITY-LIFECYCLE wire detection, env-gated by
	// NAMCOS23_TRACE_OP20 (inert when unset; the driver reads the SAME var for
	// the per-vblank ring-transaction sampler).  op-0x20 "release entity" is
	// the gate-4 VM sub-op that KILLS wave anchors across cabs: dispatch table
	// 0x8023F4D0[0x20] = RX handler 0x800ACD48, which consumes EXACTLY 2
	// operand bytes (a big-endian id16, sign-extended), maps id16 -> entity via
	// 0x8010FE84 and calls the release 0x800B5BC8 - whose ring-mark stores
	// handle=-1 into the 4-slot wave-anchor ring @0x802D2030 (the state the
	// area-completion check condition B reads; P042/P043b).  Sibling op-0x1F
	// (parameterized despawn, RX 0x800ACCA0 -> 0x800B5E1C(ent,b1,b2)) consumes
	// EXACTLY 4 operand bytes (id16 + 2 param bytes).  Both lengths VERIFIED
	// instruction-by-instruction in full.txt this session (0x800ACD48: lbu 0/
	// lbu 1/addiu $s0,2/ret $s0; 0x800ACCA0: id16 + 2x lbu = +4).  Detected
	// with the SAME non-heuristic gate-4 VM cell-walk as op-70/op6F
	// (op70_cell_walk, P018 discipline), on BOTH directions: OP20: rx-release
	// (a delivered frame carries a release this cab will ingest = the
	// suspected walk-phase anchor killer) / OP20: tx-release (this cab emitted
	// one - the local release-with-notify class, encoder 0x800ACD20 queued by
	// 0x800B5BC8/avatar flows).  NOTE the wire id16 is the big-map index
	// (parallel-entity +0x82), NOT the 32-bit anchor id in the ring - the
	// analyst joins device OP20: rx-release lines to driver OP20: ring-kill
	// lines by t= adjacency (same-frame ingest).  The 0x1F/0x20 op-length
	// table entries are GATED on m_trace_op20 so the walk stays bit-exact for
	// the stacked OP70R/PLAYCLOCK detectors when this env is unset; when
	// armed, clean runs can extend THROUGH 0x1F/0x20 cells (strictly more
	// genuine op70/op6f reports possible - documented, verified lengths).
	// Emulation-thread only.  Experiment branch only - never merge to milestone.
	bool m_trace_op20 = false;                     // env gate (NAMCOS23_TRACE_OP20)
	uint32_t m_op20_tx_index = 0;                  // running index for OP20: tx-release/tx-despawn lines
	uint32_t m_op20_rx_index = 0;                  // running index for OP20: rx-release/rx-despawn lines

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off
	// patch/linked-gate-supply): WIRE-ONLY 0x6000 injection state.  The arm +
	// mode==2 gate are pushed from the driver each vblank (set_linked_gate_txonly)
	// because only the driver can read the env var and the staging mode word
	// (0x802F3FD0).  The injection is applied in emit_tx_frame.  Both gate flags
	// and the counters are emulation-thread only (set at vblank, read in TX, same
	// thread).  Experiment branch only - never merge to milestone.
	bool m_lg_txonly_armed = false;                // env arm pushed from driver (NAMCOS23_PATCH_LINKED_GATE_TXONLY)
	bool m_lg_txonly_mode2 = false;                // staging mode word 0x802F3FD0 == 2 pushed from driver
	uint32_t m_lg_txonly_inject_count = 0;         // total op55 wire-flag bytes 0x6000-injected (per-inject)
	uint32_t m_lg_txonly_status_div = 0;           // ~1/s divider for the LINKED_GATE_TXONLY: status line

	// P050 EXPERIMENT (branch patch/single-burst-pump, off patch/reaper-patience):
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
	// fight frame and poisoning gate (b)'s newest-wins.
	bool m_bq_armed = false;                       // env NAMCOS23_PATCH_BURST_QUANTUM (device-side companion arm)
	uint32_t m_bq_single_burst_tx = 0;             // >255-hw single-burst whole frames sent (companion 1 fired)

	// GATE (b) - m_newest_wins (env NAMCOS23_PATCH_NEWEST_WINS): NEWEST-WINS
	// delivery cap.  When deliver_rx_frames drains the inbound queue it keeps
	// only the NEWEST complete-standalone VM frame and drops the older complete
	// ones (superseded).  "complete-standalone" = the ROM's strict trailer
	// invariant (bit-8 end marker in the final halfword AND the trailer's
	// claimed length == the frame's own halfword count) - a test a multi-chunk
	// message's fragments NEVER pass (an intermediate chunk has no trailer; a
	// last chunk's claimed length is the whole-message total, not the chunk's
	// own size), so CHUNK_PASSTHRU ring reassembly is structurally untouched and
	// the gate is sound WITH or WITHOUT gate (a).  Safe by the P049 RE (F3):
	// frame supersession is exactly the ROM's own compose-side semantics - state
	// re-serializes every frame and the 32-slot reliable backlog re-publishes
	// until 0xFD-acked, so the newest kept frame carries the freshest state AND
	// all outstanding reliable slots; a dropped instance is re-sent by design
	// (NOT distinguishable per-frame, but never lost - documented in the impl
	// log).  Accepted cost (R2): a dropped frame that happened to carry the
	// 1/256-frame op6F clock beacon delays clock resync up to ~4.3 s (the
	// PLAYCLOCK rails watch it; the play clocks are wall-synced anyway).
	// Emulation-thread only.  Experiment branch only - never merge to milestone.
	bool m_newest_wins = false;                    // env gate (NAMCOS23_PATCH_NEWEST_WINS)
	uint32_t m_nw_superseded = 0;                  // cumulative older complete frames dropped (1/s status)

	// P050 falsifier instrumentation (gated on m_pump_instr = either gate
	// armed): per-complete-frame FIFO-JOIN stamps + frames/s delivered per
	// direction.  The join key seq= is the ROM's per-frame drift sequence
	// (cellseq = low bytes of halfwords 0-1, the value 0x8000B8F0-914
	// reconstructs as the remote seq) - red's tx-join seq=X joins blue's
	// rx-join seq=X to yield the red->blue delivery latency the P048 method
	// measured.  Emulation-thread only.  Experiment branch only.
	bool m_pump_instr = false;                     // m_newest_wins || m_bq_armed (instrumentation live)
	uint32_t m_nw_tx_complete = 0;                 // cumulative complete frames emitted (tx fps)
	uint32_t m_nw_rx_complete = 0;                 // cumulative complete frames delivered (rx fps)
	uint32_t m_nw_tx_win = 0;                       // complete frames emitted this 1/s window
	uint32_t m_nw_rx_win = 0;                       // complete frames delivered this 1/s window
	uint32_t m_nw_status_div = 0;                   // vblank divider for the 1/s NEWEST_WINS status

	// P055 EXPERIMENT (branch patch/boat-jitter-trace, off patch/single-burst-pump):
	// READ-ONLY transport bridge for the driver-side BLUE boat-jitter trace,
	// env-gated by NAMCOS23_TRACE_BOATJITTER (inert/byte-identical when unset).
	// The driver's per-vblank entity-position trace (namcos23.cpp) needs to know,
	// for each delivered RX frame, (1) that an ingest happened (rx-apply
	// generation) and (2) whether it was a FRESH ROM compose or a stale
	// HEARTBEAT-REPLAY, so it can tag a boat's backward position snap as
	// stale-replay pollution (hypothesis A) vs a fresh-RX-vs-local-sim fight
	// (hypothesis B).  A heartbeat replay (heartbeat_tick) is a byte-identical copy
	// of the sender's last real TX EXCEPT payload bytes 0-1 (the restamped counter,
	// frame[2..3] on the wire, payload[0..1] here), so a delivered complete VM frame
	// is classified REPLAY when it matches the previous delivered complete frame
	// from byte 2 onward - which is exactly the "boat frozen on the wire" signature
	// (a moving boat makes every FRESH compose differ in its coordinate bytes).
	// MODEL PROVENANCE: Opus 4.8.  Emulation-thread only (bumped in
	// deliver_rx_frames, read at vblank - same thread, no atomics).  Experiment
	// branch only - never merge to milestone.
public:
	bool boatjitter_armed() const { return m_bj_armed; }
	bool last_delivered_was_replay() const { return m_bj_last_was_replay; } // class of the MOST RECENT delivered complete frame
	uint32_t rx_apply_gen() const { return m_bj_rx_gen; }          // ++ per delivered complete VM frame (driver diffs it per vblank)
	uint32_t rx_replay_count() const { return m_bj_replay_count; } // cumulative REPLAY classifications (driver 1/s status)
	uint32_t rx_fresh_count() const { return m_bj_fresh_count; }   // cumulative FRESH classifications
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
	bool m_bj_armed = false;                       // env gate (NAMCOS23_TRACE_BOATJITTER), device side
	bool m_comm_is_connector = false;              // set in start_comm: remote configured, no local = the connector (blue)
	bool m_bj_last_was_replay = false;             // last delivered complete frame == prev complete frame from byte 2 on
	uint32_t m_bj_rx_gen = 0;                       // delivered-complete-frame generation
	uint32_t m_bj_replay_count = 0;                 // cumulative replay classifications
	uint32_t m_bj_fresh_count = 0;                  // cumulative fresh classifications
	std::vector<uint8_t> m_bj_prev_complete;        // previous delivered complete frame bytes (replay compare basis)

	// P059 EXPERIMENT (branch patch/poscorr-trace, off patch/boat-render-trace):
	// READ-ONLY device-side RX ARRIVAL / DELIVERY / DROP instrumentation for FIX
	// FAMILY B - env-gated by NAMCOS23_TRACE_POSCORR (inert/byte-identical when
	// unset), counted ONLY on the connector/blue.  The decisive B question: does a
	// fresh +0xCC correction frame ARRIVE at blue but get DROPPED/collapsed before
	// the ROM applies it (=> B easy: deliver it), or is the low correction rate an
	// UPSTREAM emit limit (red only composes ~1000/HB_CADENCE_MS distinct frames)?
	// So per 1/s window we count, in deliver_rx_frames: complete VM frames pulled
	// from the socket (arrived), complete frames written into the RX ring
	// (delivered, split FRESH/REPLAY), and frames dropped - by the newest-wins
	// collapse (namco_c139.cpp:1545, CONDITIONAL on m_newest_wins, so 0 when
	// NAMCOS23_PATCH_NEWEST_WINS is unset = the recipe: the drop is INERT, not
	// unconditional) or as sub-2-word greetings.  arrived==delivered => no device
	// drop = the throttle is the upstream stop-and-wait emit (B hard); arrived>>
	// delivered => recoverable headroom (B easy).  Emulation-thread only (bumped in
	// deliver_rx_frames, emitted in vblank_tick - same thread, no atomics).  MODEL
	// PROVENANCE: Opus 4.8.  Experiment branch only - never merge to milestone.
	bool m_poscorr_trace = false;                   // env gate NAMCOS23_TRACE_POSCORR (device side)
	uint32_t m_pc_drains_win = 0;                    // deliver_rx_frames calls with >=1 pending, this window
	uint32_t m_pc_arrived_complete_win = 0;          // complete VM frames pulled from drain_rx (position carriers)
	uint32_t m_pc_arrived_other_win = 0;             // non-complete frames pulled (fragments / greetings)
	uint32_t m_pc_delivered_complete_win = 0;        // complete VM frames written into the RX ring
	uint32_t m_pc_delivered_fresh_win = 0;           // of delivered complete, FRESH (new positions)
	uint32_t m_pc_delivered_replay_win = 0;          // of delivered complete, heartbeat REPLAY (stale)
	uint32_t m_pc_dropped_newest_win = 0;            // frames dropped by the 1545 newest-wins collapse (0 unless armed)
	uint32_t m_pc_dropped_small_win = 0;             // sub-2-word frames skipped (greetings / odd)
	uint32_t m_pc_max_pending_win = 0;               // max pending.size() this window (queue depth headroom)
	uint32_t m_pc_multidrain_win = 0;                // drains where pending.size() > 1 (burst arrivals)
	uint32_t m_pc_arrived_complete_tot = 0;          // cumulative complete frames arrived
	uint32_t m_pc_delivered_complete_tot = 0;        // cumulative complete frames delivered
	uint32_t m_pc_dropped_newest_tot = 0;            // cumulative newest-wins drops
	uint32_t m_pc_status_div = 0;                     // vblank divider for the 1/s POSCORR device status

	// P062 EXPERIMENT (branch patch/txstage-trace, off patch/tx-complete-irq):
	// READ-ONLY / LOG-ONLY per-stage TX CONTENT-HASH trace, env-gated by
	// NAMCOS23_TRACE_TXSTAGE (inert/byte-identical when unset), BOTH cabs, run
	// with the P061 patch env UNSET so the STOCK stop-and-wait pacing is what
	// gets measured.  WHY (P061 run analysis, finding 7.5 / "measure before
	// medicine"): P061's admission gate keyed dedupe on (seq,len), but seq is a
	// SLOW shared PHASE counter (+1 per ~3-7 s) and len is class-fixed on blue,
	// so the gate ate ~90-120 genuinely-different composed frames/s (proven with
	// frame bytes).  Before any P061b retry we need ground truth about the ROM's
	// TX staging under stock pacing - numbers only CONTENT identity can give:
	//  1. TRUE COMPOSE RATE: of the ~120 stages/s in mode-2 (~2/vblank), how
	//     many are DISTINCT CONTENTS vs true content-identical re-stages (the
	//     distinct rate is the ceiling a TX-complete release could unlock; the
	//     true-duplicate rate is what a corrected gate must actually filter);
	//  2. CLASS MIX per cab (blue: fixed 108hw only in-game; red: also a
	//     variable-length scene-table class) - len distribution/rate/fresh split
	//     so a P061b key can be class-aware;
	//  3. STAGING RHYTHM: inter-stage gaps, stage#-within-vblank, the
	//     two-passage START-before-TXSIZE interleave (starts/s next to
	//     stages/s), PATH-D idle TXSIZE=0 ticks;
	//  4. BUSY-POLL BASELINE: the ROM's TXSIZE busy-poll passage rate under
	//     stock back-pressure (P061 measured 80-232 pump passages/s stock vs
	//     4.7-6.5k/s when the instant-0 poll removed the back-pressure) - the
	//     number a modeled TX-BUSY interval must reproduce;
	//  5. HEARTBEAT CAPTURE CONTENT: whether the heartbeat's captured frame
	//     (m_last_tx_payload, captured in emit_tx_frame) is the NEWEST-composed
	//     content at capture time (the last-dispatched vs newest-composed
	//     divergence the post-mortem fingered as the second starvation channel).
	// Mechanism: at every ROM TXSIZE stage (reg_w reg5 != 0 - the same site the
	// P061 dispatch trigger observes, with the same read-pointer resolution the
	// send itself uses) hash the staged RAM image in wire byte order (FNV-1a 32,
	// hi/lo per halfword - the exact bytes send_pending_tx_frame would emit and
	// m_last_tx_payload captures, so stage hashes and capture hashes are
	// directly comparable) and log ONE compact TXSTAGE: stage line; dup flags
	// against the previous stage (dup=) and against a TS_HIST-deep recent ring
	// (dupr= - robust to the two-class interleave, where alternating classes
	// always differ from the immediately-previous stage).  TXSIZE reads (the
	// busy-poll) and START rising edges are COUNTED only (no per-event lines).
	// 1/s TXSTAGE: status digest: stages/distinct/dups/zero-stages, per-length
	// class table (stages/fresh per len), polls5(+busy)/s, starts/s, heartbeat
	// captures/s + capture-vs-newest-stage hash matches.  NO behavior change
	// anywhere: no register/RAM writes, no sends, no timer changes - counters
	// and logerror only, every statement gated on m_ts_armed.  Emulation-thread
	// only.  MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	static constexpr unsigned TS_HIST    = 4;      // recent-stage (hash,len) ring depth (covers 2-class interleave + same-beat re-stage pairs)
	static constexpr unsigned TS_CLS_MAX = 6;      // per-window length-class table entries (overflow -> "other" bucket)
	struct ts_hist_rec
	{
		uint32_t hash = 0;                         // FNV-1a 32 of the staged wire bytes
		uint16_t len = 0;                          // staged TXSIZE (halfwords)
		bool valid = false;
	};
	struct ts_cls_rec
	{
		uint16_t len = 0;                          // class key = staged length (halfwords)
		uint32_t stages = 0;                       // stages of this length this window
		uint32_t fresh = 0;                        // of those, content NOT in the recent ring (distinct composes)
	};
	bool m_ts_armed = false;                       // env gate (NAMCOS23_TRACE_TXSTAGE)
	uint32_t m_ts_frame = 0;                       // per-vblank frame counter for line tags (independent, like m_txemit_frame)
	uint32_t m_ts_vbl_stage_idx = 0;               // stage# within the current vblank (reset each vblank_tick)
	uint32_t m_ts_stage_seq = 0;                   // running stage index (line tag n= / cumulative total)
	attotime m_ts_prev_stage_time;                 // previous stage instant (inter-stage gap_us)
	bool m_ts_prev_stage_valid = false;
	ts_hist_rec m_ts_hist[TS_HIST];                // last TS_HIST stages (the dupr= test)
	uint8_t m_ts_hist_idx = 0;                     // next ring write slot
	uint32_t m_ts_last_stage_hash = 0;             // newest staged content hash (heartbeat-capture compare)
	uint16_t m_ts_last_stage_len = 0;              // its length (halfwords)
	bool m_ts_last_stage_valid = false;
	uint32_t m_ts_stages_win = 0;                  // 1/s window: non-zero TXSIZE stages
	uint32_t m_ts_dup_prev_win = 0;                // 1/s window: stages content-identical to the immediately previous stage
	uint32_t m_ts_dup_recent_win = 0;              // 1/s window: stages matching ANY of the last TS_HIST stages (true re-stages)
	uint32_t m_ts_zero_stage_win = 0;              // 1/s window: PATH-D idle TXSIZE=0 writes
	uint32_t m_ts_poll5_win = 0;                   // 1/s window: TXSIZE reads (reg_r reg5 - the pump's busy-poll passages)
	uint32_t m_ts_poll5_busy_win = 0;              // 1/s window: of those, reads that observed a non-zero (busy) low byte
	uint32_t m_ts_start_edge_win = 0;              // 1/s window: START (reg3 bit0) rising edges (pump passage rate, P061's 80-232/s stock metric)
	uint32_t m_ts_hbcap_win = 0;                   // 1/s window: heartbeat captures (m_last_tx_payload assignments)
	uint32_t m_ts_hbcap_match_win = 0;             // 1/s window: captures whose hash == the newest staged hash at capture instant
	uint32_t m_ts_hbcap_hash = 0;                  // hash of the current captured replay payload
	bool m_ts_hbcap_valid = false;                 // a capture has happened
	ts_cls_rec m_ts_cls[TS_CLS_MAX];               // per-window length-class table
	uint32_t m_ts_cls_other_stages = 0;            // overflow bucket (window)
	uint32_t m_ts_cls_other_fresh = 0;
	uint32_t m_ts_status_div = 0;                  // vblank divider for the 1/s TXSTAGE status digest

	// P063 EXPERIMENT (branch patch/tx-complete-v2, off patch/txstage-trace):
	// TX-COMPLETE RELEASE v2 - the P062-measured retry of the P061 concept,
	// env-gated by NAMCOS23_PATCH_TX_COMPLETE_V2 (boolean idiom; inert/
	// byte-identical when unset; REFUSED if the dead P061 env
	// NAMCOS23_PATCH_TX_COMPLETE_IRQ is also armed - one release model at a
	// time).  ACTIVE only while the P060 set_ingame debounced mode-2 state
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
	// Not save-state registered, matching every other experiment member.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
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
	uint32_t m_txc2_dispatch_win = 0;              // 1/s window: fresh-content dispatches
	uint32_t m_txc2_dup_skip_win = 0;              // 1/s window: hash-identical re-stages skipped (gate_skips)
	uint32_t m_txc2_ring_skip_win = 0;             // of those, matched the ring but NOT prev (A-B-A cover; expect ~0)
	uint32_t m_txc2_zero_win = 0;                  // 1/s window: TXSIZE=0 writes ignored
	uint32_t m_txc2_busy_hold_win = 0;             // 1/s window: reg5 reads answered BUSY inside the modeled window
	uint32_t m_txc2_release_win = 0;               // 1/s window: parked duplicates cleared at the first post-window poll
	uint32_t m_txc2_stale_win = 0;                 // 1/s window: heartbeat replays suppressed by the age-out
	uint32_t m_txc2_midwin_win = 0;                // 1/s window: fresh dispatches that fired INSIDE an open window (expect 0)
	uint32_t m_txc2_bulk_skip_win = 0;             // 1/s window: chunk-train/bulk-context stages left to the stock path
	uint32_t m_txc2_incomplete_win = 0;            // 1/s window: trailer-test failures left to the stock path
	uint32_t m_txc2_dispatch_tot = 0;              // cumulative dispatches
	uint32_t m_txc2_stale_tot = 0;                 // cumulative stale suppressions
	uint32_t m_txc2_status_div = 0;                // 1/s status divider
};


// device type definition
DECLARE_DEVICE_TYPE(NAMCO_C139, namco_c139_device)



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************



#endif // MAME_NAMCO_NAMCO_C139_H
