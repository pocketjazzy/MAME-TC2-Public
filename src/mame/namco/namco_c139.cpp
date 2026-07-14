// license:BSD-3-Clause
// copyright-holders:Angelo Salese
/***************************************************************************

    Namco C139 - Serial I/F Controller

    QFP64 RS-422 link controller used to network arcade cabinets:
      System 21 / 22 / Super 22, and System 23 (where the silkscreen
      labels it "C422", believed to be a pin-compatible faster-clocked
      revision of C139).

    TODO:
    - Make this to actually work!
    - Is RAM shared with a specific CPU other than master/slave?
    - is this another MCU with internal ROM?
    - Wire up an asio TCP bridge so two MAME instances can act as two
      linked cabinets (Time Crisis 2 twin cabinet, Tokyo Wars, Ridge
      Racer 3-monitor, etc.).

    Status:
    - Phase 1: register file is store/load with magic-value IRQ raise/
      clear semantics carried over from the inline c422 stub previously
      embedded in namcos23.cpp.  No network I/O yet.

***************************************************************************/

#include "emu.h"
#include "namco_c139.h"

#include "config.h"     // P068 (patch/linkplay-menu): per-machine cfg <linkplay> node
#include "emuopts.h"

#include "xmlfile.h"    // P068: util::xml::data_node attribute access

#include "asio.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(NAMCO_C139, namco_c139_device, "namco_c139", "Namco C139 Serial")

// P002 EXPERIMENT (branch patch/vblank-lockstep) tuning constants.
//
// Control frames ride our internal TCP framing with BIT 15 OF THE 16-bit
// size prefix SET (game frames are capped at 0x4000 bytes, so bit 15 is
// never set on a legitimate game frame).  The low 15 bits are the control
// payload byte count.  Control payload byte 0 is a type code; type 0x01 =
// lockstep frame-token, followed by a 32-bit big-endian vblank counter.
// Control frames are consumed by the network read loop and NEVER enter the
// game RX queue / shared RAM.  NOTE: a peer running a pre-P002 binary will
// treat a control frame's size prefix as invalid and close the socket -
// run BOTH instances from the same build (launch_link.ps1 already does).
namespace {

constexpr uint8_t  LOCKSTEP_CTRL_TYPE_TOKEN     = 0x01; // payload[0]
constexpr uint16_t LOCKSTEP_CTRL_SIZE_FLAG      = 0x8000; // bit 15 of size prefix
constexpr uint16_t LOCKSTEP_CTRL_MAX_PAYLOAD    = 0x40; // sanity cap
constexpr int32_t  LOCKSTEP_MAX_LEAD            = 2;   // frames we may run ahead of the peer
constexpr int      LOCKSTEP_STALL_TIMEOUT_MS    = 100; // wall-clock cap per stalled vblank
constexpr uint32_t LOCKSTEP_SUSPEND_AFTER       = 6;   // consecutive full timeouts before action
constexpr int32_t  LOCKSTEP_REBASE_DRIFT        = 300; // |drift| beyond this = discontinuity

// P010 EXPERIMENT (branch patch/chunk-reassembly) tuning constants.
//
// The ROM's TX chunk emitter (0x8000BB6C, gold func-8000bb6c.md) splits a
// message larger than 0xFF halfwords into TXSIZE=0xFF chunks + remainder:
// it writes TXOFFSET only for the FIRST chunk (PATH C, which also reads the
// message's TOTAL halfword count from the two cells immediately BEFORE the
// offset it programs) and expects the chip's internal DMA pointer to keep
// advancing for the continuation chunks (PATH B/B').  The receive-side
// validator (0x8000BF38 Phase A) accepts slot lengths in [4..0x400], so
// 0x400 halfwords is the largest legitimate message.
constexpr uint16_t CHUNK_SAT_HW                 = 0xFF;  // emitter's saturated chunk size
constexpr uint32_t CHUNK_MAX_HW                 = 0x400; // validator's max message length
constexpr int      CHUNK_STALE_MS               = 500;   // pending reassembly older than this = dead

// P027 EXPERIMENT (branch patch/hb-cadence-wipe-restore) tuning constants.
//
// (a) The ROM declares a link timeout when the drift word 0x802F3504 reaches
// 0x11 (17 frames; writers 0x8000BB44 / 0x8000C050 also clear the freshness
// byte 0x802F3502 = "gate3502"); a validated ingest resets it to ~2 (transit
// latency), so the refresh budget is ~15 frames = 250 ms at 60 fps - EXACTLY
// the stock heartbeat period, i.e. a 0-2 frame margin.  P026 run 1: blue
// crossed that line 40 times (38 in the cutscene at ~1.1/s = the user-visible
// jitter).  NAMCOS23_PATCH_HB_CADENCE_MS makes the cadence tunable (run 1
// uses 150 ms = ~8 frames of margin); the clamp below rejects typos.
constexpr unsigned long HB_CADENCE_MIN_MS       = 10;    // sanity floor for the env override
constexpr unsigned long HB_CADENCE_MAX_MS       = 1000;  // > 283 ms (the 17-frame ceiling) is self-defeating but allowed for A/B
// P060 (branch patch/hb-phase-aware): phase-aware FAST cadence bounds + debounce.
constexpr unsigned long HBPA_FAST_MIN_MS        = 8;     // below ~half a frame the token would outrun the ROM's 60 Hz compose
constexpr unsigned long HBPA_FAST_MAX_MS        = 100;   // a "fast" cadence slower than this is pointless (SAFE recipe is 33)
constexpr uint32_t      HBPA_DEBOUNCE_VBLANKS   = 60;    // consecutive mode-2 vblanks before FAST (~1 s); SAFE resumes immediately
// P061 (branch patch/tx-complete-irq): hard per-vblank cap on TX-complete
// dispatches.  The seq+len dedupe already bounds dispatches at one per ROM
// compose (= one per vblank); this cap is the belt-and-braces guarantee that
// the peer can never receive a sustained >1 distinct frame per vblank from
// this path (the P060 FAIL-INGAME-INGEST regime began at ~2 deliveries per
// vblank).  2, not 1, so a legitimate tick/vblank phase-skew beat is never
// starved.
constexpr uint32_t      TXCI_MAX_PER_VBLANK     = 2;
// P063 (branch patch/tx-complete-v2): modeled TX-BUSY serialization interval
// bounds + heartbeat stale-replay age-out bounds.  P062 Q4 calibration: the
// ROM lives at polls ~99% busy, START passages 70-100/s, and demonstrably
// tolerates >=16.5 ms busy spans; one vblank (~12 ms default) reproduces that
// texture and caps passage-paced releases at <=~83/s.  Below 8 ms the window
// stops pacing the pump (P061's spin regime); above 20 ms it throttles below
// the measured ~55/s compose ceiling.  The age-out default was 500 ms (the
// P062 Q6.4 recommendation ~250-500 ms at the conservative end); P067
// (patch/defaults-on) bakes the ADOPTED P063 tune of 150 ms as the built-in
// default (the recipe passed NAMCOS23_PATCH_TXC2_STALE_MS=150 explicitly): a
// replay of a capture older than this carries no pacing value under
// self-release and only poisons the peer (red's >255hw stretches aged the
// capture up to 66 s).
constexpr unsigned long TXC2_BUSY_MIN_MS        = 8;
constexpr unsigned long TXC2_BUSY_MAX_MS        = 20;
constexpr unsigned long TXC2_BUSY_DEFAULT_MS    = 12;   // P067: already the adopted default
constexpr unsigned long TXC2_STALE_MIN_MS       = 50;
constexpr unsigned long TXC2_STALE_MAX_MS       = 5000;
constexpr unsigned long TXC2_STALE_DEFAULT_MS   = 150;  // P067: adopted default (was 500)
// (c) The counter stamped in payload halfword 0 advances once per ROM
// state-machine call (~1 per vblank; namcos23 is ~59.9 Hz - <0.2% error over
// the max age below, i.e. under an eighth of a frame).  A peer sample older
// than this is too stale to age credibly - fall back to the stock
// m_local_counter stamp instead of guessing.
constexpr int    HB_RESTAMP_MAX_AGE_MS          = 1000;
constexpr double HB_COUNTER_HZ                  = 60.0;

// P031 EXPERIMENT (branch patch/announce-latch) tuning constant.
//
// TTL for the announce latch: the stage->START window the latch protects is
// 1-2 frames (the wipe usually lands inside the very reg_w that carries the
// START edge), and the ROM's own re-announce of a lost class arrives within
// ~250 ms - so 150 ms comfortably covers every legitimate dispatch while
// guaranteeing a stale (offset,size) can never reach the wire long after the
// ROM's emitter moved on.  Emulated-time domain (attotime), like every other
// timeout in this device (CHUNK_STALE_MS etc.), so lockstep stalls cannot
// expire a latch mid-race.
constexpr int    AL_TTL_MS                      = 150;

// P035 EXPERIMENT (branch patch/latch-v2-snapshot) tuning constant.
//
// Capacity of the announce-latch v2 WIPE-TIME PAYLOAD SNAPSHOT, in wire
// BYTES (2 per C422 halfword).  Sized to the send path's frame-size RAM cap
// (0x1000 halfwords, see send_pending_tx_frame) so ANY size a latch
// dispatch could legally transmit always fits; the realistic capture is the
// emitter's saturated head chunk (CHUNK_SAT_HW = 0xFF halfwords = 510
// bytes - the staged TXSIZE the rx_clear wipe destroys.  The TX ring's
// 0x400-halfword slot stride is the geometry that head sits in, not the
// copied size: the snapshot copies exactly the halfwords the dispatch
// would otherwise re-read, m_al_wiped_hw of them).  The buffer is reserved
// ONCE in device_start() when v2 is selected; each capture reuses it
// (clear + append within the reserved capacity - no per-event allocation).
constexpr uint32_t AL_SNAP_MAX_BYTES            = 0x2000;

// P038 EXPERIMENT (branch patch/latch-v3-dedupe) tuning constant.
//
// Dedupe lookback: a START-edge dispatch is suppressed when a wire
// completion of the same (offset, expected_hw) class exists at or after
// (announce_time - AL_DEDUPE_LOOKBACK_MS).  The window extends BEFORE the
// announce because the P036 run measured the natural complete PRECEDING the
// wiped cadence re-announce by ~16-33 ms (flood pair spacing ~150 ms minus
// dispatch ages 117-134 ms; cross-checked by the 5 TTL-expiry siblings:
// complete -> expiry gaps of ~167 ms with the announce <= ~17 ms after the
// complete) - a "completions since announce only" rule would have deduped
// ~nothing.  One cadence hop (150 ms, == AL_TTL_MS and the recipe's
// HB_CADENCE_MS) covers that gap with 5x margin while keeping the PREVIOUS
// cadence cycle's complete (~166+ ms before the announce) OUTSIDE the
// window, so each cycle is judged only against its own natural send.  It
// also bounds the false-match cost: the ROM's 3-6x retry ladder for a
// genuinely lost class re-announces >= ~150-250 ms later, which escapes the
// window of the earlier false-matched completion - a suppression can
// therefore never abandon a class, only delay it by <= one hop.
constexpr int    AL_DEDUPE_LOOKBACK_MS          = 150;

// P013 EXPERIMENT (branch patch/tx-emitter-trace): the ROM's BF38 Phase B TX
// builder advances gp+0x75CA by 0x400 halfwords (mod 0x0C00) per call - a
// 4-slot 0x400-stride ring in C139 data RAM (gold func-8000bf38.md L33/L69).
// PATH C programs REG[0x0E] (TXOFFSET) = gp[0x75CA]+2 (the 2 size cells sit at
// the slot base), so a TX read pointer of the form slot_base+2 is a fresh
// message at slot (ptr-2)/0x400; any other (advanced) pointer is a
// continuation resume offset.  Returns the 0..3 slot index for a TX pointer.
constexpr uint16_t TXEMIT_SLOT_STRIDE_HW        = 0x400; // gp+0x75CA stride
constexpr unsigned txemit_slot_of(uint16_t tx_ptr)
{
	// Slot base = ptr rounded down to the 0x400 boundary; index = base/0x400,
	// confined to the 4-slot ring (0..3).  A slot-base read pointer is
	// slot_base+2 (PATH C); the floor division tolerates the +2 and any
	// in-slot resume offset, so an advanced continuation pointer reports the
	// same slot its message started in.  Returns unsigned for direct %u use.
	return unsigned((tx_ptr / TXEMIT_SLOT_STRIDE_HW) & 0x3);
}

// P062 EXPERIMENT (branch patch/txstage-trace): FNV-1a 32-bit content hash for
// the per-stage TX trace.  Two variants over the SAME byte stream so stage
// hashes and heartbeat-capture hashes are directly comparable:
//  - ts_hash_ram walks the C422 RAM image at the staged read pointer in WIRE
//    byte order (high byte first, low byte second per halfword, 0x1fff mask -
//    exactly the readout loop in send_pending_tx_frame);
//  - ts_hash_bytes walks an already-wire-ordered byte buffer (the heartbeat's
//    captured m_last_tx_payload, which IS that readout's output).
// FNV-1a is cheap (~2 ops/byte; worst case 0x400 hw = 2 KB per stage at
// ~120-240 stages/s) and collision-safe enough for dup detection at this
// volume; the analyst can re-verify any suspicious pair offline from the
// existing full-payload tx-frame hex dumps.  READ-ONLY - no state touched.
constexpr uint32_t TS_FNV_OFFSET                = 0x811c9dc5;
constexpr uint32_t TS_FNV_PRIME                 = 0x01000193;
inline uint32_t ts_hash_ram(uint16_t const *ram, uint16_t ptr, uint32_t hw)
{
	uint32_t h = TS_FNV_OFFSET;
	for (uint32_t i = 0; i < hw; i++)
	{
		uint16_t const w = ram[uint16_t(ptr + i) & 0x1fff];
		h = (h ^ uint32_t(uint8_t(w >> 8))) * TS_FNV_PRIME;
		h = (h ^ uint32_t(uint8_t(w & 0xff))) * TS_FNV_PRIME;
	}
	return h;
}
inline uint32_t ts_hash_bytes(uint8_t const *p, std::size_t n)
{
	uint32_t h = TS_FNV_OFFSET;
	for (std::size_t i = 0; i < n; i++)
		h = (h ^ uint32_t(p[i])) * TS_FNV_PRIME;
	return h;
}

// P067 EXPERIMENT (branch patch/defaults-on): the ADOPTED patch recipe becomes
// the BUILT-IN DEFAULT - a plain launch (no env vars at all) runs the full
// adopted link-play recipe.  Uniform resolution for every ADOPTED patch env
// var (the same helper is duplicated in namcos23.cpp for the driver-side
// reads; keep the two copies identical):
//   env UNSET (or empty) -> the baked-in adopted default value (defval);
//                           the one-shot banner reports source "(default)"
//   env == literal "0"   -> DISABLED, the kill switch: the SAME inert code
//                           path that "unset" selected before P067
//   env == anything else -> that value, parsed exactly as before;
//                           the banner reports source "(env=<value>)"
// Returns the EFFECTIVE value string (defval or the env value), or nullptr
// when killed via "0".  from_env reports where the value came from.
// NON-adopted vars (LINK_WAIT, TCP_NODELAY, NEWEST_WINS, HB_PHASE_AWARE,
// TX_COMPLETE_IRQ, CORR_SMOOTH+knobs, every retired patch, and ALL
// NAMCOS23_TRACE_*) keep the old "armed only when set" idiom - do NOT route
// them through this helper.  MODEL PROVENANCE: Fable 5.
static char const *patch_env_or_default(char const *name, char const *defval, bool &from_env)
{
	char const *const env = std::getenv(name);
	from_env = env && env[0] != '\0';
	if (!from_env)
		return defval;                              // unset/empty -> adopted default
	if (env[0] == '0' && env[1] == '\0')
		return nullptr;                             // literal "0" -> kill switch
	return env;                                     // env override, parsed as before
}

// Banner source tag for the P067 defaults: "(default)" or "(env=<value>)".
static std::string patch_env_src(bool from_env, char const *effective)
{
	if (!from_env)
		return std::string("(default)");
	return std::string("(env=") + (effective ? effective : "0") + ")";
}

} // anonymous namespace


//**************************************************************************
//  NETWORK CONTEXT
//
//  Inner class hosting the asio io_context, listening/connecting sockets,
//  and the dedicated worker thread that runs io_context::run().  The MAME
//  emulation thread interacts with this object only via std::atomic state
//  and m_ioctx.post() lambdas, never by touching asio objects directly.
//**************************************************************************

class namco_c139_device::context
{
public:
	context(namco_c139_device &device,
			std::optional<asio::ip::tcp::endpoint> const &local,
			std::optional<asio::ip::tcp::endpoint> const &remote)
		: m_device(device)
		, m_acceptor(m_ioctx)
		, m_socket(m_ioctx)
		, m_local(local)
		, m_remote(remote)
		, m_stopping(false)
		, m_connected(false)
	{
	}

	std::error_code start()
	{
		std::error_code err;

		if (m_local)
		{
			m_acceptor.open(m_local->protocol(), err);
			if (!err)
				m_acceptor.set_option(asio::socket_base::reuse_address(true), err);
			if (!err)
				m_acceptor.bind(*m_local, err);
			if (!err)
				m_acceptor.listen(1, err);
			if (err)
				return err;
		}

		m_thread = std::thread(
				[this] ()
				{
					if (m_local)
					{
						m_acceptor.async_accept(m_socket,
								[this] (std::error_code const &acc_err)
								{
									if (m_stopping.load(std::memory_order_acquire))
										return;
									if (acc_err)
									{
										m_device.logerror(
												"namco_c139: accept failed: %s\n",
												acc_err.message().c_str());
										return;
									}
									m_device.logerror("namco_c139: peer connected (incoming)\n");
									m_connected.store(true, std::memory_order_release);
									start_read();
								});
					}

					if (m_remote)
					{
						m_socket.async_connect(*m_remote,
								[this] (std::error_code const &con_err)
								{
									if (m_stopping.load(std::memory_order_acquire))
										return;
									if (con_err)
									{
										m_device.logerror(
												"namco_c139: connect to %s:%u failed: %s\n",
												m_remote->address().to_string().c_str(),
												m_remote->port(),
												con_err.message().c_str());
										return;
									}
									m_device.logerror(
											"namco_c139: connected to peer at %s:%u\n",
											m_remote->address().to_string().c_str(),
											m_remote->port());
									m_connected.store(true, std::memory_order_release);
									start_read();
								});
					}

					m_ioctx.run();
				});

		return {};
	}

	void stop()
	{
		asio::post(m_ioctx,
				[this] ()
				{
					m_stopping.store(true, std::memory_order_release);
					std::error_code err;
					if (m_acceptor.is_open())
						m_acceptor.close(err);
					if (m_socket.is_open())
						m_socket.close(err);
				});
		if (m_thread.joinable())
			m_thread.join();
	}

	bool connected() const { return m_connected.load(std::memory_order_acquire); }

	// Called from emulation thread.  Posts the buffer to the network thread
	// and triggers an async_write chain if one is not already in flight.
	void send_frame(std::vector<uint8_t> data)
	{
		asio::post(m_ioctx,
				[this, payload = std::move(data)] () mutable
				{
					if (m_stopping.load(std::memory_order_acquire))
						return;
					if (!m_socket.is_open())
						return;

					bool const idle = m_outbound.empty();
					m_outbound.push_back(std::move(payload));
					if (idle)
						start_write();
				});
	}

private:
	// Network-thread only: pop the front of m_outbound and async_write it.
	// On completion, chain to the next entry if any.
	void start_write()
	{
		auto &front = m_outbound.front();
		asio::async_write(m_socket, asio::buffer(front),
				[this] (std::error_code const &err, std::size_t /*bytes*/)
				{
					if (m_stopping.load(std::memory_order_acquire))
						return;
					if (err)
					{
						m_device.logerror(
								"namco_c139: tx write failed: %s\n",
								err.message().c_str());
						return;
					}
					m_outbound.pop_front();
					if (!m_outbound.empty())
						start_write();
				});
	}

	// Network-thread only: read a 2-byte big-endian size header followed by
	// the corresponding payload, then log "received frame N bytes" and chain
	// to the next read.  Phase 4 will replace the log with a synchronizer
	// that delivers the frame onto the emulation thread.
	void start_read()
	{
		asio::async_read(m_socket, asio::buffer(m_rx_size_bytes),
				[this] (std::error_code const &err, std::size_t /*bytes*/)
				{
					if (m_stopping.load(std::memory_order_acquire))
						return;
					if (err)
					{
						m_device.logerror(
								"namco_c139: rx size read failed: %s\n",
								err.message().c_str());
						return;
					}

					uint16_t const size = (uint16_t(m_rx_size_bytes[0]) << 8)
										|  uint16_t(m_rx_size_bytes[1]);

					// P002 EXPERIMENT (branch patch/vblank-lockstep): size
					// prefix with bit 15 set = link-layer control frame (see
					// the LOCKSTEP_CTRL_* constants).  Parsed here on the
					// network thread and consumed - control payloads never
					// reach the game RX queue or shared RAM.  Handled
					// unconditionally (whether or not WE are armed) so a
					// token-emitting peer can't desync our stream.
					if (size & LOCKSTEP_CTRL_SIZE_FLAG)
					{
						uint16_t const ctl_size = uint16_t(size & ~LOCKSTEP_CTRL_SIZE_FLAG);
						if (ctl_size == 0 || ctl_size > LOCKSTEP_CTRL_MAX_PAYLOAD)
						{
							m_device.logerror(
									"namco_c139: invalid rx control frame size %u; closing\n",
									ctl_size);
							std::error_code close_err;
							m_socket.close(close_err);
							return;
						}
						m_rx_payload.resize(ctl_size);
						asio::async_read(m_socket, asio::buffer(m_rx_payload),
								[this, ctl_size] (std::error_code const &err2, std::size_t /*bytes2*/)
								{
									if (m_stopping.load(std::memory_order_acquire))
										return;
									if (err2)
									{
										m_device.logerror(
												"namco_c139: rx control payload read failed: %s\n",
												err2.message().c_str());
										return;
									}
									if (ctl_size >= 5 && m_rx_payload[0] == LOCKSTEP_CTRL_TYPE_TOKEN)
									{
										uint32_t const token = (uint32_t(m_rx_payload[1]) << 24)
															 | (uint32_t(m_rx_payload[2]) << 16)
															 | (uint32_t(m_rx_payload[3]) << 8)
															 |  uint32_t(m_rx_payload[4]);
										m_device.lockstep_token_received(token);
									}
									m_rx_payload.clear();
									start_read();
								});
						return;
					}

					if (size == 0 || size > 0x4000)
					{
						m_device.logerror(
								"namco_c139: invalid rx frame size %u; closing\n",
								size);
						std::error_code close_err;
						m_socket.close(close_err);
						return;
					}

					m_rx_payload.resize(size);
					asio::async_read(m_socket, asio::buffer(m_rx_payload),
							[this, size] (std::error_code const &err2, std::size_t /*bytes2*/)
							{
								if (m_stopping.load(std::memory_order_acquire))
									return;
								if (err2)
								{
									m_device.logerror(
											"namco_c139: rx payload read failed: %s\n",
											err2.message().c_str());
									return;
								}
								// Hand the buffer to the emulation thread which
								// will write it into shared RAM, set the RX flag
								// bits in the status reg, and raise the IRQ.
								on_frame_received(std::move(m_rx_payload));
								m_rx_payload.clear();
								start_read();
							});
				});
	}

	namco_c139_device &m_device;
	asio::io_context m_ioctx;
	asio::ip::tcp::acceptor m_acceptor;
	asio::ip::tcp::socket m_socket;
	std::optional<asio::ip::tcp::endpoint> m_local;
	std::optional<asio::ip::tcp::endpoint> m_remote;
	std::atomic<bool> m_stopping;
	std::atomic<bool> m_connected;
	std::thread m_thread;

	// Outbound TX queue (network thread accesses).  m_outbound.front() is
	// the in-flight async_write; subsequent entries chain on completion.
	std::deque<std::vector<uint8_t>> m_outbound;

	// Inbound RX scratch buffers (network thread accesses).
	std::array<uint8_t, 2> m_rx_size_bytes;
	std::vector<uint8_t>   m_rx_payload;

	// Inbound queue of fully-received frames waiting to be delivered to
	// the emulation thread.  Network thread pushes under m_inbound_mutex,
	// emulation thread drains via drain_rx() in deliver_rx_frames().
	std::mutex                       m_inbound_mutex;
	std::deque<std::vector<uint8_t>> m_inbound;

	// Called from the network thread when a complete frame arrives.
	// Hands the buffer to the inbound queue.  The emulation thread will
	// drain the queue lazily on its next C139 register access (reg_r,
	// reg_w, status_r) - the game polls these continuously during the
	// link busy-wait so latency is microseconds.  We deliberately do NOT
	// call machine().scheduler().synchronize() here: that API is intended
	// for in-emulator timers, not foreign-thread wakeups, and on this
	// driver it caused the listener instance to grind to a halt.
	void on_frame_received(std::vector<uint8_t> data)
	{
		std::lock_guard<std::mutex> lock(m_inbound_mutex);
		m_inbound.push_back(std::move(data));
	}

public:
	// Emulation-thread accessor: atomically drain the inbound queue.
	std::deque<std::vector<uint8_t>> drain_rx()
	{
		std::lock_guard<std::mutex> lock(m_inbound_mutex);
		return std::move(m_inbound);
	}
};


bool namco_c139_device::comm_connected() const
{
	return m_context && m_context->connected();
}


// P068 (branch patch/linkplay-menu): the endpoint parser used to be a lambda
// inside start_comm(); hoisted to a file-static helper (body unchanged) so the
// deferred cfg/default bring-up path (start_comm_cfg) validates hosts/ports
// through the exact same code.  MODEL PROVENANCE: Fable 5.
static std::optional<asio::ip::tcp::endpoint> parse_comm_endpoint(
		device_t &dev, char const *host, char const *port, char const *what)
{
	if (!host || !*host)  return std::nullopt;
	if (!port || !*port)  return std::nullopt;

	std::error_code err;
	auto addr = asio::ip::make_address(host, err);
	if (err)
	{
		dev.logerror("namco_c139: invalid %s '%s': %s\n",
				what, host, err.message().c_str());
		return std::nullopt;
	}

	char *end = nullptr;
	unsigned long port_num = std::strtoul(port, &end, 10);
	if (!end || *end != '\0' || port_num == 0 || port_num > 65535)
	{
		dev.logerror("namco_c139: invalid %s port '%s'\n", what, port);
		return std::nullopt;
	}

	return asio::ip::tcp::endpoint(addr, static_cast<unsigned short>(port_num));
}


void namco_c139_device::start_comm()
{
	auto const &opts = mconfig().options();
	char const *local_host  = opts.comm_localhost();
	char const *local_port  = opts.comm_localport();
	char const *remote_host = opts.comm_remotehost();
	char const *remote_port = opts.comm_remoteport();

	// MAME's emu_options ship with non-empty defaults for these:
	//   comm_localhost  = "0.0.0.0"
	//   comm_localport  = "15112"
	//   comm_remotehost = "127.0.0.1"
	//   comm_remoteport = "15112"
	// If we see the full default tuple, treat that as "not configured"
	// rather than as an instruction to listen on / connect to localhost
	// (which would have us connecting to ourselves).  To opt in, the
	// user must change at least one of the four; to disable a side
	// while overriding only the other, pass an empty host string, e.g.
	//   listener:  -comm_localport 9876 -comm_remotehost ""
	//   connector: -comm_remotehost 127.0.0.1 -comm_remoteport 9876 -comm_localhost ""
	bool const at_defaults =
			local_host  && std::strcmp(local_host,  "0.0.0.0")   == 0 &&
			local_port  && std::strcmp(local_port,  "15112")     == 0 &&
			remote_host && std::strcmp(remote_host, "127.0.0.1") == 0 &&
			remote_port && std::strcmp(remote_port, "15112")     == 0;
	if (at_defaults)
	{
		// P068 (branch patch/linkplay-menu): the CLI didn't configure the link.
		// Pre-P068 this meant unconditional solo; now DEFER the decision to the
		// per-machine cfg / built-in loopback defaults, resolved in
		// start_comm_cfg() at config FINAL time - device_start runs BEFORE the
		// cfg file (and the DIP / machine-id values it carries) is loaded, so
		// nothing can be decided here.  DIP:5 off still means solo (checked
		// there).  MODEL PROVENANCE: Fable 5.
		m_lp_comm_deferred = true;
		return;
	}

	// P068: at least one -comm_* option was changed from MAME defaults =>
	// explicit CLI configuration.  This is the pre-P068 path, byte-identical:
	// the launch scripts (launch_link.ps1 / launch_lan.ps1) keep working
	// unchanged, and the CLI WINS over any cfg-stored menu settings.
	m_lp_cli_active = true;

	auto local  = parse_comm_endpoint(*this, local_host,  local_port,  "comm_localhost");
	auto remote = parse_comm_endpoint(*this, remote_host, remote_port, "comm_remotehost");

	// P055 (branch patch/boat-jitter-trace): remember whether this instance is
	// the CONNECTOR (blue/follower - connects out to a remote, no local listener)
	// vs the LISTENER (red/leader).  The launcher gives the connector
	// -comm_remotehost with -comm_localhost "" and the listener -comm_localport
	// with -comm_remotehost "".  READ-ONLY role tag consumed by the driver's
	// boat-jitter trace to emit only on the ingest side.  Experiment branch only.
	m_comm_is_connector = remote.has_value() && !local.has_value();

	if (!local && !remote)
	{
		// No comm configured; remain in solo mode.  Game still falls back
		// gracefully to single-cabinet play when the link probe times out.
		return;
	}

	m_context = std::make_unique<context>(*this, local, remote);
	auto const err = m_context->start();
	if (err)
	{
		logerror("namco_c139: failed to start network: %s\n",
				err.message().c_str());
		m_context.reset();
		return;
	}

	logerror("namco_c139: network started%s%s\n",
			local  ? " (listening)"  : "",
			remote ? " (connecting)" : "");
}


// P068 (branch patch/linkplay-menu): per-machine cfg <linkplay> node +
// deferred cfg/default comm bring-up.  MODEL PROVENANCE: Fable 5.
//
// Schema (cfg\<system>.cfg, next to the DIP/input state that already lives
// there - per working directory, which is per instance in all our setups):
//   <linkplay listen_host="0.0.0.0" listen_port="9876"
//             connect_host="127.0.0.1" connect_port="9876" />
//
// Load order (configuration_manager::load_settings, which runs AFTER
// device_start): INIT -> default.cfg (DEFAULT) -> <system>.cfg (SYSTEM) ->
// FINAL.  We only consume the SYSTEM node; FINAL - which fires exactly once,
// after the ioport manager has restored the DIP and "Link ID" machine-config
// values from the same file - is where the deferred bring-up runs.

void namco_c139_device::linkplay_config_load(config_type cfg_type, config_level cfg_level, util::xml::data_node const *parentnode)
{
	if (cfg_type == config_type::SYSTEM && parentnode)
	{
		char const *const lhost = parentnode->get_attribute_string("listen_host", nullptr);
		if (lhost && *lhost)
			m_lp_listen_host = lhost;
		char const *const rhost = parentnode->get_attribute_string("connect_host", nullptr);
		if (rhost && *rhost)
			m_lp_connect_host = rhost;
		long long const lport = parentnode->get_attribute_int("listen_port", m_lp_listen_port);
		if (lport >= 1 && lport <= 65535)
			m_lp_listen_port = uint16_t(lport);
		long long const rport = parentnode->get_attribute_int("connect_port", m_lp_connect_port);
		if (rport >= 1 && rport <= 65535)
			m_lp_connect_port = uint16_t(rport);
	}

	if (cfg_type == config_type::FINAL)
		start_comm_cfg();
}


void namco_c139_device::linkplay_config_save(config_type cfg_type, util::xml::data_node *parentnode)
{
	// system-specific cfg only (per-instance), matching where the values load from
	if (cfg_type != config_type::SYSTEM)
		return;

	parentnode->set_attribute("listen_host", m_lp_listen_host.c_str());
	parentnode->set_attribute_int("listen_port", m_lp_listen_port);
	parentnode->set_attribute("connect_host", m_lp_connect_host.c_str());
	parentnode->set_attribute_int("connect_port", m_lp_connect_port);
}


// The timecrs2-family "Link ID" machine configuration (namcos23.cpp
// INPUT_PORTS(timecrs2): port JVS_PLAYER1, PORT_CONFNAME mask 0x00004000,
// 0x0000 = Left/Red, 0x4000 = Right/Blue).  Returns nullptr on machines that
// carry a C139 but no red/blue link identity (e.g. other s23 games, where
// mask 0x4000 is a live button, NOT a config field) - the cfg/default
// auto-link path only applies where this field exists.
ioport_field *namco_c139_device::lp_link_id_field() const
{
	ioport_port *const port = machine().root_device().ioport("JVS_PLAYER1");
	if (!port)
		return nullptr;
	ioport_field *const field = port->field(0x00004000);
	return (field && field->type() == IPT_CONFIG) ? field : nullptr;
}


bool namco_c139_device::lp_role_is_connector() const
{
	ioport_field *const field = lp_link_id_field();
	return field && (field->port().read() & 0x00004000); // 0x4000 = Right/Blue = connector
}


// Deferred comm bring-up for the cfg / built-in-loopback-defaults path.
// Runs once, at config FINAL (see linkplay_config_load), and only when
// start_comm() saw the all-MAME-defaults -comm_* tuple (i.e. the CLI did not
// configure the link - CLI keeps absolute priority).  Resolution here:
//   - machines without the "Link ID" machine configuration: not link-capable
//     via cfg - stay solo (the pre-P068 behavior for them, CLI still works);
//   - link DIP (DIP:5, "Link Play Enabled") OFF: solo, no socket at all (the
//     P000 foundation off switch, unchanged);
//   - machine-id Left/Red:   LISTENER, binds  listen_host:listen_port;
//   - machine-id Right/Blue: CONNECTOR, dials connect_host:connect_port.
// Values consumed once per launch; menu edits apply on the NEXT launch.
void namco_c139_device::start_comm_cfg()
{
	if (!m_lp_comm_deferred || m_context)
		return;

	ioport_field *const idfield = lp_link_id_field();
	if (!idfield)
		return;

	ioport_port *const dsw = machine().root_device().ioport("DSW");
	if (!dsw || (dsw->read() & 0x08))
	{
		logerror("namco_c139: link DIP (DIP:5 'Link Play Enabled') is OFF - solo mode, comm not started\n");
		return;
	}

	bool const connector = lp_role_is_connector();
	std::string const &host = connector ? m_lp_connect_host : m_lp_listen_host;
	std::string const portstr = std::to_string(connector ? m_lp_connect_port : m_lp_listen_port);
	auto ep = parse_comm_endpoint(*this, host.c_str(), portstr.c_str(),
			connector ? "linkplay connect_host" : "linkplay listen_host");
	if (!ep)
	{
		logerror("namco_c139: linkplay cfg %s '%s:%s' unusable - solo mode\n",
				connector ? "connect endpoint" : "listen endpoint", host.c_str(), portstr.c_str());
		return;
	}

	std::optional<asio::ip::tcp::endpoint> local, remote;
	if (connector)
		remote = ep;
	else
		local = ep;
	m_comm_is_connector = connector;

	m_context = std::make_unique<context>(*this, local, remote);
	auto const err = m_context->start();
	if (err)
	{
		logerror("namco_c139: failed to start network (linkplay cfg): %s\n",
				err.message().c_str());
		m_context.reset();
		return;
	}

	logerror("namco_c139: network started via linkplay cfg/defaults: machine-id %s => %s %s:%s\n",
			connector ? "Right/Blue" : "Left/Red",
			connector ? "connecting to" : "listening on",
			host.c_str(), portstr.c_str());
}


void namco_c139_device::device_stop()
{
	if (m_context)
	{
		m_context->stop();
		m_context.reset();
	}
}


// Called from the emulation thread when the TX-Control register's bit 0
// transitions 0 -> 1.  Reads the staged TX frame out of the shared RAM,
// frames it as [size_be:2 bytes][payload:N bytes], and hands it to the
// network thread for delivery.  No-op when no peer is connected.
//
// PROTOCOL NOTE (Phase 9 fix): m_regs[5] (REG_5_TXSIZE per SailorSat's
// C139 PR #13421) holds the TX size in HALFWORDS (= word units), not
// bytes.  Each halfword in the C422 shared RAM is transmitted as 2
// bytes on the wire (big-endian: high byte first, then low byte).  So
// the wire-byte count is 2 * m_regs[5].
//
// Empirical evidence from timecrs2 ROM disassembly (full.txt):
//   - The TX-side packet builder at 0x8000C094..0x8000C2A4 writes a
//     250-halfword packet to C422 RAM with checksum designed so that
//     the receiver's validator at 0x8000BFCC sees sum mod 256 == 0
//     (= "sum=0 / heartbeat" packet that triggers the dispatcher at
//     0x8000B8F0, which is what resets the link timeout).
//   - The packet runs from word 0 (magic 0x5A) at script_pos+2 through
//     word 249 (the marker word with bit 8 of high byte set, low byte
//     = size_lo) at script_pos+251.
//   - That's 250 halfwords = 500 bytes on the wire, with m_regs[5]
//     written as 250 (= 0xFA).
//   - SailorSat's PR confirms this same word-count semantics for C139:
//     "tx_size = m_reg[REG_5_TXSIZE]; for (j=0; j<tx_size; j++) { ... 2 bytes per halfword ... }"
//
// Previous behaviour (sending m_regs[5] BYTES) was sending half the
// packet, truncating before the marker and checksum.  The validator
// would scan past the truncated end into stale RAM, sum random data,
// land on sum != 0 (success path), and the dispatcher (which we needed
// for timeout reset and gp+0x77C8/D0/D4 updates) never fired.

