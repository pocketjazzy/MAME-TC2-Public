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

// P002 (branch patch/vblank-lockstep) tuning constants.
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

// P010 (branch patch/chunk-reassembly) tuning constants.
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

// P027 (branch patch/hb-cadence-wipe-restore) tuning constants.
//
// (a) The ROM declares a link timeout when the drift word 0x802F3504 reaches
// 0x11 (17 frames; writers 0x8000BB44 / 0x8000C050 also clear the freshness
// byte 0x802F3502 = "gate3502"); a validated ingest resets it to ~2 (transit
// latency), so the refresh budget is ~15 frames = 250 ms at 60 fps - EXACTLY
// the stock heartbeat period, i.e. a 0-2 frame margin.  P026 run 1: blue
// crossed that line 40 times (38 in the cutscene at ~1.1/s = the user-visible
// jitter).  NAMCOS23_PATCH_HB_CADENCE_MS makes the cadence tunable (run 1
// uses 150 ms = ~8 frames of margin); the clamp below rejects typos.
// Retained P052/P060 knowledge: a flat 16 ms cadence broke op55 establishment
// (attract/handshake floods), and P060's phase-aware fast in-game cadence
// FAILED catastrophically (stale replay paired with every fresh emit) - keep
// the cadence at the adopted 33 ms; the token lever is DEAD.
constexpr unsigned long HB_CADENCE_MIN_MS       = 10;    // sanity floor for the env override
constexpr unsigned long HB_CADENCE_MAX_MS       = 1000;  // > 283 ms (the 17-frame ceiling) is self-defeating but allowed for A/B
// Driver-pushed staging-mode-2 debounce (P060-era plumbing, retained as shared
// infrastructure - consumer today: the adopted P063 TX-complete release v2).
constexpr uint32_t      HBPA_DEBOUNCE_VBLANKS   = 60;    // consecutive mode-2 vblanks before the debounced in-game state arms (~1 s); drops immediately
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

// P031 (branch patch/announce-latch) tuning constant.
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

// P035 (branch patch/latch-v2-snapshot) tuning constant.
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

// P038 (branch patch/latch-v3-dedupe) tuning constant.
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

// Retained P013 TX slot-ring knowledge (the TXEMIT register-programming
// trace and its txemit_slot_of helper were removed in P072): the ROM's BF38
// Phase B TX builder advances gp+0x75CA by 0x400 halfwords (mod 0x0C00) per
// call - a 4-slot 0x400-stride ring in C139 data RAM (gold
// func-8000bf38.md L33/L69).  PATH C programs REG[0x0E] (TXOFFSET) =
// gp[0x75CA]+2 (the 2 size cells sit at the slot base), so a TX read
// pointer of the form slot_base+2 is a fresh message at slot (ptr-2)/0x400;
// any other (advanced) pointer is a continuation resume offset.

// P062-origin FNV-1a 32-bit content hash (the TXSTAGE per-stage trace that
// introduced it was removed in P072; the hash lives on as the KEPT P063
// TX_COMPLETE_V2 admission key): ts_hash_ram walks the C422 RAM image at
// the staged read pointer in WIRE byte order (high byte first, low byte
// second per halfword, 0x1fff mask - exactly the readout loop in
// send_pending_tx_frame).  FNV-1a is cheap (~2 ops/byte) and
// collision-safe enough for dup detection at this volume.
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

// P067 (branch patch/defaults-on): the ADOPTED patch recipe becomes
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
// NON-adopted vars (LINK_WAIT, TCP_NODELAY) keep the old "armed only when
// set" idiom - do NOT route them through this helper.  (Every retired patch
// and all NAMCOS23_TRACE_* diagnostics that also used that idiom were
// removed in P072.)  MODEL PROVENANCE: Fable 5.
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

					// P002 (branch patch/vblank-lockstep): size
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
	// with -comm_remotehost "".  READ-ONLY role tag; its consumer today
	// is the P068 Link Play Config menu via comm_is_connector() (the P055
	// boat-jitter trace that introduced it was removed in P072).
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

// P038 (branch patch/latch-v3-dedupe): record a wire completion
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