// P038 EXPERIMENT (branch patch/latch-v3-dedupe): record a wire completion
// of a latched bulk class into the dedupe ring.  Called at exactly two
// sites, both on the emulation thread: the consumed_ok retirement (the
// ROM's own head send won its race - the NATURAL completion class the P036
// analysis time-joined the stale duplicates against) and the latch dispatch
// itself (a dispatch is equally a wire completion; recording it means an
// immediately-following wiped re-announce of the same chain dedupes instead
// of double-dispatching - the P036 "triples" case).  Deduped consumptions
// are deliberately NOT recorded: nothing went on the wire, and a phantom
// entry could suppress a later genuine rescue.  Fixed-size ring,
// overwrite-oldest, no allocation.
void namco_c139_device::al_dedupe_record(uint16_t offset, uint32_t expected_hw)
{
	m_al_completes[m_al_comp_idx].offset = offset;
	m_al_completes[m_al_comp_idx].expected_hw = expected_hw;
	m_al_completes[m_al_comp_idx].t = machine().time();
	m_al_completes[m_al_comp_idx].valid = true;
	m_al_comp_idx = uint8_t((m_al_comp_idx + 1) % AL_DEDUPE_RING);
}

// P050 (branch patch/single-burst-pump): is `payload` a COMPLETE-STANDALONE VM
// frame?  The ROM's strict trailer invariant (same test the RX restamp sampler
// and the TX completion check use): the final halfword carries the bit-8 end
// marker AND the trailer's claimed length equals the frame's own halfword
// count.  A multi-chunk message FRAGMENT never satisfies this - an intermediate
// chunk has no trailer (any coincidental bit-8 fails the length equality) and a
// last chunk's claimed length is the whole-message total, not the chunk's own
// size - so keying gate (b) supersession + the FIFO-join stamps on this test
// leaves CHUNK_PASSTHRU ring reassembly structurally untouched.  Wire byte
// order: payload[2k]=halfword high byte, payload[2k+1]=low byte.
static bool nw_frame_complete(const uint8_t *payload, std::size_t bytes)
{
	if (bytes < 8 || (bytes & 1))                 // need >= 4 halfwords, even byte count
		return false;
	std::size_t const words = bytes / 2;
	uint16_t const endhw = uint16_t((payload[bytes - 2] << 8) | payload[bytes - 1]);
	uint32_t const claimed = (uint32_t(payload[bytes - 3]) << 8) | (endhw & 0xff);
	return (endhw & 0x100) && claimed == words;
}

// P050 (branch patch/single-burst-pump): the ROM per-frame drift SEQUENCE (the
// FIFO-join key).  0x8000B8F0-914 reconstructs the remote seq from the LOW
// bytes of halfwords 0 and 1 (= cellseq) and subtracts it from local gp+0x75B8;
// it increments once per composed frame, so it uniquely tags a frame across the
// wire for red-tx-join -> blue-rx-join latency measurement.
static uint16_t nw_frame_seq(const uint8_t *payload, std::size_t bytes)
{
	if (bytes < 4)
		return 0;
	return uint16_t((uint16_t(payload[1]) << 8) | payload[3]);
}

void namco_c139_device::send_pending_tx_frame()
{
	if (!m_context || !m_context->connected())
		return;

	// P031 (branch patch/announce-latch): no longer const - the announce-latch
	// dispatch below may substitute the rx_clear-wiped staged size at the
	// zero-size abort site.  al_dispatch marks that path so the register file
	// is provably never written by it.
	uint16_t frame_size_words = m_regs[5];   // HALFWORD count
	bool al_dispatch = false;
	// P035 (branch patch/latch-v2-snapshot): true when this send is an
	// announce-latch v2 dispatch whose payload comes from the WIPE-TIME
	// snapshot instead of a dispatch-time C422 RAM re-read (the read the
	// P033 boss flood proved torn on ~32% of flood-window dispatches).
	bool al_snap_use = false;

	// P011: drop a held bulk message whose continuation never came BEFORE
	// deciding how to read this chunk - so a stale hold can never leave
	// fifo_ptr pointing at the abandoned message's resume offset.  (The
	// per-vblank sweep in vblank_tick() is the primary staleness path; this is
	// the in-send backstop for the case where the next chunk arrives just past
	// the timeout.)
	if (m_chunk_armed && !m_chunk_accum.empty()
			&& machine().time() - m_chunk_accum_since > attotime::from_msec(CHUNK_STALE_MS))
		chunk_drop("stale-timeout");

	// P010/P011 EXPERIMENT: the ROM's chunk emitter writes TXOFFSET only for
	// the FIRST chunk of a message (0x8000BB6C PATH C); continuation chunks
	// (PATH B/B') rely on the chip auto-advancing its DMA pointer and never
	// rewrite TXOFFSET.  m_regs[7] never advances in this device, so
	// continuation chunks used to re-send the message HEAD - the true tail
	// (checksum + length trailer + bit-8 marker) never reached the wire.  When
	// armed, read from the auto-advancing internal pointer instead (latched on
	// every TXOFFSET write in reg_w(), advanced here per transmitted halfword).
	//
	// P011: when a bulk message is held and NO TXOFFSET was reprogrammed since
	// the previous send, this send is one of the ROM's PATH B/B' continuation
	// chunks - read it from the held message's resume pointer (where the last
	// appended chunk ended), NOT from m_chunk_tx_ptr (which an interleaved
	// message's TXOFFSET write could have dragged elsewhere).  This makes the
	// continuation read the correct bytes even under interleaving, and is the
	// faithful model of the ROM (a continuation is a TXSIZE staged without a
	// preceding TXOFFSET reprogram).
	bool const is_continuation = m_chunk_armed
			&& !m_chunk_accum.empty()
			&& !m_chunk_saw_txoffset;
	uint16_t const fifo_ptr =
			  is_continuation                          ? m_chunk_resume_ptr
			: (m_chunk_armed && m_chunk_tx_ptr_valid)  ? m_chunk_tx_ptr
			:                                            m_regs[7];

	bool const had_txoffset_reprogram = m_chunk_saw_txoffset;

	// A zero-size trigger (the ROM's PATH D idle tick stages TXSIZE=0 and does
	// NOT write TXOFFSET) is a no-op: do not consume the TXOFFSET-reprogram
	// edge on it, or a real chunk that the host staged on a later edge would
	// be mis-classified as a continuation.
	//
	// P031 (branch patch/announce-latch): THIS IS THE VERIFIED ABORT SITE of
	// the stage->START rx_clear race that kills every room/area skip
	// (P029/P030 analysis SD): the ROM stages the bulk head's TXSIZE, a
	// pending peer frame is delivered at the top of the reg_w that carries
	// the START edge and wipes m_regs[5], the edge still fires this function
	// - and it used to return right here on size 0, silently sending
	// NOTHING.  The ROM (whose PATH A busy byte reads 0 either way) then
	// stages the remainder into dead air, re-announces the class 3-6x and
	// abandons it ROM-finally.  When the announce latch is live, WIPED since
	// its announce, inside its TTL, and the modeled DMA pointer still sits
	// at the latched offset, proceed with the latched size instead: the
	// payload is read from the ROM-staged C422 RAM below exactly as the
	// un-wiped send would have read it (byte-identical wire content), the
	// tracker state (accumulator / resume pointer / saw_txoffset
	// consumption) is populated identically, and the remainder's P014
	// txsize_commit trigger then fires as on any normal bulk.  One-shot: the
	// latch is consumed here, so an announce can never dispatch twice.
	// NOTHING is written to the register file on this path (m_regs[5] stays
	// 0 - see the guarded clear below): the ROM's rx_clear green light was
	// honored and stays honored - structurally not the P025/P027 fight.
	if (frame_size_words == 0)
	{
		if (m_al_armed && m_al_valid && m_al_wiped)
		{
			attotime const al_age = machine().time() - m_al_time;
			unsigned const al_age_ms = unsigned(al_age.as_double() * 1000.0 + 0.5);
			if (al_age > attotime::from_msec(AL_TTL_MS))
			{
				++m_al_expired;
				m_al_valid = false;
				m_al_wiped = false;
				logerror("announce-latch: expired t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u ttl_ms=%d expired=%u (START edge arrived past TTL - stale latch dropped, stock zero-size abort)\n",
						machine().time().as_double(), m_al_offset, m_al_wiped_hw,
						m_al_expected_hw, al_age_ms, AL_TTL_MS, m_al_expired);
				return;
			}
			if (m_chunk_tx_ptr_valid && m_chunk_tx_ptr == m_al_offset)
			{
				// P038 (branch patch/latch-v3-dedupe): DISPATCH DEDUPE - the
				// P036-measured stale-duplicate kill.  Every v1/v2 rail above
				// passed (live+wiped latch, inside TTL, DMA pointer at the
				// latched offset), so v2 would dispatch RIGHT NOW.  If this
				// (offset, expected_hw) class already completed on the wire
				// within AL_DEDUPE_LOOKBACK_MS of the latch's announce anchor,
				// the wiped stage was the ROM's heartbeat-cadence RE-SEND of
				// content the peer already ingested - the rx_clear wipe was
				// flow control, not loss - and dispatching it is exactly what
				// produced ALL 56 torn completes (56/56 timestamp join;
				// natural sends tore 0/239) and ~180 byte-identical +150 ms
				// duplicates in the P036 run.  Consume the latch WITHOUT
				// sending (one-shot, snapshot dies with it) and return - the
				// same nothing-sent register-file-untouched shape as the
				// stock zero-size abort.  A genuine rescue (no recent
				// same-key completion - the P031 room-1 save) falls through
				// and dispatches exactly as v2.  See AL_DEDUPE_LOOKBACK_MS
				// for why the window reaches BEFORE the announce.
				if (m_al_dedupe)
				{
					int best = -1;
					for (unsigned i = 0; i < AL_DEDUPE_RING; i++)
					{
						al_complete_rec const &rec = m_al_completes[i];
						if (rec.valid && rec.offset == m_al_offset
								&& rec.expected_hw == m_al_expected_hw
								&& rec.t + attotime::from_msec(AL_DEDUPE_LOOKBACK_MS) >= m_al_time
								&& (best < 0 || rec.t > m_al_completes[best].t))
							best = int(i);
					}
					if (best >= 0)
					{
						attotime const comp_t = m_al_completes[best].t;
						unsigned const comp_age_ms = unsigned((machine().time() - comp_t).as_double() * 1000.0 + 0.5);
						m_al_valid = false;        // consumed like a dispatch (one-shot) - but nothing is sent
						m_al_wiped = false;
						m_al_snap_valid = false;   // the wipe-time snapshot dies with its latch
						++m_al_deduped;
						logerror("announce-latch: deduped t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u complete_t=%.6f complete_age_ms=%u deduped=%u (START edge found regs[5]==0-because-wiped, but this (offset,expected_hw) class already completed on the wire within %d ms of the announce - the wiped stage was the ROM's cadence re-send of delivered content and the peer's rx_clear was flow control, not loss; dispatching would re-send it stale, the P036 torn/duplicate class - latch consumed, nothing sent, register file untouched)\n",
								machine().time().as_double(), m_al_offset, m_al_wiped_hw,
								m_al_expected_hw, al_age_ms, comp_t.as_double(),
								comp_age_ms, m_al_deduped, AL_DEDUPE_LOOKBACK_MS);
						return;
					}
				}
				frame_size_words = m_al_wiped_hw;
				al_dispatch = true;
				// P035 (branch patch/latch-v2-snapshot): under v2 the payload
				// for this dispatch is the snapshot copied at WIPE-capture time
				// (when the staged bytes were provably pristine - the ROM had
				// just staged them and the wipe hit only the register).  The
				// (offset,size) equality below always holds at a genuine
				// dispatch - m_al_snap_offset/m_al_snap.size() were recorded
				// from the SAME m_al_offset/m_al_wiped_hw this dispatch just
				// consumed, and no site mutates them in between (any arm/
				// refresh clears m_al_wiped first, forcing a fresh capture) -
				// so the reread fallback is a defensive rail, expected 0.
				al_snap_use = m_al_snapshot && m_al_snap_valid
						&& m_al_snap_offset == m_al_offset
						&& m_al_snap.size() == std::size_t(frame_size_words) * 2;
				m_al_valid = false;   // one-shot: consumed now, never dispatchable twice
				m_al_wiped = false;
				++m_al_dispatched;
				// P038 (branch patch/latch-v3-dedupe): a latch dispatch is
				// itself a wire completion of this (offset, expected_hw) -
				// record it so an immediately-following wiped re-announce of
				// the same chain dedupes instead of double-dispatching.
				if (m_al_dedupe)
					al_dedupe_record(m_al_offset, m_al_expected_hw);
				if (al_snap_use)
				{
					m_al_snap_valid = false;   // one-shot with its latch: a snapshot never transmits twice
					++m_al_snap_dispatched;
					// P037 (branch patch/latch-genstamp): GENERATION-STAMP compare,
					// READ-ONLY.  Ask what a dispatch-time RAM re-read would have
					// fetched for the head's first 2 halfwords and compare against
					// the snapshot: gen=differ means the ROM has ALREADY recomposed
					// the ring slot by this START edge - the P035 residual's
					// head/remainder generation-skew hypothesis made measurable.
					// The transmitted bytes remain the snapshot, untouched.  The
					// scratch armed below lets the P014 remainder commit repeat
					// the compare at ITS stage instant (the skew's other end).
					uint16_t const gen_cur_hw0 = m_ram[m_al_offset & 0x1fff];
					uint16_t const gen_cur_hw1 = m_ram[uint16_t(m_al_offset + 1) & 0x1fff];
					bool const gen_two_hw = m_al_snap.size() >= 4;
					uint16_t const gen_snap_hw0 = uint16_t((uint16_t(m_al_snap[0]) << 8) | m_al_snap[1]);
					uint16_t const gen_snap_hw1 = gen_two_hw
							? uint16_t((uint16_t(m_al_snap[2]) << 8) | m_al_snap[3]) : 0;
					bool const gen_differ = (gen_cur_hw0 != gen_snap_hw0)
							|| (gen_two_hw && gen_cur_hw1 != gen_snap_hw1);
					if (gen_differ)
						++m_al_gen_differ;
					m_al_gen_head_live = true;
					m_al_gen_head_offset = m_al_offset;
					m_al_gen_head_hw0 = gen_snap_hw0;
					m_al_gen_head_hw1 = gen_snap_hw1;
					m_al_gen_head_hws = gen_two_hw ? 2 : 1;
					m_al_gen_head_time = m_al_time;   // announce anchor: remainder head_age_ms is comparable to this dispatch's age_ms
					if (gen_differ)
						logerror("announce-latch: dispatched-snap t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u dispatched=%u snap_tx=%u snap_bytes=%u gen=differ cur_hw0=%04x cur_hw1=%04x snap_hw0=%04x snap_hw1=%04x gen_differ=%u (START edge found regs[5]==0-because-wiped; send transmits the WIPE-TIME payload snapshot - no dispatch-time RAM re-read; the ring slot ALREADY differs from the snapshot at this instant: the ROM recomposed it since the wipe)\n",
								machine().time().as_double(), m_al_offset, frame_size_words,
								m_al_expected_hw, al_age_ms, m_al_dispatched,
								m_al_snap_dispatched, unsigned(m_al_snap.size()),
								gen_cur_hw0, gen_cur_hw1, gen_snap_hw0, gen_snap_hw1,
								m_al_gen_differ);
					else
						logerror("announce-latch: dispatched-snap t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u dispatched=%u snap_tx=%u snap_bytes=%u gen=same (START edge found regs[5]==0-because-wiped; send transmits the WIPE-TIME payload snapshot - no dispatch-time RAM re-read, the ring slot may already be recomposing)\n",
								machine().time().as_double(), m_al_offset, frame_size_words,
								m_al_expected_hw, al_age_ms, m_al_dispatched,
								m_al_snap_dispatched, unsigned(m_al_snap.size()));
				}
				else if (m_al_snapshot)
				{
					++m_al_snap_fallback;
					logerror("announce-latch: dispatched-reread t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u dispatched=%u snap_fallback=%u snap_valid=%d snap_offset=0x%04x snap_bytes=%u (v2 armed but the snapshot does not match this dispatch - falling back to the v1 dispatch-time RAM re-read; investigate if this ever fires)\n",
							machine().time().as_double(), m_al_offset, frame_size_words,
							m_al_expected_hw, al_age_ms, m_al_dispatched,
							m_al_snap_fallback, m_al_snap_valid ? 1 : 0,
							m_al_snap_offset, unsigned(m_al_snap.size()));
				}
				else
					logerror("announce-latch: dispatched t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u dispatched=%u (START edge found regs[5]==0-because-wiped; send reconstructed from the ROM-staged bytes at the latched offset)\n",
							machine().time().as_double(), m_al_offset, frame_size_words,
							m_al_expected_hw, al_age_ms, m_al_dispatched);
				// fall through: the send proceeds with the latched size
			}
			else
			{
				++m_al_superseded;
				m_al_valid = false;
				m_al_wiped = false;
				logerror("announce-latch: superseded t=%.6f offset=0x%04x hw=%u dma_ptr=0x%04x superseded=%u (TX pointer moved since the announce - latch dropped, stock zero-size abort; stale offsets are never dispatched)\n",
						machine().time().as_double(), m_al_offset, m_al_wiped_hw,
						m_chunk_tx_ptr, m_al_superseded);
				return;
			}
		}
		else
			return;   // stock behavior: PATH D idle tick / nothing latched
	}

	// Consumed the "TXOFFSET reprogrammed since last send" edge for this send.
	m_chunk_saw_txoffset = false;

	// Sanity: cap at half the C422 RAM (8 KB == 4096 halfwords).  The
	// realistic max per chunk is 0xFF = 255 halfwords (= 510 bytes)
	// based on the game's chunked-TX path, but allow some headroom.
	if (frame_size_words > 0x1000)
	{
		logerror("namco_c139: tx frame size %u halfwords exceeds RAM cap; dropping\n",
				frame_size_words);
		return;
	}

	// P037 (branch patch/latch-genstamp): scope the head-genstamp scratch to
	// the currently-open tracked message.  Any NEW message head that is NOT a
	// v2 snapshot dispatch retires it (the al_snap_use dispatch block above
	// armed/rewrote it; a remainder/continuation never touches it), so a
	// later message's P014 remainder commit can never be compared against a
	// stale head snapshot.  LOG-ONLY rider state - no other effect.
	if (m_al_snapshot && !is_continuation && !al_snap_use)
		m_al_gen_head_live = false;

	uint32_t const frame_size_bytes = uint32_t(frame_size_words) * 2;

	// Read frame_size_words halfwords from C422 RAM starting at fifo_ptr,
	// emitting each as 2 bytes (high byte first, low byte second - matches
	// MIPS big-endian byte ordering of the in-RAM halfword).  This carries
	// the sender's marker bit (bit 8 set in the last halfword's high byte)
	// and checksum (designed for sum=0 mod 256) intact to the receiver.
	std::vector<uint8_t> payload;
	payload.reserve(frame_size_bytes);
	// P035 (branch patch/latch-v2-snapshot): a v2 latch dispatch transmits the
	// WIPE-TIME snapshot verbatim - by the time the ROM's START edge arrives
	// (up to AL_TTL_MS after the wipe) the ROM may already be recomposing the
	// same ring slot for its NEXT frame, which is exactly the torn-payload
	// race P033 measured at boss-flood scale (127 of red's 133
	// marker=MISMATCH completes; the peer's first 2 chkfails).  The snapshot
	// was copied by the identical loop below at the wipe instant (same
	// fifo_ptr - a dispatch provably reads from m_al_offset - same masking,
	// same byte order), so everything downstream (tracker append, resume/DMA
	// pointer advance, CHUNK_PASSTHRU fwd, emit_tx_frame) is unchanged and
	// the accumulator holds the same bytes that went on the wire.
	if (al_snap_use)
		payload = m_al_snap;
	else
	{
		for (uint32_t i = 0; i < frame_size_bytes; i++)
		{
			uint16_t const word_idx = uint16_t((fifo_ptr + (i >> 1)) & 0x1fff);
			uint16_t const word     = m_ram[word_idx];
			uint8_t  const b        = (i & 1) ? uint8_t(word & 0xff)
											  : uint8_t(word >> 8);
			payload.push_back(b);
		}
	}

	// P010: advance the modeled DMA pointer past what we just consumed so
	// the next continuation chunk picks up where this one ended.
	if (m_chunk_armed)
	{
		m_chunk_tx_ptr = uint16_t((fifo_ptr + frame_size_words) & 0x1fff);
		m_chunk_tx_ptr_valid = true;
		logerror("namco_c139: tx chunk staged %u halfwords (%u bytes) from ptr=0x%04x\n",
				frame_size_words, frame_size_bytes, fifo_ptr);
	}

	// Clear TX Frame Size to signal "TX complete" to the host CPU.
	// Without this, the game's link probe function at 0x8000bb6c reads
	// Frame Size != 0 on every subsequent invocation and exits early
	// thinking a TX is still in progress, blocking all further protocol
	// traffic.  Real C422 hardware clears this when TX finishes; we do
	// it synchronously since our "transmission" is instantaneous.  This
	// also holds for a chunk we HOLD for reassembly below: the chunk has
	// been accepted by the "chip", so the emitter may stage the next one.
	//
	// P013 EXPERIMENT (branch patch/tx-emitter-trace): READ-ONLY trace of this
	// SYNCHRONOUS "TX complete" (TXSIZE -> 0).  This is the candidate-(iii)
	// behavior made explicit: the device declares the chunk done the instant it
	// is staged, so the ROM's next BB6C PATH A gate (REG[0x0A] busy-byte) reads
	// 0 and it stages the next chunk freely.  If the >255hw remainder is gated
	// on a chip TX-complete signal the real hardware would withhold until the
	// chunk physically transmits, this synchronous clear is precisely what
	// short-circuits that wait.  Logged with the same from_ptr/size as the "tx
	// chunk staged" line above so they pair 1:1.  (Logged before the clear so
	// val= is the busy value the ROM would otherwise have polled.)
	if (m_txemit_trace)
		logerror("TXEMIT: done fr=%u t=%.6f reason=sync-txsize-clear from_ptr=0x%04x txsize_hw=%u dma_ptr=0x%04x slot=%u (no TX-complete IRQ modeled)\n",
				m_txemit_frame, machine().time().as_double(), fifo_ptr, frame_size_words,
				m_chunk_tx_ptr, txemit_slot_of(m_chunk_tx_ptr));

	// P031: on an announce-latch dispatch m_regs[5] is ALREADY 0 (the rx_clear
	// wiped it and was honored) - skip the store so the latch path literally
	// never writes the register file.  Every other path clears as always.
	if (!al_dispatch)
		m_regs[5] = 0;

	// P010/P011 EXPERIMENT: decide whether this staged chunk is a complete
	// message (emit now), the start of a bulk message (hold), a CONTINUATION
	// of the currently-held bulk message (append; emit when the accumulated
	// halfword count reaches the total announced in the message's size cells),
	// or an unrelated interleaved message (emit now, keep holding the bulk).
	//
	// Completeness is keyed on the sender-side size cells, NOT on a trailer
	// heuristic: the bulk builder writes a saturated boundary trailer (0x1FF)
	// mid-message (the blue->red "truncated-but-marked" shape), which would
	// fool any end-marker test applied at chunk granularity.
	//
	// P011 ASSOCIATION FIX (option (a), the P010 run analysis recommendation):
	// P010 only ever held the FIRST 255hw chunk and stale-swept it because its
	// continuation test was "is this saturated chunk arriving while the
	// bulk-announce flag is still set" - and the ROM's TXSIZE-staged-after-
	// trigger timing means the continuation TXSIZE is consumed on a LATER
	// trigger edge that the announce flag no longer covers, and any interleaved
	// message's TXOFFSET write tore down the accumulation (P010 dropped the
	// held message on EVERY new TXOFFSET).  Here the continuation is identified
	// purely by TX POINTER CONTINUITY: the chunk's read pointer (fifo_ptr)
	// resuming the held message's advanced DMA pointer (m_chunk_resume_ptr).
	// The ROM's continuation chunks (0x8000BB6C PATH B/B') never rewrite
	// TXOFFSET, so on real hardware the DMA pointer keeps advancing from where
	// the previous chunk ended - exactly m_chunk_resume_ptr.  A NEW message
	// (PATH C) rewrites TXOFFSET to a slot base, so its first chunk's read
	// pointer is the slot base, NOT m_chunk_resume_ptr - that distinguishes
	// "continuation" from "a new bulk message reusing the same slot".
	if (m_chunk_armed)
	{
		// (Staleness was already swept at the top of this function and once per
		// vblank, so any m_chunk_accum still held here is live.)
		if (!m_chunk_accum.empty())
		{
			// A bulk message is held.  Is THIS chunk its continuation?
			//
			// Option (a) [default]: the chunk is a continuation iff the ROM
			// did NOT reprogram TXOFFSET before staging it (PATH B/B' never
			// write TXOFFSET) - is_continuation, computed above, which also
			// forced fifo_ptr to m_chunk_resume_ptr.  A defensive secondary
			// accept covers the no-interleaving case where the DMA pointer
			// naturally lands on the resume pointer even though a (same-slot)
			// TXOFFSET write was seen: only accept that when the size matches a
			// continuation, never when it is a fresh saturated announce (that
			// is the "new bulk reusing the same slot" case the analysis warned
			// about, which must supersede, not append).
			// Option (b) [debug fallback, NAMCOS23_CHUNK_ASSOC_ANNOUNCE=1]:
			// the P010 behavior - treat the next chunk unconditionally as the
			// continuation.
			bool const fresh_saturated_announce =
					(frame_size_words == CHUNK_SAT_HW
						&& m_chunk_expected_hw > CHUNK_SAT_HW
						&& m_chunk_expected_hw <= CHUNK_MAX_HW);
			bool const resumes = m_chunk_assoc_pointer
					? (is_continuation
						|| (!had_txoffset_reprogram && fifo_ptr == m_chunk_resume_ptr
							&& !fresh_saturated_announce))
					: true;

			if (resumes)
			{
				// Continuation chunk: append and advance the resume pointer.
				// Completion is against the HELD message's snapshotted expected
				// total, NOT m_chunk_expected_hw (which an interleaved TXOFFSET
				// write may have re-latched to some other message's size).
				//
				// P026: in pass-through mode the append still happens (the
				// accumulator is the association/progress tracker every P011/
				// P014 predicate keys on) but the chunk ALSO goes on the wire
				// right now as its own frame - so the payload bytes below are
				// inserted as a copy, then the vector is moved to the wire.
				m_chunk_accum.insert(m_chunk_accum.end(), payload.begin(), payload.end());
				uint32_t const have_hw = uint32_t(m_chunk_accum.size() / 2);
				m_chunk_resume_ptr = uint16_t((m_chunk_resume_ptr + frame_size_words) & 0x1fff);
				++m_chunk_msg_chunks;     // per-message chunk count (first + continuations)
				++m_chunk_continued;      // cumulative continuation count (1/s status)
				if (m_chunk_passthru)
				{
					++m_chunk_pt_chunks_fwd;
					logerror("CHUNK_PASSTHRU: fwd t=%.6f msg=%u chunk=%u/%u hw=%u accum_hw=%u/%u from_ptr=0x%04x next_resume=0x%04x\n",
							machine().time().as_double(), m_chunk_pt_msg_id,
							m_chunk_msg_chunks,
							unsigned((m_chunk_held_expected_hw + CHUNK_SAT_HW - 1) / CHUNK_SAT_HW),
							frame_size_words, have_hw, m_chunk_held_expected_hw,
							fifo_ptr, m_chunk_resume_ptr);
					emit_tx_frame(std::move(payload), true);
				}
				if (have_hw == m_chunk_held_expected_hw)
				{
					// Complete.  Coalesce mode: emit the concatenation as ONE
					// wire frame.  P026 pass-through mode: every chunk already
					// went out individually - emit NOTHING here, just log the
					// same end-marker diagnostics (the tracker holds the full
					// concatenation either way) and retire the tracker.
					uint16_t const endhw = uint16_t((m_chunk_accum[m_chunk_accum.size() - 2] << 8)
													| m_chunk_accum[m_chunk_accum.size() - 1]);
					uint32_t const claimed = (uint32_t(m_chunk_accum[m_chunk_accum.size() - 3]) << 8)
													| (endhw & 0xff);
					bool const marker_ok = (endhw & 0x100) && claimed == have_hw;
					if (m_chunk_passthru)
					{
						++m_chunk_pt_msgs_complete;
						logerror("CHUNK_PASSTHRU: complete t=%.6f msg=%u total_hw=%u expected=%u chunk_count=%u start_ptr=0x%04x endhw=%04x marker=%s (all chunks already on the wire as separate frames)\n",
								machine().time().as_double(), m_chunk_pt_msg_id, have_hw,
								m_chunk_held_expected_hw, m_chunk_msg_chunks,
								m_chunk_msg_start_ptr, endhw, marker_ok ? "ok" : "MISMATCH");
					}
					else
					{
						++m_chunk_reassembled;
						logerror("CHUNK_REASM: reassembled t=%.6f total_hw=%u expected=%u chunk_count=%u start_ptr=0x%04x endhw=%04x marker=%s end=size-cells\n",
								machine().time().as_double(), have_hw, m_chunk_held_expected_hw,
								m_chunk_msg_chunks, m_chunk_msg_start_ptr, endhw,
								marker_ok ? "ok" : "MISMATCH");
						emit_tx_frame(std::move(m_chunk_accum));
					}
					m_chunk_accum.clear();
					m_chunk_bulk_pending = false;
					m_chunk_msg_chunks = 0;
				}
				else if (have_hw > m_chunk_held_expected_hw || have_hw > CHUNK_MAX_HW)
				{
					chunk_drop("overshoot");
				}
				else if (!m_chunk_passthru)
				{
					logerror("CHUNK_REASM: hold t=%.6f reason=append from_ptr=0x%04x chunk_hw=%u accum_hw=%u/%u chunk_count=%u next_resume=0x%04x slot=0x%04x\n",
							machine().time().as_double(), fifo_ptr, frame_size_words,
							have_hw, m_chunk_held_expected_hw, m_chunk_msg_chunks,
							m_chunk_resume_ptr, m_chunk_msg_start_ptr);
				}
				return;
			}

			// This chunk does NOT resume the held message.  Two cases:
			//  - a NEW saturated bulk message (e.g. reusing the same slot at
			//    its base): supersede the stale held message and start fresh
			//    (the old one will never complete now);
			//  - an unrelated interleaved small/exact message: emit it now and
			//    KEEP holding the bulk message (the key P011 fix - P010 would
			//    have torn the bulk down here, which is why it never
			//    reassembled).  Falls through to the start-or-passthrough
			//    logic below WITHOUT clearing the accumulation.
			if (frame_size_words == CHUNK_SAT_HW
					&& m_chunk_expected_hw > CHUNK_SAT_HW && m_chunk_expected_hw <= CHUNK_MAX_HW)
			{
				// New bulk message arriving while one is still held: the held
				// one is stale (its continuation never resumed) - retire it.
				chunk_drop("superseded");
				// fall through to start a fresh hold below
			}
			else
			{
				// Interleaved unrelated message: pass it straight through,
				// keep the bulk held.
				++m_chunk_passthru_during_bulk;
				logerror("CHUNK_REASM: passthru t=%.6f reason=interleaved-not-resume from_ptr=0x%04x chunk_hw=%u held_accum_hw=%u/%u held_resume=0x%04x held_slot=0x%04x\n",
						machine().time().as_double(), fifo_ptr, frame_size_words,
						unsigned(m_chunk_accum.size() / 2), m_chunk_held_expected_hw,
						m_chunk_resume_ptr, m_chunk_msg_start_ptr);
				emit_tx_frame(std::move(payload));
				return;
			}
		}

		if (frame_size_words == CHUNK_SAT_HW
				&& m_chunk_expected_hw > CHUNK_SAT_HW && m_chunk_expected_hw <= CHUNK_MAX_HW)
		{
			// Saturated first chunk of an announced multi-chunk message:
			// start a reassembly.  (A genuine single-chunk 255-halfword
			// message announces expected == 0xFF and passes through.)  Record
			// the slot pointer this message began at and the pointer the next
			// chunk must resume from (this chunk's read pointer + its size).
			//
			// P026 pass-through: the head chunk goes on the wire NOW as its own
			// hardware-shaped frame; the accumulator keeps a COPY purely as the
			// association/progress tracker (so is_continuation and the P014
			// commit trigger see the identical !m_chunk_accum.empty() state).
			if (m_chunk_passthru)
				m_chunk_accum = payload;             // copy: payload still needed for the wire
			else
				m_chunk_accum = std::move(payload);
			m_chunk_accum_since = machine().time();
			m_chunk_msg_start_ptr = fifo_ptr;
			m_chunk_resume_ptr = uint16_t((fifo_ptr + frame_size_words) & 0x1fff);
			m_chunk_held_expected_hw = m_chunk_expected_hw;   // snapshot so an interleaved TXOFFSET write can't clobber it
			m_chunk_msg_chunks = 1;

			// P031 (branch patch/announce-latch): the head of this announce is
			// going on the wire - the vulnerable stage->START window is over
			// (the remainder dispatches synchronously at its own TXSIZE write
			// via the P014 commit trigger and can never be wiped).  Retire the
			// latch.  On a latch DISPATCH m_al_valid was already consumed at
			// the abort site, so this counts only heads the ROM sent on its
			// own (no dispatch needed) - silent (it pairs 1:1 with the fwd
			// chunk=1 line at this instant); consumed_ok closes the counter
			// arithmetic in the 1/s status line.
			if (m_al_armed && m_al_valid && m_al_offset == fifo_ptr)
			{
				m_al_valid = false;
				m_al_wiped = false;
				++m_al_consumed_ok;
				// P038 (branch patch/latch-v3-dedupe): this NATURAL head send
				// is THE completion class the dedupe ring exists for (the
				// P036 join: each stale duplicate dispatch had a same-class
				// natural complete ~150 ms earlier).  Recorded with the
				// latch's own key so the ROM's wiped cadence re-announce of
				// this class dedupes at its START edge.  Silent, matching the
				// site's existing behavior (pairs 1:1 with the fwd chunk=1
				// line at this instant).
				if (m_al_dedupe)
					al_dedupe_record(m_al_offset, m_al_expected_hw);
			}
			if (m_chunk_passthru)
			{
				++m_chunk_pt_msg_id;
				++m_chunk_pt_chunks_fwd;
				logerror("CHUNK_PASSTHRU: fwd t=%.6f msg=%u chunk=1/%u hw=%u accum_hw=%u/%u from_ptr=0x%04x next_resume=0x%04x\n",
						machine().time().as_double(), m_chunk_pt_msg_id,
						unsigned((m_chunk_held_expected_hw + CHUNK_SAT_HW - 1) / CHUNK_SAT_HW),
						frame_size_words, frame_size_words, m_chunk_held_expected_hw,
						fifo_ptr, m_chunk_resume_ptr);
				emit_tx_frame(std::move(payload), true);
			}
			else
			{
				logerror("CHUNK_REASM: hold t=%.6f reason=first-saturated from_ptr=0x%04x chunk_hw=%u accum_hw=%u/%u chunk_count=1 next_resume=0x%04x slot=0x%04x\n",
						machine().time().as_double(), fifo_ptr, frame_size_words,
						frame_size_words, m_chunk_held_expected_hw, m_chunk_resume_ptr,
						m_chunk_msg_start_ptr);
			}
			return;
		}

		// P050a COMPANION (branch patch/single-burst-pump): under the
		// single-burst quantum poke a >255-hw VM frame is programmed as ONE
		// burst (frame_size == expected_hw > 0xFF, NOT the 0xFF-hw saturated
		// head the reasm path keys on), so it reaches the self-contained
		// passthrough below - the whole frame goes on the wire via the emit at
		// the end of this function.  Retire any announce-latch this frame's own
		// PATH C bulk announce armed (bulk_pending latches on expected_hw >
		// 0xFF, but the first-saturated consumed_ok site above only runs for a
		// 0xFF-hw head) and record the dedupe key, exactly as that site does -
		// so the latch counters close as consumed_ok (not a spurious supersede
		// by the next announce) and a wiped cadence re-announce of this class
		// dedupes.  Inert unless BURST_QUANTUM is armed AND a genuine >255-hw
		// single burst arrives (a DRC-stale poke leaves the ROM chunking, so
		// frame_size_words stays <= 0xFF here and this never fires).  A WIPED
		// large frame instead dispatched from the latch at the abort site
		// (al_dispatch), which already set m_al_valid=false - so the guard is
		// false on that path (no double-retire); clearing bulk_pending there is
		// still correct (the frame went out).
		if (m_bq_armed && frame_size_words > CHUNK_SAT_HW)
		{
			m_chunk_bulk_pending = false;
			++m_bq_single_burst_tx;
			if (m_al_armed && m_al_valid && m_al_offset == fifo_ptr)
			{
				m_al_valid = false;
				m_al_wiped = false;
				++m_al_consumed_ok;
				if (m_al_dedupe)
					al_dedupe_record(m_al_offset, m_al_expected_hw);
			}
		}
		// else: self-contained message (all small/exact frames, the
		// 1-halfword boot ping, or an implausible announced size) -
		// passthrough, identical to unarmed behavior.
	}

	emit_tx_frame(std::move(payload));
}


// P010 EXPERIMENT (branch patch/chunk-reassembly): single exit point for a
// COMPLETE game frame onto the wire (a passthrough chunk, or a reassembled
// multi-chunk bulk message).  Also the (pre-existing) hex dump, P009
// fingerprint and heartbeat capture, so QUAL_TRACE tx-real lines and
// heartbeat replays always describe exactly what went on the wire.
// P026 (branch patch/reasm-chunk-passthru): bulk_chunk=true = a FRAGMENT of an
// in-progress >255hw message forwarded individually under CHUNK_PASSTHRU -
// excluded from the P021 injection and from heartbeat capture (see below).
void namco_c139_device::emit_tx_frame(std::vector<uint8_t> payload, bool bulk_chunk)
{
	if (!m_context || !m_context->connected())
		return;
	if (payload.empty())
		return;

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only): WIRE-ONLY 0x6000
	// "fully-linked" injection.  Done HERE, on this outgoing-frame copy, BEFORE
	// the wire frame / heartbeat capture is built, so every wire image of this
	// frame (the live send AND any heartbeat replay of it) carries the advertised
	// 0x6000 consistently - while this cab's game RAM +0x370 record is never
	// touched (the payload is a copy read out of the C422 link RAM upstream).
	// Inert unless the driver has pushed armed && mode==2.  See the header decl
	// + linkbits_op55_inject_6000 for the full invariant proof.
	// P026: never walk a bulk-chunk FRAGMENT - the op55 cell-walk association is
	// only meaningful on a complete message, and a blind OR into fragment bytes
	// would corrupt the mid-message stream the peer's ring is reassembling.
	if (!bulk_chunk)
		linkbits_op55_inject_6000(payload);

	uint32_t const payload_bytes = uint32_t(payload.size());
	uint32_t const payload_words = payload_bytes / 2;

	std::vector<uint8_t> frame;
	frame.reserve(payload_bytes + 2);
	// Our internal TCP framing: 16-bit big-endian byte count, then payload.
	// (The receiver reads 2-byte size prefix to know how many payload
	// bytes follow on the TCP stream.)
	frame.push_back(static_cast<uint8_t>((payload_bytes >> 8) & 0xff));
	frame.push_back(static_cast<uint8_t>(payload_bytes & 0xff));
	frame.insert(frame.end(), payload.begin(), payload.end());

	// Hex-dump the payload (skip the 2-byte size prefix at frame[0..1]).
	std::string hex;
	hex.reserve(frame.size() * 3);
	for (std::size_t i = 2; i < frame.size(); i++)
	{
		char buf[4];
		std::snprintf(buf, sizeof(buf), "%02x ", frame[i]);
		hex += buf;
	}
	logerror("namco_c139: tx frame %u halfwords (%u bytes) [ %s]\n",
			payload_words, payload_bytes, hex.c_str());

	// P009 EXPERIMENT (branch patch/qual-trace): compact fingerprint of the
	// real game TX so it can be diffed against the peer's RX fingerprints
	// and against our own heartbeat replays.  Inert unless NAMCOS23_TRACE_QUAL.
	log_qual_fingerprint("tx-real", frame.data() + 2, frame.size() - 2);

	// P018 EXPERIMENT (branch patch/op70-real-detector): TX-side REAL op-70
	// detection.  This is the single wire-exit point for every game frame, so an
	// OP70R: tx line here answers Q1: does THIS cab's ROM actually EMIT an op-70
	// cutscene-timer cell?  (Red is the cutscene driver; the emit path is gated
	// at 0x80016DC4 on gp+0x7074==2 then jal 0x800B2514 li 0x70 / sb HH / sb LL
	// into the TX trailer.)  Walk the cell stream (low byte of each payload
	// halfword) with the same gate-4 VM cell-walk used on RX - flagging a 0x70
	// only when it lands as a real opcode.  Inert unless NAMCOS23_TRACE_OP70.
	// P025 (branch patch/playclock-humangate-trace): the same walk also reports
	// op6F play-clock cells (PLAYCLOCK: op6f-tx) when NAMCOS23_TRACE_PLAYCLOCK -
	// answering "does THIS cab (as 0x802F3FFC-bit7 master) EMIT the +0x390/394
	// clock beacon" (emitter 0x800B22CC, cadence once per 256 frames).
	// P044 (branch patch/anchor-lifecycle): and entity-lifecycle cells
	// (OP20: tx-release/tx-despawn) when NAMCOS23_TRACE_OP20 - does THIS cab
	// queue op-0x20 releases (the local half of the cross-cab anchor-kill
	// transaction class).
	if ((m_trace_op70 || m_trace_playclock || m_trace_op20) && payload_words >= 4)
	{
		std::vector<uint8_t> cells;
		cells.reserve(payload_words);
		for (uint32_t k = 0; k < payload_words; k++)
			cells.push_back(uint8_t(payload[k * 2 + 1]));
		unsigned const cellseq = (unsigned(cells[0]) << 8) | (cells.size() > 1 ? cells[1] : 0);
		op70_cell_walk("tx", cells.data(), cells.size(),
				payload_words > CHUNK_SAT_HW, cellseq, m_op70_tx_index);
	}

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): TX-side op55
	// wire-flag tap.  This is the single wire-exit point for every game frame, so
	// a LINKBITS: txflags line here shows whether THIS cab transmits the 0x6000
	// fully-linked bits in its op55 24-bit flags (candidate (Y)).  Walk the cell
	// stream (low byte of each payload halfword).  Inert unless
	// NAMCOS23_TRACE_LINKBITS.
	if (m_trace_linkbits && payload_words >= 8)
	{
		std::vector<uint8_t> cells;
		cells.reserve(payload_words);
		for (uint32_t k = 0; k < payload_words; k++)
			cells.push_back(uint8_t(payload[k * 2 + 1]));
		linkbits_op55_scan("tx", cells.data(), cells.size());
	}

	// PHASE 9D: cache real TXs (>= 2 halfwords, i.e. proper protocol frames -
	// not the 1-halfword boot ping) for the heartbeat timer to replay.  Skip
	// the 2-byte length prefix at frame[0..1].  Re-arm the heartbeat to fire
	// 250 ms from now (well below the 17-frame timeout threshold at 60 fps).
	// REMOVE BEFORE COMMIT.
	//
	// P010: when chunk reassembly is armed, reassembled BULK frames
	// (> 0xFF halfwords) are EXCLUDED from heartbeat capture: replays are
	// restamped (cell-0 marker destroyed) and silently dropped pre-enqueue
	// on the peer anyway (P009 run analysis §2), so replaying half a KB of
	// stale bulk state would only burn wire bandwidth - and must never
	// interleave a partial/stale bulk image into the stream.
	// P026: a pass-through bulk CHUNK is likewise excluded (bulk_chunk) - it is
	// a message fragment; capturing it would both replay garbage and restamp
	// bytes 0-1 of a frame the peer's ring is mid-reassembling.
	// P027 (a) (branch patch/hb-cadence-wipe-restore): the re-arm interval is
	// m_hb_cadence_ms (250 unless NAMCOS23_PATCH_HB_CADENCE_MS overrides it,
	// so unset = bit-identical stock).  Because this re-arm runs on every
	// captured real TX, the replay always fires "N ms after the last real TX"
	// - genuine traffic resets the clock.  When the override is armed, a
	// forwarded bulk CHUNK also resets the clock (else-branch below): it is
	// real wire traffic - the peer's drift resets when the completed message
	// drains - but it must still NEVER be captured as the replay payload.
	if (payload_words >= 2 && !bulk_chunk && (!m_chunk_armed || payload_words <= CHUNK_SAT_HW))
	{
		m_last_tx_payload.assign(frame.begin() + 2, frame.end());
		// P062 EXPERIMENT (branch patch/txstage-trace): HEARTBEAT CAPTURE
		// CONTENT - hash the just-captured replay payload (wire byte order,
		// same stream ts_hash_ram walks at the stage site) and test whether the
		// capture IS the newest-composed content at this instant (capture hash
		// == the hash of the most recent TXSIZE stage).  The P061 post-mortem's
		// second starvation channel was the replay path carrying seconds-stale
		// content because the capture tracked rare DISPATCHES, not composes -
		// this measures that divergence under stock pacing.  Counted here (no
		// per-capture line at ~30/s); surfaced on the 1/s TXSTAGE digest as
		// hb_caps / hb_cap_match + the digest-instant cap_is_newest flag.
		// READ-ONLY.  MODEL PROVENANCE: Fable 5.
		if (m_ts_armed)
		{
			m_ts_hbcap_hash = ts_hash_bytes(m_last_tx_payload.data(), m_last_tx_payload.size());
			m_ts_hbcap_valid = true;
			++m_ts_hbcap_win;
			if (m_ts_last_stage_valid && m_ts_hbcap_hash == m_ts_last_stage_hash)
				++m_ts_hbcap_match_win;
		}
		// P063 (branch patch/tx-complete-v2): stamp the capture instant - the
		// stale-replay age-out anchor.  P062 proved the capture IS the newest
		// composed content at this instant; its AGE at replay time is the
		// poison metric.  A member store, no behavior change; guarded anyway
		// so the unset path is audit-trivially untouched.
		if (m_txc2_armed)
			m_txc2_cap_time = machine().time();
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(hb_cadence_effective_ms()));
	}
	// P050a COMPANION (branch patch/single-burst-pump): under the quantum poke
	// the fight-era frames arrive as >255-hw SINGLE BURSTS (bulk_chunk=false,
	// not the 255-hw chunk train the P027 (a) case re-arms on).  Re-arm the
	// clock on them too - real wire traffic - but do NOT capture (restamping
	// bytes 0-1 would corrupt the length header, same reason coalesced bulk is
	// excluded above).  Effect: the keepalive replay stays suppressed while real
	// large-frame traffic flows (a replay fires only after m_hb_cadence_ms of
	// total-TX silence), so a stale small replay can never out-arrive a fresh
	// fight frame and be kept by gate (b)'s newest-wins.  Inert unless
	// BURST_QUANTUM is armed.
	else if (m_heartbeat_timer
			&& ((m_hb_cadence_override && bulk_chunk)
				|| (m_bq_armed && !bulk_chunk && payload_words > CHUNK_SAT_HW)))
		m_heartbeat_timer->adjust(attotime::from_msec(hb_cadence_effective_ms()));

	// P050 FIFO-JOIN instrumentation (branch patch/single-burst-pump): for every
	// COMPLETE-STANDALONE frame that goes on the wire, emit a compact tx-join
	// stamp keyed by the ROM per-frame drift sequence (seq = cellseq) and bump
	// the tx frames/s counter.  The peer logs the matching rx-join seq= in
	// deliver_rx_frames, so red tx-join seq=X joins blue rx-join seq=X to yield
	// the red->blue delivery latency (P048 method).  Inert unless either pump
	// gate is armed.  A bulk chunk / mid-message fragment is not complete-
	// standalone (nw_frame_complete false) so it never stamps.
	if (m_pump_instr && nw_frame_complete(payload.data(), payload.size()))
	{
		++m_nw_tx_complete;
		++m_nw_tx_win;
		logerror("NEWEST_WINS: tx-join seq=%04x hw0=%04x bytes=%u t=%.6f tx_complete=%u (complete VM frame on the wire; join to peer rx-join seq= for red->blue latency)\n",
				unsigned(nw_frame_seq(payload.data(), payload.size())),
				unsigned((payload[0] << 8) | payload[1]),
				payload_bytes, machine().time().as_double(), m_nw_tx_complete);
	}

	// P013 EXPERIMENT (branch patch/tx-emitter-trace): READ-ONLY trace of the
	// ACTUAL socket send - the single wire exit point for every game frame (a
	// passthrough chunk or a reassembled bulk message).  Logs what really went
	// out (halfwords/bytes, the end-marker halfword) plus the device's current
	// TX DMA-pointer/slot state, so the analyst can line every send up against
	// the TXEMIT: regw register-programming sequence and the CHUNK_REASM "tx
	// chunk staged ... from ptr=" line by frame/t - and see whether a remainder
	// is programmed but never sent (no send line), or sent on a path the chunk
	// hook misses.  bulk=1 marks a >255hw payload (the reassembled class P010/
	// P011 never produced).
	if (m_txemit_trace)
	{
		uint16_t const endhw = (payload_bytes >= 2)
				? uint16_t((payload[payload_bytes - 2] << 8) | payload[payload_bytes - 1])
				: 0;
		++m_txemit_sends;
		logerror("TXEMIT: send fr=%u t=%.6f bytes=%u halfwords=%u endhw=%04x dma_ptr=0x%04x slot=%u resume=0x%04x msg_start=0x%04x held_accum_hw=%u bulk=%d\n",
				m_txemit_frame, machine().time().as_double(), payload_bytes, payload_words,
				endhw, m_chunk_tx_ptr, txemit_slot_of(m_chunk_tx_ptr), m_chunk_resume_ptr,
				m_chunk_msg_start_ptr, unsigned(m_chunk_accum.size() / 2),
				(payload_words > CHUNK_SAT_HW) ? 1 : 0);
	}

	m_context->send_frame(std::move(frame));
}


// P010/P011 EXPERIMENT: abandon a pending reassembly (stale tail, overshoot,
// superseded by a new bulk message).  The partial bulk frame is dropped, never
// sent - exactly the pre-P010 outcome for this message, minus the garbage
// chunks on the wire.
void namco_c139_device::chunk_drop(const char *reason)
{
	if (m_chunk_accum.empty() && !m_chunk_bulk_pending)
		return;
	if (!m_chunk_accum.empty())
	{
		++m_chunk_dropped;
		// P026: mode field - in pass-through the "dropped" bytes were ALREADY
		// forwarded to the wire chunk-by-chunk (only the TRACKER is retired
		// here; the peer holds a truncated message its own staleness/marker
		// machinery must age out, exactly as an abandoned mid-message on real
		// hardware would).  In coalesce mode nothing was ever sent.
		logerror("CHUNK_REASM: drop t=%.6f reason=%s had_hw=%u expected=%u chunk_count=%u start_ptr=0x%04x resume_ptr=0x%04x mode=%s\n",
				machine().time().as_double(), reason,
				unsigned(m_chunk_accum.size() / 2), m_chunk_held_expected_hw,
				m_chunk_msg_chunks, m_chunk_msg_start_ptr, m_chunk_resume_ptr,
				m_chunk_passthru ? "passthru(chunks-already-on-wire)" : "coalesce(nothing-sent)");
		m_chunk_accum.clear();
	}
	m_chunk_bulk_pending = false;
	m_chunk_msg_chunks = 0;          // P011: per-message chunk count resets with the message
	m_chunk_held_expected_hw = 0;    // P011: no held message after a drop
}


// Drains any frames the network thread has pushed onto the inbound queue
// and delivers them into the shared RAM at the RX FIFO Pointer (reg 6),
// advancing the pointer and setting the Status/Control register's RX
// flag bits so the game's busy-wait poll-loop breaks out.  Called on the
// emulation thread from every C139 register access path so RX latency
// is bounded by the game's own polling cadence.
//
// IRQ assertion is intentionally absent for now: the original c422 stub
// only ever raised the IRQ in response to specific magic-value writes
// from the host (0xfffb assert / 0x000f clear), and our naive ASSERT_LINE
// here without a matching ack pattern caused the host MIPS to spin in
// its IRQ handler indefinitely.  The game's polling loop is sufficient
// to detect frame arrival via the Status/Control flag bits.  We will
// reintroduce a properly-acked IRQ in a later phase once we understand
// the game's IRQ-handler ack semantics.
//
// The int32_t param is retained for compatibility with timer callbacks
// in case we later want to wake the emulation thread via an emu_timer.
void namco_c139_device::deliver_rx_frames(int32_t /*param*/)
{
	if (!m_context)
		return;

	auto pending = m_context->drain_rx();
	if (pending.empty())
		return;

	// P059 EXPERIMENT (branch patch/poscorr-trace): READ-ONLY RX arrival counting
	// (connector/blue only, env-gated).  Counted here - BEFORE the newest-wins
	// collapse - so "arrived" is what the socket actually queued this drain, and
	// the later delivered/dropped counts reveal the fix-B headroom.  Inert/byte-
	// identical when unset or on red.  Full design in the header member block.
	bool const pc_on = m_poscorr_trace && m_comm_is_connector;
	if (pc_on)
	{
		++m_pc_drains_win;
		uint32_t const depth = uint32_t(pending.size());
		if (depth > m_pc_max_pending_win) m_pc_max_pending_win = depth;
		if (depth > 1) ++m_pc_multidrain_win;
		for (auto const &f : pending)
		{
			if (nw_frame_complete(f.data(), f.size()))
			{
				++m_pc_arrived_complete_win;
				++m_pc_arrived_complete_tot;
			}
			else
			{
				++m_pc_arrived_other_win;
			}
		}
	}

	// P050b (branch patch/single-burst-pump): NEWEST-WINS delivery cap.  Cap the
	// per-direction undelivered queue at ONE complete VM frame - the ROM never
	// asked for the queue our transport built.  Mirrors the ROM's own
	// compose-side supersession (P049 F1: the pump only ever sends the newest
	// 0x400-hw slot) and is safe by F3: state re-serializes every frame and the
	// 32-slot reliable backlog re-publishes until 0xFD-acked, so the newest
	// frame carries the freshest state AND all outstanding reliable slots (any
	// slot acked between two composes is dropped from both, so nothing owed is
	// lost) - a dropped instance is re-sent by design (see the impl log for the
	// not-distinguishable-but-never-lost reasoning).
	//
	// FAIL-SAFE gate: supersede ONLY when EVERY frame in this drain is a
	// complete-standalone VM frame (the ROM strict trailer test - no chunk /
	// mid-message fragment / boot ping present); then keeping just the newest
	// cannot disturb any in-ring CHUNK_PASSTHRU reassembly.  Under gate (a)
	// every frame IS a single-burst complete frame, so fight-era batches always
	// qualify; under stock (gate (b) alone, or a DRC-stale gate-(a) poke that
	// leaves the ROM chunking) a batch carrying any fragment is delivered
	// byte-identical - the gate self-adapts to whether the poke took, and never
	// risks a fragment.  Inert unless armed / <2 frames pending.
	if (m_newest_wins && pending.size() > 1)
	{
		bool all_complete = true;
		for (auto const &f : pending)
			if (!nw_frame_complete(f.data(), f.size()))
			{
				all_complete = false;
				break;
			}
		if (all_complete)
		{
			uint32_t const q_depth = uint32_t(pending.size());
			uint32_t const dropped = q_depth - 1;      // keep only the newest arrival
			std::vector<uint8_t> newest = std::move(pending.back());
			uint16_t const kept_seq  = nw_frame_seq(newest.data(), newest.size());
			uint32_t const kept_bytes = uint32_t(newest.size());
			pending.clear();
			pending.push_back(std::move(newest));
			m_nw_superseded += dropped;
			// P059: attribute the newest-wins collapse as a device-side DROP (the
			// exact site the RE fingered @namco_c139.cpp:1545).  Only reached when
			// m_newest_wins is armed, so it stays 0 under the stock recipe =
			// empirical proof the drop is INERT (conditional), not unconditional.
			if (pc_on)
			{
				m_pc_dropped_newest_win += dropped;
				m_pc_dropped_newest_tot += dropped;
			}
			logerror("NEWEST_WINS: superseded t=%.6f dropped=%u queue_depth=%u kept_seq=%04x kept_bytes=%u superseded_total=%u (all-complete drain collapsed to the newest VM frame; it carries the freshest state + re-published reliable slots - P049 F3)\n",
					machine().time().as_double(), dropped, q_depth,
					unsigned(kept_seq), kept_bytes, m_nw_superseded);
		}
	}

	while (!pending.empty())
	{
		auto frame = std::move(pending.front());
		pending.pop_front();

		uint16_t const fifo_ptr = m_regs[6];   // RX FIFO Pointer (WORDS)
		uint16_t const rx_base = 0x1000;
		std::size_t const num_payload_words = frame.size() / 2;

		// Skip frames smaller than 2 payload words.  Even-byte payloads
		// of 2 bytes (= 1 halfword) are the connection-level greetings
		// (the initial 1-halfword ping the game emits at boot via
		// m_regs[5] = 1), not protocol frames - they don't carry the
		// sender's bit-8 end-of-frame marker so the validator can't do
		// anything with them.  Skipping is also a guard against odd-
		// length frames (frame.size() = 1) which would produce 0
		// payload halfwords here.
		if (num_payload_words < 2)
		{
			if (pc_on) ++m_pc_dropped_small_win; // P059: sub-2-word greeting/odd, not a position carrier
			continue;
		}

		// Write payload bytes into the high-half RX area at the current
		// FIFO pointer.  fifo_ptr is RX-area-relative; the RX area sits
		// at words 0x1000..0x1FFF (per the namcos22 RE comment giving
		// the RX FIFO Pointer a 12-bit range).
		//
		// PROTOCOL NOTE (Phase 9 fix): we do NOT inject our own end-of-
		// frame marker words anymore.  The sender already includes the
		// message-end framing within the transmitted halfwords - the
		// LAST halfword of the packet has bit 8 set in its high byte
		// (the marker) and size_lo in its low byte; the second-to-last
		// halfword's low byte holds size_hi.  This is set up by the
		// sender's TX-side packet builder at 0x8000C28C..0x8000C2A4 in
		// the timecrs2 ROM, and the on-wire bytes carry it intact to
		// us.  Previously (Phase 8) we overwrote the last two halfwords
		// with our own framing, which corrupted the payload and used a
		// different 'size' encoding than what the validator expected -
		// so the validator's checksum range never aligned with the
		// sender's, the sum was always non-zero (success path), and
		// the sum=0 dispatcher (which legitimately resets the timeout
		// counter and updates the gp+0x77C8/D0/D4 link-state vars) was
		// never reached.  Just writing the payload bytes faithfully
		// gives the validator the sender's intended packet structure.
		// P010 EXPERIMENT (branch patch/chunk-reassembly): when armed, wrap
		// within the RX window (rx_base + 12-bit offset) instead of within
		// the whole 8 KB RAM.  The pre-existing `& 0x1fff` lets a frame
		// crossing the end of the RX area bleed into the TX half (words
		// 0x0000..) instead of wrapping to 0x1000, while the ROM's drain
		// loop wraps its source index with `& 0x0FFF` inside the RX window
		// (0x8000BF38, PC 0x8000BFB8).  Reassembled bulk frames (up to
		// 0x400 halfwords) cross the boundary far more often than the old
		// small frames, so the armed path gets the faithful wrap.
		for (std::size_t i = 0; i < frame.size(); i++)
		{
			uint16_t const word_idx = m_chunk_armed
					? uint16_t(rx_base + ((fifo_ptr + (i >> 1)) & 0x0fff))
					: uint16_t((rx_base + fifo_ptr + (i >> 1)) & 0x1fff);
			uint16_t &w = m_ram[word_idx];
			if (i & 1)
				w = uint16_t((w & 0xff00) | uint16_t(frame[i]));
			else
				w = uint16_t((w & 0x00ff) | (uint16_t(frame[i]) << 8));
		}

		// Advance the RX FIFO Pointer past the last received halfword
		// (RX area is 4096 words, so mask to 0x0FFF).  The game's
		// scanner at 0x8000BD80 expects to find the marker at
		// (m_regs[6] - 1) when scanning backwards 8 iterations.
		m_regs[6] = uint16_t((fifo_ptr + num_payload_words) & 0x0fff);

		// Set the RX flag bits in the Status/Control Flags register so
		// the dispatcher at 0x8000beac picks up the event.
		m_regs[1] |= 0x0006;

		// Clear m_regs[5] (Frame Size).  Empirically (phase 8 trace),
		// after staging a TX the game writes m_regs[5] = expected size
		// and busy-polls it, expecting hardware to clear the register
		// once a corresponding RX frame has arrived.  Without this, the
		// game gets stuck in the post-RX busy-wait loop forever despite
		// our RX-flag bits already being set in m_regs[1].
		// PHASE 9D DIAGNOSTIC: log m_regs[5] clears so we can correlate
		// with game writes.  REMOVE BEFORE COMMIT.
		//
		// P010 EXPERIMENT (branch patch/chunk-reassembly): while a bulk
		// message is mid-flight (reassembly accumulating, or a saturated
		// chunk staged for an announced bulk message), do NOT let an RX
		// delivery wipe the staged-but-not-yet-triggered TXSIZE - that
		// race silently deletes a chunk from the middle of the message
		// (the reassembly would then stale-drop the whole frame).  Real
		// hardware keeps the busy byte non-zero until the chunk actually
		// transmits; the ROM emitter just waits in its PATH A early-out.
		// Scoped to bulk-in-flight only so the link-up busy-poll (which
		// the rx_clear exists for) is untouched.
		// P026 (branch patch/reasm-chunk-passthru): the suppression is RETIRED in
		// pass-through mode.  Both windows it protected are gone or not worth it:
		//   - a staged CONTINUATION no longer exists between RX deliveries - the
		//     P014 commit trigger dispatches the remainder SYNCHRONOUSLY inside
		//     the very reg_w that staged its TXSIZE (deliver_rx_frames runs at
		//     the top of reg_w BEFORE the store, so it can never wipe it);
		//   - the HEAD's stage->START-edge window remains, but P025 showed the
		//     suppression itself wedging: the 351-hw bulk announce whose START
		//     edge never came left bulk_pending=1 + TXSIZE=0xFF parked forever,
		//     and the device fought the ROM's rx_clear 8/s for 104 s (832
		//     suppressions) while the ROM's emitter PATH A read busy and never
		//     staged another message - red's TX starved.  Un-suppressed, the
		//     rx_clear frees the ROM's emitter (PATH A reads 0) at the cost of a
		//     rare lost head (one message -> one chkfail on the peer, bounded,
		//     logged below) - which real line noise produces too.
		bool const chunk_in_flight = m_chunk_armed && !m_chunk_passthru
				&& (!m_chunk_accum.empty()
					|| (m_chunk_bulk_pending && (m_regs[5] & 0xff) == CHUNK_SAT_HW));
		if (chunk_in_flight)
		{
			if (m_regs[5] != 0)
				++m_chunk_rxclear_suppressed;
		}
		else
		{
			if (m_regs[5] != 0)
			{
				// P026 diagnostic (log-only, no behavior change): attribute the
				// wipe when it hits a staged TXSIZE while a bulk message is
				// announced or a chunk sequence is open - the exact race the
				// old suppression hid.  Pairs with a missing CHUNK_PASSTHRU:
				// fwd / commit-trigger line for the same message.
				bool const staged_bulk = m_chunk_passthru
						&& (m_chunk_bulk_pending || !m_chunk_accum.empty());

				// P027 (b) EXPERIMENT (branch patch/hb-cadence-wipe-restore):
				// ONE-SHOT restore.  P026 run 1 measured this race at 116 hits
				// on red (~54% of staged chunk sends lost PRE-WIRE, 2-6/s
				// through every bulk phase; chkfail=0 on both cabs proved no
				// on-wire truncation ever - the ROM re-composes and the next
				// slot survives, so the cost is pure CADENCE: each wipe
				// stretches an inter-bulk gap past the peer's 17-frame drift
				// ceiling).  The precise race: the ROM stages the head's
				// TXSIZE, then the START-edge write itself (or any register
				// access in between) runs deliver_rx_frames at its top - a
				// pending peer frame then wipes m_regs[5] before the trigger
				// fires and the send reads size 0.  Restoring means SKIPPING
				// the clear so the staged value survives to its START edge -
				// which is what real hardware does anyway (an RX never clears
				// a staged TX; the rx_clear emulation exists for the LINK-UP
				// busy-poll, which never coincides with pass-through bulk
				// state, see chunk_in_flight scoping above).  Strictly
				// one-shot per bulk announce (budget re-armed only by the
				// ROM's own next TXOFFSET bulk announce in reg_w): a SECOND
				// wipe of the same stage logs rxclear-restore-yielded and
				// wipes normally - this must NEVER become the P025 infinite
				// suppression fight (832 suppressions/104 s, red's TX
				// starved), so we never fight the ROM's own buffer
				// management, we only ride out the one mid-stage race.
				if (staged_bulk && m_wipe_restore_armed && m_wipe_restore_budget)
				{
					m_wipe_restore_budget = false;
					++m_wipe_restored;
					logerror("CHUNK_PASSTHRU: rxclear-restaged-txsize t=%.6f txsize_hw=%u bulk_pending=%d open_accum_hw=%u expected_hw=%u restored=%u (one-shot: staged chunk TXSIZE preserved through the rx_clear; a second wipe of this stage yields)\n",
							machine().time().as_double(), unsigned(m_regs[5] & 0xff),
							m_chunk_bulk_pending ? 1 : 0,
							unsigned(m_chunk_accum.size() / 2), m_chunk_expected_hw,
							m_wipe_restored);
					// m_regs[5] intentionally NOT cleared: the staged value
					// survives to its START edge / commit trigger.
				}
				else
				{
					if (staged_bulk)
					{
						++m_chunk_pt_rxclear_wiped;
						if (m_wipe_restore_armed)
						{
							++m_wipe_yielded;
							logerror("CHUNK_PASSTHRU: rxclear-restore-yielded t=%.6f txsize_hw=%u yielded=%u (restore budget spent for this announce - yielding to the ROM's rx_clear, never fighting it)\n",
									machine().time().as_double(),
									unsigned(m_regs[5] & 0xff), m_wipe_yielded);
						}
						logerror("CHUNK_PASSTHRU: rxclear-wiped-staged-txsize t=%.6f txsize_hw=%u bulk_pending=%d open_accum_hw=%u expected_hw=%u (staged chunk lost pre-trigger; peer will see a truncated message)\n",
								machine().time().as_double(), unsigned(m_regs[5] & 0xff),
								m_chunk_bulk_pending ? 1 : 0,
								unsigned(m_chunk_accum.size() / 2), m_chunk_expected_hw);

						// P031 (branch patch/announce-latch): WIPE-CAPTURE.  The
						// wipe below proceeds untouched (the ROM's PATH A green
						// light is never fought); the latch merely REMEMBERS the
						// staged size it is about to destroy, so the ROM's START
						// edge - which still arrives and today reads a zeroed
						// size - can dispatch the send from the latch (see the
						// abort site in send_pending_tx_frame).  Only the FIRST
						// wipe per announce is captured: a second staged TXSIZE
						// wiped under the same latch means the ROM re-staged the
						// register without a TXOFFSET reprogram after a missed
						// dispatch (e.g. the remainder into dead air) - the
						// latch's model of "the wiped value is the head" is no
						// longer trustworthy, so it drops rather than ever
						// dispatching a mismatched (offset,size).
						if (m_al_armed && m_al_valid)
						{
							attotime const al_age = machine().time() - m_al_time;
							unsigned const al_age_ms = unsigned(al_age.as_double() * 1000.0 + 0.5);
							if (al_age > attotime::from_msec(AL_TTL_MS))
							{
								++m_al_expired;
								m_al_valid = false;
								m_al_wiped = false;
								logerror("announce-latch: expired t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u ttl_ms=%d expired=%u (wipe hit a latch past TTL - dropped, never dispatched)\n",
										machine().time().as_double(), m_al_offset,
										unsigned(m_regs[5]), m_al_expected_hw,
										al_age_ms, AL_TTL_MS, m_al_expired);
							}
							else if (!m_al_wiped)
							{
								m_al_wiped = true;
								m_al_wiped_hw = m_regs[5];
								++m_al_wipes_captured;
								// P035 (branch patch/latch-v2-snapshot): v2
								// SNAPSHOT, taken at the ONLY instant the staged
								// payload is provably pristine - the ROM staged
								// these bytes and the wipe (below, untouched) hits
								// only the register.  Copy exactly the halfwords
								// the START-edge dispatch would otherwise re-read
								// from C422 RAM (m_al_wiped_hw of them starting at
								// m_al_offset - the dispatch's read pointer is
								// provably m_al_offset, see the abort site), with
								// the identical masking and wire byte order as the
								// send loop.  READ-ONLY: m_ram and the register
								// file are not written; the rx_clear proceeds
								// byte-identically.  The buffer was reserved once
								// in device_start (clear + append within capacity,
								// no per-event allocation).
								if (m_al_snapshot)
								{
									m_al_snap_valid = false;
									m_al_snap.clear();
									uint32_t const snap_bytes = uint32_t(m_al_wiped_hw) * 2;
									if (snap_bytes != 0 && snap_bytes <= AL_SNAP_MAX_BYTES)
									{
										for (uint32_t i = 0; i < snap_bytes; i++)
										{
											uint16_t const word_idx = uint16_t((m_al_offset + (i >> 1)) & 0x1fff);
											uint16_t const word     = m_ram[word_idx];
											uint8_t  const b        = (i & 1) ? uint8_t(word & 0xff)
																			  : uint8_t(word >> 8);
											m_al_snap.push_back(b);
										}
										m_al_snap_offset = m_al_offset;
										m_al_snap_valid = true;
										++m_al_snap_copied;
									}
									logerror("announce-latch: wiped t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u wiped_seen=%u snap=%d snap_bytes=%u snap_copied=%u (staged TXSIZE lost to rx_clear; latch holds the stage AND its wipe-time payload snapshot for the ROM's START edge)\n",
											machine().time().as_double(), m_al_offset,
											unsigned(m_al_wiped_hw), m_al_expected_hw,
											al_age_ms, m_al_wipes_captured,
											m_al_snap_valid ? 1 : 0,
											unsigned(m_al_snap.size()), m_al_snap_copied);
								}
								else
									logerror("announce-latch: wiped t=%.6f offset=0x%04x hw=%u expected_hw=%u age_ms=%u wiped_seen=%u (staged TXSIZE lost to rx_clear; latch holds the stage for the ROM's START edge)\n",
											machine().time().as_double(), m_al_offset,
											unsigned(m_al_wiped_hw), m_al_expected_hw,
											al_age_ms, m_al_wipes_captured);
							}
							else
							{
								++m_al_superseded;
								m_al_valid = false;
								m_al_wiped = false;
								logerror("announce-latch: superseded t=%.6f offset=0x%04x hw=%u expected_hw=%u superseded=%u (second staged TXSIZE wiped under one announce - unattributable re-stage, latch dropped)\n",
										machine().time().as_double(), m_al_offset,
										unsigned(m_regs[5]), m_al_expected_hw,
										m_al_superseded);
							}
						}
					}
					logerror("namco_c139: m_regs[5] %04x -> 0000 (src=rx_clear)\n", m_regs[5]);
					m_regs[5] = 0;
				}
			}
			else
				m_regs[5] = 0;
		}

		// Hex-dump the received payload for diagnostic visibility.
		std::string hex;
		hex.reserve(frame.size() * 3);
		for (std::size_t i = 0; i < frame.size(); i++)
		{
			char buf[4];
			std::snprintf(buf, sizeof(buf), "%02x ", frame[i]);
			hex += buf;
		}
		logerror("namco_c139: delivered rx frame %u bytes (%u halfwords) to word=0x%04x [ %s]\n",
				static_cast<unsigned>(frame.size()),
				static_cast<unsigned>(num_payload_words),
				fifo_ptr, hex.c_str());

		// P009 EXPERIMENT (branch patch/qual-trace): compact qualification
		// fingerprint of every delivered RX frame, to be paired offline
		// with the driver-side QUAL_TRACE qual/chkfail event lines (both
		// carry t=).  Inert unless NAMCOS23_TRACE_QUAL.
		log_qual_fingerprint("rx", frame.data(), frame.size());

		// P050 FIFO-JOIN instrumentation (branch patch/single-burst-pump): for
		// every COMPLETE-STANDALONE frame delivered into the RX ring, emit a
		// compact rx-join stamp keyed by the same ROM per-frame drift sequence
		// the sender stamped, and bump the rx frames/s counter.  Join this cab's
		// rx-join seq=X to the PEER's tx-join seq=X for the per-direction
		// delivery latency (the P048 method).  A chunk / mid-message fragment is
		// not complete-standalone so it never stamps.  Inert unless either pump
		// gate is armed.
		if (m_pump_instr && nw_frame_complete(frame.data(), frame.size()))
		{
			++m_nw_rx_complete;
			++m_nw_rx_win;
			logerror("NEWEST_WINS: rx-join seq=%04x hw0=%04x bytes=%u t=%.6f rx_complete=%u (complete VM frame delivered to the RX ring; join to peer tx-join seq= for delivery latency)\n",
					unsigned(nw_frame_seq(frame.data(), frame.size())),
					unsigned((frame[0] << 8) | frame[1]),
					unsigned(frame.size()), machine().time().as_double(), m_nw_rx_complete);
		}

		// P055 (branch patch/boat-jitter-trace): classify each delivered COMPLETE
		// VM frame FRESH vs HEARTBEAT-REPLAY for the driver's boat-jitter trace and
		// bump the rx-apply generation the driver diffs per vblank.  A heartbeat
		// replay is a byte-identical copy of the sender's last real TX except
		// payload bytes 0-1 (the restamped counter), so a complete frame that
		// matches the previous complete frame from byte 2 onward carries STALE
		// coordinates (the "boat frozen on the wire" signature); a FRESH compose of
		// a moving boat differs in those bytes.  READ-ONLY - no wire/state/game
		// effect, only device-internal bookkeeping the driver reads via getters.
		// Inert unless m_bj_armed.  Emulation-thread only.
		if (m_bj_armed && nw_frame_complete(frame.data(), frame.size()))
		{
			bool replay = false;
			if (m_bj_prev_complete.size() == frame.size() && frame.size() > 2)
				replay = (std::memcmp(frame.data() + 2, m_bj_prev_complete.data() + 2,
						frame.size() - 2) == 0);
			m_bj_last_was_replay = replay;
			++m_bj_rx_gen;
			if (replay)
				++m_bj_replay_count;
			else
				++m_bj_fresh_count;
			m_bj_prev_complete.assign(frame.begin(), frame.end());

			// P059: this complete VM frame is being DELIVERED into the RX ring -
			// count it (split FRESH/REPLAY) as the fix-B "delivered" figure to
			// join against "arrived" above.  Connector/blue only.
			if (pc_on)
			{
				++m_pc_delivered_complete_win;
				++m_pc_delivered_complete_tot;
				if (replay) ++m_pc_delivered_replay_win; else ++m_pc_delivered_fresh_win;
			}
		}

		// P027 (c) EXPERIMENT (branch patch/hb-cadence-wipe-restore): sample
		// the peer's counter domain for the heartbeat restamp.  Payload
		// halfword 0 (bytes 0-1) of a protocol message is the sender-side
		// counter the receiving ROM's dispatcher tail (0x8000BB10-1C)
		// subtracts from its own gp+0x75B8 to recompute the drift word
		// 0x802F3504 - the same field the heartbeat replay stamps.  Sample
		// ONLY a COMPLETE message: a pass-through bulk HEAD chunk carries no
		// end marker at its tail, and a REMAINDER chunk's bytes 0-1 are
		// mid-message payload (garbage as a counter), so both are rejected by
		// requiring the ROM's own trailer invariant - bit-8 end marker in the
		// final halfword AND the trailer's claimed length == this frame's own
		// halfword count (the exact marker_ok test the TX-side completion
		// check uses).  Note the sample deliberately includes the peer's own
		// heartbeat replays (they are restamped copies of complete messages
		// and indistinguishable on the wire); with BOTH cabs restamping, the
		// estimates re-anchor on every real frame - see the impl log's risk
		// note.  Inert unless NAMCOS23_PATCH_HB_RESTAMP.
		if (m_hb_restamp_armed && num_payload_words >= 4)
		{
			std::size_t const n = frame.size();
			uint16_t const endhw = uint16_t((frame[n - 2] << 8) | frame[n - 1]);
			uint32_t const claimed = (uint32_t(frame[n - 3]) << 8) | (endhw & 0xff);
			if ((endhw & 0x100) && claimed == num_payload_words)
			{
				m_hb_peer_ctr = uint16_t((uint16_t(frame[0]) << 8) | frame[1]);
				m_hb_peer_ctr_time = machine().time();
				m_hb_peer_ctr_valid = true;
			}
		}

		// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace):
		// READ-ONLY ADOPT ingest line.  The cell stream the ROM validator
		// consumes is the LOW byte of each received halfword (see
		// log_qual_fingerprint); cell 2 = body0, cell 3 = op.  We log the
		// landing word (fifo_ptr - P014's "to word=0x01c6"), the resulting
		// m_ram word index (rx_base + fifo_ptr), the frame size, op/cellseq,
		// and the first 4 body cells so the analyst can confirm the op-0x17/
		// 0x71/0xfe actor/scene batch (cellseq 5a05 op 04, bulk >255hw) lands
		// where the driver-side adoption taps then watch it.  Inert unless
		// NAMCOS23_TRACE_ADOPT.
		if (m_trace_adopt && num_payload_words >= 4)
		{
			// cell(k) = low byte of received halfword k; safe-guarded so a
			// short frame cannot read past the payload.
			auto cell = [&frame, num_payload_words](std::size_t k) -> unsigned {
					return (k < num_payload_words) ? frame[k * 2 + 1] : 0; };
			unsigned const cellseq = (cell(0) << 8) | cell(1);
			bool const bulk = num_payload_words > 255;
			logerror("ADOPT: ingest n=%u t=%.6f bytes=%u hw=%u land_word=0x%04x ram_word=0x%04x bulk=%d cellseq=%04x op=%02x body=%02x %02x %02x %02x\n",
					++m_adopt_rx_index, machine().time().as_double(),
					static_cast<unsigned>(frame.size()),
					static_cast<unsigned>(num_payload_words),
					fifo_ptr, unsigned(rx_base + fifo_ptr), bulk ? 1 : 0,
					cellseq, cell(3), cell(2), cell(4), cell(5), cell(6));
		}

		// P018 EXPERIMENT (branch patch/op70-real-detector): READ-ONLY device-side
		// REAL op-70 cutscene-timer DETECTION.  Inert unless NAMCOS23_TRACE_OP70.
		// Replaces the P016 heuristic byte-scan (which flagged any 0x70 byte and
		// over-reported mid-payload operand bytes 0x11c6 / 0x4ea8) with a proper
		// gate-4 VM cell-walk (op70_cell_walk): it advances the cursor by each
		// op's true on-wire length and flags a 0x70 ONLY when the cursor lands on
		// it AS an opcode.  Logs OP70R: rx per genuine op-70 cell; pair with the
		// driver-side OP70: gate/status by t=.
		// P025 (branch patch/playclock-humangate-trace): the same walk also
		// reports op6F play-clock cells (PLAYCLOCK: op6f-rx) when
		// NAMCOS23_TRACE_PLAYCLOCK - answering "does the master's +0x390/394
		// clock beacon actually ARRIVE at this cab" (the slave adopts it into
		// its OWN record at 0x800B2504-08 only while its role byte 0x802F3FFC
		// bit7 is CLEAR; pair with the driver-side PLAYCLOCK: status by t=).
		// P044 (branch patch/anchor-lifecycle): and entity-lifecycle cells
		// (OP20: rx-release/rx-despawn) when NAMCOS23_TRACE_OP20 - a delivered
		// op-0x20 is a release THIS cab is about to ingest (RX 0x800ACD48 ->
		// release 0x800B5BC8 -> wave-anchor ring handle=-1): the suspected
		// killer of walk-phase anchors (P043b class ii).  Join to the
		// driver-side OP20: ring-kill lines by t= adjacency.
		if ((m_trace_op70 || m_trace_playclock || m_trace_op20) && num_payload_words >= 4)
		{
			auto cell = [&frame, num_payload_words](std::size_t k) -> unsigned {
					return (k < num_payload_words) ? frame[k * 2 + 1] : 0; };
			unsigned const cellseq = (cell(0) << 8) | cell(1);
			// The cell stream the ROM walks is the LOW byte of each received
			// halfword (frame[k*2+1]); pack it contiguously, then VM-walk it.
			std::vector<uint8_t> cells;
			cells.reserve(num_payload_words);
			for (std::size_t k = 0; k < num_payload_words; k++)
				cells.push_back(uint8_t(frame[k * 2 + 1]));
			op70_cell_walk("rx", cells.data(), cells.size(),
					num_payload_words > 255, cellseq, m_op70_rx_index);
		}

		// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): RX-side op55
		// wire-flag tap.  A LINKBITS: rxflags line here shows whether a DELIVERED
		// frame carries the 0x6000 fully-linked bits in its op55 24-bit flags - the
		// value the ROM op55 RX (0x800B058C -> 0x80013E10) lands into the PARTNER
		// record +0x370 (candidate (Z): on the wire but partner6000 still 0).
		// Inert unless NAMCOS23_TRACE_LINKBITS.
		if (m_trace_linkbits && num_payload_words >= 8)
		{
			std::vector<uint8_t> cells;
			cells.reserve(num_payload_words);
			for (std::size_t k = 0; k < num_payload_words; k++)
				cells.push_back(uint8_t(frame[k * 2 + 1]));
			linkbits_op55_scan("rx", cells.data(), cells.size());
		}
	}

	// Pulse the IRQ output line so the game's interrupt handler picks
	// up the frame even if its busy-wait poll-loop has already timed out
	// into solo mode.  We auto-clear via m_irq_pulse_timer after a short
	// delay so we never get stuck with a level-asserted IRQ that the game
	// fails to ack with the expected magic value.
	if (m_irq_pulse_timer)
	{
		m_irq_cb(ASSERT_LINE);
		m_irq_pulse_timer->adjust(attotime::from_usec(200));
	}
}