// Retained P050 wire-format knowledge (the nw_frame_complete/nw_frame_seq
// helpers that encoded it fed only the FIFO-JOIN stamps and were removed in
// P072 phase C; the same trailer arithmetic lives on inline in the
// TX_COMPLETE_V2 admission gate in reg_w):
// - COMPLETE-STANDALONE test = the ROM's strict trailer invariant: the final
//   halfword carries the bit-8 end marker AND the trailer's claimed length
//   equals the frame's own halfword count.  A multi-chunk message FRAGMENT
//   never satisfies it - an intermediate chunk has no trailer (a coincidental
//   bit-8 fails the length equality) and a last chunk's claimed length is the
//   whole-message total, not the chunk's own size.  Wire byte order:
//   payload[2k] = halfword high byte, payload[2k+1] = low byte.
// - The ROM per-frame drift SEQUENCE (cellseq, the P048/P050 latency join
//   key): 0x8000B8F0-914 reconstructs the remote seq from the LOW bytes of
//   halfwords 0 and 1 and subtracts it from local gp+0x75B8; it increments
//   once per composed frame, so it uniquely tags a frame across the wire.

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

	// P010/P011: the ROM's chunk emitter writes TXOFFSET only for
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
	}

	// Clear TX Frame Size to signal "TX complete" to the host CPU.
	// Without this, the game's link probe function at 0x8000bb6c reads
	// Frame Size != 0 on every subsequent invocation and exits early
	// thinking a TX is still in progress, blocking all further protocol
	// traffic.  Real C422 hardware clears this when TX finishes; we do
	// it synchronously since our "transmission" is instantaneous.  This
	// also holds for a chunk we HOLD for reassembly below: the chunk has
	// been accepted by the "chip", so the emitter may stage the next one.

	// P031: on an announce-latch dispatch m_regs[5] is ALREADY 0 (the rx_clear
	// wiped it and was honored) - skip the store so the latch path literally
	// never writes the register file.  Every other path clears as always.
	if (!al_dispatch)
		m_regs[5] = 0;

	// P010/P011: decide whether this staged chunk is a complete
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
			// Pointer-continuity association (P011): the chunk is a
			// continuation iff the ROM did NOT reprogram TXOFFSET before
			// staging it (PATH B/B' never write TXOFFSET) - is_continuation,
			// computed above, which also forced fifo_ptr to
			// m_chunk_resume_ptr.  A defensive secondary accept covers the
			// no-interleaving case where the DMA pointer naturally lands on
			// the resume pointer even though a (same-slot) TXOFFSET write was
			// seen: only accept that when the size matches a continuation,
			// never when it is a fresh saturated announce (that is the "new
			// bulk reusing the same slot" case the analysis warned about,
			// which must supersede, not append).  (The P010 announce-flag
			// debug fallback - treat the next chunk unconditionally as the
			// continuation - was superseded by P011 and removed in P072: the
			// ROM's TXSIZE-staged-after-trigger timing never lined up with
			// the announce flag, so P010 held only the FIRST 255hw chunk.)
			bool const fresh_saturated_announce =
					(frame_size_words == CHUNK_SAT_HW
						&& m_chunk_expected_hw > CHUNK_SAT_HW
						&& m_chunk_expected_hw <= CHUNK_MAX_HW);
			bool const resumes = is_continuation
					|| (!had_txoffset_reprogram && fifo_ptr == m_chunk_resume_ptr
						&& !fresh_saturated_announce);

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
				if (m_chunk_passthru)
					emit_tx_frame(std::move(payload), true);
				if (have_hw == m_chunk_held_expected_hw)
				{
					// Complete.  Coalesce mode: emit the concatenation as ONE
					// wire frame.  P026 pass-through mode: every chunk already
					// went out individually - emit NOTHING here (the tracker
					// holds the full concatenation either way), just retire the
					// tracker.
					if (!m_chunk_passthru)
						emit_tx_frame(std::move(m_chunk_accum));
					m_chunk_accum.clear();
					m_chunk_bulk_pending = false;
					m_chunk_msg_chunks = 0;
				}
				else if (have_hw > m_chunk_held_expected_hw || have_hw > CHUNK_MAX_HW)
				{
					chunk_drop("overshoot");
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
			// own (no dispatch needed) - silent (the consumed_ok counter and the
			// 1/s status line it closed were removed in P072 phase C).
			if (m_al_armed && m_al_valid && m_al_offset == fifo_ptr)
			{
				m_al_valid = false;
				m_al_wiped = false;
				// P038 (branch patch/latch-v3-dedupe): this NATURAL head send
				// is THE completion class the dedupe ring exists for (the
				// P036 join: each stale duplicate dispatch had a same-class
				// natural complete ~150 ms earlier).  Recorded with the
				// latch's own key so the ROM's wiped cadence re-announce of
				// this class dedupes at its START edge.  Silent, matching the
				// site's existing behavior.
				if (m_al_dedupe)
					al_dedupe_record(m_al_offset, m_al_expected_hw);
			}
			if (m_chunk_passthru)
				emit_tx_frame(std::move(payload), true);
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
			if (m_al_armed && m_al_valid && m_al_offset == fifo_ptr)
			{
				m_al_valid = false;
				m_al_wiped = false;
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


// P010 (branch patch/chunk-reassembly): single exit point for a
// COMPLETE game frame onto the wire (a passthrough chunk, or a reassembled
// multi-chunk bulk message).  Also the heartbeat capture, so heartbeat
// replays always describe exactly what went on the wire.
// P026 (branch patch/reasm-chunk-passthru): bulk_chunk=true = a FRAGMENT of an
// in-progress >255hw message forwarded individually under CHUNK_PASSTHRU -
// excluded from the P021 injection and from heartbeat capture (see below).
void namco_c139_device::emit_tx_frame(std::vector<uint8_t> payload, bool bulk_chunk)
{
	if (!m_context || !m_context->connected())
		return;
	if (payload.empty())
		return;

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

	// Retained P018 op-70 TX-path RE (the OP70R cell-walk detector was removed
	// in P072): red is the cutscene driver; the op-70 EMIT path is gated at
	// 0x80016DC4 on gp+0x7074==2, then jal 0x800B2514 (li 0x70 / sb HH /
	// sb LL into the TX trailer).

	// Phase 9d (permanent): cache real TXs (>= 2 halfwords, i.e. proper
	// protocol frames - not the 1-halfword boot ping) for the heartbeat timer
	// to replay.  Skip the 2-byte length prefix at frame[0..1].  Re-arm the
	// heartbeat to fire m_hb_cadence_ms from now (well below the 17-frame
	// timeout threshold at 60 fps).
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
		// P063 (branch patch/tx-complete-v2): stamp the capture instant - the
		// stale-replay age-out anchor.  P062 proved the capture IS the newest
		// composed content at this instant; its AGE at replay time is the
		// poison metric.  A member store, no behavior change; guarded anyway
		// so the unset path is audit-trivially untouched.
		if (m_txc2_armed)
			m_txc2_cap_time = machine().time();
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(m_hb_cadence_ms));
	}
	// P050a COMPANION (branch patch/single-burst-pump): under the quantum poke
	// the fight-era frames arrive as >255-hw SINGLE BURSTS (bulk_chunk=false,
	// not the 255-hw chunk train the P027 (a) case re-arms on).  Re-arm the
	// clock on them too - real wire traffic - but do NOT capture (restamping
	// bytes 0-1 would corrupt the length header, same reason coalesced bulk is
	// excluded above).  Effect: the keepalive replay stays suppressed while real
	// large-frame traffic flows (a replay fires only after m_hb_cadence_ms of
	// total-TX silence), so a stale small replay can never out-arrive a fresh
	// fight frame.  Inert unless BURST_QUANTUM is armed.
	else if (m_heartbeat_timer
			&& ((m_hb_cadence_override && bulk_chunk)
				|| (m_bq_armed && !bulk_chunk && payload_words > CHUNK_SAT_HW)))
		m_heartbeat_timer->adjust(attotime::from_msec(m_hb_cadence_ms));

	m_context->send_frame(std::move(frame));
}