// P009 EXPERIMENT (branch patch/qual-trace): compact per-frame
// qualification fingerprint.  READ-ONLY; inert unless NAMCOS23_TRACE_QUAL.
//
// Goal: enough decoded fields per frame to diff the ~4/s frames that
// QUALIFY (reset the ROM drift word 0x802F3504 via the validated-frame
// dispatcher 0x8000B8F0) against the ~17/s that are merely DELIVERED.
// Field provenance (gold/rom 30-netplay + 40-gameplay-sync, pass6 atlas):
//   - The ROM's drain validator (0x8000BF38 Phase A) consumes the LOW
//     byte of each received halfword ("cell"); it requires slot length in
//     [4..0x400], cell 0 == 0x5A on one documented path, accumulates a
//     byte checksum over the drained cells, and only a ZERO sum reaches
//     0x8000B8F0 (which writes remote_seq -> 0x802F3510, body bytes ->
//     0x802F3502/03, drift word 0x802F3504 = local_seq - remote_seq).  A
//     non-zero sum increments gp+0x75C8 instead.
//   - cellseq = cells 0-1 big-endian = what 0x8000B8F0 will read as the
//     remote sequence number.
//   - The heartbeat replay stamps RAW payload bytes 0-1 (= halfword 0)
//     with the current local counter; tailid (rolling 16-bit hash over
//     raw bytes 4..end) is therefore IDENTICAL between a heartbeat replay
//     and the real TX it replays, and differs between distinct real TXs -
//     that classifies the peer's RX lines as real vs replay offline.
//   - endhw = last halfword raw (sender end-marker: bit 8 set, low byte =
//     size_lo); sizehi = low byte of the second-to-last halfword.
//   - lowsum = byte sum (mod 256) of ALL cells - the validator's zero-sum
//     test runs over its own drained range, so treat lowsum as a relative
//     discriminator between frames, not an absolute pass/fail.
// kind: "rx" (delivered, payload = raw frame), "tx-real" / "tx-hb"
// (payload = wire frame minus the 2-byte TCP size prefix).
void namco_c139_device::log_qual_fingerprint(const char *kind, const uint8_t *payload, std::size_t bytes)
{
	if (!m_trace_qual || !payload || bytes < 4)
		return;

	std::size_t const cells = bytes / 2;
	auto cell = [payload](std::size_t k) { return payload[k * 2 + 1]; };

	unsigned const cellseq = (unsigned(cell(0)) << 8) | cell(1);
	uint8_t const body0  = (cells > 2) ? cell(2) : 0;
	uint8_t const opcode = (cells > 3) ? cell(3) : 0;

	unsigned lowsum = 0;
	for (std::size_t k = 0; k < cells; k++)
		lowsum += cell(k);

	uint16_t tailid = 0;
	for (std::size_t i = 4; i < bytes; i++)
		tailid = uint16_t((uint16_t((tailid << 1) | (tailid >> 15))) + payload[i]);

	uint16_t const endhw  = uint16_t((payload[bytes - 2] << 8) | payload[bytes - 1]);
	uint8_t const sizehi  = payload[bytes - 3];

	unsigned const n = (kind[0] == 'r') ? ++m_qual_rx_index : ++m_qual_tx_index;
	logerror("QUAL_TRACE: %s n=%u t=%.6f bytes=%u cellseq=%04x raw01=%02x%02x body0=%02x op=%02x lowsum=%02x sizehi=%02x endhw=%04x tailid=%04x\n",
			kind, n, machine().time().as_double(), unsigned(bytes),
			cellseq, payload[0], payload[1], body0, opcode,
			lowsum & 0xff, sizehi, endhw, tailid);
}


// P018 EXPERIMENT (branch patch/op70-real-detector): REAL (non-heuristic) op-70
// cell-walk.  READ-ONLY; logs OP70R: lines only when m_trace_op70.
//
// WHY a walk (vs the P016 byte-scan): the gate-4 link-protocol payload is a
// VARIABLE-LENGTH byte stream.  The ROM dispatcher 0x800AA840 reads ONE opcode
// byte at the cursor, dispatches through table 0x8023F4D0 to a handler that
// consumes a fixed number of operand bytes and returns the advanced cursor, and
// loops until the 0x00 terminator.  So a byte value 0x70 is op-70 ONLY when the
// cursor LANDS on it as an opcode; a 0x70 that is an operand of another op (or a
// data byte inside a struct) is NOT op-70.  P016 flagged every 0x70 byte and so
// over-reported the fixed mid-payload operand bytes 0x11c6 / 0x4ea8 (cells
// 70 11 c6 / 70 4e a8 at fixed offsets in the 250/252hw gameplay frames).  This
// walk advances by each op's TRUE on-wire length and never lands the cursor on a
// mid-payload byte, so those false-positives cannot fire.
//
// OP-LENGTH TABLE (operand bytes consumed AFTER the opcode byte) - verified from
// gold/rom/40-gameplay-sync func-*.md + full.txt (table 0x8023F4D0):
//   0x00 -> terminator (stop the walk)
//   0x3B(0x800B2278)=2; 0x64/65/66(0x800B1C3C)=3; 0x67(0x800B1D04)=4;
//   0x68(0x800B1DDC)=4; 0x69(0x800B1E98)=4; 0x6A(0x800B1F60)=5;
//   0x6B(0x800B2034)=5; 0x6C/6D/6E(0x800B2100)=3; 0x6F(0x800B2448)=6;
//   0x70(0x800B2544)=2 <-- op-70, value = cur[0..1] big-endian & 0x7fff;
//   0x71(0x800B25AC)=1; 0x72(0x800B2620)=3; 0x73(0x800B26AC)=190(0xBE);
//   0x74(0x800B272C)=4.
//   Intrinsics handled inline by the dispatcher: 0xFD = 3 (FD b0 b1, may run as a
//   leading run); 0xFE / 0xFF = variable self-relative skip (NOT length-known
//   from a fixed table) -> treated as walk-stoppers (cannot safely continue).
//   ANY opcode not in this set -> UNKNOWN length -> STOP this walk (never guess;
//   guessing is exactly what produced the P016 mid-payload matches).
//
// START OFFSET: the device sees the raw delivered/sent frame, but the ROM VM
// walks a stream that sits AFTER an L2/L3/L4 header whose exact length depends on
// runtime gating (func-8000b8f0 -4 header + func-800aaf3c +3/+8 app header) that
// the device cannot reproduce read-only.  So we try every plausible start cell
// and require the walk from that start to consist ENTIRELY of known-length ops
// (a "clean" run) terminating at 0x00 or end-of-stream; only op-70 cells reached
// inside such a clean run are reported.  A 0x70 buried in a non-token region
// fails to anchor a clean known-op run and is therefore not reported.  Op-70 is
// rare, so reporting every genuine one (with value + cab + frame seq + cell idx)
// stays lean.
// P025 EXPERIMENT (branch patch/playclock-humangate-trace): the SAME walk also
// reports op6F PLAY-CLOCK cells when m_trace_playclock (independent of
// m_trace_op70; either gate runs the walk, each gate enables only its own
// report).  op6F = gate-4 table 0x8023F4D0[0x6F] = RX handler 0x800B2448:
// 6 operand bytes = TWO sign-extended 24-bit big-endian values (the sender's
// LOCAL record +0x390/+0x394 play-clock pair, TX-serialized by 0x800B22CC).
// Same acceptance discipline (clean 0x00-terminated run, >=2 known ops) - the
// P018 lesson: never a heuristic byte-scan.
// P044 EXPERIMENT (branch patch/anchor-lifecycle): the SAME walk also reports
// entity-lifecycle cells when m_trace_op20 - op-0x20 RELEASE (RX 0x800ACD48,
// EXACTLY 2 operand bytes = BE id16; the ingest path that marks the wave-
// anchor ring handle -1 via release 0x800B5BC8) and op-0x1F DESPAWN (RX
// 0x800ACCA0, EXACTLY 4 operand bytes = id16 + b1 + b2 -> 0x800B5E1C), both
// lengths VERIFIED in full.txt.  Their op-length table entries are GATED on
// m_trace_op20: unset, the walk (and thus the stacked OP70R/PLAYCLOCK
// detectors) is bit-exact with the parent branch; armed, clean runs can
// extend THROUGH 0x1F/0x20 cells (strictly more genuine reports possible).
void namco_c139_device::op70_cell_walk(const char *dir, const uint8_t *cells, std::size_t n, bool bulk, unsigned cellseq, uint32_t &index)
{
	if ((!m_trace_op70 && !m_trace_playclock && !m_trace_op20) || !cells || n < 3)
		return;

	// Operand bytes consumed AFTER the opcode byte; -1 = unknown/stopper.
	// (0x1F/0x20 are known ONLY while the P044 op20 trace is armed - see the
	// P044 header note; trace_op20 captured by value, the member is stable.)
	bool const know_lifecycle = m_trace_op20;
	auto op_operand_len = [know_lifecycle](uint8_t op) -> int {
			switch (op)
			{
			case 0x1F: return know_lifecycle ? 4 : -1; // P044: despawn id16+b1+b2 (VERIFIED 0x800ACCA0)
			case 0x20: return know_lifecycle ? 2 : -1; // P044: release id16 (VERIFIED 0x800ACD48)
			case 0x3B: return 2;
			case 0x64: case 0x65: case 0x66: return 3;
			case 0x67: return 4;
			case 0x68: return 4;
			case 0x69: return 4;
			case 0x6A: return 5;
			case 0x6B: return 5;
			case 0x6C: case 0x6D: case 0x6E: return 3;
			case 0x6F: return 6;
			case 0x70: return 2;   // op-70: value = cur[0..1] big-endian & 0x7fff
			case 0x71: return 1;
			case 0x72: return 3;
			case 0x73: return 190; // 0xBE
			case 0x74: return 4;
			case 0xFD: return 2;   // FD b0 b1 intrinsic (3 bytes total)
			default:   return -1;  // 0x00 terminator, 0xFE/0xFF variable, or unknown
			}
		};

	// Walk a clean known-op run from each candidate start cell.  To avoid
	// double-reporting the SAME op-70 cell from overlapping start offsets, only
	// report op-70 positions we have not already reported in THIS frame.
	std::vector<bool> reported(n, false);

	for (std::size_t start = 0; start + 2 < n; start++)
	{
		std::size_t cur = start;
		bool clean = true;             // run consists entirely of known-length ops
		bool terminated = false;       // run reached an explicit 0x00 terminator
		unsigned op_count = 0;         // ops decoded in this run (confidence signal)
		std::vector<std::size_t> op70s; // op-70 cursor positions found in THIS run
		std::vector<std::size_t> op6fs; // P025: op6F cursor positions found in THIS run
		std::vector<std::size_t> op20s; // P044: op-0x20 release cursor positions found in THIS run
		std::vector<std::size_t> op1fs; // P044: op-0x1F despawn cursor positions found in THIS run
		std::size_t guard = 0;         // iteration cap (defensive; the ROM has none)

		while (cur < n && guard++ < n)
		{
			uint8_t const op = cells[cur];
			if (op == 0x00)
			{
				terminated = true;  // explicit terminator -> a real token run ends here
				break;
			}

			int const len = op_operand_len(op);
			if (len < 0)
			{
				// 0xFE/0xFF (variable self-relative skip) or an opcode whose
				// length we do not know -> we cannot safely advance; abandon
				// this start without guessing.  (This is the discipline that
				// prevents the P016 mid-payload false-positives.)
				clean = false;
				break;
			}
			++op_count;

			if (op == 0x70)
			{
				// the value bytes must be present for a real op-70 cell
				if (cur + 2 >= n) { clean = false; break; }
				op70s.push_back(cur);
			}

			// P025: op6F play-clock cell - all 6 value bytes must be present
			// (the RX handler 0x800B2448 consumes exactly 6 after the opcode).
			if (op == 0x6F)
			{
				if (cur + 6 >= n) { clean = false; break; }
				op6fs.push_back(cur);
			}

			// P044: entity-lifecycle cells (reachable only while know_lifecycle,
			// i.e. m_trace_op20 - otherwise op_operand_len already stopped the
			// run above).  All operand bytes must be present, per the RX
			// handlers' exact consumption (0x800ACD48 = 2, 0x800ACCA0 = 4).
			if (op == 0x20)
			{
				if (cur + 2 >= n) { clean = false; break; }
				op20s.push_back(cur);
			}
			if (op == 0x1F)
			{
				if (cur + 4 >= n) { clean = false; break; }
				op1fs.push_back(cur);
			}

			// advance past the opcode byte + its operand bytes
			cur += std::size_t(1 + len);
		}

		// ACCEPTANCE: report op-70 cells ONLY when the run is CLEAN (every op a
		// known fixed length), reached an explicit 0x00 TERMINATOR, and decoded
		// at least 2 ops.  A real gate-4 token stream is a sequence of valid ops
		// ending in 0x00; a stray 0x70 data byte in a gameplay actor batch (the
		// P016 false-positives 0x11c6 / 0x4ea8) cannot anchor such a run - the
		// surrounding bytes hit an unknown op (e.g. 0x17) or 0xFE/0xFF and the
		// run is abandoned before any terminator.  This terminator+clean gate is
		// what makes the detector non-heuristic.  reported[] dedups the same cell
		// reached from overlapping start offsets within this frame.
		if (clean && terminated && op_count >= 2)
		{
			if (m_trace_op70)
			{
				for (std::size_t k : op70s)
				{
					if (reported[k])
						continue;
					reported[k] = true;
					unsigned const raw      = (unsigned(cells[k + 1]) << 8) | cells[k + 2];
					unsigned const peer_val = raw & 0x7fff;
					logerror("OP70R: %s n=%u t=%.6f cellidx=%u start=%u ops=%u peer_val=%u (raw=%04x masked=%04x) value=gp+0x7054(0x802CE874) hw=%u bulk=%d cellseq=%04x\n",
							dir, ++index, machine().time().as_double(),
							unsigned(k), unsigned(start), op_count, peer_val, raw, peer_val,
							unsigned(n), bulk ? 1 : 0, cellseq);
				}
			}

			// P025: report genuine op6F play-clock cells.  Decode the two 24-bit
			// big-endian operands exactly as the RX handler 0x800B2448 does
			// (sign-extend on bit 0x00800000; the handler stores them to the
			// receiver's OWN record +0x390/+0x394 at 0x800B2504-08 when its role
			// byte 0x802F3FFC bit7 is CLEAR).  Same dedup array as op-70 (a cell
			// position holds exactly one opcode, so the sets cannot collide).
			if (m_trace_playclock)
			{
				uint32_t &index6f = (dir[0] == 't') ? m_pc6f_tx_index : m_pc6f_rx_index;
				for (std::size_t k : op6fs)
				{
					if (reported[k])
						continue;
					reported[k] = true;
					uint32_t const raw1 = (uint32_t(cells[k + 1]) << 16) | (uint32_t(cells[k + 2]) << 8) | cells[k + 3];
					uint32_t const raw2 = (uint32_t(cells[k + 4]) << 16) | (uint32_t(cells[k + 5]) << 8) | cells[k + 6];
					int32_t const val1 = (raw1 & 0x00800000) ? int32_t(raw1 | 0xff000000) : int32_t(raw1);
					int32_t const val2 = (raw2 & 0x00800000) ? int32_t(raw2 | 0xff000000) : int32_t(raw2);
					logerror("PLAYCLOCK: op6f-%s n=%u t=%.6f cellidx=%u start=%u ops=%u val390=%d val394=%d (raw=%06x/%06x) target=rec+0x390/394(0x802F43F0/F4 slave-adopt @0x800B2504) hw=%u bulk=%d cellseq=%04x\n",
							dir, ++index6f, machine().time().as_double(),
							unsigned(k), unsigned(start), op_count, val1, val2, raw1, raw2,
							unsigned(n), bulk ? 1 : 0, cellseq);
				}
			}

			// P044: report genuine entity-lifecycle cells.  op-0x20 = 2 operand
			// bytes = the big-endian wire id16 (the big-map index the RX handler
			// 0x800ACD48 sign-extends and maps via 0x8010FE84 before calling
			// release 0x800B5BC8); op-0x1F = id16 + 2 param bytes (0x800ACCA0 ->
			// 0x800B5E1C).  An rx-release line is a release THIS cab is about to
			// ingest (the walk-phase anchor-killer candidate - join to the
			// driver's OP20: ring-kill by t=); a tx-release line is one this cab
			// queued (local release-with-notify, encoder 0x800ACD20).  Same
			// dedup array as op-70/op6F (one opcode per cell position).
			if (m_trace_op20)
			{
				uint32_t &index20 = (dir[0] == 't') ? m_op20_tx_index : m_op20_rx_index;
				for (std::size_t k : op20s)
				{
					if (reported[k])
						continue;
					reported[k] = true;
					unsigned const id16 = (unsigned(cells[k + 1]) << 8) | cells[k + 2];
					logerror("OP20: %s-release n=%u t=%.6f cellidx=%u start=%u ops=%u id16=%04x (op-0x20 -> 0x8010FE84 map -> entity -> release 0x800B5BC8 -> ring handle=-1; join driver ring-kill by t=) hw=%u bulk=%d cellseq=%04x\n",
							dir, ++index20, machine().time().as_double(),
							unsigned(k), unsigned(start), op_count, id16,
							unsigned(n), bulk ? 1 : 0, cellseq);
				}
				for (std::size_t k : op1fs)
				{
					if (reported[k])
						continue;
					reported[k] = true;
					unsigned const id16 = (unsigned(cells[k + 1]) << 8) | cells[k + 2];
					logerror("OP20: %s-despawn n=%u t=%.6f cellidx=%u start=%u ops=%u id16=%04x b1=%02x b2=%02x (op-0x1F -> 0x800B5E1C parameterized despawn) hw=%u bulk=%d cellseq=%04x\n",
							dir, ++index20, machine().time().as_double(),
							unsigned(k), unsigned(start), op_count, id16,
							cells[k + 3], cells[k + 4],
							unsigned(n), bulk ? 1 : 0, cellseq);
				}
			}
		}
	}
}


// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): READ-ONLY op55
// wire-flag tap.  See the header decl for full rationale.  This answers, off
// the wire, whether the partner-record 0x6000 fully-linked bits are even being
// transmitted (TX, candidate (Y)) and delivered (RX, candidate (Z)).
//
// op55 is gate-4 table entry 0x8023F4D0[0x55]=0x800B058C.  Its RX parse
// (0x800B05BC-0604) consumes, after the 0x55 opcode: hw(operand 0..1) +
// hw(operand 2..3) + 24-bit flags(operand 4..5..6) = $s2.  The call at
// 0x800B0738 stores $s2 | 0x40000000 | 0x80000000 into the PARTNER record
// rec1 +0x370 (0x80013E10).  $s2 = op[4]<<16 | op[5]<<8 | op[6], so the partner
// +0x370 0x6000 bits come from op[5] (the high byte of the low 16 = (flags>>8))
// & 0x60.  In the device CELL stream (low byte of each halfword), the operands
// follow the opcode cell: cell[op55] = 0x55, flags = cells[op55+5..+7].
//
// We scan every cell == 0x55 candidate and decode its flags; because op55 is a
// real, distinctive opcode that the dispatcher lands on, and because we report
// only the decoded value (no behaviour change), an occasional spurious 0x55 in
// a data region just produces an extra annotated line the analyst can grade by
// the flags value.  READ-ONLY; logs only when m_trace_linkbits.
void namco_c139_device::linkbits_op55_scan(const char *dir, const uint8_t *cells, std::size_t n)
{
	if (!m_trace_linkbits || !cells || n < 8)
		return;

	uint32_t &index = (dir[0] == 't') ? m_lb_tx_index : m_lb_rx_index;
	unsigned const cellseq = (unsigned(cells[0]) << 8) | cells[1];

	// Cap reported 0x55 candidates per frame: a real op55 (full actor sync) frame
	// carries a small number; a large bulk gate-4 batch can hold many incidental
	// 0x55 data bytes, so bound the volume while still surfacing whether 0x6000
	// is present.  The analyst grades each line by its flags value.
	unsigned hits = 0;
	for (std::size_t i = 0; i + 7 < n && hits < 8; i++)
	{
		if (cells[i] != 0x55)
			continue;
		++hits;
		// 24-bit flags = operand bytes 4/5/6 = cells[i+5..i+7] (operand 0 is
		// cells[i+1]).  Matches the RX parse 0x800B05EC-0604.
		uint32_t const flags24 = (uint32_t(cells[i + 5]) << 16)
				| (uint32_t(cells[i + 6]) << 8)
				| uint32_t(cells[i + 7]);
		bool const has6000 = (flags24 & 0x6000) == 0x6000;
		bool const has2000 = (flags24 & 0x2000) != 0;
		bool const has4000 = (flags24 & 0x4000) != 0;
		// the op55 RX also tests bit 0x0080 of the first flags byte for a
		// 0xFF00.. sign-extend (0x800B0608); report the raw 24-bit value so the
		// analyst can see the field exactly as parsed.
		logerror("LINKBITS: %sflags n=%u t=%.6f cellidx=%u flags24=%06x bit2000=%u bit4000=%u has6000=%u (op55 0x55 -> partner +0x370 via 0x80013E10) cellseq=%04x hw=%u\n",
				dir, ++index, machine().time().as_double(),
				unsigned(i), flags24, has2000 ? 1 : 0, has4000 ? 1 : 0,
				has6000 ? 1 : 0, cellseq, unsigned(n));
	}
}


// P021 EXPERIMENT (branch patch/linked-gate-tx-only, off patch/linked-gate-
// supply): WIRE-ONLY 0x6000 "fully-linked" injection into the OUTGOING frame
// copy.  See the header decl for the full rationale + invariant.
//
// INVARIANT (the whole point of P021): this function modifies ONLY the
// `payload` byte vector handed to emit_tx_frame.  That vector is a COPY read
// out of the C422 link RAM (m_ram) in send_pending_tx_frame's FIFO-readout
// loop (`payload.push_back(b)` from m_ram[word_idx]) - or a moved-in
// reassembled accumulation (m_chunk_accum), itself built from those same
// copies.  It is NOT m_ram and NOT the MIPS main RAM record at +0x370
// (0x802F43D0, which only the driver can address via m_mainram).  Therefore
// the local gun-actor (0x80013644) / own-record tick (0x80014CC0) NEVER see
// 0x2000 from this injection: their read is of game RAM, which is untouched.
// We advertise 0x6000 to the PEER (its op55 RX 0x800B058C lands the wire flags
// into ITS partner record -> peer gate7074==2) without poisoning our own live
// state - the separation P020 lacked.
//
// The 0x6000 bits ride op55's 24-bit wire flags at cells[op55+5..+7] (the same
// field linkbits_op55_scan decodes); 0x6000 = bits 13/14 = (flags24 & 0xff00)
// >> 8 & 0x60, i.e. the middle flag byte cells[op55+6].  In the device byte
// stream a "cell" is the LOW byte of each halfword, so cells[op55+6] is
// payload[(op55+6)*2 + 1].  OR-ing 0x60 into that byte sets both 0x2000 and
// 0x4000 on the wire (idempotent / OR-only).  Gated by the driver-pushed arm +
// mode==2 (matching the P020 mode==2 gate); emulation-thread only.
void namco_c139_device::linkbits_op55_inject_6000(std::vector<uint8_t> &payload)
{
	if (!m_lg_txonly_armed || !m_lg_txonly_mode2)
		return;

	uint32_t const payload_words = uint32_t(payload.size() / 2);
	if (payload_words < 8)
		return;   // need op55 opcode + 7 operand cells; matches the scan's n < 8 guard

	// Build the cell stream (low byte of each halfword) the same way the op55
	// scan / op-70 walk do, so we locate op55 at exactly the cell index the ROM
	// dispatcher would.  Cap candidates per frame at 8 (same bound as the scan)
	// so an incidental 0x55 data byte in a large bulk frame can't run away.
	std::vector<uint8_t> cells;
	cells.reserve(payload_words);
	for (uint32_t k = 0; k < payload_words; k++)
		cells.push_back(uint8_t(payload[k * 2 + 1]));

	unsigned hits = 0;
	for (std::size_t i = 0; i + 7 < cells.size() && hits < 8; i++)
	{
		if (cells[i] != 0x55)
			continue;
		++hits;
		// 0x6000 lives in operand byte 5 = cells[i+6] (the middle of the 24-bit
		// flags operand bytes 4/5/6 = cells[i+5..+7]); in the payload byte
		// vector that cell is the LOW byte of halfword (i+6): payload[(i+6)*2+1].
		std::size_t const flag_byte = (i + 6) * 2 + 1;
		if (flag_byte >= payload.size())
			continue;   // defensive; i+7 < cells.size() already bounds this
		uint8_t const before = payload[flag_byte];
		if ((before & 0x60) == 0x60)
			continue;   // already advertises 0x6000 - OR-only no-op
		payload[flag_byte] = uint8_t(before | 0x60);
		++m_lg_txonly_inject_count;
		logerror("LINKED_GATE_TXONLY: inject t=%.6f cellidx=%u flagbyte=%u before=%02x after=%02x count=%u (op55 wire 0x6000 -> peer partner +0x370 via 0x80013E10; LOCAL game RAM +0x370 untouched)\n",
				machine().time().as_double(), unsigned(i), unsigned(flag_byte),
				before, payload[flag_byte], m_lg_txonly_inject_count);
	}
}


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

void namco_c139_device::data_map(address_map &map)
{
	map(0x0000, 0x3fff).ram().share("sharedram");
}

void namco_c139_device::regs_map(address_map &map)
{
	map(0x00, 0x01).r(FUNC(namco_c139_device::status_r)); // WRITE clears flags
	map(0x02, 0x0f).rw(FUNC(namco_c139_device::reg_r), FUNC(namco_c139_device::reg_w));
//  map(0x0a, 0x0b) // WRITE tx_w
//  map(0x0c, 0x0d) // READ rx_r
//  map(0x0e, 0x0f) //
}

//-------------------------------------------------
//  namco_c139_device - constructor
//-------------------------------------------------

namco_c139_device::namco_c139_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, NAMCO_C139, tag, owner, clock),
	device_memory_interface(mconfig, *this),
	m_space_config("data", ENDIANNESS_BIG, 16, 14, 0, address_map_constructor(FUNC(namco_c139_device::data_map), this)),
	m_irq_cb(*this)
{
	std::fill(std::begin(m_regs), std::end(m_regs), uint16_t(0));
}


// Out-of-line so the compiler can emit the unique_ptr<context> destructor
// at a point where class context is a complete type.
namco_c139_device::~namco_c139_device() = default;




//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void namco_c139_device::device_start()
{
	m_ram = (uint16_t*)memshare("sharedram")->ptr();

	save_item(NAME(m_regs));

	// One-shot timer that auto-clears the IRQ output line a short time
	// after we assert it on RX delivery.  This implements pulse-style IRQ
	// semantics so we never get stuck with a level-asserted IRQ line that
	// the game's handler fails to ack.
	m_irq_pulse_timer = timer_alloc(FUNC(namco_c139_device::irq_pulse_off), this);

	// PHASE 9D: heartbeat timer that periodically replays the last real TX
	// to keep the partner's link-state dispatcher firing (which resets the
	// partner's timeout counter via state-counter sync).  REMOVE BEFORE COMMIT.
	m_heartbeat_timer = timer_alloc(FUNC(namco_c139_device::heartbeat_tick), this);

	// P002 EXPERIMENT (H2 "drift lockstep", branch patch/vblank-lockstep):
	// arm the per-vblank frame-token barrier.  P067 (patch/defaults-on):
	// ADOPTED - armed by DEFAULT; NAMCOS23_PATCH_VBLANK_LOCKSTEP=0 is the kill
	// switch (fully inert, the pre-P067 unset path), any other set value arms
	// as before.  The driver reads the same variable in its machine_start() -
	// keep the two resolutions agreeing.  Applied per-frame in vblank_tick().
	bool lockstep_from_env = false;
	char const *const lockstep_val = patch_env_or_default("NAMCOS23_PATCH_VBLANK_LOCKSTEP", "1", lockstep_from_env);
	m_lockstep_armed = lockstep_val != nullptr;
	if (m_lockstep_armed)
		logerror("VBLANK_LOCKSTEP: armed (NAMCOS23_PATCH_VBLANK_LOCKSTEP=%s %s) - frame-token per vblank, max lead %d frames, stall cap %d ms, suspend after %u idle timeouts\n",
				lockstep_val, patch_env_src(lockstep_from_env, lockstep_val).c_str(),
				LOCKSTEP_MAX_LEAD, LOCKSTEP_STALL_TIMEOUT_MS, LOCKSTEP_SUSPEND_AFTER);
	else
		logerror("VBLANK_LOCKSTEP: device DISABLED (NAMCOS23_PATCH_VBLANK_LOCKSTEP=0 kill switch - adopted default overridden; no frame-token barrier)\n");

	// P009 EXPERIMENT (branch patch/qual-trace): arm the read-only per-frame
	// qualification fingerprint from the environment, same gate idiom as the
	// other experiments and independent of all of them.  The driver-side
	// event tap in namcos23.cpp reads the same variable in machine_start().
	// Experiment branch only - never merge to milestone.
	char const *const qual_env = std::getenv("NAMCOS23_TRACE_QUAL");
	m_trace_qual = qual_env && qual_env[0] != '\0'
			&& !(qual_env[0] == '0' && qual_env[1] == '\0');
	if (m_trace_qual)
		logerror("QUAL_TRACE: device armed (NAMCOS23_TRACE_QUAL=%s) - per-frame rx / tx-real / tx-hb fingerprints (cellseq, body0, op, lowsum, end marker, tailid)\n",
				qual_env);

	// P010/P011 EXPERIMENT: arm TX-side chunked bulk-frame reassembly from the
	// environment, same gate idiom as the other experiments and independent of
	// all of them.  REUSES NAMCOS23_PATCH_CHUNK_REASSEMBLY so the user's run
	// command is unchanged - P011 simply replaces P010's now-superseded broken
	// behavior.  Experiment branch only - never merge to milestone.
	char const *const chunk_env = std::getenv("NAMCOS23_PATCH_CHUNK_REASSEMBLY");
	m_chunk_armed = chunk_env && chunk_env[0] != '\0'
			&& !(chunk_env[0] == '0' && chunk_env[1] == '\0');

	// P011 secondary debug toggle: NAMCOS23_CHUNK_ASSOC_ANNOUNCE=1 falls back
	// to P010's announce-flag association (treat the next chunk after a hold as
	// the continuation, unconditionally) instead of option (a) pointer
	// continuity.  Defaults to option (a) (pointer continuity) when unset.
	char const *const assoc_env = std::getenv("NAMCOS23_CHUNK_ASSOC_ANNOUNCE");
	bool const assoc_announce = assoc_env && assoc_env[0] != '\0'
			&& !(assoc_env[0] == '0' && assoc_env[1] == '\0');
	m_chunk_assoc_pointer = !assoc_announce;

	// P026 EXPERIMENT (branch patch/reasm-chunk-passthru): CHUNK PASS-THROUGH
	// emit mode.  CHUNK_PASSTHRU IMPLIES the reassembly machinery is armed (it
	// is the association engine pass-through rides on); when both are set,
	// pass-through wins the EMIT SHAPE (the P011 association and P014 commit
	// trigger are identical in both modes; only where the bytes go at the
	// hold/append/complete points differs).  When BOTH are off the TX path is
	// byte-for-byte stock.  P067 (patch/defaults-on): ADOPTED - armed by
	// DEFAULT; NAMCOS23_PATCH_CHUNK_PASSTHRU=0 is the kill switch (fully
	// inert, the pre-P067 unset path).  CHUNK_REASSEMBLY itself stays opt-in
	// (retired from the recipe), unchanged above.
	bool passthru_from_env = false;
	char const *const passthru_val = patch_env_or_default("NAMCOS23_PATCH_CHUNK_PASSTHRU", "1", passthru_from_env);
	m_chunk_passthru = passthru_val != nullptr;
	if (m_chunk_passthru)
		m_chunk_armed = true;   // implied: pass-through needs the chunk association machinery
	else
		logerror("CHUNK_PASSTHRU: DISABLED (NAMCOS23_PATCH_CHUNK_PASSTHRU=0 kill switch - adopted default overridden; stock TX emit shape unless CHUNK_REASSEMBLY is set)\n");

	if (m_chunk_armed)
		logerror("CHUNK_REASM: armed (NAMCOS23_PATCH_CHUNK_REASSEMBLY=%s) - P011 association=%s emit=%s; auto-advancing TX DMA pointer + %s (expected size from the cells at TXOFFSET-2/-1; max %u halfwords, stale after %d ms, per-vblank sweep)\n",
				chunk_env ? chunk_env : "(unset; implied by CHUNK_PASSTHRU)",
				m_chunk_assoc_pointer ? "pointer-continuity[a]" : "announce-flag[b]",
				m_chunk_passthru ? "chunk-passthru" : "coalesce",
				m_chunk_passthru
					? "forward each associated chunk as its OWN <=255-halfword wire frame"
					: "coalesce saturated 0xFF-halfword chunk sequences into one wire frame",
				CHUNK_MAX_HW, CHUNK_STALE_MS);

	if (m_chunk_passthru)
		logerror("CHUNK_PASSTHRU: armed (NAMCOS23_PATCH_CHUNK_PASSTHRU=%s %s) - P026: the ROM's saturated chunk sequence goes on the wire as SEPARATE hardware-shaped frames (the receiving ROM's RX ring reassembles them itself: marker back-scan 0x8000BD80 + forward drain 0x8000BF38, exactly as on real hardware; the receiver stacks back-to-back frames in its 4096-word RX ring at the advancing FIFO pointer - nothing is overwritten, no receiver pacing needed); P011 pointer-continuity association + P014 txsize_commit dispatch UNCHANGED (m_chunk_accum kept as the association tracker, never sent); RETIRED in this mode: rx_clear suppression (P025: 832 suppressions/104 s wedge on the never-committing 351-hw announce) and heartbeat replay while a chunk sequence is open; per-chunk CHUNK_PASSTHRU: fwd lines + 1/s status\n",
				passthru_val, patch_env_src(passthru_from_env, passthru_val).c_str());

	// P027 EXPERIMENT (branch patch/hb-cadence-wipe-restore): three
	// independently gated transport-cadence changes targeting the P026 run-1
	// blue jitter (40 mode-2 freshness dips, 38 in the cutscene at ~1.1/s:
	// the sparse snapshot+heartbeat exchange rides the ROM's 17-frame drift
	// ceiling with a 0-2 frame margin; aggravated by 116 rxclear wipe races
	// eating ~54% of red's staged chunk sends pre-wire).  Same gate idiom as
	// the other experiments; each is inert when unset.  Experiment branch
	// only - never merge to milestone.
	//
	// (a) NAMCOS23_PATCH_HB_CADENCE_MS=<ms>: heartbeat replay cadence.
	// Numeric; values outside [HB_CADENCE_MIN_MS..HB_CADENCE_MAX_MS] are
	// rejected with a log line (reject-to-stock 250 ms, unchanged).  P067
	// (patch/defaults-on): ADOPTED - default 33 ms (P053 daily-driver);
	// "0" is the kill switch = stock 250 ms cadence (the pre-P067 unset
	// path); any other set value overrides, parsed exactly as before.
	bool hbcad_from_env = false;
	char const *const hbcad_val = patch_env_or_default("NAMCOS23_PATCH_HB_CADENCE_MS", "33", hbcad_from_env);
	if (hbcad_val)
	{
		unsigned long const ms = std::strtoul(hbcad_val, nullptr, 10);
		if (ms >= HB_CADENCE_MIN_MS && ms <= HB_CADENCE_MAX_MS)
		{
			m_hb_cadence_ms = uint32_t(ms);
			m_hb_cadence_override = true;
			logerror("HB_CADENCE: armed (NAMCOS23_PATCH_HB_CADENCE_MS=%s %s) - heartbeat replay re-arm %u ms (stock 250 ms = a 0-2 frame margin against the ROM's reset-to-2/ceiling-17 drift budget); every real TX including forwarded bulk chunks re-arms the clock, so a replay fires only after %u ms of real-TX silence\n",
					hbcad_val, patch_env_src(hbcad_from_env, hbcad_val).c_str(), m_hb_cadence_ms, m_hb_cadence_ms);
		}
		else
			logerror("HB_CADENCE: NAMCOS23_PATCH_HB_CADENCE_MS=%s out of range [%lu..%lu] ms - IGNORED (stock 250 ms cadence)\n",
					hbcad_val, HB_CADENCE_MIN_MS, HB_CADENCE_MAX_MS);
	}
	else
		logerror("HB_CADENCE: DISABLED (NAMCOS23_PATCH_HB_CADENCE_MS=0 kill switch - adopted default 33 ms overridden; stock 250 ms cadence)\n");

	// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
	// NAMCOS23_PATCH_HB_PHASE_AWARE=<fast_ms> - PHASE-AWARE heartbeat token
	// cadence.  Read AFTER the HB_CADENCE block above so the banner reports the
	// true SAFE value (the recipe's 33 ms, or stock 250).  Value-checked
	// [HBPA_FAST_MIN_MS..HBPA_FAST_MAX_MS]; anything else (incl. non-numeric) is
	// refused with a one-shot banner and the gate stays off ("0" counts as
	// unset, matching the other gates).  Full rationale in the member-block
	// comment (namco_c139.h).  MODEL PROVENANCE: Fable 5.  Experiment branch
	// only - never merge to milestone.
	char const *const hbpa_env = std::getenv("NAMCOS23_PATCH_HB_PHASE_AWARE");
	if (hbpa_env && hbpa_env[0] != '\0' && !(hbpa_env[0] == '0' && hbpa_env[1] == '\0'))
	{
		unsigned long const fast_ms = std::strtoul(hbpa_env, nullptr, 10);
		if (fast_ms >= HBPA_FAST_MIN_MS && fast_ms <= HBPA_FAST_MAX_MS)
		{
			m_hbpa_fast_ms = uint32_t(fast_ms);
			m_hbpa_armed = true;
			logerror("HB_PHASE_AWARE: armed (NAMCOS23_PATCH_HB_PHASE_AWARE=%s) - heartbeat token cadence FAST=%u ms while staging mode-2 (in-game; debounced %u consecutive vblanks ~1 s), SAFE=%u ms otherwise (attract/handshake/mode-select - flat 16 ms broke op55 establishment there, P052); mode pushed by the driver each vblank (set_ingame), SAFE resumes IMMEDIATELY on mode-2 loss/reset; only WHEN the heartbeat fires changes - payload/regs[5] untouched; ARM ON BOTH CABS (each cab's token releases the PEER's emits)\n",
					hbpa_env, m_hbpa_fast_ms, HBPA_DEBOUNCE_VBLANKS, m_hb_cadence_ms);
		}
		else
			logerror("HB_PHASE_AWARE: NAMCOS23_PATCH_HB_PHASE_AWARE=%s out of range [%lu..%lu] ms - REFUSED (phase-aware cadence stays OFF; %u ms everywhere)\n",
					hbpa_env, HBPA_FAST_MIN_MS, HBPA_FAST_MAX_MS, m_hb_cadence_ms);
	}

	// P061 EXPERIMENT (branch patch/tx-complete-irq, off patch/hb-phase-aware):
	// NAMCOS23_PATCH_TX_COMPLETE_IRQ - model the C422 TX-complete so a staged
	// standalone TX is dispatched at its own stage instant (the chip's
	// txsize_commit trigger, gold mame/10-c139-register-map.md) instead of
	// waiting for a delivered peer frame's rx_clear to release the reg5
	// stop-and-wait.  Boolean idiom ("0"/empty = unset = byte-identical).
	// ACTIVE only while the P060 set_ingame debounced mode-2 state holds -
	// establishment keeps the stock stop-and-wait untouched.  Full rationale in
	// the member-block comment (namco_c139.h).  MODEL PROVENANCE: Fable 5.
	// Experiment branch only - never merge to milestone.
	char const *const txci_env = std::getenv("NAMCOS23_PATCH_TX_COMPLETE_IRQ");
	m_txci_armed = txci_env && txci_env[0] != '\0'
			&& !(txci_env[0] == '0' && txci_env[1] == '\0');
	if (m_txci_armed)
		logerror("TX_COMPLETE_IRQ: armed (NAMCOS23_PATCH_TX_COMPLETE_IRQ=%s) - C422 TX-COMPLETE MODEL: while the debounced staging mode-2 state holds (driver set_ingame push, %u consecutive vblanks ~1 s, dropped immediately on loss/reset), a freshly staged standalone TXSIZE (fresh TXOFFSET this passage, no unresolved bulk, ROM trailer test passes) is dispatched SYNCHRONOUSLY inside the reg_w that staged it via send_pending_tx_frame(), whose own sync TXSIZE clear is the TX-complete the ROM's pump gate 0x8000BB80 busy-polls - fresh emits at the ROM's 60 Hz compose cadence with ZERO stale-replay overhead (the heartbeat re-arms on every capture and organically starves).  Dedupe on frame seq+len caps dispatches at one per compose (hard cap %u/vblank); a duplicate re-stage is PARKED and cleared at the ROM's next TXSIZE busy-poll (the modeled TX completing; content-re-verified, strictly sacrificial - the one deliberate reg5 write, the audit-sanctioned exception).  rx_clear untouched; no extra IRQ pulse (status bit2 - the corpus-verified TX cause - is already permanently advertised).  Outside mode-2 the stock stop-and-wait is byte-identical.  ARM ON BOTH CABS.  Lines: one-shot first-dispatch/first-release + 1/s TX_COMPLETE_IRQ: status\n",
				txci_env, HBPA_DEBOUNCE_VBLANKS, TXCI_MAX_PER_VBLANK);

	// P062 EXPERIMENT (branch patch/txstage-trace, off patch/tx-complete-irq):
	// NAMCOS23_TRACE_TXSTAGE - READ-ONLY / LOG-ONLY per-stage TX content-hash
	// trace, the P061 post-mortem's "instrument FIRST" measurement (finding
	// 7.5).  Run with NAMCOS23_PATCH_TX_COMPLETE_IRQ UNSET so the STOCK
	// stop-and-wait pacing is what gets measured.  Boolean idiom ("0"/empty =
	// unset = byte-identical).  Full rationale in the member-block comment
	// (namco_c139.h).  MODEL PROVENANCE: Fable 5.  Experiment branch only -
	// never merge to milestone.
	char const *const ts_env = std::getenv("NAMCOS23_TRACE_TXSTAGE");
	m_ts_armed = ts_env && ts_env[0] != '\0'
			&& !(ts_env[0] == '0' && ts_env[1] == '\0');
	if (m_ts_armed)
		logerror("TXSTAGE: armed (NAMCOS23_TRACE_TXSTAGE=%s) - READ-ONLY per-stage TX content trace, BOTH cabs, stock pacing (run with TX_COMPLETE_IRQ unset): every non-zero TXSIZE stage logs one TXSTAGE: stage line (t/fr/k/n, len, seq cells 0-1, FNV-1a hash of the staged wire bytes, dup=prev-stage match, dupr=any-of-last-%u match, cont, ptr, gap_us); TXSIZE busy-poll reads and START rising edges are counted only; heartbeat captures are hashed and compared to the newest staged hash; 1/s TXSTAGE: status digest (stages/distinct/dups/zero, per-length class table, polls5, starts, hb_caps).  NO behavior change: no register/RAM writes, no sends - counters and log lines only\n",
				ts_env, TS_HIST);

	// P063 EXPERIMENT (branch patch/tx-complete-v2, off patch/txstage-trace):
	// NAMCOS23_PATCH_TX_COMPLETE_V2 - the P062-measured retry of the TX-complete
	// release.  Boolean idiom ("0"/empty = unset = byte-identical).  Refused if
	// the dead P061 env is also armed: the two release models act on the same
	// register/poll sites and must never run together.  Tunables:
	// NAMCOS23_PATCH_TXC2_BUSY_MS (modeled TX serialization interval,
	// [TXC2_BUSY_MIN_MS..TXC2_BUSY_MAX_MS], default TXC2_BUSY_DEFAULT_MS) and
	// NAMCOS23_PATCH_TXC2_STALE_MS (heartbeat replay age-out,
	// [TXC2_STALE_MIN_MS..TXC2_STALE_MAX_MS], default TXC2_STALE_DEFAULT_MS);
	// out-of-range values log and fall back to the default (the main gate stays
	// armed - a typo on a tunable must not silently change the experiment to a
	// stock run).  Full rationale in the member-block comment (namco_c139.h).
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	// P067 (patch/defaults-on): ADOPTED - the main gate is armed by DEFAULT;
	// NAMCOS23_PATCH_TX_COMPLETE_V2=0 is the kill switch (fully inert, the
	// pre-P067 unset path = stock reg5 stop-and-wait).  The dead-P061 refuse
	// guard below MUST keep working under default-on: default-armed V2 + the
	// P061 env set -> still refuse, loudly.  The driver reads the same main
	// gate in machine_start() for the mode push - keep the two agreeing.  The
	// two tunables: "0" (or unset) = the adopted default (TXC2_BUSY_DEFAULT_MS
	// / TXC2_STALE_DEFAULT_MS - a knob's kill switch reverts the OVERRIDE, the
	// patch itself stays armed); out-of-range still logs and falls back to the
	// default, exactly as before.
	bool txc2_from_env = false;
	char const *const txc2_val = patch_env_or_default("NAMCOS23_PATCH_TX_COMPLETE_V2", "1", txc2_from_env);
	m_txc2_armed = txc2_val != nullptr;
	if (!m_txc2_armed)
		logerror("TX_COMPLETE_V2: device DISABLED (NAMCOS23_PATCH_TX_COMPLETE_V2=0 kill switch - adopted default overridden; stock reg5 stop-and-wait release)\n");
	if (m_txc2_armed && m_txci_armed)
	{
		m_txc2_armed = false;
		logerror("TX_COMPLETE_V2: REFUSED (NAMCOS23_PATCH_TX_COMPLETE_IRQ is also armed; V2 was armed %s) - the P061 and P063 release models share the reg5 stage/poll sites and must never run together; unset TX_COMPLETE_IRQ to run V2\n",
				patch_env_src(txc2_from_env, txc2_val).c_str());
	}
	if (m_txc2_armed)
	{
		m_txc2_busy_ms = uint32_t(TXC2_BUSY_DEFAULT_MS);
		bool txc2_busy_overridden = false;
		char const *const txc2_busy_env = std::getenv("NAMCOS23_PATCH_TXC2_BUSY_MS");
		if (txc2_busy_env && txc2_busy_env[0] != '\0'
				&& !(txc2_busy_env[0] == '0' && txc2_busy_env[1] == '\0'))
		{
			unsigned long const ms = std::strtoul(txc2_busy_env, nullptr, 10);
			if (ms >= TXC2_BUSY_MIN_MS && ms <= TXC2_BUSY_MAX_MS)
			{
				m_txc2_busy_ms = uint32_t(ms);
				txc2_busy_overridden = true;
			}
			else
				logerror("TX_COMPLETE_V2: NAMCOS23_PATCH_TXC2_BUSY_MS=%s out of range [%lu..%lu] ms - using default %lu ms\n",
						txc2_busy_env, TXC2_BUSY_MIN_MS, TXC2_BUSY_MAX_MS, TXC2_BUSY_DEFAULT_MS);
		}
		m_txc2_stale_ms = uint32_t(TXC2_STALE_DEFAULT_MS);
		bool txc2_stale_overridden = false;
		char const *const txc2_stale_env = std::getenv("NAMCOS23_PATCH_TXC2_STALE_MS");
		if (txc2_stale_env && txc2_stale_env[0] != '\0'
				&& !(txc2_stale_env[0] == '0' && txc2_stale_env[1] == '\0'))
		{
			unsigned long const ms = std::strtoul(txc2_stale_env, nullptr, 10);
			if (ms >= TXC2_STALE_MIN_MS && ms <= TXC2_STALE_MAX_MS)
			{
				m_txc2_stale_ms = uint32_t(ms);
				txc2_stale_overridden = true;
			}
			else
				logerror("TX_COMPLETE_V2: NAMCOS23_PATCH_TXC2_STALE_MS=%s out of range [%lu..%lu] ms - using default %lu ms\n",
						txc2_stale_env, TXC2_STALE_MIN_MS, TXC2_STALE_MAX_MS, TXC2_STALE_DEFAULT_MS);
		}
		logerror("TX_COMPLETE_V2: armed (NAMCOS23_PATCH_TX_COMPLETE_V2=%s %s) busy_ms=%u %s stale_ms=%u %s hist=%u - P062-measured TX-complete release: while the debounced staging mode-2 state holds (driver set_ingame push, %u consecutive vblanks ~1 s, dropped immediately on loss/reset), a freshly staged standalone TXSIZE whose FNV-1a content hash differs from the previous dispatched stage is transmitted SYNCHRONOUSLY at its stage instant (prev-hash admission gate + %u-deep ring; TXSIZE=0 writes ignored entirely); after each dispatch (and for a duplicate re-stage landing on an idle serializer) the TXSIZE busy-poll reads a synthesized BUSY for busy_ms then clears - passages pace at the stock 70-100/s texture, NOT the P061 spin; a parked duplicate is released (reg5 -> 0) at the first post-window poll; when ACTIVE the heartbeat replay is SUPPRESSED if its captured payload is older than stale_ms (the red >255hw stale-replay channel; capture logic itself untouched).  Outside mode-2 the stock stop-and-wait is byte-identical.  ARM ON BOTH CABS.  Lines: one-shot ACTIVE/INACTIVE/first-dispatch/first-release/first-stale + 1/s TX_COMPLETE_V2: status\n",
				txc2_val, patch_env_src(txc2_from_env, txc2_val).c_str(),
				m_txc2_busy_ms, patch_env_src(txc2_busy_overridden, txc2_busy_env).c_str(),
				m_txc2_stale_ms, patch_env_src(txc2_stale_overridden, txc2_stale_env).c_str(),
				unsigned(TXC2_HIST), HBPA_DEBOUNCE_VBLANKS, unsigned(TXC2_HIST));
	}

	// (b) NAMCOS23_PATCH_WIPE_RESTORE=1: one-shot restore of a staged+
	// announced bulk-chunk TXSIZE through a peer rx_clear wipe (the head's
	// stage->START-edge race deliver_rx_frames loses by construction).  Acts
	// only on the CHUNK_PASSTHRU bulk state, once per bulk announce; a second
	// wipe of the same stage yields with a log line - never the P025
	// suppression fight (that suppression stays retired).
	char const *const wiperes_env = std::getenv("NAMCOS23_PATCH_WIPE_RESTORE");
	m_wipe_restore_armed = wiperes_env && wiperes_env[0] != '\0'
			&& !(wiperes_env[0] == '0' && wiperes_env[1] == '\0');
	if (m_wipe_restore_armed)
	{
		logerror("WIPE_RESTORE: armed (NAMCOS23_PATCH_WIPE_RESTORE=%s) - one-shot restore of a staged+announced bulk-chunk TXSIZE through a peer rx_clear (budget: ONE per bulk announce, re-armed only by the ROM's next TXOFFSET bulk announce; a repeat wipe of the same stage logs rxclear-restore-yielded and yields - NEVER the P025 infinite suppression fight, which stays retired); lines: rxclear-restaged-txsize / rxclear-restore-yielded / rxclear-wiped-staged-txsize + 1/s WIPE_RESTORE: status\n",
				wiperes_env);
		if (!m_chunk_passthru)
			logerror("WIPE_RESTORE: NOTE - NAMCOS23_PATCH_CHUNK_PASSTHRU is not armed, so the pass-through bulk state the restore predicate requires never exists; the gate is effectively inert this run\n");
	}

	// (c) NAMCOS23_PATCH_HB_RESTAMP=1 (reserve knob, unset in run 1): stamp
	// replayed heartbeats from the peer's last-seen counter + elapsed frames
	// instead of m_local_counter, so the receiver's drift reset lands near
	// transit latency.  P026: red rode drift 13-14 through the whole cutscene
	// on blue's 250 ms heartbeats - 3 frames under the timeout ceiling for
	// 60 s - because the reset offset is whatever the device counter happens
	// to be vs the peer ROM's counter, and it is ours to set.
	char const *const restamp_env = std::getenv("NAMCOS23_PATCH_HB_RESTAMP");
	m_hb_restamp_armed = restamp_env && restamp_env[0] != '\0'
			&& !(restamp_env[0] == '0' && restamp_env[1] == '\0');
	if (m_hb_restamp_armed)
		logerror("HB_RESTAMP: armed (NAMCOS23_PATCH_HB_RESTAMP=%s) - replayed-heartbeat counter stamped as peer_last_seen_counter + elapsed_frames (sampled only from COMPLETE delivered messages: trailer bit-8 marker + claimed-length==frame-halfwords, so chunk fragments can never poison it; counter domain aged at %.0f Hz); falls back to the stock local_counter stamp when no sample yet or sample older than %d ms\n",
				restamp_env, HB_COUNTER_HZ, HB_RESTAMP_MAX_AGE_MS);

	// P031 EXPERIMENT (branch patch/announce-latch): the announce-latch
	// dispatcher - the code-side fix for the stage->START rx_clear announce
	// race behind every room/area skip (P026/P028/P029/P030, one signature,
	// not recipe-fixable: loss probability rises with the peer arrival
	// rate).  Same gate idiom as the other experiments; inert when unset.
	// Requires CHUNK_PASSTHRU (the bulk-announce recognition and the wipe
	// diagnostic site it hooks live in that mode) - a NOTE is logged and
	// nothing ever arms without it.  Experiment branch only - never merge.
	// P067 (patch/defaults-on): ADOPTED - default mode 3 (v3 snapshot+dedupe,
	// the P038 generation); NAMCOS23_PATCH_ANNOUNCE_LATCH=0 is the kill switch
	// (fully inert, the pre-P067 unset path); 1/2/3 select the generation
	// exactly as before.
	bool al_from_env = false;
	char const *const al_val = patch_env_or_default("NAMCOS23_PATCH_ANNOUNCE_LATCH", "3", al_from_env);
	m_al_armed = al_val != nullptr;
	if (!m_al_armed)
		logerror("ANNOUNCE_LATCH: DISABLED (NAMCOS23_PATCH_ANNOUNCE_LATCH=0 kill switch - adopted default 3 overridden; no announce latch)\n");
	// P035 (branch patch/latch-v2-snapshot): the VALUE selects the latch
	// generation.  "2" = v2, the WIPE-TIME PAYLOAD SNAPSHOT: the wipe-capture
	// copies the staged head payload out of C422 RAM at the wipe instant and
	// the START-edge dispatch transmits THAT copy instead of re-reading the
	// ring slot (which the ROM may already be recomposing - the P033 boss
	// flood tore ~32% of flood-window dispatch re-reads: 127 of red's 133
	// marker=MISMATCH completes + blue's first 2 chkfails of the latch era).
	// "1" (or any other armed value, preserving the pre-P035 truthy-string
	// semantics) = the P031 v1 dispatch-time re-read, byte-identical; unset/
	// "0" = fully inert as always.  Rails (TTL, one-shot, supersede, DMA-
	// pointer equality) and the v1 counters are IDENTICAL in both modes.
	// P038 (branch patch/latch-v3-dedupe): "3" = v3 - the FULL v2 snapshot
	// behavior PLUS dispatch dedupe: a START-edge dispatch whose
	// (offset, expected_hw) class already completed on the wire within
	// AL_DEDUPE_LOOKBACK_MS of its announce is consumed WITHOUT sending (the
	// P036 stale +150 ms torn/duplicate class - ALL 56 MISMATCHes were such
	// dispatches); genuine rescues (no recent same-class completion, the
	// P031 room-1 save) dispatch exactly as v2.  "2" and "1" remain
	// byte-identical to their tested behaviors.
	long const al_mode = m_al_armed ? std::strtol(al_val, nullptr, 10) : 0;
	m_al_snapshot = al_mode == 2 || al_mode == 3;
	m_al_dedupe = al_mode == 3;
	if (m_al_armed)
	{
		logerror("ANNOUNCE_LATCH: armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - latch (offset,expected_hw,t) at each bulk TXOFFSET announce; the peer rx_clear wipe of a staged TXSIZE proceeds EXACTLY as before (green light never fought, register file never written by the latch) but is REMEMBERED; when the ROM's START edge then reads regs[5]==0-because-wiped, the send is reconstructed from the latch (byte-identical ROM-staged payload, one-shot per announce, TTL %d ms, superseded by any newer ROM stage); lines: announce-latch: latched/wiped/dispatched/expired/superseded + 1/s ANNOUNCE_LATCH: status\n",
				al_val, patch_env_src(al_from_env, al_val).c_str(), AL_TTL_MS);
		if (m_al_snapshot)
		{
			m_al_snap.reserve(AL_SNAP_MAX_BYTES);
			logerror("ANNOUNCE_LATCH: v2 snapshot armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - the wipe-capture COPIES the staged head payload bytes (up to %u) at the wipe instant (provably pristine: the ROM had just staged them, the wipe hits only the register) and the START-edge dispatch transmits the SNAPSHOT instead of re-reading C422 RAM (the P033 torn-dispatch fix; the bulk REMAINDER is unaffected - its P014 commit trigger already sends synchronously at stage time inside the very reg_w that staged it, no re-read window exists); v1 rails/counters unchanged; lines: wiped ... snap=/snap_bytes=/snap_copied=, dispatched-snap/dispatched-reread + snap_copied/snap_tx/snap_fallback on the 1/s status; P037 genstamp rider (LOG-ONLY): dispatched-snap lines carry gen=same|differ (ring slot's CURRENT first-2-hw vs the snapshot, read-only - the wire still carries the snapshot), a latch-dispatched message's P014 remainder commit logs announce-latch: remainder gen=same|differ head_age_ms=, and the 1/s status gains gen_differ=\n",
					al_val, patch_env_src(al_from_env, al_val).c_str(), AL_SNAP_MAX_BYTES);
			// P038 (branch patch/latch-v3-dedupe): v3 = v2 + dedupe.  The v2
			// banner above intentionally still prints under =3 (v3 contains
			// all v2 behavior; its grep keys stay valid).
			if (m_al_dedupe)
				logerror("ANNOUNCE_LATCH: v3 dedupe armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - v2 snapshot behavior PLUS dispatch dedupe: every wire completion of a latched class (natural consumed_ok head sends AND latch dispatches; deduped consumptions are NOT recorded) enters a %u-entry (offset,expected_hw,t) ring; a START-edge dispatch whose class already completed within %d ms of its announce is CONSUMED WITHOUT SENDING (the P036 measured defect: ALL 56 torn completes were latch dispatches of content that had completed naturally ~150 ms earlier - the ROM's cadence re-announce was wiped by the peer as flow control and the latch un-dropped it as a stale head-gen-N/tail-gen-N+k duplicate that passes the byte-sum checksum and is INGESTED); the genuine rescue class (no recent same-class completion - the P031 room-1 save) dispatches exactly as v2; a same-class re-announce that retires a pending wiped dispatch is now counted (refresh-retired-wiped, expect 0 in floods); lines: announce-latch: deduped / refresh-retired-wiped + deduped=/refresh_retired= on the 1/s status\n",
						al_val, patch_env_src(al_from_env, al_val).c_str(), unsigned(AL_DEDUPE_RING), AL_DEDUPE_LOOKBACK_MS);
		}
		if (!m_chunk_passthru)
			logerror("ANNOUNCE_LATCH: NOTE - NAMCOS23_PATCH_CHUNK_PASSTHRU is not armed, so the bulk-announce state and wipe-diagnostic site the latch hooks never exist; the gate is effectively inert this run\n");
	}

	// P013 EXPERIMENT (branch patch/tx-emitter-trace): arm the READ-ONLY TX
	// register-programming trace from the environment, same gate idiom as the
	// other experiments and independent of all of them.  Inert (no log, no
	// behavior change) unless NAMCOS23_TRACE_TXEMIT is set to a non-empty value
	// other than "0".  Experiment branch only - never merge to milestone.
	char const *const txemit_env = std::getenv("NAMCOS23_TRACE_TXEMIT");
	m_txemit_trace = txemit_env && txemit_env[0] != '\0'
			&& !(txemit_env[0] == '0' && txemit_env[1] == '\0');
	if (m_txemit_trace)
		logerror("TXEMIT: armed (NAMCOS23_TRACE_TXEMIT=%s) - READ-ONLY trace of the TX register-programming sequence (REG[0x06]/START=m_regs[3], REG[0x0A]/TXSIZE=m_regs[5], REG[0x0E]/TXOFFSET=m_regs[7]), every socket send, and TXSIZE busy/clear (TX-complete) reads/writes; cursor=m_regs[7] (device image of gp+0x75CA+2), slot=(cursor-2)/0x400. Locates the >255hw bulk REMAINDER: regw/send/status-read/done lines, grep-decomposable\n",
				txemit_env);

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): arm the
	// READ-ONLY device-side half of the adoption trace, same gate idiom as the
	// other experiments and independent of all of them.  Inert (no log, no
	// behavior change) unless NAMCOS23_TRACE_ADOPT is set to a non-empty value
	// other than "0".  The driver reads the same env var in its own
	// machine_start() for the render-gate/cutscene/actor taps.  Experiment
	// branch only - never merge to milestone.
	char const *const adopt_env = std::getenv("NAMCOS23_TRACE_ADOPT");
	m_trace_adopt = adopt_env && adopt_env[0] != '\0'
			&& !(adopt_env[0] == '0' && adopt_env[1] == '\0');
	if (m_trace_adopt)
		logerror("ADOPT: device armed (NAMCOS23_TRACE_ADOPT=%s) - READ-ONLY per-delivered-frame ingest line (landing word, op/cellseq, first body words) so the bulk op-0x17/0x71/0xfe batch landing is paired with the driver-side ADOPT render-gate/cutscene/actor taps by t=\n",
				adopt_env);

	// P018 EXPERIMENT (branch patch/op70-real-detector, off
	// patch/op70-cutscene-timer-arm): arm the READ-ONLY device-side REAL op-70
	// detection, same gate idiom as the others and independent of all of them.
	// Inert (no log, no behavior change) unless NAMCOS23_TRACE_OP70 is set to a
	// non-empty value other than "0".  Env var name REUSED so the run command is
	// unchanged; this REPLACES the P016 heuristic byte-scan with a real cell-walk
	// (op70_cell_walk) on BOTH directions.  The driver reads the same env var in
	// its own machine_start() for the (unchanged) op-70 gate / accept-reject tap.
	// Experiment branch only - never merge to milestone.
	char const *const op70_env = std::getenv("NAMCOS23_TRACE_OP70");
	m_trace_op70 = op70_env && op70_env[0] != '\0'
			&& !(op70_env[0] == '0' && op70_env[1] == '\0');
	if (m_trace_op70)
		logerror("OP70R: device armed (NAMCOS23_TRACE_OP70=%s) - READ-ONLY REAL op-70 cutscene-timer DETECTION (gate-4 VM cell-walk advancing by each op's true on-wire length, flags 0x70 ONLY as a genuine opcode; op table 0x8023F4D0[0x70]=0x800B2544 -> 0x80016E28, value cur[0..1]&0x7fff); walks delivered RX (OP70R: rx) AND emitted TX (OP70R: tx, answers does-this-cab-emit-op-70); replaces the P016 heuristic byte-scan; pair with driver-side OP70: gate/accept-reject (gp+0x7054=0x802CE874) by t=\n",
				op70_env);

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm, off
	// patch/op70-real-detector): arm the READ-ONLY device-side op55 wire-flag
	// tap, same gate idiom as the others and independent of all of them.  Inert
	// (no log, no behavior change) unless NAMCOS23_TRACE_LINKBITS is set to a
	// non-empty value other than "0".  The driver reads the SAME env var in its
	// own machine_start() for the +0x370 0x6000 sample; this device half decodes
	// the op55 24-bit wire flags so (Y)/(Z) can be read off the wire.  Experiment
	// branch only - never merge to milestone.
	char const *const linkbits_env = std::getenv("NAMCOS23_TRACE_LINKBITS");
	m_trace_linkbits = linkbits_env && linkbits_env[0] != '\0'
			&& !(linkbits_env[0] == '0' && linkbits_env[1] == '\0');
	if (m_trace_linkbits)
		logerror("LINKBITS: device armed (NAMCOS23_TRACE_LINKBITS=%s) - READ-ONLY op55 WIRE-FLAG tap: scans the cell stream for op55 (0x8023F4D0[0x55]=0x800B058C) and decodes the 24-bit flags at operand bytes 4/5/6 (cells[op55+5..+7]); the ROM op55 RX 0x800B058C lands flags|0xC0000000 into the PARTNER record +0x370 (0x80013E10), so flags&0x6000 = whether the fully-linked bits are on the wire; LINKBITS: txflags (emit, candidate (Y) does-this-cab-send-0x6000) + LINKBITS: rxflags (delivered, candidate (Z) is-0x6000-delivered); pair with driver-side LINKBITS: status (local6000/partner6000/t7074) by t=\n",
				linkbits_env);

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace, off
	// patch/op55-carrier-repeat): arm the READ-ONLY device-side REAL op6F
	// play-clock detection, same gate idiom as the others and independent of all
	// of them.  Inert (no log, no behavior change) unless NAMCOS23_TRACE_PLAYCLOCK
	// is set to a non-empty value other than "0".  Reuses the P018 op70_cell_walk
	// (the non-heuristic gate-4 VM cell-walk) on BOTH directions; the driver reads
	// the SAME env var in its own machine_start() for the per-vblank
	// +0x390/+0x394 / bit-0x0004 / role-byte sampling.  Experiment branch only -
	// never merge to milestone.
	char const *const playclock_env = std::getenv("NAMCOS23_TRACE_PLAYCLOCK");
	m_trace_playclock = playclock_env && playclock_env[0] != '\0'
			&& !(playclock_env[0] == '0' && playclock_env[1] == '\0');
	if (m_trace_playclock)
		logerror("PLAYCLOCK: device armed (NAMCOS23_TRACE_PLAYCLOCK=%s) - READ-ONLY REAL op6F PLAY-CLOCK detection (gate-4 VM cell-walk, the P018 op70_cell_walk discipline: 0x6F counts ONLY as a genuine opcode inside a clean 0x00-terminated run; op table 0x8023F4D0[0x6F]=0x800B2448, 6 operand bytes = TWO sign-extended 24-bit BE values = the sender's rec+0x390/+0x394 play-clock pair, TX emitter 0x800B22CC cadence (frame&0xFF)==0 gated keepalive>0 + role 0x802F3FFC bit7 SET, slave adopts into OWN rec +0x390/394 @0x800B2504-08 when bit7 CLEAR); PLAYCLOCK: op6f-tx (emitted) / op6f-rx (delivered); pair with driver-side PLAYCLOCK: status/edges by t=\n",
				playclock_env);

	// P044 EXPERIMENT (branch patch/anchor-lifecycle, off patch/wave-init-hold):
	// arm the READ-ONLY device-side entity-lifecycle (op-0x20 release / op-0x1F
	// despawn) wire detection, same gate idiom as the others and independent of
	// all of them.  Inert (no log, no walk-behavior change) unless
	// NAMCOS23_TRACE_OP20 is set to a non-empty value other than "0".  Reuses
	// the P018 op70_cell_walk on BOTH directions; the 0x1F/0x20 op-length table
	// entries it needs are gated on this flag so the stacked OP70R/PLAYCLOCK
	// detectors stay bit-exact when it is unset.  The driver reads the SAME env
	// var in its own machine_start() for the per-vblank wave-anchor
	// ring-transaction sampler (OP20: ring-kill/ring-reg/born-dead-commit).
	// Experiment branch only - never merge to milestone.
	char const *const op20_env = std::getenv("NAMCOS23_TRACE_OP20");
	m_trace_op20 = op20_env && op20_env[0] != '\0'
			&& !(op20_env[0] == '0' && op20_env[1] == '\0');
	if (m_trace_op20)
		logerror("OP20: device armed (NAMCOS23_TRACE_OP20=%s) - READ-ONLY entity-lifecycle wire detection (gate-4 VM cell-walk, P018 discipline: an 0x20/0x1F counts ONLY as a genuine opcode inside a clean 0x00-terminated run of >=2 known ops): op-0x20 RELEASE (table 0x8023F4D0[0x20]=0x800ACD48, 2 operand bytes = BE id16 -> big-map 0x8010FE84 -> entity -> release 0x800B5BC8 -> wave-anchor ring @0x802D2030 handle=-1 = the walk-phase anchor-killer candidate) and op-0x1F DESPAWN (0x800ACCA0, 4 operand bytes = id16+b1+b2 -> 0x800B5E1C); both lengths VERIFIED in full.txt. Lines: OP20: rx-release/rx-despawn (delivered - about to be ingested) and OP20: tx-release/tx-despawn (emitted). Wire id16 is the big-map index, NOT the ring's 32-bit anchor id - join to driver OP20: ring-kill lines by t= adjacency. Armed-only interplay: clean walk runs can now extend THROUGH 0x1F/0x20 cells, so OP70R/PLAYCLOCK may see strictly MORE genuine cells than in earlier runs\n",
				op20_env);

	// P050 EXPERIMENT (branch patch/single-burst-pump): device-side arms.  Same
	// "non-empty and not literal 0" gate idiom as every gate above; both
	// independent and inert when unset.  MODEL PROVENANCE: Opus 4.8.
	//   (a) NAMCOS23_PATCH_BURST_QUANTUM: the DEVICE half of the single-burst
	//       pump.  The driver owns the RAM-code poke @0x8000BC78; the device
	//       reads the SAME env so its two companions (latch-retire + heartbeat
	//       clock re-arm for >255-hw single bursts) arm.  Self-guarding: they
	//       only fire when a >255-hw single burst actually reaches the send/emit
	//       path, so a DRC-stale poke (ROM still chunking) leaves them dormant.
	//   (b) NAMCOS23_PATCH_NEWEST_WINS: the newest-wins delivery cap in
	//       deliver_rx_frames (see the member-block comment + the supersede
	//       loop).
	// P067 (patch/defaults-on): BURST_QUANTUM ADOPTED - device half armed by
	// DEFAULT (the driver reads the same var in machine_start() for the poke -
	// keep the two resolutions agreeing); "0" is the kill switch (fully inert,
	// the pre-P067 unset path).  NEWEST_WINS is NOT adopted (dropped after
	// P051/P052) and keeps the old armed-only-when-set idiom.
	bool bq_from_env = false;
	char const *const bq_val = patch_env_or_default("NAMCOS23_PATCH_BURST_QUANTUM", "1", bq_from_env);
	m_bq_armed = bq_val != nullptr;
	if (!m_bq_armed)
		logerror("BURST_QUANTUM: device DISABLED (NAMCOS23_PATCH_BURST_QUANTUM=0 kill switch - adopted default overridden; stock 0xFF-hw chunking companions)\n");
	char const *const nw_env = std::getenv("NAMCOS23_PATCH_NEWEST_WINS");
	m_newest_wins = nw_env && nw_env[0] != '\0'
			&& !(nw_env[0] == '0' && nw_env[1] == '\0');
	m_pump_instr = m_bq_armed || m_newest_wins;
	if (m_bq_armed)
		logerror("BURST_QUANTUM: device armed (NAMCOS23_PATCH_BURST_QUANTUM=%s %s) - companion to the driver's @0x8000BC78 slti 0x100->0x401 poke: a >255-hw single-burst VM frame (frame_size == expected_hw, != 0xFF) crosses UNCHANGED via the self-contained passthrough (the saturation heuristic keys on EXACTLY 0xFF hw) and reassembles as one frame in the peer's RX ring (validator accepts <= 0x400 hw). Two self-guarding companions: (1) retire the announce-latch + record dedupe at the single-burst whole-frame send (the first-saturated consumed_ok site never runs for a single burst); (2) a >255-hw single burst re-arms the heartbeat clock so the keepalive replay stays suppressed under real large-frame traffic (also protects gate (b)'s newest-wins from a stale small replay out-arriving a fresh fight frame)\n",
				bq_val, patch_env_src(bq_from_env, bq_val).c_str());
	if (m_newest_wins)
		logerror("NEWEST_WINS: device armed (NAMCOS23_PATCH_NEWEST_WINS=%s) - deliver_rx_frames keeps only the NEWEST complete-standalone VM frame in each inbound drain and drops the older complete ones (superseded_frames++, per-event line). complete-standalone = the ROM strict trailer test (bit-8 end marker AND claimed len == frame's own hw count), which a multi-chunk fragment NEVER passes, so CHUNK_PASSTHRU ring reassembly is untouched (sound WITH or WITHOUT gate (a)). Safe by the P049 RE F3: state re-serializes + the 32-slot reliable backlog re-publishes until 0xFD-acked, so the kept newest frame carries the freshest state + all outstanding slots (a dropped instance is re-sent by design). Accepted: a dropped op6F clock carrier delays resync <= ~4.3 s (PLAYCLOCK rails watch it)\n",
				nw_env);
	if (m_pump_instr)
		logerror("NEWEST_WINS: FIFO-join instrumentation armed (either pump gate) - per-complete-frame 'tx-join'/'rx-join seq=' stamps (seq = cellseq = the ROM per-frame drift sequence 0x8000B8F0-914) + 1/s frames-delivered fps per direction; join red tx-join seq=X to blue rx-join seq=X for the red->blue delivery latency (P048 method)\n");

	// P068 (branch patch/linkplay-menu): register the per-machine cfg
	// <linkplay> node (cfg\<system>.cfg).  Registration must happen here in
	// device_start - configuration_manager::load_settings runs right after
	// start_all_devices (machine.cpp) and only calls handlers registered by
	// then.  The load handler also hosts the deferred comm bring-up at config
	// FINAL (start_comm_cfg).  MODEL PROVENANCE: Fable 5.
	machine().configuration().config_register(
			"linkplay",
			configuration_manager::load_delegate(&namco_c139_device::linkplay_config_load, this),
			configuration_manager::save_delegate(&namco_c139_device::linkplay_config_save, this));

	// Bring up the asio context if -comm_localhost / -comm_remotehost are
	// configured on the command line.  P068: when the CLI is at MAME defaults
	// this now DEFERS to the cfg/loopback-defaults path (start_comm_cfg at
	// config FINAL) instead of unconditionally staying solo; note the P055/P059
	// banners below print the role from m_comm_is_connector, which on the
	// deferred path is still false here - trace runs use CLI launches, where
	// the role is already resolved at this point.
	start_comm();

	// P055 EXPERIMENT (branch patch/boat-jitter-trace): device-side arm for the
	// READ-ONLY boat-jitter ingest bridge, env-gated by NAMCOS23_TRACE_BOATJITTER
	// (same "non-empty and not literal 0" gate as every trace above; inert/byte-
	// identical when unset).  The driver reads the SAME env var in its own
	// machine_start() for the per-vblank BLUE entity-position trace.  Placed AFTER
	// start_comm() so the connector/listener role is already known for the banner.
	// MODEL PROVENANCE: Opus 4.8.
	//
	// P056 EXPERIMENT (branch patch/boat-render-trace): the P056 rendered-position +
	// liveness trace reuses this SAME device bridge (rx-apply generation +
	// FRESH/REPLAY classifier), so arm it when EITHER trace's env var is set.  Both
	// are READ-ONLY and the bridge is byte-identical when disarmed.
	char const *const bj_env = std::getenv("NAMCOS23_TRACE_BOATJITTER");
	char const *const br_env = std::getenv("NAMCOS23_TRACE_BOATRENDER");
	char const *const pc_env = std::getenv("NAMCOS23_TRACE_POSCORR");
	// P065 (branch patch/corr-smooth): the driver's CORR_SMOOTH blend keys its
	// ingest detection on the SAME rx-apply-generation bridge, so arm it on
	// NAMCOS23_PATCH_CORR_SMOOTH too (rx_apply_gen() must tick even with all
	// three traces unset, and on BOTH cabs - the classifier below has never
	// been connector-gated).  The bridge stays READ-ONLY device bookkeeping.
	char const *const cs_env = std::getenv("NAMCOS23_PATCH_CORR_SMOOTH");
	bool const bj_on = bj_env && bj_env[0] != '\0' && !(bj_env[0] == '0' && bj_env[1] == '\0');
	bool const br_on = br_env && br_env[0] != '\0' && !(br_env[0] == '0' && br_env[1] == '\0');
	bool const pc_on = pc_env && pc_env[0] != '\0' && !(pc_env[0] == '0' && pc_env[1] == '\0');
	bool const cs_on = cs_env && cs_env[0] != '\0' && !(cs_env[0] == '0' && cs_env[1] == '\0');
	m_bj_armed = bj_on || br_on || pc_on || cs_on;
	m_poscorr_trace = pc_on;
	if (m_bj_armed)
		logerror("BOATJITTER: device armed (NAMCOS23_TRACE_BOATJITTER=%s NAMCOS23_TRACE_BOATRENDER=%s NAMCOS23_TRACE_POSCORR=%s NAMCOS23_PATCH_CORR_SMOOTH=%s) - READ-ONLY per delivered-complete-frame FRESH/REPLAY classifier + rx-apply generation shared by the P055 boat-jitter, P056 boat-render and P059 poscorr driver traces and the P065 corr-smooth blend. A delivered complete VM frame is REPLAY when it matches the previous complete frame from byte 2 onward (heartbeat replay = last real TX with only the counter halfword restamped; a moving boat makes every FRESH compose differ). This cab is the %s\n",
				bj_env ? bj_env : "(unset)", br_env ? br_env : "(unset)", pc_env ? pc_env : "(unset)", cs_env ? cs_env : "(unset)",
				m_comm_is_connector ? "CONNECTOR/blue (INGEST side - the traces run here)" : "LISTENER/red (owner side - driver traces stand down; the P065 blend runs on BOTH cabs)");

	// P059 EXPERIMENT (branch patch/poscorr-trace): device-side arm for the
	// READ-ONLY RX arrival/delivery/drop instrumentation (the FIX FAMILY B
	// headroom).  Counts run ONLY on the connector/blue and only when armed, so
	// this is byte-identical when unset or on red.  The 1/s 'POSCORR: dev' rail is
	// emitted from vblank_tick.
	if (m_poscorr_trace)
		logerror("POSCORR: device armed (NAMCOS23_TRACE_POSCORR=%s) - READ-ONLY RX arrival/delivery/drop counters in deliver_rx_frames (connector/blue only). Per 1/s 'POSCORR: dev' rail: complete VM frames ARRIVED from the socket vs DELIVERED into the RX ring (split FRESH/REPLAY) vs DROPPED (newest-wins collapse @namco_c139.cpp:1545 - CONDITIONAL on m_newest_wins, so 0 while NAMCOS23_PATCH_NEWEST_WINS is unset = INERT, not unconditional - and sub-2-word greetings), plus max queue depth (max_pending) and burst-drain count. arrived==delivered => no device drop = the throttle is the upstream stop-and-wait emit (B hard); arrived>>delivered => recoverable headroom (B easy). newest_wins=%d this run. This cab is the %s\n",
				pc_env, m_newest_wins ? 1 : 0,
				m_comm_is_connector ? "CONNECTOR/blue (INGEST side - counting LIVE)" : "LISTENER/red (owner side - counting stands down)");
}