// P010/P011: abandon a pending reassembly (stale tail, overshoot,
// superseded by a new bulk message).  The partial bulk frame is dropped, never
// sent - exactly the pre-P010 outcome for this message, minus the garbage
// chunks on the wire.
void namco_c139_device::chunk_drop(const char *reason)
{
	if (m_chunk_accum.empty() && !m_chunk_bulk_pending)
		return;
	if (!m_chunk_accum.empty())
	{
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

	// (The P050b newest-wins delivery cap acted here - dropped after P051/P052,
	// removed in P072; the retained P049/P050 supersession + trailer-invariant
	// knowledge lives in the wire-format note above send_pending_tx_frame and
	// in the namco_c139.h P049/P050 note.)

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
		// P010 (branch patch/chunk-reassembly): when armed, wrap
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
		//
		// P010 (branch patch/chunk-reassembly): while a bulk
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
			// Coalesce-mode bulk mid-flight: the staged TXSIZE survives this
			// delivery (no wipe) - see the scoping rationale above.
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

				// P026 run 1 measured this stage->START rx_clear race at 116
				// hits on red (~54% of staged chunk sends lost PRE-WIRE, 2-6/s
				// through every bulk phase; chkfail=0 on both cabs proved no
				// on-wire truncation ever - the cost is pure CADENCE: each wipe
				// stretches an inter-bulk gap past the peer's 17-frame drift
				// ceiling).  The race: the ROM stages the head's TXSIZE, then
				// the START-edge write itself (or any register access in
				// between) runs deliver_rx_frames at its top - a pending peer
				// frame wipes m_regs[5] before the trigger fires and the send
				// reads size 0.  On real hardware the staged value survives (an
				// RX never clears a staged TX; the rx_clear emulation exists
				// for the LINK-UP busy-poll green light, which never coincides
				// with pass-through bulk state - see chunk_in_flight scoping
				// above).  The P027 one-shot TXSIZE restore that SKIPPED this
				// clear was refuted by the P028 A/B (the restore gate alone was
				// the regression - round-start exchange decimated both
				// directions; fix = drop the restore) and removed in P072; the
				// P031 ANNOUNCE-LATCH below repairs the race without ever
				// fighting the ROM's green light.
				if (staged_bulk)
				{
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
				m_regs[5] = 0;
			}
			else
				m_regs[5] = 0;
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


// Retained P016/P018 gate-4 VM wire-protocol knowledge (the OP70R cell-walk
// detector - and the stacked op6F/op-0x20 reports that rode it - were
// removed in P072): the link payload's "cell stream" is the LOW byte of
// each halfword; the ROM dispatcher 0x800AA840 reads ONE opcode byte at the
// cursor, dispatches through table 0x8023F4D0 to a handler that consumes a
// fixed operand count and returns the advanced cursor, looping to the 0x00
// terminator.  VERIFIED operand byte counts (consumed AFTER the opcode
// byte; gold/rom/40-gameplay-sync func-*.md + full.txt):
//   0x1F despawn(0x800ACCA0)=4; 0x20 release(0x800ACD48)=2;
//   0x3B(0x800B2278)=2; 0x64/65/66(0x800B1C3C)=3; 0x67(0x800B1D04)=4;
//   0x68(0x800B1DDC)=4; 0x69(0x800B1E98)=4; 0x6A(0x800B1F60)=5;
//   0x6B(0x800B2034)=5; 0x6C/6D/6E(0x800B2100)=3; 0x6F(0x800B2448)=6
//   (the play-clock pair); 0x70(0x800B2544)=2 (cutscene timer, value =
//   cur[0..1] big-endian & 0x7fff -> adoption 0x80016E28);
//   0x71(0x800B25AC)=1; 0x72(0x800B2620)=3; 0x73(0x800B26AC)=190(0xBE);
//   0x74(0x800B272C)=4.  Intrinsics handled inline by the dispatcher:
//   0xFD = 3 bytes total (FD b0 b1); 0xFE/0xFF = variable self-relative
//   skip (length not table-known).
// A byte value is an OPCODE only when the cursor lands on it - the P016
// lesson: naive byte-scans over-report mid-payload operand bytes (the fixed
// 0x70 data bytes at 0x11c6/0x4ea8 in the 250/252hw gameplay frames).  The
// device sees the raw frame; the ROM VM stream sits AFTER an L2/L3/L4
// header whose exact length depends on runtime gating (func-8000b8f0 -4
// header + func-800aaf3c +3/+8 app header) that the device cannot
// reproduce read-only.


// Retained P019/P021-era op55 wire-format knowledge (the P019 LINKBITS
// wire-flag tap and the P021 wire-only 0x6000 injection were retired and
// removed in P072): op55 is gate-4 table entry 0x8023F4D0[0x55]=0x800B058C.
// Its RX parse (0x800B05BC-0604) consumes, after the 0x55 opcode:
// hw(operand 0..1) + hw(operand 2..3) + 24-bit flags(operand 4..5..6) = $s2;
// the call at 0x800B0738 stores $s2 | 0xC0000000 into the PARTNER record
// rec1 +0x370 (0x80013E10) - the partner-record 0x6000 bits come verbatim
// from the wire.  The flags ride at cells[op55+5..+7]; 0x6000 = bits 13/14 =
// the middle flag byte cells[op55+6] & 0x60.  In the device byte stream a
// "cell" is the LOW byte of each halfword, so cells[op55+6] is
// payload[(op55+6)*2 + 1].  An outgoing emit_tx_frame payload is a COPY read
// out of the C422 link RAM (m_ram) - NOT MIPS main RAM - so wire-side edits
// never touch the live +0x370 record.


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

	// Phase 9d (permanent): heartbeat timer that periodically replays the
	// last real TX to keep the partner's link-state dispatcher firing (which
	// resets the partner's timeout counter via state-counter sync).
	m_heartbeat_timer = timer_alloc(FUNC(namco_c139_device::heartbeat_tick), this);

	// P002 (H2 "drift lockstep", branch patch/vblank-lockstep):
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


	// P010/P011: arm TX-side chunked bulk-frame reassembly from the
	// environment, same gate idiom as the other experiments and independent of
	// all of them.  REUSES NAMCOS23_PATCH_CHUNK_REASSEMBLY so the user's run
	// command is unchanged - P011 simply replaces P010's now-superseded broken
	// behavior.
	char const *const chunk_env = std::getenv("NAMCOS23_PATCH_CHUNK_REASSEMBLY");
	m_chunk_armed = chunk_env && chunk_env[0] != '\0'
			&& !(chunk_env[0] == '0' && chunk_env[1] == '\0');

	// P026 (branch patch/reasm-chunk-passthru): CHUNK PASS-THROUGH
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
				"pointer-continuity[a]", // sole association mode since P072 (the P010 announce-flag fallback is removed; "[a]" kept for log-parser stability)
				m_chunk_passthru ? "chunk-passthru" : "coalesce",
				m_chunk_passthru
					? "forward each associated chunk as its OWN <=255-halfword wire frame"
					: "coalesce saturated 0xFF-halfword chunk sequences into one wire frame",
				CHUNK_MAX_HW, CHUNK_STALE_MS);

	if (m_chunk_passthru)
		logerror("CHUNK_PASSTHRU: armed (NAMCOS23_PATCH_CHUNK_PASSTHRU=%s %s) - P026: the ROM's saturated chunk sequence goes on the wire as SEPARATE hardware-shaped frames (the receiving ROM's RX ring reassembles them itself: marker back-scan 0x8000BD80 + forward drain 0x8000BF38, exactly as on real hardware; the receiver stacks back-to-back frames in its 4096-word RX ring at the advancing FIFO pointer - nothing is overwritten, no receiver pacing needed); P011 pointer-continuity association + P014 txsize_commit dispatch UNCHANGED (m_chunk_accum kept as the association tracker, never sent); RETIRED in this mode: rx_clear suppression (P025: 832 suppressions/104 s wedge on the never-committing 351-hw announce) and heartbeat replay while a chunk sequence is open\n",
				passthru_val, patch_env_src(passthru_from_env, passthru_val).c_str());

	// P027 (branch patch/hb-cadence-wipe-restore): transport-
	// cadence changes targeting the P026 run-1 blue jitter (40 mode-2
	// freshness dips, 38 in the cutscene at ~1.1/s: the sparse
	// snapshot+heartbeat exchange rides the ROM's 17-frame drift ceiling
	// with a 0-2 frame margin; aggravated by 116 rxclear wipe races eating
	// ~54% of red's staged chunk sends pre-wire).  Of the three original
	// gates only (a) remains: the (b) one-shot wipe-restore was refuted by
	// the P028 A/B (the restore gate alone was the regression) and the (c)
	// heartbeat restamp was refuted (broken stamps) - both removed in P072.
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

	// P063 (branch patch/tx-complete-v2, off patch/txstage-trace):
	// NAMCOS23_PATCH_TX_COMPLETE_V2 - the P062-measured retry of the TX-complete
	// release.  Boolean idiom ("0"/empty = unset = byte-identical).  Tunables:
	// NAMCOS23_PATCH_TXC2_BUSY_MS (modeled TX serialization interval,
	// [TXC2_BUSY_MIN_MS..TXC2_BUSY_MAX_MS], default TXC2_BUSY_DEFAULT_MS) and
	// NAMCOS23_PATCH_TXC2_STALE_MS (heartbeat replay age-out,
	// [TXC2_STALE_MIN_MS..TXC2_STALE_MAX_MS], default TXC2_STALE_DEFAULT_MS);
	// out-of-range values log and fall back to the default (the main gate stays
	// armed - a typo on a tunable must not silently change the experiment to a
	// stock run).  Full rationale in the member-block comment (namco_c139.h).
	// MODEL PROVENANCE: Fable 5.
	// P067 (patch/defaults-on): ADOPTED - the main gate is armed by DEFAULT;
	// NAMCOS23_PATCH_TX_COMPLETE_V2=0 is the kill switch (fully inert, the
	// pre-P067 unset path = stock reg5 stop-and-wait).  The driver reads the
	// same main gate in machine_start() for the mode push - keep the two
	// agreeing.  The
	// two tunables: "0" (or unset) = the adopted default (TXC2_BUSY_DEFAULT_MS
	// / TXC2_STALE_DEFAULT_MS - a knob's kill switch reverts the OVERRIDE, the
	// patch itself stays armed); out-of-range still logs and falls back to the
	// default, exactly as before.
	bool txc2_from_env = false;
	char const *const txc2_val = patch_env_or_default("NAMCOS23_PATCH_TX_COMPLETE_V2", "1", txc2_from_env);
	m_txc2_armed = txc2_val != nullptr;
	if (!m_txc2_armed)
		logerror("TX_COMPLETE_V2: device DISABLED (NAMCOS23_PATCH_TX_COMPLETE_V2=0 kill switch - adopted default overridden; stock reg5 stop-and-wait release)\n");
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
		logerror("TX_COMPLETE_V2: armed (NAMCOS23_PATCH_TX_COMPLETE_V2=%s %s) busy_ms=%u %s stale_ms=%u %s hist=%u - P062-measured TX-complete release: while the debounced staging mode-2 state holds (driver set_ingame push, %u consecutive vblanks ~1 s, dropped immediately on loss/reset), a freshly staged standalone TXSIZE whose FNV-1a content hash differs from the previous dispatched stage is transmitted SYNCHRONOUSLY at its stage instant (prev-hash admission gate + %u-deep ring; TXSIZE=0 writes ignored entirely); after each dispatch (and for a duplicate re-stage landing on an idle serializer) the TXSIZE busy-poll reads a synthesized BUSY for busy_ms then clears - passages pace at the stock 70-100/s texture, NOT the P061 spin; a parked duplicate is released (reg5 -> 0) at the first post-window poll; when ACTIVE the heartbeat replay is SUPPRESSED if its captured payload is older than stale_ms (the red >255hw stale-replay channel; capture logic itself untouched).  Outside mode-2 the stock stop-and-wait is byte-identical.  ARM ON BOTH CABS.  Lines: one-shot ACTIVE/INACTIVE/first-dispatch/first-release/first-stale\n",
				txc2_val, patch_env_src(txc2_from_env, txc2_val).c_str(),
				m_txc2_busy_ms, patch_env_src(txc2_busy_overridden, txc2_busy_env).c_str(),
				m_txc2_stale_ms, patch_env_src(txc2_stale_overridden, txc2_stale_env).c_str(),
				unsigned(TXC2_HIST), HBPA_DEBOUNCE_VBLANKS, unsigned(TXC2_HIST));
	}

	// P031 (branch patch/announce-latch): the announce-latch
	// dispatcher - the code-side fix for the stage->START rx_clear announce
	// race behind every room/area skip (P026/P028/P029/P030, one signature,
	// not recipe-fixable: loss probability rises with the peer arrival
	// rate).  Same gate idiom as the other experiments; inert when unset.
	// Requires CHUNK_PASSTHRU (the bulk-announce recognition and the wipe
	// diagnostic site it hooks live in that mode) - a NOTE is logged and
	// nothing ever arms without it.
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
		logerror("ANNOUNCE_LATCH: armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - latch (offset,expected_hw,t) at each bulk TXOFFSET announce; the peer rx_clear wipe of a staged TXSIZE proceeds EXACTLY as before (green light never fought, register file never written by the latch) but is REMEMBERED; when the ROM's START edge then reads regs[5]==0-because-wiped, the send is reconstructed from the latch (byte-identical ROM-staged payload, one-shot per announce, TTL %d ms, superseded by any newer ROM stage); lines: announce-latch: latched/wiped/dispatched/expired/superseded\n",
				al_val, patch_env_src(al_from_env, al_val).c_str(), AL_TTL_MS);
		if (m_al_snapshot)
		{
			m_al_snap.reserve(AL_SNAP_MAX_BYTES);
			logerror("ANNOUNCE_LATCH: v2 snapshot armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - the wipe-capture COPIES the staged head payload bytes (up to %u) at the wipe instant (provably pristine: the ROM had just staged them, the wipe hits only the register) and the START-edge dispatch transmits the SNAPSHOT instead of re-reading C422 RAM (the P033 torn-dispatch fix; the bulk REMAINDER is unaffected - its P014 commit trigger already sends synchronously at stage time inside the very reg_w that staged it, no re-read window exists); v1 rails/counters unchanged; lines: wiped ... snap=/snap_bytes=/snap_copied=, dispatched-snap/dispatched-reread; P037 genstamp rider (LOG-ONLY): dispatched-snap lines carry gen=same|differ (ring slot's CURRENT first-2-hw vs the snapshot, read-only - the wire still carries the snapshot), a latch-dispatched message's P014 remainder commit logs announce-latch: remainder gen=same|differ head_age_ms=\n",
					al_val, patch_env_src(al_from_env, al_val).c_str(), AL_SNAP_MAX_BYTES);
			// P038 (branch patch/latch-v3-dedupe): v3 = v2 + dedupe.  The v2
			// banner above intentionally still prints under =3 (v3 contains
			// all v2 behavior; its grep keys stay valid).
			if (m_al_dedupe)
				logerror("ANNOUNCE_LATCH: v3 dedupe armed (NAMCOS23_PATCH_ANNOUNCE_LATCH=%s %s) - v2 snapshot behavior PLUS dispatch dedupe: every wire completion of a latched class (natural consumed_ok head sends AND latch dispatches; deduped consumptions are NOT recorded) enters a %u-entry (offset,expected_hw,t) ring; a START-edge dispatch whose class already completed within %d ms of its announce is CONSUMED WITHOUT SENDING (the P036 measured defect: ALL 56 torn completes were latch dispatches of content that had completed naturally ~150 ms earlier - the ROM's cadence re-announce was wiped by the peer as flow control and the latch un-dropped it as a stale head-gen-N/tail-gen-N+k duplicate that passes the byte-sum checksum and is INGESTED); the genuine rescue class (no recent same-class completion - the P031 room-1 save) dispatches exactly as v2; a same-class re-announce that retires a pending wiped dispatch is now counted (refresh-retired-wiped, expect 0 in floods); lines: announce-latch: deduped / refresh-retired-wiped\n",
						al_val, patch_env_src(al_from_env, al_val).c_str(), unsigned(AL_DEDUPE_RING), AL_DEDUPE_LOOKBACK_MS);
		}
		if (!m_chunk_passthru)
			logerror("ANNOUNCE_LATCH: NOTE - NAMCOS23_PATCH_CHUNK_PASSTHRU is not armed, so the bulk-announce state and wipe-diagnostic site the latch hooks never exist; the gate is effectively inert this run\n");
	}

	// P050 (branch patch/single-burst-pump): device-side arm.  Same
	// "non-empty and not literal 0" gate idiom as every gate above; inert when
	// unset.  MODEL PROVENANCE: Opus 4.8.
	//   NAMCOS23_PATCH_BURST_QUANTUM: the DEVICE half of the single-burst
	//       pump.  The driver owns the RAM-code poke @0x8000BC78; the device
	//       reads the SAME env so its two companions (latch-retire + heartbeat
	//       clock re-arm for >255-hw single bursts) arm.  Self-guarding: they
	//       only fire when a >255-hw single burst actually reaches the send/emit
	//       path, so a DRC-stale poke (ROM still chunking) leaves them dormant.
	// P067 (patch/defaults-on): BURST_QUANTUM ADOPTED - device half armed by
	// DEFAULT (the driver reads the same var in machine_start() for the poke -
	// keep the two resolutions agreeing); "0" is the kill switch (fully inert,
	// the pre-P067 unset path).
	bool bq_from_env = false;
	char const *const bq_val = patch_env_or_default("NAMCOS23_PATCH_BURST_QUANTUM", "1", bq_from_env);
	m_bq_armed = bq_val != nullptr;
	if (!m_bq_armed)
		logerror("BURST_QUANTUM: device DISABLED (NAMCOS23_PATCH_BURST_QUANTUM=0 kill switch - adopted default overridden; stock 0xFF-hw chunking companions)\n");
	if (m_bq_armed)
		logerror("BURST_QUANTUM: device armed (NAMCOS23_PATCH_BURST_QUANTUM=%s %s) - companion to the driver's @0x8000BC78 slti 0x100->0x401 poke: a >255-hw single-burst VM frame (frame_size == expected_hw, != 0xFF) crosses UNCHANGED via the self-contained passthrough (the saturation heuristic keys on EXACTLY 0xFF hw) and reassembles as one frame in the peer's RX ring (validator accepts <= 0x400 hw). Two self-guarding companions: (1) retire the announce-latch + record dedupe at the single-burst whole-frame send (the first-saturated consumed_ok site never runs for a single burst); (2) a >255-hw single burst re-arms the heartbeat clock so the keepalive replay stays suppressed under real large-frame traffic (a stale small replay can never out-arrive a fresh fight frame)\n",
				bq_val, patch_env_src(bq_from_env, bq_val).c_str());

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
	// config FINAL) instead of unconditionally staying solo.  (The P055/P059
	// role-banner note that lived here went with those banners in P072.)
	start_comm();
}