// P002 EXPERIMENT (H2 "drift lockstep", branch patch/vblank-lockstep).
//
// Called by the host driver once per frame on the vblank rising edge,
// on the emulation thread.  Two jobs:
//
//   1. Send a frame-token control frame carrying our vblank counter so
//      the peer can measure how far ahead/behind we are in EMULATED time.
//   2. Barrier: if we are more than LOCKSTEP_MAX_LEAD frames ahead of the
//      peer's last token (after subtracting the launch-stagger baseline
//      offset captured at link-up), block this thread until the peer
//      catches up or LOCKSTEP_STALL_TIMEOUT_MS of wall-clock passes.
//
// Stall mechanism: we simply sleep-poll on the emulation thread inside
// this callback.  While we are blocked here the MAME scheduler cannot
// advance, so emulated time (CPUs, timers, our own TX) is frozen - which
// is exactly the semantics a hardware-crystal-locked cabinet pair has.
// Trade-offs (documented in the agent log): video/input/audio hitch for
// the stall duration, and after a stall MAME's throttle sees emulated
// time behind wall clock and runs the next frames unthrottled until it
// catches up - the barrier (not the throttle) remains the authority on
// how far ahead we may get, so mutual drift stays capped at
// LOCKSTEP_MAX_LEAD either way.
//
// Robustness:
//   - Inert unless armed, connected, and at least one peer token has
//     arrived (so an unpatched/solo/booting peer can never stall us).
//   - The launch stagger (-DelaySeconds) makes raw frame counters differ
//     by ~50 frames; we baseline the offset on the first peer token and
//     cap RELATIVE drift from link-up (which is also roughly where the
//     ROM's own gp+0x75B8 counters align, per the P001 run-2 analysis).
//   - Every stalled vblank is capped at LOCKSTEP_STALL_TIMEOUT_MS.  After
//     LOCKSTEP_SUSPEND_AFTER consecutive full timeouts we look at whether
//     the peer's token advanced over the streak: if it did not (peer hung,
//     died, or was closed) we SUSPEND the barrier and free-run, resuming
//     (with a fresh baseline) only when peer tokens flow again; if it did
//     advance (pathological mutual-stall / baseline skew) we re-baseline
//     instead.  |drift| > LOCKSTEP_REBASE_DRIFT is treated as a
//     discontinuity and also re-baselines.
//
// Experiment branch only - never merge to milestone.
void namco_c139_device::vblank_tick()
{
	// P013 EXPERIMENT (branch patch/tx-emitter-trace): advance the independent
	// per-vblank frame counter that tags every TXEMIT: line (so the trace has a
	// frame= field regardless of whether lockstep is armed), and emit a 1/s
	// TXEMIT status line.  READ-ONLY; inert unless NAMCOS23_TRACE_TXEMIT.  The
	// status line surfaces the candidate-(iii) signal as an aggregate: if
	// txsize_zero_reads keeps climbing while no non-zero TXSIZE read ever logs,
	// the ROM never observed a busy chip - the synchronous-done model means the
	// remainder is never gated on a TX-complete signal we model.
	if (m_txemit_trace)
	{
		++m_txemit_frame;
		if (++m_txemit_status_div >= 60)
		{
			m_txemit_status_div = 0;
			logerror("TXEMIT: status fr=%u t=%.6f sends=%u txsize_zero_reads=%u tx_ptr=0x%04x slot=%u txoffset=0x%04x txsize=%u held_accum_hw=%u held_expected_hw=%u resume=0x%04x saw_txoffset=%d\n",
					m_txemit_frame, machine().time().as_double(), m_txemit_sends,
					m_txemit_txsize_zero_reads, m_chunk_tx_ptr, txemit_slot_of(m_chunk_tx_ptr),
					m_regs[7], unsigned(m_regs[5] & 0xff), unsigned(m_chunk_accum.size() / 2),
					m_chunk_held_expected_hw, m_chunk_resume_ptr,
					m_chunk_saw_txoffset ? 1 : 0);
		}
	}

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only): ~1/s status line for
	// the WIRE-ONLY 0x6000 injection.  The arm + mode==2 gate are pushed from the
	// driver each vblank (set_linked_gate_txonly); the injection itself happens in
	// emit_tx_frame.  This surfaces, once per second, whether the injection is
	// armed, whether mode==2 is currently in effect, and the cumulative count of
	// op55 wire-flag bytes that have been 0x6000-injected onto the wire - so the
	// run can confirm the wire now carries 0x6000 (pair with the device
	// LINKBITS: txflags has6000 -> peer rxflags has6000 -> peer partner6000 ->
	// gate7074 chain).  Runs before the lockstep gate (called unconditionally
	// every vblank).  Experiment branch only - never merge to milestone.
	if (m_lg_txonly_armed)
	{
		if (++m_lg_txonly_status_div >= 60)
		{
			m_lg_txonly_status_div = 0;
			logerror("LINKED_GATE_TXONLY: status t=%.6f armed=1 mode2=%d injected_total=%u (op55 wire 0x6000 advertised to peer; LOCAL game RAM +0x370 NOT modified)\n",
					machine().time().as_double(), m_lg_txonly_mode2 ? 1 : 0,
					m_lg_txonly_inject_count);
		}
	}

	// P050 (branch patch/single-burst-pump): 1/s status for the pump gates -
	// frames-delivered fps per direction (the primary falsifier: fights should
	// climb from 5.5-11/s toward the warm 12-19/s band), the newest-wins
	// supersede count, and the BURST_QUANTUM single-burst-send count (nonzero =
	// the ROM poke took and large frames are crossing as single bursts; still 0
	// in fights while poked=1 => the DRC compiled a stale pre-poke block).  Runs
	// whenever either gate is armed.  Experiment branch only - never merge.
	if (m_pump_instr)
	{
		if (++m_nw_status_div >= 60)
		{
			m_nw_status_div = 0;
			logerror("NEWEST_WINS: status t=%.6f newest_wins=%d burst_quantum=%d tx_fps=%u rx_fps=%u tx_complete=%u rx_complete=%u superseded=%u single_burst_tx=%u (tx/rx_fps = complete VM frames/s this cab emits/delivers; join tx-join<->peer rx-join by seq for red->blue latency)\n",
					machine().time().as_double(),
					m_newest_wins ? 1 : 0, m_bq_armed ? 1 : 0,
					m_nw_tx_win, m_nw_rx_win,
					m_nw_tx_complete, m_nw_rx_complete,
					m_nw_superseded, m_bq_single_burst_tx);
			m_nw_tx_win = 0;
			m_nw_rx_win = 0;
		}
	}

	// P061 EXPERIMENT (branch patch/tx-complete-irq): per-vblank dispatch-cap
	// reset + 1/s status rail, BOTH cabs (the POSCORR dev rail below is
	// connector-only; red's dispatch economy needs its own line).  dispatches
	// ~55-60/s with dup_skips of the same order = the model pacing exactly one
	// distinct frame per compose; cap_skips must stay 0 (a nonzero value means
	// the dedupe missed and the hard cap saved the peer - investigate);
	// ingame=0 windows must show dispatches=0 (the mode-2 gate holding).
	if (m_txci_armed)
	{
		m_txci_vbl_dispatches = 0;
		if (++m_txci_status_div >= 60)
		{
			m_txci_status_div = 0;
			logerror("TX_COMPLETE_IRQ: status t=%.6f ingame=%d dispatches=%u dup_skips=%u releases=%u cap_skips=%u bulk_skips=%u incomplete_skips=%u | tot_dispatch=%u tot_release=%u (dispatches = staged TXs transmitted at their own stage instant - the C422 txsize_commit/TX-complete model; releases = parked duplicate stages cleared at the busy-poll; expect ~55-60/s dispatches in mode-2, 0 outside; cap_skips must stay 0)\n",
					machine().time().as_double(), m_hbpa_ingame ? 1 : 0,
					m_txci_dispatch_win, m_txci_dup_skips_win, m_txci_release_win,
					m_txci_cap_skips_win, m_txci_bulk_skips_win,
					m_txci_incomplete_skips_win,
					m_txci_dispatch_tot, m_txci_release_tot);
			m_txci_dispatch_win = 0;
			m_txci_dup_skips_win = 0;
			m_txci_release_win = 0;
			m_txci_cap_skips_win = 0;
			m_txci_bulk_skips_win = 0;
			m_txci_incomplete_skips_win = 0;
		}
	}

	// P063 EXPERIMENT (branch patch/tx-complete-v2): 1/s status rail, BOTH
	// cabs.  Healthy in-game window (P062 calibration): dispatches ~45-60
	// (tracking fresh compose; content floors 20-30 in repeat-heavy moments),
	// gate_skips = the strictly-adjacent dup re-stages (~20-50), ring_skips ~0
	// (A-B-A transients only), zero_ignored ~0 steady / bursting at
	// transitions, busy_holds in the thousands (the ~99%-busy poll texture),
	// releases of the same order as gate_skips, stale_suppressed 0 in
	// 006c-phases and >0 only in capture-silent >255hw stretches, midwin 0,
	// bulk/incomplete 0.  ingame=0 windows must show every counter 0 except
	// possibly stale/zero at the transition edge (the mode-2 gate holding).
	if (m_txc2_armed)
	{
		if (++m_txc2_status_div >= 60)
		{
			m_txc2_status_div = 0;
			logerror("TX_COMPLETE_V2: status t=%.6f ingame=%d dispatches=%u gate_skips=%u ring_skips=%u zero_ignored=%u busy_holds=%u releases=%u stale_suppressed=%u midwin=%u bulk_skips=%u incomplete_skips=%u | tot_dispatch=%u tot_stale=%u busy_ms=%u stale_ms=%u (dispatches = fresh-content stages transmitted at their stage instant, prev-hash gated; busy_holds = polls answered by the modeled TX-busy window; releases = parked duplicate stages cleared at the first post-window poll; expect dispatches ~45-60/s in-game with busy_holds in the thousands and midwin 0)\n",
					machine().time().as_double(), m_hbpa_ingame ? 1 : 0,
					m_txc2_dispatch_win, m_txc2_dup_skip_win, m_txc2_ring_skip_win,
					m_txc2_zero_win, m_txc2_busy_hold_win, m_txc2_release_win,
					m_txc2_stale_win, m_txc2_midwin_win, m_txc2_bulk_skip_win,
					m_txc2_incomplete_win, m_txc2_dispatch_tot, m_txc2_stale_tot,
					m_txc2_busy_ms, m_txc2_stale_ms);
			m_txc2_dispatch_win = 0;
			m_txc2_dup_skip_win = 0;
			m_txc2_ring_skip_win = 0;
			m_txc2_zero_win = 0;
			m_txc2_busy_hold_win = 0;
			m_txc2_release_win = 0;
			m_txc2_stale_win = 0;
			m_txc2_midwin_win = 0;
			m_txc2_bulk_skip_win = 0;
			m_txc2_incomplete_win = 0;
		}
	}

	// P062 EXPERIMENT (branch patch/txstage-trace): advance the per-vblank
	// frame counter tagging every TXSTAGE: stage line, reset the stage#-within-
	// vblank index (so k= shows the 2/vblank pattern directly), and emit the
	// 1/s TXSTAGE status digest.  distinct = stages - dup_recent = fresh
	// composed contents this second (the ceiling a P061b TX-complete release
	// could unlock); dup_recent = true content-identical re-stages (what a
	// corrected gate must actually filter); polls5/starts = the stock
	// busy-poll / pump-passage back-pressure baseline a modeled TX-BUSY
	// interval must reproduce; cls = per-length class table (the class-aware
	// key evidence); hb_caps/hb_cap_match + cap_is_newest = heartbeat-capture
	// vs newest-composed divergence.  READ-ONLY; inert unless armed.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	if (m_ts_armed)
	{
		++m_ts_frame;
		m_ts_vbl_stage_idx = 0;
		if (++m_ts_status_div >= 60)
		{
			m_ts_status_div = 0;
			char cls[224];
			int pos = 0;
			for (unsigned i = 0; i < TS_CLS_MAX; i++)
			{
				if (m_ts_cls[i].stages == 0)
					continue;
				int const n = std::snprintf(cls + pos, sizeof(cls) - pos, "%s%04x:%u/%u",
						(pos > 0) ? " " : "", unsigned(m_ts_cls[i].len),
						m_ts_cls[i].stages, m_ts_cls[i].fresh);
				if (n > 0 && n < int(sizeof(cls)) - pos)
					pos += n;
			}
			if (m_ts_cls_other_stages != 0)
			{
				int const n = std::snprintf(cls + pos, sizeof(cls) - pos, "%sother:%u/%u",
						(pos > 0) ? " " : "", m_ts_cls_other_stages, m_ts_cls_other_fresh);
				if (n > 0 && n < int(sizeof(cls)) - pos)
					pos += n;
			}
			cls[pos] = '\0';
			bool const cap_is_newest = m_ts_hbcap_valid && m_ts_last_stage_valid
					&& m_ts_hbcap_hash == m_ts_last_stage_hash;
			logerror("TXSTAGE: status t=%.6f stages=%u distinct=%u dup_prev=%u dup_recent=%u zero=%u polls5=%u polls5_busy=%u starts=%u hb_caps=%u hb_cap_match=%u cap_is_newest=%d cls=[%s] | tot_stages=%u (distinct=stages-dup_recent = fresh composed contents/s; dup_recent = true content-identical re-stages; polls5/starts = stock back-pressure baseline; cls = len:stages/fresh per staged length; hb_cap_match = captures that were the newest stage at capture instant)\n",
					machine().time().as_double(),
					m_ts_stages_win, m_ts_stages_win - m_ts_dup_recent_win,
					m_ts_dup_prev_win, m_ts_dup_recent_win, m_ts_zero_stage_win,
					m_ts_poll5_win, m_ts_poll5_busy_win, m_ts_start_edge_win,
					m_ts_hbcap_win, m_ts_hbcap_match_win, cap_is_newest ? 1 : 0,
					cls, m_ts_stage_seq);
			m_ts_stages_win = 0;
			m_ts_dup_prev_win = 0;
			m_ts_dup_recent_win = 0;
			m_ts_zero_stage_win = 0;
			m_ts_poll5_win = 0;
			m_ts_poll5_busy_win = 0;
			m_ts_start_edge_win = 0;
			m_ts_hbcap_win = 0;
			m_ts_hbcap_match_win = 0;
			for (unsigned i = 0; i < TS_CLS_MAX; i++)
			{
				m_ts_cls[i].len = 0;
				m_ts_cls[i].stages = 0;
				m_ts_cls[i].fresh = 0;
			}
			m_ts_cls_other_stages = 0;
			m_ts_cls_other_fresh = 0;
		}
	}

	// P059 EXPERIMENT (branch patch/poscorr-trace): 1/s device-side RX
	// arrival/delivery/drop rail on the connector/blue - the FIX FAMILY B headroom
	// measurement.  arrived_complete==delivered_complete (dropped_*==0) => no device
	// drop = the throttle is the upstream stop-and-wait emit (B hard: must raise the
	// emit cadence, bounded because 16 ms broke establishment); arrived_complete>>
	// delivered_complete => recoverable headroom (B easy: deliver the dropped
	// frames).  dropped_newest_wins is the RE-fingered @1545 collapse: it is
	// CONDITIONAL on newest_wins, so 0 here while NAMCOS23_PATCH_NEWEST_WINS is
	// unset = the drop is INERT, not unconditional.  Counters are bumped in
	// deliver_rx_frames (same emulation thread).  Inert unless armed + connector.
	if (m_poscorr_trace && m_comm_is_connector)
	{
		if (++m_pc_status_div >= 60)
		{
			m_pc_status_div = 0;
			// P060 (branch patch/hb-phase-aware): append the current heartbeat
			// phase to this existing 1/s rail ONLY when HB_PHASE_AWARE is armed
			// (empty %s otherwise = the P059 line stays byte-identical, old
			// parsers unaffected).
			logerror("POSCORR: dev t=%.6f arrived_complete=%u delivered_complete=%u delivered_fresh=%u delivered_replay=%u dropped_newest_wins=%u dropped_small=%u arrived_other=%u drains=%u multidrain=%u max_pending=%u newest_wins=%d | tot arrived=%u delivered=%u dropped_nw=%u%s (arrived==delivered => no device drop = upstream-emit bound / B hard; arrived>>delivered => recoverable headroom / B easy; delivered_fresh = actual new-position corrections/s)\n",
					machine().time().as_double(),
					m_pc_arrived_complete_win, m_pc_delivered_complete_win,
					m_pc_delivered_fresh_win, m_pc_delivered_replay_win,
					m_pc_dropped_newest_win, m_pc_dropped_small_win,
					m_pc_arrived_other_win, m_pc_drains_win, m_pc_multidrain_win,
					m_pc_max_pending_win, m_newest_wins ? 1 : 0,
					m_pc_arrived_complete_tot, m_pc_delivered_complete_tot,
					m_pc_dropped_newest_tot,
					!m_hbpa_armed ? "" : (m_hbpa_ingame ? " hbphase=FAST" : " hbphase=SAFE"));
			m_pc_arrived_complete_win = 0;
			m_pc_delivered_complete_win = 0;
			m_pc_delivered_fresh_win = 0;
			m_pc_delivered_replay_win = 0;
			m_pc_dropped_newest_win = 0;
			m_pc_dropped_small_win = 0;
			m_pc_arrived_other_win = 0;
			m_pc_drains_win = 0;
			m_pc_multidrain_win = 0;
			m_pc_max_pending_win = 0;
		}
	}

	// P010/P011 EXPERIMENT: per-vblank stale sweep + 1/s status line for a
	// pending reassembly whose continuation never arrived (e.g. peer
	// disconnected mid-message, chunk lost, or the continuation never resumed
	// the pointer).  Runs before the lockstep gate - the driver calls
	// vblank_tick() unconditionally every vblank.
	//
	// P011: sweep EVERY vblank (not only once per second).  Under option (a) a
	// real continuation resumes within a few frames, so a hold that survives
	// the CHUNK_STALE_MS window is a genuine abandon - surfacing it within ~17
	// frames (the 500 ms timeout) rather than up to 1 s later makes a
	// sequencing regression obvious in the logs immediately.  The timeout
	// itself stays at 500 ms (a generous backstop; the primary completion path
	// is the next triggered continuation send, not this sweep).
	if (m_chunk_armed)
	{
		if (!m_chunk_accum.empty()
				&& machine().time() - m_chunk_accum_since > attotime::from_msec(CHUNK_STALE_MS))
			chunk_drop("stale-sweep");

		if (++m_chunk_status_div >= 60)
		{
			m_chunk_status_div = 0;
			logerror("CHUNK_REASM: status t=%.6f reassembled=%u dropped=%u continued=%u passthru_bulk=%u pending_hw=%u expected_hw=%u msg_chunks=%u bulk_pending=%d rxclear_suppressed=%u\n",
					machine().time().as_double(), m_chunk_reassembled, m_chunk_dropped,
					m_chunk_continued, m_chunk_passthru_during_bulk,
					unsigned(m_chunk_accum.size() / 2), m_chunk_expected_hw,
					m_chunk_msg_chunks, m_chunk_bulk_pending ? 1 : 0,
					m_chunk_rxclear_suppressed);
			// P026: pass-through counters, 1/s next to the CHUNK_REASM status.
			// open=1 + a non-advancing open_hw across consecutive lines = a
			// message whose chunks stopped arriving (the 500 ms stale sweep
			// retires it - watch for the paired drop reason=stale-sweep).
			if (m_chunk_passthru)
			{
				logerror("CHUNK_PASSTHRU: status t=%.6f msgs=%u chunks_fwd=%u complete=%u open=%d open_hw=%u/%u hb_held=%u rxclear_wiped=%u\n",
						machine().time().as_double(), m_chunk_pt_msg_id,
						m_chunk_pt_chunks_fwd, m_chunk_pt_msgs_complete,
						m_chunk_accum.empty() ? 0 : 1,
						unsigned(m_chunk_accum.size() / 2), m_chunk_held_expected_hw,
						m_chunk_pt_hb_held, m_chunk_pt_rxclear_wiped);
				// P027 (b): 1/s cumulative wipe-restore counters next to the
				// pass-through status.  restored + wiped partition all
				// staged-bulk rx_clear hits (every armed-mode wipe also logs
				// yielded, so wiped == yielded here); budget shows whether
				// the current announce still holds its one-shot.
				if (m_wipe_restore_armed)
					logerror("WIPE_RESTORE: status t=%.6f restored=%u yielded=%u wiped=%u budget=%d\n",
							machine().time().as_double(), m_wipe_restored,
							m_wipe_yielded, m_chunk_pt_rxclear_wiped,
							m_wipe_restore_budget ? 1 : 0);
				// P031: 1/s cumulative announce-latch counters next to the
				// pass-through status.  dispatched == wiped-then-dispatched by
				// construction (a dispatch requires the wiped mark).  Counter
				// arithmetic closes as: latched == dispatched + expired +
				// superseded(fresh-arm ones) + consumed_ok + live; a live=1
				// latch whose live_age_ms climbs across consecutive lines with
				// live_wiped=1 is a START edge that never came (the P025 shape)
				// - bounded by the TTL, watch for the paired expired line.
				// P035 (branch patch/latch-v2-snapshot): under v2 the same
				// status line additionally carries the snapshot counters
				// (snap_copied ~= wiped_seen: every capturable wipe is copied;
				// snap_tx ~= dispatched: every dispatch should ride its
				// snapshot; snap_fallback stays 0 - a nonzero value means a
				// dispatch found a non-matching snapshot and re-read RAM,
				// investigate).  v1 keeps the exact P031 line.
				// P037 (branch patch/latch-genstamp): the v2 line additionally
				// carries gen_differ - cumulative generation-stamp compare
				// misses (dispatch-time + remainder-time) - the residual-
				// MISMATCH skew hypothesis's 1/s cross-check.
				// P038 (branch patch/latch-v3-dedupe): the v3 line inserts
				// deduped= (dispatches suppressed because the class already
				// completed within the lookback) and refresh_retired= (same-
				// class re-announces that retired a pending wiped dispatch)
				// after dispatched=.  v3 closure: latched == dispatched +
				// deduped + expired + superseded(fresh-arm) + consumed_ok +
				// live.  =2 and =1 keep their exact tested lines.
				if (m_al_armed && m_al_dedupe)
					logerror("ANNOUNCE_LATCH: status t=%.6f latched=%u refreshed=%u wiped_seen=%u dispatched=%u deduped=%u refresh_retired=%u expired=%u superseded=%u consumed_ok=%u snap_copied=%u snap_tx=%u snap_fallback=%u gen_differ=%u live=%d live_wiped=%d live_offset=0x%04x live_age_ms=%u\n",
							machine().time().as_double(), m_al_latched, m_al_refreshed,
							m_al_wipes_captured, m_al_dispatched,
							m_al_deduped, m_al_refresh_retired,
							m_al_expired,
							m_al_superseded, m_al_consumed_ok,
							m_al_snap_copied, m_al_snap_dispatched, m_al_snap_fallback,
							m_al_gen_differ,
							m_al_valid ? 1 : 0, m_al_wiped ? 1 : 0,
							m_al_valid ? m_al_offset : 0,
							m_al_valid ? unsigned((machine().time() - m_al_time).as_double() * 1000.0 + 0.5) : 0);
				else if (m_al_armed && m_al_snapshot)
					logerror("ANNOUNCE_LATCH: status t=%.6f latched=%u refreshed=%u wiped_seen=%u dispatched=%u expired=%u superseded=%u consumed_ok=%u snap_copied=%u snap_tx=%u snap_fallback=%u gen_differ=%u live=%d live_wiped=%d live_offset=0x%04x live_age_ms=%u\n",
							machine().time().as_double(), m_al_latched, m_al_refreshed,
							m_al_wipes_captured, m_al_dispatched, m_al_expired,
							m_al_superseded, m_al_consumed_ok,
							m_al_snap_copied, m_al_snap_dispatched, m_al_snap_fallback,
							m_al_gen_differ,
							m_al_valid ? 1 : 0, m_al_wiped ? 1 : 0,
							m_al_valid ? m_al_offset : 0,
							m_al_valid ? unsigned((machine().time() - m_al_time).as_double() * 1000.0 + 0.5) : 0);
				else if (m_al_armed)
					logerror("ANNOUNCE_LATCH: status t=%.6f latched=%u refreshed=%u wiped_seen=%u dispatched=%u expired=%u superseded=%u consumed_ok=%u live=%d live_wiped=%d live_offset=0x%04x live_age_ms=%u\n",
							machine().time().as_double(), m_al_latched, m_al_refreshed,
							m_al_wipes_captured, m_al_dispatched, m_al_expired,
							m_al_superseded, m_al_consumed_ok,
							m_al_valid ? 1 : 0, m_al_wiped ? 1 : 0,
							m_al_valid ? m_al_offset : 0,
							m_al_valid ? unsigned((machine().time() - m_al_time).as_double() * 1000.0 + 0.5) : 0);
			}
		}
	}

	if (!m_lockstep_armed)
		return;
	if (!m_context || !m_context->connected())
		return;

	// 1. Send our frame token (always, even if we are about to stall, so
	// the peer's view of us is current while it decides whether to stall).
	uint32_t const frame_no = ++m_lockstep_local_frame;
	std::vector<uint8_t> token;
	token.reserve(7);
	token.push_back(uint8_t(((LOCKSTEP_CTRL_SIZE_FLAG | 5) >> 8) & 0xff)); // size prefix 0x8005
	token.push_back(uint8_t((LOCKSTEP_CTRL_SIZE_FLAG | 5) & 0xff));
	token.push_back(LOCKSTEP_CTRL_TYPE_TOKEN);
	token.push_back(uint8_t((frame_no >> 24) & 0xff));
	token.push_back(uint8_t((frame_no >> 16) & 0xff));
	token.push_back(uint8_t((frame_no >> 8) & 0xff));
	token.push_back(uint8_t(frame_no & 0xff));
	m_context->send_frame(std::move(token));
	++m_lockstep_tokens_tx;

	// 2. Barrier - engage only once the peer has sent us at least one token.
	if (m_lockstep_tokens_rx.load(std::memory_order_acquire) == 0)
		return;

	uint32_t const peer_now = m_lockstep_peer_token.load(std::memory_order_acquire);

	if (!m_lockstep_have_baseline)
	{
		m_lockstep_offset = int32_t(frame_no - peer_now);
		m_lockstep_have_baseline = true;
		logerror("VBLANK_LOCKSTEP: baseline local=%u peer=%u offset=%d\n",
				frame_no, peer_now, m_lockstep_offset);
		return;
	}

	// Effective drift = how many frames WE are ahead of the peer, relative
	// to the link-up baseline.  Negative = peer is ahead (its problem).
	auto effective_drift =
			[this] () -> int32_t
			{
				return int32_t(m_lockstep_local_frame
						- m_lockstep_peer_token.load(std::memory_order_acquire))
						- m_lockstep_offset;
			};

	int32_t const drift = effective_drift();

	// Discontinuity guard (peer reset / reconnected / we were suspended for
	// a long time): re-baseline rather than stalling toward a huge gap.
	if (drift > LOCKSTEP_REBASE_DRIFT || drift < -LOCKSTEP_REBASE_DRIFT)
	{
		m_lockstep_offset = int32_t(frame_no - peer_now);
		logerror("VBLANK_LOCKSTEP: drift discontinuity %d - re-baseline (local=%u peer=%u offset=%d)\n",
				drift, frame_no, peer_now, m_lockstep_offset);
		return;
	}

	if (m_lockstep_suspended)
	{
		if (peer_now != m_lockstep_peer_at_suspend)
		{
			// Peer tokens flowing again: fresh baseline, resume the barrier.
			m_lockstep_offset = int32_t(frame_no - peer_now);
			m_lockstep_suspended = false;
			logerror("VBLANK_LOCKSTEP: resumed (peer token %u, new offset=%d)\n",
					peer_now, m_lockstep_offset);
		}
		return;
	}

	if (drift <= LOCKSTEP_MAX_LEAD)
	{
		m_lockstep_consec_timeouts = 0;
		return;
	}

	// We are ahead: stall (bounded) until the peer catches up.
	++m_lockstep_stall_events;
	uint32_t const peer_at_entry = peer_now;
	auto const t0 = std::chrono::steady_clock::now();
	bool timed_out = false;
	while (effective_drift() > LOCKSTEP_MAX_LEAD)
	{
		if (std::chrono::steady_clock::now() - t0
				>= std::chrono::milliseconds(LOCKSTEP_STALL_TIMEOUT_MS))
		{
			timed_out = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
	m_lockstep_stall_us += uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t0).count());

	if (!timed_out)
	{
		m_lockstep_consec_timeouts = 0;
		return;
	}

	++m_lockstep_stall_timeouts;
	++m_lockstep_consec_timeouts;
	if (m_lockstep_consec_timeouts == 1)
		m_lockstep_peer_at_streak = peer_at_entry;
	if (m_lockstep_consec_timeouts < LOCKSTEP_SUSPEND_AFTER)
		return;

	// LOCKSTEP_SUSPEND_AFTER straight full timeouts (~0.6 s of degraded
	// running).  Decide between "peer is gone" and "mutual-stall/skew".
	uint32_t const peer_after = m_lockstep_peer_token.load(std::memory_order_acquire);
	m_lockstep_consec_timeouts = 0;
	if (int32_t(peer_after - m_lockstep_peer_at_streak) < 3)
	{
		// Peer token barely moved across the whole streak: peer hung or
		// disconnected.  Free-run until its tokens resume.
		m_lockstep_suspended = true;
		m_lockstep_peer_at_suspend = peer_after;
		logerror("VBLANK_LOCKSTEP: suspended after %u stalled vblanks with idle peer (local=%u peer=%u) - free-running\n",
				LOCKSTEP_SUSPEND_AFTER, m_lockstep_local_frame, peer_after);
	}
	else
	{
		// Peer IS advancing yet we kept timing out: baseline skew or both
		// sides stalling on each other.  Re-baseline to break the cycle.
		m_lockstep_offset = int32_t(m_lockstep_local_frame - peer_after);
		logerror("VBLANK_LOCKSTEP: re-baseline after persistent stall (peer advancing; local=%u peer=%u new offset=%d)\n",
				m_lockstep_local_frame, peer_after, m_lockstep_offset);
	}
}


// Auto-clear the IRQ line after the pulse delay set by deliver_rx_frames.
TIMER_CALLBACK_MEMBER(namco_c139_device::irq_pulse_off)
{
	m_irq_cb(CLEAR_LINE);
}


// P060 EXPERIMENT (branch patch/hb-phase-aware, off patch/poscorr-trace):
// driver-pushed staging-phase signal + debounce for the phase-aware heartbeat
// token cadence.  Called from the namcos23 vblank handler once per frame with
// mode2 = (staging mode word 0x802F3FD0 == 2) and the raw word for log
// context.  HYSTERESIS (anti-flap): FAST arms only after HBPA_DEBOUNCE_VBLANKS
// (60, ~1 s) CONSECUTIVE mode-2 vblanks; ANY non-mode-2 vblank drops back to
// SAFE immediately and zeroes the streak - so attract, the op55 handshake,
// mode-select and any re-establishment across resets/area transitions ALWAYS
// see the SAFE cadence, even if the mode word flickers.  The pending heartbeat
// timer is NOT re-adjusted on a transition: the next fire/TX re-arms with the
// new interval, so a flip takes effect within at most one OLD interval
// (<= 33 ms SAFE->FAST, <= fast_ms FAST->SAFE) - not worth touching a live
// timer for.  Emulation-thread only.  MODEL PROVENANCE: Fable 5.
// P061 (branch patch/tx-complete-irq): the debounced state (m_hbpa_ingame +
// streak) now has a SECOND consumer - the TX-complete dispatch gate
// (m_txci_armed) - so the debounce runs when EITHER env is armed; the
// HB_PHASE_AWARE transition lines stay byte-identical and only appear when
// HBPA itself is armed (the cadence choice in hb_cadence_effective_ms() still
// requires m_hbpa_armed, so arming only TX_COMPLETE_IRQ never changes when the
// heartbeat fires).  Experiment branch only - never merge to milestone.
void namco_c139_device::set_ingame(bool mode2, uint32_t mode_word)
{
	if (!m_hbpa_armed && !m_txci_armed && !m_txc2_armed)
		return;

	if (mode2)
	{
		if (m_hbpa_mode2_streak < HBPA_DEBOUNCE_VBLANKS)
			++m_hbpa_mode2_streak;
		if (m_hbpa_mode2_streak >= HBPA_DEBOUNCE_VBLANKS && !m_hbpa_ingame)
		{
			m_hbpa_ingame = true;
			++m_hbpa_transitions;
			if (m_hbpa_armed)
				logerror("HB_PHASE_AWARE: SAFE->FAST t=%.6f mode_word=%u cadence=%u ms (mode-2 stable %u vblanks; transitions=%u)\n",
						machine().time().as_double(), mode_word, m_hbpa_fast_ms,
						HBPA_DEBOUNCE_VBLANKS, m_hbpa_transitions);
			if (m_txci_armed)
				logerror("TX_COMPLETE_IRQ: ACTIVE t=%.6f mode_word=%u (mode-2 stable %u vblanks; transitions=%u - staged standalone TXs now dispatch at their stage instant; stock stop-and-wait suspended)\n",
						machine().time().as_double(), mode_word,
						HBPA_DEBOUNCE_VBLANKS, m_hbpa_transitions);
			// P063 (branch patch/tx-complete-v2): V2 goes ACTIVE on the same
			// debounced edge.  The first stage of the stretch always dispatches
			// (prev-hash/ring were invalidated at the last INACTIVE/reset).
			if (m_txc2_armed)
				logerror("TX_COMPLETE_V2: ACTIVE t=%.6f mode_word=%u (mode-2 stable %u vblanks; transitions=%u - prev-hash-gated dispatch at stage instant + %u ms modeled TX-busy + %u ms heartbeat stale age-out; stock stop-and-wait suspended)\n",
						machine().time().as_double(), mode_word,
						HBPA_DEBOUNCE_VBLANKS, m_hbpa_transitions,
						m_txc2_busy_ms, m_txc2_stale_ms);
		}
	}
	else
	{
		m_hbpa_mode2_streak = 0;
		if (m_hbpa_ingame)
		{
			m_hbpa_ingame = false;
			++m_hbpa_transitions;
			if (m_hbpa_armed)
				logerror("HB_PHASE_AWARE: FAST->SAFE t=%.6f mode_word=%u cadence=%u ms (mode-2 lost - immediate; transitions=%u)\n",
						machine().time().as_double(), mode_word, m_hb_cadence_ms,
						m_hbpa_transitions);
			if (m_txci_armed)
			{
				// A stale dedupe key must not skip the first dispatch of the
				// next mode-2 stretch, and no park may survive into the
				// stock-owned phase.
				m_txci_last_valid = false;
				m_txci_parked_dup = false;
				logerror("TX_COMPLETE_IRQ: INACTIVE t=%.6f mode_word=%u (mode-2 lost - immediate; stock stop-and-wait resumes; transitions=%u)\n",
						machine().time().as_double(), mode_word, m_hbpa_transitions);
			}
			// P063 (branch patch/tx-complete-v2): drop V2 IMMEDIATELY on
			// mode-2 loss - the gate key/ring die (the next stretch's first
			// stage must always dispatch), the park dies (a stage sitting in
			// reg5 is now the stock stop-and-wait's business: rx_clear
			// releases it, exactly as stock), and the busy window dies (no
			// synthesized busy read may leak into establishment - the very
			// next poll reads the raw register).
			if (m_txc2_armed)
			{
				m_txc2_prev_valid = false;
				for (unsigned i = 0; i < TXC2_HIST; i++)
					m_txc2_hist[i].valid = false;
				m_txc2_parked_dup = false;
				m_txc2_busy_until = attotime::zero;
				m_txc2_busy_hw = 0;
				logerror("TX_COMPLETE_V2: INACTIVE t=%.6f mode_word=%u (mode-2 lost - immediate; stock stop-and-wait resumes, busy window/gate key/park cleared; transitions=%u)\n",
						machine().time().as_double(), mode_word, m_hbpa_transitions);
			}
		}
	}
}


// PHASE 9D: synthetic heartbeat - keeps the partner's link-state dispatcher
// firing during periods of no game-initiated TX.  The hypothesis is that the
// real wedge mechanism is the partner's timeout counter (mem[0x802F3504])
// climbing past 17 because no sum=0 packet arrived to call the dispatcher's
// state-counter sync at 0x8000BB1C.  Replaying the last real TX should
// trigger the partner's validator->dispatcher path and reset its timeout.
//
// Critical detail: bytes 0-1 of the replayed packet are the partner_counter
// the dispatcher subtracts from its own gp+0x75B8 to compute the timeout
// delta.  If we replay a stale packet with stale bytes 0-1, the delta will be
// large and the timeout reset will *itself* leave the timeout above threshold.
// We stamp our current local counter into bytes 0-1 every time we replay.
//
// REMOVE BEFORE COMMIT.
TIMER_CALLBACK_MEMBER(namco_c139_device::heartbeat_tick)
{
	if (!m_context || !m_context->connected())
		return;
	if (m_last_tx_payload.empty())
		return;

	// P026 (branch patch/reasm-chunk-passthru): while a pass-through chunk
	// sequence is open (head forwarded, remainder still to come), HOLD the
	// replay - a device-invented frame interleaved between the ROM's chunks
	// would land in the peer's RX ring mid-message, and the peer's drain would
	// then checksum a chunk1+heartbeat span (guaranteed chkfail) and desync the
	// message.  The ROM's own emitter never interleaves anything mid-message;
	// keep the wire that shape.  Re-arm so the replay resumes once the
	// sequence completes/drops (the 500 ms stale sweep bounds the wait).
	if (m_chunk_passthru && !m_chunk_accum.empty())
	{
		++m_chunk_pt_hb_held;
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(hb_cadence_effective_ms()));
		return;
	}

	// P063 EXPERIMENT (branch patch/tx-complete-v2): STALE-REPLAY AGE-OUT -
	// the red >255hw channel (P062 Q5).  Chunk-armed frames >255hw and bulk
	// chunks are capture-EXCLUDED (P010/P026, correctly - restamping a
	// chunked/bulk frame corrupts its headers), so in red's scene-table
	// stretches the captured replay payload ages up to 66 s while ~20
	// replays/s keep firing - blue ingests restamped minute-old state (the
	// measured asymmetric pose-snap channel).  Under V2 in stable mode-2 the
	// peer no longer needs the replay token (it self-releases at its own
	// stage instants), so a stale replay is pure poison with no pacing value:
	// SUPPRESS the replay when the capture is older than m_txc2_stale_ms -
	// re-arm and return, the same hold shape as the chunk-passthru block
	// above.  The capture logic itself is UNTOUCHED (P062 proved
	// newest-at-capture in 19129/19129 + 13357/13357 captures - the gap is
	// eligibility, and widening it stays banned).  Outside mode-2 (or
	// unarmed) this block is dead and the heartbeat still paces op55
	// establishment exactly as before.  MODEL PROVENANCE: Fable 5.
	if (m_txc2_armed && m_hbpa_ingame
			&& machine().time() - m_txc2_cap_time > attotime::from_msec(m_txc2_stale_ms))
	{
		++m_txc2_stale_win;
		++m_txc2_stale_tot;
		if (!m_txc2_first_stale_logged)
		{
			m_txc2_first_stale_logged = true;
			logerror("TX_COMPLETE_V2: first-stale-suppress t=%.6f cap_age_ms=%u stale_ms=%u (heartbeat replay withheld - captured payload too old to restamp under self-release; steady-state visibility is the 1/s status rail)\n",
					machine().time().as_double(),
					unsigned((machine().time() - m_txc2_cap_time).as_double() * 1000.0 + 0.5),
					m_txc2_stale_ms);
		}
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(hb_cadence_effective_ms()));
		return;
	}

	uint32_t const payload_size = uint32_t(m_last_tx_payload.size());

	// Build the on-wire frame: 16-bit big-endian length prefix + payload.
	std::vector<uint8_t> frame;
	frame.reserve(payload_size + 2);
	frame.push_back(uint8_t((payload_size >> 8) & 0xff));
	frame.push_back(uint8_t(payload_size & 0xff));
	frame.insert(frame.end(), m_last_tx_payload.begin(), m_last_tx_payload.end());

	// Stamp our current state counter into bytes 0-1 of the payload.  These
	// land at frame[2..3] after the 2-byte length prefix.  The partner's
	// dispatcher reads this as partner_counter and computes a small delta
	// against its own gp+0x75B8, which keeps mem[0x802F3504] under the
	// 17-frame timeout threshold.
	//
	// P027 (c) EXPERIMENT (branch patch/hb-cadence-wipe-restore): the offset
	// of that delta is whatever m_local_counter happens to be vs the PEER
	// ROM's counter - P026 measured red riding drift 13-14 through the whole
	// cutscene on blue's heartbeats (a constant 3-frame margin under the
	// 17-frame ceiling).  When NAMCOS23_PATCH_HB_RESTAMP is armed, stamp
	// peer_last_seen_counter + elapsed_frames instead (the peer's counter
	// advances ~1/frame, so aging the sample tracks it): the receiver's
	// drift reset then lands near transit latency, like a real frame's.
	// Falls back to the stock m_local_counter stamp while no complete-message
	// sample exists or the sample is older than HB_RESTAMP_MAX_AGE_MS.
	// Unset = stock stamp and stock log line, bit-identical.
	uint16_t stamp = m_local_counter;
	bool restamped = false;
	uint32_t age_fr = 0;
	if (m_hb_restamp_armed)
	{
		if (m_hb_peer_ctr_valid)
		{
			attotime const age = machine().time() - m_hb_peer_ctr_time;
			if (age < attotime::from_msec(HB_RESTAMP_MAX_AGE_MS))
			{
				age_fr = uint32_t(age.as_double() * HB_COUNTER_HZ + 0.5);
				stamp = uint16_t(m_hb_peer_ctr + age_fr);
				restamped = true;
				++m_hb_restamped;
			}
		}
		if (!restamped)
			++m_hb_restamp_fallback;
	}
	if (payload_size >= 2)
	{
		frame[2] = uint8_t((stamp >> 8) & 0xff);
		frame[3] = uint8_t(stamp & 0xff);
	}

	if (restamped)
		logerror("namco_c139: heartbeat tx (replay %u bytes, local_counter=%u restamp=%u peer_base=%u age_fr=%u)\n",
				payload_size, unsigned(m_local_counter), unsigned(stamp),
				unsigned(m_hb_peer_ctr), age_fr);
	else
		logerror("namco_c139: heartbeat tx (replay %u bytes, local_counter=%u)\n",
				payload_size, unsigned(m_local_counter));

	// P009 EXPERIMENT (branch patch/qual-trace): fingerprint the (stamped)
	// replay so the peer's RX fingerprints can be classified real-TX vs
	// heartbeat-replay by tailid.  Inert unless NAMCOS23_TRACE_QUAL.
	log_qual_fingerprint("tx-hb", frame.data() + 2, frame.size() - 2);

	m_context->send_frame(std::move(frame));

	// Re-arm.  P027 (a): interval = m_hb_cadence_ms (250 unless
	// NAMCOS23_PATCH_HB_CADENCE_MS overrides - unset is bit-identical stock).
	// P060 (branch patch/hb-phase-aware): hb_cadence_effective_ms() substitutes
	// the FAST interval (m_hbpa_fast_ms) while the debounced staging mode-2
	// signal is live - identical to m_hb_cadence_ms when HB_PHASE_AWARE is
	// unset or outside in-game.  Same substitution at the other three re-arm
	// sites (TX capture, bulk-chunk/burst re-arm, chunk-passthru hold).
	if (m_heartbeat_timer)
		m_heartbeat_timer->adjust(attotime::from_msec(hb_cadence_effective_ms()));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void namco_c139_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), uint16_t(0));

	// P010/P011 EXPERIMENT: clear runtime reassembly state (the env gates
	// m_chunk_armed / m_chunk_assoc_pointer, set in device_start(), survive
	// reset).
	m_chunk_tx_ptr = 0;
	m_chunk_tx_ptr_valid = false;
	m_chunk_expected_hw = 0;
	m_chunk_held_expected_hw = 0;
	m_chunk_bulk_pending = false;
	m_chunk_accum.clear();
	m_chunk_accum_since = attotime::zero;
	m_chunk_msg_start_ptr = 0;
	m_chunk_resume_ptr = 0;
	m_chunk_msg_chunks = 0;
	m_chunk_saw_txoffset = false;

	// P026 EXPERIMENT (branch patch/reasm-chunk-passthru): clear the
	// pass-through runtime counters (the env gate m_chunk_passthru, set in
	// device_start(), survives reset).
	m_chunk_pt_msg_id = 0;
	m_chunk_pt_chunks_fwd = 0;
	m_chunk_pt_msgs_complete = 0;
	m_chunk_pt_hb_held = 0;
	m_chunk_pt_rxclear_wiped = 0;

	// P050 EXPERIMENT (branch patch/single-burst-pump): clear the pump-gate
	// runtime counters (the env gates m_bq_armed / m_newest_wins / m_pump_instr,
	// set in device_start(), survive reset).
	m_bq_single_burst_tx = 0;
	m_nw_superseded = 0;
	m_nw_tx_complete = 0;
	m_nw_rx_complete = 0;
	m_nw_tx_win = 0;
	m_nw_rx_win = 0;
	m_nw_status_div = 0;

	// P027 EXPERIMENT (branch patch/hb-cadence-wipe-restore): clear the
	// wipe-restore / restamp runtime state (the env gates m_hb_cadence_override
	// + m_hb_cadence_ms, m_wipe_restore_armed and m_hb_restamp_armed, set in
	// device_start(), survive reset).
	m_wipe_restore_budget = false;
	m_wipe_restored = 0;
	m_wipe_yielded = 0;
	m_hb_peer_ctr_valid = false;
	m_hb_peer_ctr = 0;
	m_hb_peer_ctr_time = attotime::zero;
	m_hb_restamped = 0;
	m_hb_restamp_fallback = 0;

	// P060 EXPERIMENT (branch patch/hb-phase-aware): drop to the SAFE cadence
	// across a reset - re-establishment must ALWAYS see SAFE; the debounce then
	// requires a fresh ~1 s of stable mode-2 before FAST returns.  (The env
	// gates m_hbpa_armed + m_hbpa_fast_ms, set in device_start(), survive
	// reset, matching the other gates.)
	if (m_hbpa_ingame && m_hbpa_armed)
		logerror("HB_PHASE_AWARE: FAST->SAFE (device_reset) cadence=%u ms\n", m_hb_cadence_ms);
	// P061 (branch patch/tx-complete-irq): the debounced in-game state is shared
	// with the TX-complete dispatch gate - a reset drops it the same way, so
	// re-establishment always sees the stock stop-and-wait.
	if (m_hbpa_ingame && m_txci_armed)
		logerror("TX_COMPLETE_IRQ: INACTIVE (device_reset) - stock stop-and-wait resumes; debounce restarts\n");
	// P063 (branch patch/tx-complete-v2): same drop for the v2 release.
	if (m_hbpa_ingame && m_txc2_armed)
		logerror("TX_COMPLETE_V2: INACTIVE (device_reset) - stock stop-and-wait resumes; debounce restarts\n");
	m_hbpa_ingame = false;
	m_hbpa_mode2_streak = 0;
	m_hbpa_transitions = 0;

	// P061 EXPERIMENT (branch patch/tx-complete-irq): clear the TX-complete
	// dispatch runtime state (the env gate m_txci_armed, set in device_start(),
	// survives reset, matching the other gates).
	m_txci_last_valid = false;
	m_txci_last_seq = 0;
	m_txci_last_len = 0;
	m_txci_parked_dup = false;
	m_txci_vbl_dispatches = 0;
	m_txci_first_logged = false;
	m_txci_first_release_logged = false;
	m_txci_dispatch_win = 0;
	m_txci_dup_skips_win = 0;
	m_txci_release_win = 0;
	m_txci_cap_skips_win = 0;
	m_txci_bulk_skips_win = 0;
	m_txci_incomplete_skips_win = 0;
	m_txci_dispatch_tot = 0;
	m_txci_release_tot = 0;
	m_txci_status_div = 0;

	// P063 EXPERIMENT (branch patch/tx-complete-v2): clear the v2 release
	// runtime state (the env gates m_txc2_armed / m_txc2_busy_ms /
	// m_txc2_stale_ms, set in device_start(), survive reset, matching the
	// other gates).  Re-establishment after a reset always sees the stock
	// stop-and-wait: gate key/ring/park/busy window all die here, and the
	// capture-time anchor restarts with the first post-reset capture.
	m_txc2_prev_valid = false;
	m_txc2_prev_hash = 0;
	m_txc2_prev_len = 0;
	for (unsigned i = 0; i < TXC2_HIST; i++)
	{
		m_txc2_hist[i].hash = 0;
		m_txc2_hist[i].len = 0;
		m_txc2_hist[i].valid = false;
	}
	m_txc2_hist_idx = 0;
	m_txc2_busy_until = attotime::zero;
	m_txc2_busy_hw = 0;
	m_txc2_parked_dup = false;
	m_txc2_cap_time = attotime::zero;
	m_txc2_first_dispatch_logged = false;
	m_txc2_first_release_logged = false;
	m_txc2_first_stale_logged = false;
	m_txc2_dispatch_win = 0;
	m_txc2_dup_skip_win = 0;
	m_txc2_ring_skip_win = 0;
	m_txc2_zero_win = 0;
	m_txc2_busy_hold_win = 0;
	m_txc2_release_win = 0;
	m_txc2_stale_win = 0;
	m_txc2_midwin_win = 0;
	m_txc2_bulk_skip_win = 0;
	m_txc2_incomplete_win = 0;
	m_txc2_dispatch_tot = 0;
	m_txc2_stale_tot = 0;
	m_txc2_status_div = 0;

	// P031 EXPERIMENT (branch patch/announce-latch): clear the announce-latch
	// runtime state (the env gate m_al_armed, set in device_start(), survives
	// reset).
	m_al_valid = false;
	m_al_wiped = false;
	m_al_offset = 0;
	m_al_expected_hw = 0;
	m_al_wiped_hw = 0;
	m_al_time = attotime::zero;
	m_al_latched = 0;
	m_al_refreshed = 0;
	m_al_wipes_captured = 0;
	m_al_dispatched = 0;
	m_al_expired = 0;
	m_al_superseded = 0;
	m_al_consumed_ok = 0;

	// P035 (branch patch/latch-v2-snapshot): clear the v2 snapshot runtime
	// state (the env-selected m_al_snapshot mode, set in device_start(),
	// survives reset like m_al_armed; clear() keeps the reserved capacity).
	m_al_snap_valid = false;
	m_al_snap_offset = 0;
	m_al_snap.clear();
	m_al_snap_copied = 0;
	m_al_snap_dispatched = 0;
	m_al_snap_fallback = 0;

	// P037 (branch patch/latch-genstamp): clear the LOG-ONLY genstamp rider's
	// counter + head-compare scratch (the env-selected m_al_snapshot mode it
	// rides survives reset, as above).
	m_al_gen_differ = 0;
	m_al_gen_head_live = false;
	m_al_gen_head_offset = 0;
	m_al_gen_head_hw0 = 0;
	m_al_gen_head_hw1 = 0;
	m_al_gen_head_hws = 0;
	m_al_gen_head_time = attotime::zero;

	// P038 (branch patch/latch-v3-dedupe): clear the dedupe ring + counters
	// (the env-selected m_al_dedupe mode, set in device_start(), survives
	// reset like m_al_armed/m_al_snapshot).
	for (auto &rec : m_al_completes)
		rec = al_complete_rec();
	m_al_comp_idx = 0;
	m_al_deduped = 0;
	m_al_refresh_retired = 0;

	// P013 EXPERIMENT (branch patch/tx-emitter-trace): clear the read-only
	// trace's runtime counters (the env gate m_txemit_trace, set in
	// device_start(), survives reset).
	m_txemit_frame = 0;
	m_txemit_txsize_zero_reads = 0;
	m_txemit_sends = 0;
	m_txemit_status_div = 0;

	// P015 EXPERIMENT (branch patch/render-gate-actor-spawn-trace): clear the
	// read-only adoption trace's running index (the env gate m_trace_adopt, set
	// in device_start(), survives reset).
	m_adopt_rx_index = 0;

	// P018 EXPERIMENT (branch patch/op70-real-detector): clear the read-only
	// real-op-70 detection running indices (the env gate m_trace_op70, set in
	// device_start(), survives reset).
	m_op70_rx_index = 0;
	m_op70_tx_index = 0;

	// P019 STEP 1 EXPERIMENT (branch patch/op70-linked-gate-arm): clear the
	// read-only op55 wire-flag running indices (the env gate m_trace_linkbits,
	// set in device_start(), survives reset).
	m_lb_tx_index = 0;
	m_lb_rx_index = 0;

	// P025 EXPERIMENT (branch patch/playclock-humangate-trace): clear the
	// read-only real-op6F play-clock detection running indices (the env gate
	// m_trace_playclock, set in device_start(), survives reset).
	m_pc6f_tx_index = 0;
	m_pc6f_rx_index = 0;

	// P044 EXPERIMENT (branch patch/anchor-lifecycle): clear the read-only
	// entity-lifecycle (op-0x20/op-0x1F) detection running indices (the env
	// gate m_trace_op20, set in device_start(), survives reset).
	m_op20_tx_index = 0;
	m_op20_rx_index = 0;

	// P021 EXPERIMENT (branch patch/linked-gate-tx-only): clear the WIRE-ONLY
	// 0x6000 injection counters.  The arm + mode==2 gate are pushed fresh from
	// the driver each vblank (set_linked_gate_txonly), so they need no reset, but
	// clear them defensively so a reset before the first push starts inert.
	m_lg_txonly_armed = false;
	m_lg_txonly_mode2 = false;
	m_lg_txonly_inject_count = 0;
	m_lg_txonly_status_div = 0;
}

//-------------------------------------------------
//  memory_space_config - return a description of
//  any address spaces owned by this device
//-------------------------------------------------

device_memory_interface::space_config_vector namco_c139_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_DATA, &m_space_config)
	};
}

//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

uint16_t namco_c139_device::ram_r(offs_t offset)
{
	// Drain any pending RX frames before serving the read.  The link
	// probe window is short (~300 game ticks) and the game RAMs are
	// accessed heavily during it; piggy-backing the drain on RAM reads
	// gets received frames into the game's hands faster than waiting
	// for the next reg_r / status_r call.
	deliver_rx_frames(0);
	return m_ram[offset];
}

void namco_c139_device::ram_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_ram[offset]);
}

uint16_t namco_c139_device::status_r()
{
	deliver_rx_frames(0);
	/*
	 x-- RX READY or irq pending?
	 -x- IRQ direction: 1 RX cause - 0 TX cause
	*/
	uint16_t result = 4;   // existing "ready" bit from Salese's stub

	// Mirror the RX-flag bit from Status/Control (m_regs[1] bit 1) so an
	// IRQ handler reading the RX-Status register sees "frame received"
	// when one's been delivered into RAM.  Cleared when the game writes
	// to Status/Control to clear bits 1+2.
	if (m_regs[1] & 0x02)
		result |= 0x02;

	return result;
}

uint16_t namco_c139_device::reg_r(offs_t offset)
{
	deliver_rx_frames(0);
	// regs_map calls us with offset relative to the 0x02..0x0f range, i.e.
	// host byte address 0x02 -> offset 0, address 0x04 -> offset 1, etc.
	// We mirror that into m_regs[1..7], leaving m_regs[0] reserved for the
	// status register handled by status_r().
	const offs_t reg_idx = (offset + 1) & 0x7;

	// P063 EXPERIMENT (branch patch/tx-complete-v2): the MODELED TX-BUSY - the
	// P061 back-pressure lesson made mechanism.  While the modeled
	// serialization window of the last dispatch (or of a duplicate re-stage
	// that landed on an idle serializer - see reg_w) is open, every TXSIZE
	// read answers a synthesized BUSY value (the in-flight TXSIZE, low byte
	// forced non-zero for the ROM's `bnez reg5&0xFF` gate) WITHOUT touching
	// the register file - the pump's 0x8000BB80 entry gate bails exactly as
	// on real hardware while the frame serializes (P062 Q4: the ROM lives at
	// polls ~99% busy, 70-100 passages/s, and tolerates >=16.5 ms busy spans).
	// The FIRST poll after the window closes releases a parked duplicate
	// (m_regs[5] -> 0 - the one deliberate reg5 write of this patch, same
	// sanctioned modeled-TX-completion class as P061's release: the parked
	// bytes were hash-verified already-crossed content at stage time; if the
	// ROM meanwhile recomposed the slot, the pump re-stages the new content on
	// the very passage this release enables and the gate dispatches it fresh).
	// A park whose register was meanwhile wiped by rx_clear / the ROM's own
	// TXSIZE=0 write is dropped silently (nothing to clear).  Runs BEFORE the
	// P062 poll counters below so the TXSTAGE busy classification reflects
	// what the ROM observes.  Gated on armed + debounced mode-2: outside
	// mode-2 no synthesized value is ever returned (set_ingame/device_reset
	// also clear the window, so none can leak into establishment).
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	bool txc2_busy_read = false;
	uint16_t txc2_busy_val = 0;
	if (m_txc2_armed && m_hbpa_ingame && reg_idx == 5)
	{
		if (machine().time() < m_txc2_busy_until)
		{
			txc2_busy_read = true;
			txc2_busy_val = ((m_txc2_busy_hw & 0xff) != 0)
					? m_txc2_busy_hw : uint16_t(m_txc2_busy_hw | 0x0001);
			++m_txc2_busy_hold_win;
		}
		else if (m_txc2_parked_dup)
		{
			m_txc2_parked_dup = false;
			if (m_regs[5] != 0)
			{
				m_regs[5] = 0;
				++m_txc2_release_win;
				if (!m_txc2_first_release_logged)
				{
					m_txc2_first_release_logged = true;
					logerror("TX_COMPLETE_V2: first-release t=%.6f (parked duplicate stage cleared at the first TXSIZE busy-poll after the modeled window - the poll returns 0 and the pump starts its next passage; steady-state visibility is the 1/s status rail)\n",
							machine().time().as_double());
				}
			}
		}
	}

	// P062 EXPERIMENT (branch patch/txstage-trace): BUSY-POLL BASELINE - count
	// every TXSIZE read (the pump's 0x8000BB80 entry-gate busy-poll; in mode-2
	// the only reg5 reader, P013) and how many observed a busy (non-zero low
	// byte) value, WITHOUT logging each (60-240/s steady state).  This is the
	// stock back-pressure number a P061b modeled TX-BUSY interval must
	// reproduce (P061 measured 80-232 pump passages/s stock vs 4.7-6.5k/s once
	// the instant-0 poll removed the back-pressure).  Counted BEFORE the P061
	// release block below so the busy classification reflects the sitting
	// stage (the P061 env stays UNSET on a P062 run anyway, so that block is
	// dead and the value the ROM reads is exactly what we count).  READ-ONLY.
	// P063: under TX_COMPLETE_V2 the value the ROM reads is the synthesized
	// busy while the modeled window is open - classify on the EFFECTIVE value
	// (byte-identical when V2 is unset: txc2_busy_read is then always false),
	// so the P062 digest keeps reporting the ROM's view on the confirm run.
	if (m_ts_armed && reg_idx == 5)
	{
		++m_ts_poll5_win;
		if (((txc2_busy_read ? txc2_busy_val : m_regs[5]) & 0xff) != 0)
			++m_ts_poll5_busy_win;
	}

	// P013 EXPERIMENT (branch patch/tx-emitter-trace): READ-ONLY trace of the
	// ROM's TX-status poll.  The emitter (0x8000BB6C) gates at entry on
	// `REG[0x0A] (TXSIZE) low byte != 0` -> PATH A early-out ("device still
	// busy with previous request").  On real C139 the chip holds TXSIZE
	// non-zero until the chunk actually transmits; THIS device clears
	// m_regs[5] synchronously the instant a chunk is staged (send_pending_tx_
	// frame / deliver_rx_frames), so the ROM should observe TXSIZE == 0 on
	// essentially every poll and immediately stage the NEXT chunk.  That
	// synchronous-done model is EXACTLY candidate (iii): if the remainder of a
	// >255hw message is gated on a chip TX-complete signal we never assert,
	// the ROM keeps staging chunks freely and the absence of any non-zero
	// TXSIZE read is what makes (iii) visible.  Volume discipline: a TXSIZE
	// read of ZERO is the overwhelmingly common case (and uninteresting), so
	// log ONLY a non-zero TXSIZE read (the ROM observing a busy chip) - that
	// rare event would falsify (iii).  Zero-reads are tallied in the 1/s
	// status line instead.
	// P061 EXPERIMENT (branch patch/tx-complete-irq): the PARKED-DUP RELEASE -
	// the TX-complete half of the model, acting at the ROM's TXSIZE busy-poll
	// (the pump's 0x8000BB80 entry gate reads this register; in mode-2 that is
	// the only reg5 reader, P013).  A dup-skipped stage (an already-transmitted
	// frame the pump re-staged - see the dispatch trigger in reg_w) would on
	// real hardware serialize and complete, TXSIZE reading 0 again ~a frame
	// later; left non-zero here it blocks the next kick's fresh stage.  Release
	// it exactly where the device's rx_clear release also acts (a reg access
	// delivering the "chip finished" state to the busy-poll), but only after
	// RE-VERIFYING the parked stage still IS the last-dispatched frame (same
	// length + same seq read back from the staged RAM image) - a stage that is
	// not a verified, already-crossed duplicate is NEVER touched.  This is the
	// one deliberate reg5 write in the patch (mode-2 gated, content-verified
	// sacrificial - the audit-sanctioned emulator-faithful exception; full
	// rationale in the member-block comment).  Runs BEFORE the TXEMIT
	// status-read trace below so the trace reports the value the ROM actually
	// observes.  No per-event log (60/s at steady state) - counted on the 1/s
	// rail, one-shot first-release line.
	if (m_txci_armed && m_hbpa_ingame && m_txci_parked_dup && reg_idx == 5
			&& (m_regs[5] & 0xff) != 0 && m_txci_last_valid
			&& m_regs[5] == m_txci_last_len)
	{
		uint16_t const ptr = (m_chunk_armed && m_chunk_tx_ptr_valid)
				? m_chunk_tx_ptr : m_regs[7];
		uint16_t const seq = uint16_t(((m_ram[ptr & 0x1fff] & 0xff) << 8)
				| (m_ram[uint16_t(ptr + 1) & 0x1fff] & 0xff));
		if (seq == m_txci_last_seq)
		{
			m_txci_parked_dup = false;
			m_regs[5] = 0;
			++m_txci_release_win;
			++m_txci_release_tot;
			if (!m_txci_first_release_logged)
			{
				m_txci_first_release_logged = true;
				logerror("TX_COMPLETE_IRQ: first-release t=%.6f seq=%04x hw=%u (parked duplicate stage cleared at the ROM's TXSIZE busy-poll - the modeled TX completing; the poll returns 0 and the pump stages its next fresh frame; steady-state visibility is the 1/s status rail)\n",
						machine().time().as_double(), unsigned(seq),
						unsigned(m_txci_last_len));
			}
		}
	}

	if (m_txemit_trace && reg_idx == 5)
	{
		uint16_t const txsize = uint16_t(m_regs[5] & 0xff);
		if (txsize != 0)
			logerror("TXEMIT: status-read fr=%u t=%.6f reg=TXSIZE[0a] val=%04x txsize_hw=%u BUSY cursor=0x%04x slot=%u held_accum_hw=%u\n",
					m_txemit_frame, machine().time().as_double(), m_regs[5],
					txsize, m_regs[7], txemit_slot_of(m_regs[7]),
					unsigned(m_chunk_accum.size() / 2));
		else
			++m_txemit_txsize_zero_reads;
	}

	// P063: while the modeled TX-busy window is open the ROM reads the
	// synthesized busy value (register file untouched); every other read - and
	// every read when TX_COMPLETE_V2 is unset - returns the stored register.
	return txc2_busy_read ? txc2_busy_val : m_regs[reg_idx];
}