// P002 (H2 "drift lockstep", branch patch/vblank-lockstep).
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
void namco_c139_device::vblank_tick()
{
	// (The 1/s FIFO_JOIN / TX_COMPLETE_V2 / CHUNK_REASM / CHUNK_PASSTHRU /
	// ANNOUNCE_LATCH status rails that opened this function were removed in
	// P072 phase C; per-event lines remain at their sites.)

	// P010/P011: per-vblank stale sweep for a pending reassembly whose
	// continuation never arrived (e.g. peer disconnected mid-message, chunk
	// lost, or the continuation never resumed the pointer).  Runs before the
	// lockstep gate - the driver calls vblank_tick() unconditionally every
	// vblank.
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

	if (!timed_out)
	{
		m_lockstep_consec_timeouts = 0;
		return;
	}

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


// Driver-pushed staging-phase signal + debounce (P060-era plumbing, retained
// as shared infrastructure).  Called from the namcos23 vblank handler once per
// frame with mode2 = (staging mode word 0x802F3FD0 == 2) and the raw word for
// log context.  HYSTERESIS (anti-flap): the debounced in-game state arms only
// after HBPA_DEBOUNCE_VBLANKS (60, ~1 s) CONSECUTIVE mode-2 vblanks; ANY
// non-mode-2 vblank drops it immediately and zeroes the streak - so attract,
// the op55 handshake, mode-select and any re-establishment across
// resets/area transitions ALWAYS see the stock behavior, even if the mode
// word flickers.  Emulation-thread only.  MODEL PROVENANCE: Fable 5.
// Consumer today: the P063 TX-complete release v2 gate (m_txc2_armed).
void namco_c139_device::set_ingame(bool mode2, uint32_t mode_word)
{
	if (!m_txc2_armed)
		return;

	if (mode2)
	{
		if (m_hbpa_mode2_streak < HBPA_DEBOUNCE_VBLANKS)
			++m_hbpa_mode2_streak;
		if (m_hbpa_mode2_streak >= HBPA_DEBOUNCE_VBLANKS && !m_hbpa_ingame)
		{
			m_hbpa_ingame = true;
			++m_hbpa_transitions;
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


// Phase 9d (permanent): synthetic heartbeat - keeps the partner's link-state dispatcher
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
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(m_hb_cadence_ms));
		return;
	}

	// P063 (branch patch/tx-complete-v2): STALE-REPLAY AGE-OUT -
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
		if (!m_txc2_first_stale_logged)
		{
			m_txc2_first_stale_logged = true;
			logerror("TX_COMPLETE_V2: first-stale-suppress t=%.6f cap_age_ms=%u stale_ms=%u (heartbeat replay withheld - captured payload too old to restamp under self-release; one-shot line, later suppressions are silent)\n",
					machine().time().as_double(),
					unsigned((machine().time() - m_txc2_cap_time).as_double() * 1000.0 + 0.5),
					m_txc2_stale_ms);
		}
		if (m_heartbeat_timer)
			m_heartbeat_timer->adjust(attotime::from_msec(m_hb_cadence_ms));
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
	// dispatcher tail (0x8000BB10-1C) reads this as partner_counter and
	// computes a small delta against its own gp+0x75B8, which keeps
	// mem[0x802F3504] under the 17-frame timeout threshold.  Retained P026
	// knowledge: the offset of that delta is whatever m_local_counter happens
	// to be vs the PEER ROM's counter - P026 measured red riding drift 13-14
	// through a whole cutscene on blue's heartbeats (a constant 3-frame
	// margin under the 17-frame ceiling).  (The P027 (c) heartbeat-restamp
	// reserve knob that stamped the peer's aged counter instead was refuted -
	// its stamps were broken - and removed in P072.)
	uint16_t const stamp = m_local_counter;
	if (payload_size >= 2)
	{
		frame[2] = uint8_t((stamp >> 8) & 0xff);
		frame[3] = uint8_t(stamp & 0xff);
	}

	m_context->send_frame(std::move(frame));

	// Re-arm.  P027 (a): interval = m_hb_cadence_ms (250 unless
	// NAMCOS23_PATCH_HB_CADENCE_MS overrides - unset is bit-identical stock).
	if (m_heartbeat_timer)
		m_heartbeat_timer->adjust(attotime::from_msec(m_hb_cadence_ms));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void namco_c139_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), uint16_t(0));

	// P010/P011: clear runtime reassembly state (the env gate
	// m_chunk_armed, set in device_start(), survives reset).
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

	// Drop the debounced staging-mode state across a reset - re-establishment
	// must ALWAYS see the stock behavior; the debounce then requires a fresh
	// ~1 s of stable mode-2 before it re-arms.
	// P063 (branch patch/tx-complete-v2): the debounced in-game state gates
	// the v2 release - a reset drops it, so re-establishment always sees the
	// stock stop-and-wait.
	if (m_hbpa_ingame && m_txc2_armed)
		logerror("TX_COMPLETE_V2: INACTIVE (device_reset) - stock stop-and-wait resumes; debounce restarts\n");
	m_hbpa_ingame = false;
	m_hbpa_mode2_streak = 0;
	m_hbpa_transitions = 0;

	// P063 (branch patch/tx-complete-v2): clear the v2 release
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

	// P031 (branch patch/announce-latch): clear the announce-latch
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

	// P063 (branch patch/tx-complete-v2): the MODELED TX-BUSY - the
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
	// TXSIZE=0 write is dropped silently (nothing to clear).  Gated on armed
	// + debounced mode-2: outside
	// mode-2 no synthesized value is ever returned (set_ingame/device_reset
	// also clear the window, so none can leak into establishment).
	// MODEL PROVENANCE: Fable 5.
	bool txc2_busy_read = false;
	uint16_t txc2_busy_val = 0;
	if (m_txc2_armed && m_hbpa_ingame && reg_idx == 5)
	{
		if (machine().time() < m_txc2_busy_until)
		{
			txc2_busy_read = true;
			txc2_busy_val = ((m_txc2_busy_hw & 0xff) != 0)
					? m_txc2_busy_hw : uint16_t(m_txc2_busy_hw | 0x0001);
		}
		else if (m_txc2_parked_dup)
		{
			m_txc2_parked_dup = false;
			if (m_regs[5] != 0)
			{
				m_regs[5] = 0;
				if (!m_txc2_first_release_logged)
				{
					m_txc2_first_release_logged = true;
					logerror("TX_COMPLETE_V2: first-release t=%.6f (parked duplicate stage cleared at the first TXSIZE busy-poll after the modeled window - the poll returns 0 and the pump starts its next passage; one-shot line, later releases are silent)\n",
							machine().time().as_double());
				}
			}
		}
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

	COMBINE_DATA(&m_regs[reg_idx]);

	// P010/P011: a TXOFFSET write is the ROM chunk emitter's "new
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

	// P014 (branch patch/txsize-commit-trigger): the txsize_commit
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

	// ROM TX-pump mechanism (P061-era decode, retained knowledge): the pump
	// writes START(reg3=3) BEFORE TXOFFSET/TXSIZE in every passage
	// (0x8000BC5C/BC6C/BCAC), so a reg3-edge send always fires one stage early
	// (the zero-size abort beat); under the stock stop-and-wait a staged frame
	// reaches the wire only when a peer delivery lands exactly at a pump gate
	// read - the rx_clear wipes reg5 inside that very reg_r, the delivery
	// IRQ's handler (0x8000BEAC, jal pump on status bit2 - the corpus-verified
	// TX cause) nests a pump passage that ABORTS its own START and re-stages,
	// and the interrupted OUTER passage's START edge then transmits that fresh
	// stage (the wipe -> START(abort) -> restage -> START(emit) quartet).  A
	// timer-fired wipe outside that alignment just produces the abort+restage
	// beat and the next kick bails on the restage - deadlock back into
	// heartbeat pacing.  The real chip needs no such interleave because a
	// non-zero TXSIZE written while START is armed IS its transmit trigger
	// (gold txsize_commit) and TXSIZE->0 + the TX-done IRQ follow from the
	// serialization itself - hence the synchronous dispatch at the stage
	// instant modeled by TX_COMPLETE_V2 below.

	// P063 (branch patch/tx-complete-v2, off patch/txstage-trace):
	// the TX-COMPLETE RELEASE v2 admission gate - the P061-era txsize_commit
	// dispatch re-keyed on the P062-measured CONTENT identity.  Structurally
	// disjoint from the P014 commit trigger above (fresh_standalone requires
	// saw_txoffset + empty accumulator = the exact complement of P014's
	// continuation predicate).
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
	// MODEL PROVENANCE: Fable 5.
	if (m_txc2_armed && m_hbpa_ingame && reg_idx == 5)
	{
		if (m_regs[5] == 0)
		{
			// idle pump passage - ignored entirely (spec rule 2)
		}
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
				m_txc2_parked_dup = false;
			}
			else
			{
				uint16_t const ptr = (m_chunk_armed && m_chunk_tx_ptr_valid)
						? m_chunk_tx_ptr : m_regs[7];
				// ROM strict trailer invariant on the staged RAM image (see
				// the retained P050 wire-format note above send_pending_tx_frame).
				uint16_t const endhw  = m_ram[uint16_t(ptr + stage_hw - 1) & 0x1fff];
				uint16_t const sizehi = m_ram[uint16_t(ptr + stage_hw - 2) & 0x1fff];
				uint32_t const claimed = (uint32_t(sizehi & 0xff) << 8) | (endhw & 0xff);
				if (!((endhw & 0x100) && claimed == stage_hw))
				{
					// Incomplete/torn stage: leave it to the stock path (gate
					// state untouched, so a later completed re-stage of this
					// content hashes differently and dispatches fresh).
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
							if (!m_txc2_first_dispatch_logged)
							{
								m_txc2_first_dispatch_logged = true;
								logerror("TX_COMPLETE_V2: first-dispatch t=%.6f hw=%u h=%08x ptr=0x%04x (fresh-content stage transmitted at its stage instant; busy-poll reads BUSY for %u ms; one-shot line, later dispatches are silent)\n",
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