void namco_c139_device::reg_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	deliver_rx_frames(0);
	const offs_t reg_idx = (offset + 1) & 0x7;

	// Register 1 (host byte 0x02..0x03) doubles as an IRQ control register
	// in addition to a status mirror.  Two magic values were established
	// empirically by the inline c422 stub previously embedded in namcos23:
	//   0xfffb -> assert IRQ output line
	//   0x000f -> deassert IRQ output line
	// Other values just store and let the host poll bits.
	if (reg_idx == 1)
	{
		if (data == 0xfffb)
			m_irq_cb(ASSERT_LINE);
		else if (data == 0x000f)
			m_irq_cb(CLEAR_LINE);
	}

	// Snapshot register 3 (host byte 0x06, TX Control)'s bit-0 state
	// before the update so we can detect a 0 -> 1 edge after.
	uint16_t const old_tx_bit0 = (reg_idx == 3) ? (m_regs[3] & 0x01) : 0;

	// PHASE 9D DIAGNOSTIC: log all reg_w events so we can see the game's
	// register-write sequence around TX/RX cycles.  Filter to writes that
	// actually change the value (skip COMBINE_DATA-no-op writes).
	// REMOVE BEFORE COMMIT.
	uint16_t const before_val = m_regs[reg_idx];
	uint16_t after_val = before_val;
	COMBINE_DATA(&after_val);
	if (after_val != before_val)
		logerror("namco_c139: reg_w r%u %04x -> %04x (mask=%04x)\n",
				unsigned(reg_idx), before_val, after_val, mem_mask);

	COMBINE_DATA(&m_regs[reg_idx]);

	// P013 EXPERIMENT (branch patch/tx-emitter-trace): READ-ONLY trace of the
	// raw TX register-programming SEQUENCE.  Scoped to the three TX-relevant
	// registers only (REG[0x06]/START=m_regs[3], REG[0x0A]/TXSIZE=m_regs[5],
	// REG[0x0E]/TXOFFSET=m_regs[7]) so the trace is not flooded by unrelated
	// reg_w.  Logged AFTER the m_regs store (so val= is the freshly-written
	// value) but BEFORE any of the device's TX-trigger side effects, so it
	// shows what the ROM emitter programmed.  This is the core of P013: it
	// reveals the full per-send register sequence, so the analyst can see
	// whether a SECOND TXSIZE/offset is ever staged for the REMAINDER of a
	// >255hw message, what cursor/offset it uses, and whether the offset
	// resets to a slot base (PATH C, new message) or stays advanced (PATH
	// B/B' continuation).  Pairs with the existing CHUNK_REASM "tx chunk
	// staged ... from ptr=" line by frame/t.
	if (m_txemit_trace && (reg_idx == 3 || reg_idx == 5 || reg_idx == 7))
	{
		char const *const rname = (reg_idx == 3) ? "START[06]"
								: (reg_idx == 5) ? "TXSIZE[0a]"
								:                  "TXOFFSET[0e]";
		uint16_t const cursor = m_regs[7];   // device image of gp+0x75CA+2
		// For a TXSIZE write annotate whether it equals the saturated 0xFF
		// chunk (the head of a >255hw message) or a smaller remainder, and the
		// total announced by the size cells the current TXOFFSET points just
		// past - so a remainder TXSIZE staged with NO intervening TXOFFSET is
		// visible as such (this is exactly outcome (i)/(ii)/(iii)'s pivot).
		if (reg_idx == 5)
		{
			uint16_t const txsize = uint16_t(m_regs[5] & 0xff);
			uint32_t const announced = (uint32_t(m_ram[(cursor - 2) & 0x1fff] & 0xff) << 8)
									 |  uint32_t(m_ram[(cursor - 1) & 0x1fff] & 0xff);
			logerror("TXEMIT: regw fr=%u t=%.6f reg=%s val=%04x txsize_hw=%u cursor=0x%04x slot=%u dma_ptr=0x%04x ptr_valid=%d saw_txoffset=%d announced_hw=%u bulk_pending=%d sat=%d\n",
					m_txemit_frame, machine().time().as_double(), rname, m_regs[5],
					txsize, cursor, txemit_slot_of(cursor), m_chunk_tx_ptr,
					m_chunk_tx_ptr_valid ? 1 : 0, m_chunk_saw_txoffset ? 1 : 0,
					announced, m_chunk_bulk_pending ? 1 : 0,
					(txsize == CHUNK_SAT_HW) ? 1 : 0);
		}
		else
		{
			logerror("TXEMIT: regw fr=%u t=%.6f reg=%s val=%04x cursor=0x%04x slot=%u dma_ptr=0x%04x ptr_valid=%d saw_txoffset=%d held_accum_hw=%u held_expected_hw=%u held_resume=0x%04x\n",
					m_txemit_frame, machine().time().as_double(), rname, m_regs[reg_idx],
					cursor, txemit_slot_of(cursor), m_chunk_tx_ptr,
					m_chunk_tx_ptr_valid ? 1 : 0, m_chunk_saw_txoffset ? 1 : 0,
					unsigned(m_chunk_accum.size() / 2), m_chunk_held_expected_hw,
					m_chunk_resume_ptr);
		}
	}

	// P062 EXPERIMENT (branch patch/txstage-trace): READ-ONLY per-stage TX
	// content trace + START-edge passage counter - the P061 post-mortem's
	// "instrument FIRST" measurement, run under STOCK pacing (TX_COMPLETE_IRQ
	// unset).  Runs right after the m_regs store (the stage instant, the one
	// moment the staged bytes are provably pristine - P035) and BEFORE every
	// send-triggering block below (P014 commit trigger, P061 dispatch, the
	// START-edge trigger), so it observes what the ROM staged with zero
	// interference; it fires on EVERY reg5 write including same-value rewrites
	// (which the always-on value-change diagnostic above filters out - and
	// those are exactly the true re-stages this trace must count).  READ-ONLY:
	// counters + one log line; no register/RAM writes, no sends.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	if (m_ts_armed && reg_idx == 3 && !old_tx_bit0 && (m_regs[3] & 0x01))
		++m_ts_start_edge_win;   // pump passage rate (P061's 80-232/s stock metric); counted only, the value-change line above already logs each edge
	if (m_ts_armed && reg_idx == 5)
	{
		if (m_regs[5] == 0)
			++m_ts_zero_stage_win;   // PATH-D idle tick (reg3=3 then TXSIZE=0) - counted, not logged
		else
		{
			uint16_t const stage_hw = m_regs[5];
			// Same read-pointer resolution send_pending_tx_frame() would use
			// for this stage (continuation -> resume pointer; else the latched
			// DMA pointer / raw TXOFFSET), so the hash covers the exact bytes a
			// send of this stage would put on the wire.
			bool const ts_cont = m_chunk_armed && !m_chunk_accum.empty()
					&& !m_chunk_saw_txoffset;
			uint16_t const ts_ptr = ts_cont ? m_chunk_resume_ptr
					: (m_chunk_armed && m_chunk_tx_ptr_valid) ? m_chunk_tx_ptr
					: m_regs[7];
			// Hash span capped at the validator's max message length (a garbage
			// TXSIZE can never walk more than 0x400 halfwords; the 0x1fff mask
			// inside ts_hash_ram makes any read safe regardless).
			uint32_t const ts_hash_hw = (uint32_t(stage_hw) > CHUNK_MAX_HW)
					? CHUNK_MAX_HW : uint32_t(stage_hw);
			uint32_t const ts_hash = ts_hash_ram(m_ram, ts_ptr, ts_hash_hw);
			// seq cells 0-1 (the ROM drift/phase counter - the refuted P061
			// dedupe key) logged per line so the slow-phase-counter finding is
			// re-verifiable against content identity in the same stream.
			uint16_t const ts_seq = uint16_t(((m_ram[ts_ptr & 0x1fff] & 0xff) << 8)
					| (m_ram[uint16_t(ts_ptr + 1) & 0x1fff] & 0xff));
			bool const ts_dup_prev = m_ts_last_stage_valid
					&& ts_hash == m_ts_last_stage_hash
					&& stage_hw == m_ts_last_stage_len;
			bool ts_dup_recent = false;
			for (unsigned i = 0; i < TS_HIST; i++)
			{
				if (m_ts_hist[i].valid && m_ts_hist[i].hash == ts_hash
						&& m_ts_hist[i].len == stage_hw)
				{
					ts_dup_recent = true;
					break;
				}
			}
			attotime const ts_now = machine().time();
			uint32_t ts_gap_us = 0;
			if (m_ts_prev_stage_valid)
			{
				double const gap = (ts_now - m_ts_prev_stage_time).as_double() * 1e6;
				ts_gap_us = (gap >= 9999999.0) ? 9999999 : uint32_t(gap + 0.5);
			}
			++m_ts_stage_seq;
			++m_ts_stages_win;
			++m_ts_vbl_stage_idx;
			if (ts_dup_prev)
				++m_ts_dup_prev_win;
			if (ts_dup_recent)
				++m_ts_dup_recent_win;
			// Per-window length-class table: first TS_CLS_MAX distinct lengths
			// get their own row (blue in-game needs 1-2; red needs a few for
			// the variable-length scene-table class); overflow -> "other".
			bool ts_cls_done = false;
			for (unsigned i = 0; i < TS_CLS_MAX && !ts_cls_done; i++)
			{
				if (m_ts_cls[i].stages == 0)
				{
					m_ts_cls[i].len = stage_hw;
					m_ts_cls[i].stages = 1;
					m_ts_cls[i].fresh = ts_dup_recent ? 0 : 1;
					ts_cls_done = true;
				}
				else if (m_ts_cls[i].len == stage_hw)
				{
					++m_ts_cls[i].stages;
					if (!ts_dup_recent)
						++m_ts_cls[i].fresh;
					ts_cls_done = true;
				}
			}
			if (!ts_cls_done)
			{
				++m_ts_cls_other_stages;
				if (!ts_dup_recent)
					++m_ts_cls_other_fresh;
			}
			// The compact per-stage line: t, vblank frame + stage#-within-
			// vblank (k), running stage index (n), staged length, seq cells,
			// content hash, dup-of-previous-stage / dup-of-recent-ring flags,
			// continuation flag, read pointer, gap since the previous stage.
			logerror("TXSTAGE: stage t=%.6f fr=%u k=%u n=%u len=%04x seq=%04x h=%08x dup=%d dupr=%d cont=%d ptr=%04x gap_us=%u\n",
					ts_now.as_double(), m_ts_frame, m_ts_vbl_stage_idx,
					m_ts_stage_seq, unsigned(stage_hw), unsigned(ts_seq), ts_hash,
					ts_dup_prev ? 1 : 0, ts_dup_recent ? 1 : 0, ts_cont ? 1 : 0,
					unsigned(ts_ptr), ts_gap_us);
			m_ts_hist[m_ts_hist_idx].hash = ts_hash;
			m_ts_hist[m_ts_hist_idx].len = stage_hw;
			m_ts_hist[m_ts_hist_idx].valid = true;
			m_ts_hist_idx = uint8_t((m_ts_hist_idx + 1) % TS_HIST);
			m_ts_last_stage_hash = ts_hash;
			m_ts_last_stage_len = stage_hw;
			m_ts_last_stage_valid = true;
			m_ts_prev_stage_time = ts_now;
			m_ts_prev_stage_valid = true;
		}
	}

	// P010/P011 EXPERIMENT: a TXOFFSET write is the ROM chunk emitter's "new
	// message" event (0x8000BB6C PATH C - continuation chunks B/B' never write
	// it).  Latch the auto-advancing DMA pointer and read the new message's
	// TOTAL halfword count from the two size cells the emitter just consumed at
	// TXOFFSET-2 / TXOFFSET-1 (PATH C reads C139_DATA[2*idx],
	// C139_DATA[2*idx+2], then programs REG[0x0E]=idx+2).  expected > 0xFF
	// announces a multi-chunk bulk message.
	//
	// P011: do NOT tear down a held bulk reassembly here.  P010 called
	// chunk_drop("new-txoffset") on EVERY TXOFFSET write, which dropped the
	// held bulk message the instant the ROM staged ANY interleaved message -
	// the direct cause of "only the first 255hw chunk is ever held, then
	// stale-swept" (0 reassembled in every P010 log).  Under option (a) the
	// held message survives unrelated TXOFFSET writes; it is retired only when
	// it completes, overshoots, goes stale, or is explicitly superseded by a
	// NEW saturated bulk message in send_pending_tx_frame().  The new pointer
	// latched here becomes the read pointer for the next triggered send; if
	// that send turns out to be the held message's continuation it will resume
	// m_chunk_resume_ptr, otherwise it is an interleaved message that passes
	// through while the bulk stays held.
	if (m_chunk_armed && reg_idx == 7)
	{
		m_chunk_tx_ptr = m_regs[7];
		m_chunk_tx_ptr_valid = true;
		m_chunk_saw_txoffset = true;   // P011: PATH C reprogram - the next send is NOT a continuation
		uint16_t const ptr = m_regs[7];
		m_chunk_expected_hw = (uint32_t(m_ram[(ptr - 2) & 0x1fff] & 0xff) << 8)
							|  uint32_t(m_ram[(ptr - 1) & 0x1fff] & 0xff);
		m_chunk_bulk_pending = (m_chunk_expected_hw > CHUNK_SAT_HW
							 && m_chunk_expected_hw <= CHUNK_MAX_HW);

		// P027 (b) EXPERIMENT (branch patch/hb-cadence-wipe-restore): each
		// bulk ANNOUNCE (this TXOFFSET write, the ROM emitter's PATH C "new
		// message" event) grants exactly ONE wipe-restore budget for the
		// stage->trigger window it opens.  The budget is consumed by the
		// first restore and NEVER re-armed except here, by the ROM's own
		// next bulk announce - that is the structural guarantee this can
		// never degenerate into the P025 suppression fight (the device
		// yields to every rx_clear beyond the first per message).
		if (m_wipe_restore_armed && m_chunk_bulk_pending)
			m_wipe_restore_budget = true;

		// P031 (branch patch/announce-latch): ARM / REFRESH / SUPERSEDE at
		// the ROM's PATH C announce - the same event CHUNK_PASSTHRU keys
		// bulk_pending on.  A BULK announce (expected in (0xFF..0x400])
		// arms the latch with (offset, expected, now); the SAME class
		// re-announced (same offset+expected - the ROM's own <=250 ms retry
		// of a lost class) REFRESHES it (new TTL anchor, fresh one-shot);
		// any OTHER announce SUPERSEDES it - per the ROM's normal flow the
		// newest stage always wins, and a stale (offset,size) must never
		// reach the wire.  A NON-bulk announce while a latch is live also
		// supersedes (the ROM moved on to another message; its TXSIZE will
		// overwrite the staged register anyway).  Dispatch-time reads the
		// payload from m_ram, so a refresh automatically prefers the
		// freshest ROM-staged bytes.
		if (m_al_armed && m_chunk_passthru)
		{
			if (m_chunk_bulk_pending)
			{
				bool const al_refresh = m_al_valid && m_al_offset == m_regs[7]
						&& m_al_expected_hw == m_chunk_expected_hw;
				if (m_al_valid && !al_refresh)
				{
					++m_al_superseded;
					logerror("announce-latch: superseded t=%.6f offset=0x%04x expected_hw=%u new_offset=0x%04x new_expected_hw=%u superseded=%u (new ROM bulk announce replaces the latch)\n",
							machine().time().as_double(), m_al_offset, m_al_expected_hw,
							m_regs[7], m_chunk_expected_hw, m_al_superseded);
				}
				// P038 (branch patch/latch-v3-dedupe): supersede-on-re-announce
				// OBSERVABILITY.  The retirement itself has been effective since
				// v1: a same-class re-announce REFRESHES the latch below with
				// m_al_wiped=false, which silently kills any pending wiped
				// dispatch (the START edge then takes the stock zero-size
				// abort), and a different-class announce SUPERSEDES (logged
				// above).  The P036 run's superseded=0 AND refreshed=0 while
				// ~180 duplicates flowed is NOT a miss of this site: every
				// flood latch terminated (dispatch/consumed_ok/expiry) BEFORE
				// its slot's next announce, so no re-announce ever found a live
				// latch - the duplicates flowed through each announce's OWN
				// legitimate dispatch, structurally out of any supersede's
				// reach (hence the dispatch-site dedupe).  This v3-gated
				// counter+line makes the previously-silent refresh-retires-
				// pending-wipe event visible so that claim is verifiable
				// run-over-run; expect 0 in the flood pattern.  =1/=2 output
				// is untouched.
				if (m_al_dedupe && al_refresh && m_al_wiped)
				{
					++m_al_refresh_retired;
					logerror("announce-latch: refresh-retired-wiped t=%.6f offset=0x%04x hw=%u expected_hw=%u refresh_retired=%u (same-class re-announce arrived while a wiped dispatch was still pending - the ROM's own newer announce retires the older wiped stage; expect 0 in the P036 flood pattern where latches always terminated first)\n",
							machine().time().as_double(), m_al_offset, m_al_wiped_hw,
							m_al_expected_hw, m_al_refresh_retired);
				}
				m_al_valid = true;
				m_al_wiped = false;
				m_al_wiped_hw = 0;
				m_al_offset = m_regs[7];
				m_al_expected_hw = m_chunk_expected_hw;
				m_al_time = machine().time();
				if (al_refresh)
					++m_al_refreshed;
				else
					++m_al_latched;
				logerror("announce-latch: latched t=%.6f offset=0x%04x expected_hw=%u refresh=%d latched=%u refreshed=%u\n",
						machine().time().as_double(), m_al_offset, m_al_expected_hw,
						al_refresh ? 1 : 0, m_al_latched, m_al_refreshed);
			}
			else if (m_al_valid)
			{
				++m_al_superseded;
				m_al_valid = false;
				m_al_wiped = false;
				logerror("announce-latch: superseded t=%.6f offset=0x%04x expected_hw=%u new_offset=0x%04x new_expected_hw=%u superseded=%u (non-bulk ROM announce - latch dropped)\n",
						machine().time().as_double(), m_al_offset, m_al_expected_hw,
						m_regs[7], m_chunk_expected_hw, m_al_superseded);
			}
		}
	}

	// IRQ-ack write (0x000f) also clears the pending RX-status bits (1, 2).
	// Without this, status_r keeps reporting bit 1 set after the game's
	// IRQ handler runs; the dispatcher re-fires the RX handler on stale
	// data, the missed-marker counter ticks up, and the link times out
	// even though one frame did successfully validate.
	if (reg_idx == 1 && data == 0x000f)
		m_regs[1] &= ~uint16_t(0x06);

	// P014 EXPERIMENT (branch patch/txsize-commit-trigger): the txsize_commit
	// TX trigger - dispatch a bulk REMAINDER chunk that the START-edge trigger
	// below never fires.
	//
	// P013's TX-emitter trace DECIDED that a >255hw bulk message's REMAINDER is
	// programmed correctly (right size = announced-255, right resume DMA pointer)
	// but is written by the ROM as `TXSIZE=<remainder>` followed by `START=0002`
	// (bit0 LOW, NO rising edge), so the START-rising-edge-only trigger below
	// (the original :1826 hook) never calls send_pending_tx_frame() for it - the
	// remainder sits dead until the next message clobbers the slot.  Gold
	// (mame/10-c139-register-map.md) documents the chip's SECOND trigger
	// condition `txsize_commit` = "a non-zero TXSIZE written while the chip is
	// armed", which the device never implemented.  This adds it for the held-
	// continuation case.
	//
	// CRITICAL GATING - we key on the pending-continuation STATE, not gold's
	// literal `(m_regs[3] & 0x01)` raw START bit: P013 showed the head's START
	// rising edge clears bit0 synchronously (:1834) BEFORE the ROM writes the
	// remainder TXSIZE, so the raw START bit is 0 at the remainder write and
	// gold's textbook txsize_commit would miss it.  Instead we fire only when a
	// bulk continuation is genuinely held and resumable:
	//   - m_chunk_armed                 : the reassembly machinery is on (so this
	//                                     held-continuation state even exists; the
	//                                     trigger is naturally inert when the env
	//                                     gate is off and the stock TX path is
	//                                     byte-for-byte unchanged);
	//   - !m_chunk_accum.empty()        : a head (or earlier remainder) is held;
	//   - !m_chunk_saw_txoffset         : NO TXOFFSET reprogram since the last
	//                                     send => PATH B/B' continuation, NOT a
	//                                     PATH C new message (the head write has
	//                                     saw_txoffset=1 and an EMPTY accum, so it
	//                                     is excluded on both counts and can never
	//                                     be re-fired here);
	//   - (m_regs[5] & 0xff) != 0       : an actual remainder size was just staged
	//                                     (a PATH-D idle TXSIZE=0 tick is ignored).
	// send_pending_tx_frame()'s existing is_continuation branch (computed from the
	// SAME three flags) then reads from m_chunk_resume_ptr and appends/reassembles;
	// it also synchronously clears m_regs[5]->0, so a duplicate fire on the same
	// write is impossible (reg_w runs once per host write) and the trailing
	// START=0002 / the next message's START=0003 see TXSIZE==0 (no stale re-send).
	// A multi-remainder message (>510hw) is handled naturally: each remainder is
	// its own TXSIZE write, so each gets its own commit trigger - no loop.
	if (m_chunk_armed && reg_idx == 5
			&& (m_regs[5] & 0xff) != 0
			&& !m_chunk_accum.empty()
			&& !m_chunk_saw_txoffset)
	{
		// CHUNK_REASM (armed-gated like the rest of the family, so it shows on a
		// chunk-reassembly-only run too): pairs 1:1 with the resulting
		// `tx chunk staged ... from ptr=<resume>` and `reassembled` lines.
		logerror("CHUNK_REASM: commit-trigger fired t=%.6f ptr=0x%04x txsize_hw=%u held_accum_hw=%u/%u -> send\n",
				machine().time().as_double(), m_chunk_resume_ptr,
				unsigned(m_regs[5] & 0xff), unsigned(m_chunk_accum.size() / 2),
				m_chunk_held_expected_hw);
		// P037 (branch patch/latch-genstamp): GENERATION-STAMP compare at the
		// REMAINDER's stage instant, READ-ONLY, only when the held message's
		// head was a v2 snapshot dispatch (m_al_gen_head_live - armed at the
		// dispatch, retired by any other new head, so this is provably the
		// same message).  If the ring slot's head halfwords no longer match
		// the dispatched snapshot HERE, the ROM recomposed the slot between
		// the wipe (head = generation N) and this remainder stage (generation
		// N+1) - the receiver will assemble two generations into one message
		// with no device re-read involved: the P035 residual-MISMATCH
		// hypothesis, proven or killed by time-joining these lines to the
		// peer's marker=MISMATCH completes.  head_age_ms shares the dispatch
		// age_ms anchor (the announce time), so ages superset dispatch ages.
		// Multi-remainder messages log one line per commit, each at its own
		// instant.  Nothing else: the send below is untouched.
		if (m_al_snapshot && m_al_gen_head_live)
		{
			uint16_t const gen_cur_hw0 = m_ram[m_al_gen_head_offset & 0x1fff];
			uint16_t const gen_cur_hw1 = m_ram[uint16_t(m_al_gen_head_offset + 1) & 0x1fff];
			bool const gen_two_hw = m_al_gen_head_hws >= 2;
			bool const gen_differ = (gen_cur_hw0 != m_al_gen_head_hw0)
					|| (gen_two_hw && gen_cur_hw1 != m_al_gen_head_hw1);
			unsigned const gen_head_age_ms =
					unsigned((machine().time() - m_al_gen_head_time).as_double() * 1000.0 + 0.5);
			if (gen_differ)
			{
				++m_al_gen_differ;
				logerror("announce-latch: remainder gen=differ t=%.6f head_offset=0x%04x head_age_ms=%u remainder_hw=%u cur_hw0=%04x cur_hw1=%04x snap_hw0=%04x snap_hw1=%04x gen_differ=%u (the ring slot's head halfwords no longer match the dispatched wipe-time snapshot at this remainder's stage instant - head and remainder come from DIFFERENT ROM compose generations)\n",
						machine().time().as_double(), m_al_gen_head_offset,
						gen_head_age_ms, unsigned(m_regs[5] & 0xff),
						gen_cur_hw0, gen_cur_hw1,
						m_al_gen_head_hw0, m_al_gen_head_hw1, m_al_gen_differ);
			}
			else
				logerror("announce-latch: remainder gen=same t=%.6f head_offset=0x%04x head_age_ms=%u remainder_hw=%u (ring slot head halfwords still match the dispatched wipe-time snapshot at this remainder's stage instant)\n",
						machine().time().as_double(), m_al_gen_head_offset,
						gen_head_age_ms, unsigned(m_regs[5] & 0xff));
		}
		send_pending_tx_frame();
	}

	// P061 EXPERIMENT (branch patch/tx-complete-irq, off patch/hb-phase-aware):
	// the TX-COMPLETE dispatch - the txsize_commit trigger generalized from
	// P014's bulk-remainder case to the mode-2 standalone frame.
	//
	// WHY HERE and not a deferred wipe/IRQ (the audit's sketch): the ROM's pump
	// writes START(reg3=3) BEFORE TXOFFSET/TXSIZE in every passage
	// (0x8000BC5C/BC6C/BCAC decoded this session), so our reg3-edge send always
	// fires one stage early (the zero-size abort beat) and today a staged frame
	// only reaches the wire when a peer delivery lands exactly at a pump gate
	// read: the rx_clear wipes reg5 inside that very reg_r, the delivery IRQ's
	// handler (0x8000BEAC bit2 -> pump, corpus-verified) nests a pump passage
	// that ABORTS its own START and re-stages, and the interrupted OUTER
	// passage's START edge then transmits that fresh stage (the P060 red log's
	// wipe -> START(abort) -> restage -> START(emit) quartet).  A timer-fired
	// wipe outside that alignment just produces the abort+restage beat and the
	// next kick bails on the restage - deadlock back into heartbeat pacing.
	// The real chip needs no such interleave because a non-zero TXSIZE written
	// while START is armed IS its transmit trigger (gold txsize_commit) and
	// TXSIZE->0 + the TX-done IRQ follow from the serialization itself.  So the
	// faithful model is: dispatch the freshly staged frame synchronously RIGHT
	// HERE, and let send_pending_tx_frame()'s own existing sync TXSIZE clear
	// (the P052 clear site (i), the value the pump gate busy-polls) be the
	// TX-complete release.  This block never writes regs[5] or any register -
	// it only calls the device's own dispatch path (the standing "never touch
	// regs[5]" rule is honored literally).  No IRQ pulse is added: status_r
	// already returns bit2 (TX cause) permanently, and an extra pulse per tick
	// would run extra handler pump passages whose duplicate re-stages block the
	// next kick at the gate (measured mechanics, see the agent log).
	//
	// GATING: armed + debounced mode-2 only (m_hbpa_ingame - the P060 set_ingame
	// plumbing; outside mode-2 this block is dead and the stop-and-wait is
	// byte-identical, protecting op55 establishment).  Fresh PATH-C-style
	// standalone stages only: TXOFFSET rewritten this passage (saw_txoffset -
	// the exact complement of P014's !saw_txoffset, so the two triggers are
	// structurally disjoint), no bulk announced/held, plausible size, and the
	// ROM's strict trailer invariant holds on the staged RAM image (a
	// saturated head / continuation / fragment never passes).  PACING (the
	// P060 lesson): deduped on the frame's own drift seq (cells 0-1) + length
	// - the compose bumps the seq once per composed frame, so the pump's
	// post-send restage and any IRQ-handler pump passage staging the same slot
	// again are SKIPPED and PARKED (released at the ROM's next TXSIZE
	// busy-poll, see reg_r - the modeled TX completing - so a parked dup can
	// never block the next kick's fresh stage) - dispatches are structurally
	// <= 1 per compose = <= 1 distinct frame per vblank, with
	// TXCI_MAX_PER_VBLANK as the hard belt-and-braces cap.
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	if (m_txci_armed && m_hbpa_ingame && reg_idx == 5 && (m_regs[5] & 0xff) != 0)
	{
		uint16_t const stage_hw = m_regs[5];
		// A >255-hw VM frame under the BURST_QUANTUM poke announces its whole
		// size (bulk_pending latches TRUE at the TXOFFSET write) but stages as
		// ONE burst with TXSIZE == the announced total - that IS a fresh
		// standalone frame (the fight-era class) and must dispatch here.  A
		// saturated 0xFF head of a genuine chunk train stages LESS than its
		// announce and stays excluded (chunk machinery keeps it, stock path).
		bool const single_burst_whole = m_chunk_bulk_pending && m_bq_armed
				&& stage_hw == m_chunk_expected_hw;
		bool const fresh_standalone = stage_hw >= 4 && stage_hw <= CHUNK_MAX_HW
				&& (!m_chunk_armed
					|| (m_chunk_saw_txoffset && m_chunk_accum.empty()
						&& (!m_chunk_bulk_pending || single_burst_whole)));
		// Any new stage first invalidates a previous park: only a stage this
		// block itself verifies as the last-dispatched frame may be released
		// at the busy-poll below (reg_r) - everything else is the stock
		// stop-and-wait's business.
		m_txci_parked_dup = false;
		if (!fresh_standalone)
			++m_txci_bulk_skips_win;
		else
		{
			// Same read-pointer resolution the dispatch itself will use (our
			// gate excludes the continuation case).
			uint16_t const ptr = (m_chunk_armed && m_chunk_tx_ptr_valid)
					? m_chunk_tx_ptr : m_regs[7];
			// ROM strict trailer invariant on the staged RAM image - the same
			// arithmetic nw_frame_complete applies to the wire bytes: the last
			// halfword carries the bit-8 end marker AND claims exactly this
			// stage's halfword count.
			uint16_t const endhw  = m_ram[uint16_t(ptr + stage_hw - 1) & 0x1fff];
			uint16_t const sizehi = m_ram[uint16_t(ptr + stage_hw - 2) & 0x1fff];
			uint32_t const claimed = (uint32_t(sizehi & 0xff) << 8) | (endhw & 0xff);
			if (!((endhw & 0x100) && claimed == stage_hw))
				++m_txci_incomplete_skips_win;
			else
			{
				// The frame's own ROM drift seq (cells 0-1 = the low bytes of
				// halfwords 0/1) - the nw_frame_seq key, read from the staged
				// image.
				uint16_t const seq = uint16_t(((m_ram[ptr & 0x1fff] & 0xff) << 8)
						| (m_ram[uint16_t(ptr + 1) & 0x1fff] & 0xff));
				if (m_txci_last_valid && seq == m_txci_last_seq
						&& stage_hw == m_txci_last_len)
				{
					// Already-transmitted frame re-staged (the pump's own
					// economy).  PARK it: the busy-poll release in reg_r
					// models this stage's TX completing (see the member-block
					// comment) so it can never block the next kick's fresh
					// stage at the 0x8000BB80 gate.
					++m_txci_dup_skips_win;
					m_txci_parked_dup = true;
				}
				else if (m_txci_vbl_dispatches >= TXCI_MAX_PER_VBLANK)
					++m_txci_cap_skips_win;
				else
				{
					send_pending_tx_frame();
					// The emit path's own synchronous TXSIZE clear is the
					// TX-complete; if it did not run (no peer connected /
					// size-cap drop), nothing crossed - leave the dedupe key
					// alone so the stock behavior owns the stage.
					if ((m_regs[5] & 0xff) == 0)
					{
						m_txci_last_seq = seq;
						m_txci_last_len = stage_hw;
						m_txci_last_valid = true;
						++m_txci_vbl_dispatches;
						++m_txci_dispatch_win;
						++m_txci_dispatch_tot;
						if (!m_txci_first_logged)
						{
							m_txci_first_logged = true;
							logerror("TX_COMPLETE_IRQ: first-dispatch t=%.6f seq=%04x hw=%u ptr=0x%04x (staged TX transmitted at its own stage instant; steady-state visibility is the 1/s status rail - no per-frame lines)\n",
									machine().time().as_double(), unsigned(seq),
									unsigned(stage_hw), unsigned(ptr));
						}
					}
				}
			}
		}
	}

	// P063 EXPERIMENT (branch patch/tx-complete-v2, off patch/txstage-trace):
	// the TX-COMPLETE RELEASE v2 admission gate - the P061 txsize_commit
	// dispatch re-keyed on the P062-measured CONTENT identity.  Structurally
	// disjoint from the P014 commit trigger above (fresh_standalone requires
	// saw_txoffset + empty accumulator = the exact complement of P014's
	// continuation predicate) and mutually exclusive with the dead P061 block
	// (device_start refuses V2 when TX_COMPLETE_IRQ is armed).
	//
	// THE GATE (P062 Q2/Q6): dups are STRICTLY ADJACENT re-stages of the
	// pending frame ([fresh, dup x1-2] at +199us/+16.5ms; dup==prev in ~99.9%
	// of stages; a fresh stage's hash recurred 0 times in 38k fresh hashes on
	// both cabs) - so dispatch IFF the staged image's FNV-1a hash differs from
	// the previously dispatched stage (class-agnostic: no seq - a slow shared
	// PHASE counter, P061's fatal key - no len, no per-class state; the
	// TXC2_HIST ring is free belt-and-braces for the 0.07/s A-B-A transients).
	// TXSIZE=0 writes are idle pump passages (bursting to 5.9k/s at
	// transitions): counted, otherwise COMPLETELY ignored - no dispatch, no
	// gate/window/park perturbation.  A hash-identical re-stage is PARKED
	// (released at the first busy-poll after the modeled window closes, see
	// reg_r) and - when it lands on an IDLE serializer - opens a busy window
	// itself, so the passage that staged it can never be instantly re-released
	// (the P061 back-pressure lesson: the pump needs its stock 70-100
	// passages/s texture, not 4.7-6.5k/s).
	// MODEL PROVENANCE: Fable 5.  Experiment branch only - never merge.
	if (m_txc2_armed && m_hbpa_ingame && reg_idx == 5)
	{
		if (m_regs[5] == 0)
			++m_txc2_zero_win;   // idle passage - ignored entirely (spec rule 2)
		else
		{
			uint16_t const stage_hw = m_regs[5];
			// Structural admission (the P061 checks, unchanged): a fresh
			// PATH-C standalone stage, incl. the BURST_QUANTUM >255-hw
			// whole-frame class; chunk-train heads/continuations stay with the
			// stock/P014 machinery.
			bool const single_burst_whole = m_chunk_bulk_pending && m_bq_armed
					&& stage_hw == m_chunk_expected_hw;
			bool const fresh_standalone = stage_hw >= 4 && stage_hw <= CHUNK_MAX_HW
					&& (!m_chunk_armed
						|| (m_chunk_saw_txoffset && m_chunk_accum.empty()
							&& (!m_chunk_bulk_pending || single_burst_whole)));
			if (!fresh_standalone)
			{
				// Not this gate's business - and the register now holds a
				// stage the stock path owns, so a stale park must never clear
				// it at a poll.
				++m_txc2_bulk_skip_win;
				m_txc2_parked_dup = false;
			}
			else
			{
				uint16_t const ptr = (m_chunk_armed && m_chunk_tx_ptr_valid)
						? m_chunk_tx_ptr : m_regs[7];
				// ROM strict trailer invariant on the staged RAM image (same
				// arithmetic as nw_frame_complete on the wire bytes).
				uint16_t const endhw  = m_ram[uint16_t(ptr + stage_hw - 1) & 0x1fff];
				uint16_t const sizehi = m_ram[uint16_t(ptr + stage_hw - 2) & 0x1fff];
				uint32_t const claimed = (uint32_t(sizehi & 0xff) << 8) | (endhw & 0xff);
				if (!((endhw & 0x100) && claimed == stage_hw))
				{
					// Incomplete/torn stage: leave it to the stock path (gate
					// state untouched, so a later completed re-stage of this
					// content hashes differently and dispatches fresh).
					++m_txc2_incomplete_win;
					m_txc2_parked_dup = false;
				}
				else
				{
					uint32_t const h = ts_hash_ram(m_ram, ptr, uint32_t(stage_hw));
					bool const dup_prev = m_txc2_prev_valid
							&& h == m_txc2_prev_hash && stage_hw == m_txc2_prev_len;
					bool dup_ring = false;
					for (unsigned i = 0; i < TXC2_HIST && !dup_ring; i++)
						dup_ring = m_txc2_hist[i].valid
								&& m_txc2_hist[i].hash == h
								&& m_txc2_hist[i].len == stage_hw;
					if (dup_prev || dup_ring)
					{
						// Already-crossed content re-offered by the pump.
						// PARK; the first post-window busy-poll releases it
						// (reg_r).  On an idle serializer the re-stage opens
						// its own window - on real hardware these bytes WOULD
						// serialize for ~a frame before TXSIZE cleared, and
						// without that hold the poll right after this write
						// would release a new passage instantly (spin).
						++m_txc2_dup_skip_win;
						if (!dup_prev)
							++m_txc2_ring_skip_win;
						m_txc2_parked_dup = true;
						if (machine().time() >= m_txc2_busy_until)
						{
							m_txc2_busy_until = machine().time()
									+ attotime::from_msec(m_txc2_busy_ms);
							m_txc2_busy_hw = stage_hw;
						}
						// window already open: pure no-op (never extended -
						// the +199us same-vblank dup beat rides through)
					}
					else
					{
						// FRESH content - dispatch synchronously at the stage
						// instant (the txsize_commit trigger; the staged bytes
						// are provably pristine right now, P035).
						bool const midwin = machine().time() < m_txc2_busy_until;
						send_pending_tx_frame();
						// The emit path's own synchronous TXSIZE clear is the
						// TX handoff; if it did not run (no peer connected /
						// size-cap abort), nothing crossed - gate state stays
						// untouched and the stock behavior owns the stage.
						// FULL 16-bit compare (not the low byte): a 0x100/
						// 0x200-hw stage has a zero low byte, which would fake
						// a clear on an aborted send.
						if (m_regs[5] == 0)
						{
							m_txc2_prev_hash = h;
							m_txc2_prev_len = stage_hw;
							m_txc2_prev_valid = true;
							m_txc2_hist[m_txc2_hist_idx].hash = h;
							m_txc2_hist[m_txc2_hist_idx].len = stage_hw;
							m_txc2_hist[m_txc2_hist_idx].valid = true;
							m_txc2_hist_idx = uint8_t((m_txc2_hist_idx + 1) % TXC2_HIST);
							m_txc2_parked_dup = false;
							m_txc2_busy_until = machine().time()
									+ attotime::from_msec(m_txc2_busy_ms);
							m_txc2_busy_hw = stage_hw;
							++m_txc2_dispatch_win;
							++m_txc2_dispatch_tot;
							if (midwin)
								++m_txc2_midwin_win;   // structurally impossible per the passage model; observed = investigate
							if (!m_txc2_first_dispatch_logged)
							{
								m_txc2_first_dispatch_logged = true;
								logerror("TX_COMPLETE_V2: first-dispatch t=%.6f hw=%u h=%08x ptr=0x%04x (fresh-content stage transmitted at its stage instant; busy-poll reads BUSY for %u ms; steady-state visibility is the 1/s status rail)\n",
										machine().time().as_double(), unsigned(stage_hw),
										h, unsigned(ptr), m_txc2_busy_ms);
							}
						}
					}
				}
			}
		}
	}

	// TX trigger: writing register 3 with bit 0 transitioning 0 -> 1
	// initiates a frame send pulled from RAM at TX FIFO Pointer (reg 7)
	// of length TX Frame Size (reg 5).
	if (reg_idx == 3 && !old_tx_bit0 && (m_regs[3] & 0x01))
	{
		send_pending_tx_frame();
		// Real hardware clears the TX trigger bit on completion.  We
		// model this synchronously since our "transmission" finishes
		// the moment we hand off to asio.  Leaving the bit set would
		// have the game's link probe interpret "TX still in flight"
		// and refuse to advance its TX script position.
		m_regs[3] &= ~uint16_t(0x01);
	}
}
