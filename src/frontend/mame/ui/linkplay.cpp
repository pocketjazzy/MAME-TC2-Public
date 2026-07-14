// license:BSD-3-Clause
// copyright-holders:CrouchCrisis project
/***************************************************************************

    ui/linkplay.cpp

    "Link Play Config" menu - native UI editor for the Namco C139/C422
    twin-cabinet link settings (CrouchCrisis tc2-linkplay fork, P068).

    Character entry follows the menu_barcode_reader precedent
    (ui/barcode.cpp): the selected item's subtext is a live edit buffer
    fed by IPT_SPECIAL unichar events through ui::input_character with a
    per-field character filter; IPT_UI_CLEAR clears, IPT_UI_PASTE pastes.
    A field is COMMITTED (validated, then stored on the device) when the
    user presses Select on it, and any still-edited fields are committed
    when the menu closes; invalid input is rejected and the old value
    kept.  The device persists the values in cfg\<system>.cfg and reads
    them once at machine start, so changes apply on the NEXT launch (no
    live socket rebinding is attempted).

    MODEL PROVENANCE: Fable 5.

***************************************************************************/

#include "emu.h"
#include "ui/linkplay.h"

#include "ui/ui.h"
#include "ui/utils.h"

#include "namco/namco_c139.h"


namespace ui {

namespace {

// itemrefs: 1..FIELD_COUNT = the editable fields (1 + field index), then reset
constexpr uintptr_t ITEMREF_FIRST_FIELD = 1;
constexpr uintptr_t ITEMREF_RESET       = ITEMREF_FIRST_FIELD + menu_linkplay::FIELD_COUNT;

void *itemref_for_field(unsigned field)
{
	return reinterpret_cast<void *>(ITEMREF_FIRST_FIELD + field);
}

// strict dotted-quad IPv4: four 0-255 decimal parts, no blanks, no
// leading zeros (inet_pton-compatible), digits and dots only
bool valid_ipv4(std::string const &s)
{
	int parts = 1, digits = 0, value = 0;
	bool leading_zero = false;
	for (char const c : s)
	{
		if (c == '.')
		{
			if (!digits || (++parts > 4))
				return false;
			digits = 0;
			value = 0;
			leading_zero = false;
		}
		else if ((c >= '0') && (c <= '9'))
		{
			if (leading_zero)
				return false;
			if (!digits && (c == '0'))
				leading_zero = true;
			value = (value * 10) + (c - '0');
			if ((++digits > 3) || (value > 255))
				return false;
		}
		else
		{
			return false;
		}
	}
	return (parts == 4) && digits;
}

// TCP port 1-65535, digits only
bool valid_port(std::string const &s, unsigned &value)
{
	if (s.empty() || (s.length() > 5))
		return false;
	value = 0;
	for (char const c : s)
	{
		if ((c < '0') || (c > '9'))
			return false;
		value = (value * 10) + unsigned(c - '0');
	}
	return (value >= 1) && (value <= 65535);
}

} // anonymous namespace


//-------------------------------------------------
//  find_c139 - the main menu's device-presence
//  gate: first namco_c139 in the machine, if any
//-------------------------------------------------

namco_c139_device *menu_linkplay::find_c139(running_machine &machine)
{
	return device_type_enumerator<namco_c139_device>(machine.root_device()).first();
}


//-------------------------------------------------
//  ctor/dtor
//-------------------------------------------------

menu_linkplay::menu_linkplay(mame_ui_manager &mui, render_container &container)
	: menu(mui, container)
	, m_device(find_c139(mui.machine()))
{
	set_heading(_("Link Play Config"));
	refresh_buffers();
}


menu_linkplay::~menu_linkplay()
{
}


//-------------------------------------------------
//  refresh_buffers / device_value
//-------------------------------------------------

void menu_linkplay::refresh_buffers()
{
	if (m_device)
		for (unsigned field = 0; FIELD_COUNT > field; ++field)
			m_buffer[field] = device_value(field);
}


std::string menu_linkplay::device_value(unsigned field) const
{
	switch (field)
	{
	case FIELD_LISTEN_HOST:  return m_device->lp_listen_host();
	case FIELD_LISTEN_PORT:  return std::to_string(m_device->lp_listen_port());
	case FIELD_CONNECT_HOST: return m_device->lp_connect_host();
	case FIELD_CONNECT_PORT: return std::to_string(m_device->lp_connect_port());
	}
	return std::string();
}


//-------------------------------------------------
//  commit_field - validate one edit buffer and
//  store it on the device; garbage is rejected
//  and the buffer reverted to the old value
//-------------------------------------------------

bool menu_linkplay::commit_field(unsigned field, bool quiet)
{
	std::string const current = device_value(field);
	if (m_buffer[field] == current)
		return true;    // unchanged - nothing to do

	bool ok = false;
	switch (field)
	{
	case FIELD_LISTEN_HOST:
	case FIELD_CONNECT_HOST:
		ok = valid_ipv4(m_buffer[field]);
		if (ok)
		{
			if (FIELD_LISTEN_HOST == field)
				m_device->set_lp_listen_host(m_buffer[field]);
			else
				m_device->set_lp_connect_host(m_buffer[field]);
		}
		break;

	case FIELD_LISTEN_PORT:
	case FIELD_CONNECT_PORT:
		{
			unsigned value = 0;
			ok = valid_port(m_buffer[field], value);
			if (ok)
			{
				if (FIELD_LISTEN_PORT == field)
					m_device->set_lp_listen_port(uint16_t(value));
				else
					m_device->set_lp_connect_port(uint16_t(value));
			}
		}
		break;
	}

	if (ok)
	{
		if (!quiet)
			ui().popup_time(3, "%s", _("Link play setting saved - applies on the next launch"));
	}
	else
	{
		ui().popup_time(4, _("Invalid link play value \"%1$s\" rejected - kept \"%2$s\""), m_buffer[field], current);
		m_buffer[field] = current;  // keep the old value
	}
	return ok;
}


//-------------------------------------------------
//  populate
//-------------------------------------------------

void menu_linkplay::populate()
{
	if (!m_device)
		return;

	// session status (informational)
	if (m_device->comm_cli_override())
		item_append(_("This session: -comm_* command line override active"), FLAG_DISABLE, nullptr);
	else if (m_device->comm_started())
		item_append(
				m_device->comm_is_connector()
					? _("This session: connector, using these settings")
					: _("This session: listener, using these settings"),
				FLAG_DISABLE, nullptr);
	else
		item_append(_("This session: solo (link DIP:5 off, or comm unavailable)"), FLAG_DISABLE, nullptr);

	// role (the driver's red/blue Machine Configuration choice)
	if (m_device->lp_role_available())
		item_append(
				m_device->lp_role_is_connector()
					? _("Machine ID: Right/Blue - connector (dials target IP:port)")
					: _("Machine ID: Left/Red - listener (binds listener IP:port)"),
				FLAG_DISABLE, nullptr);

	item_append(menu_item_type::SEPARATOR);

	item_append(_("Listener bind IP"),     m_buffer[FIELD_LISTEN_HOST],  0, itemref_for_field(FIELD_LISTEN_HOST));
	item_append(_("Listener port"),        m_buffer[FIELD_LISTEN_PORT],  0, itemref_for_field(FIELD_LISTEN_PORT));
	item_append(_("Connector target IP"),  m_buffer[FIELD_CONNECT_HOST], 0, itemref_for_field(FIELD_CONNECT_HOST));
	item_append(_("Connector target port"), m_buffer[FIELD_CONNECT_PORT], 0, itemref_for_field(FIELD_CONNECT_PORT));

	item_append(menu_item_type::SEPARATOR);

	item_append(_("Reset to defaults"), 0, reinterpret_cast<void *>(ITEMREF_RESET));

	item_append(menu_item_type::SEPARATOR);

	item_append(_("Type digits/dots, Select applies - changes take effect on the next launch"), FLAG_DISABLE, nullptr);
}


//-------------------------------------------------
//  handle
//-------------------------------------------------

bool menu_linkplay::handle(event const *ev)
{
	if (!ev || !m_device)
		return false;

	// which editable field (if any) does the event's itemref name?
	uintptr_t const ref = reinterpret_cast<uintptr_t>(ev->itemref);
	bool const on_field = (ref >= ITEMREF_FIRST_FIELD) && (ref < (ITEMREF_FIRST_FIELD + FIELD_COUNT));
	unsigned const field = on_field ? unsigned(ref - ITEMREF_FIRST_FIELD) : 0;

	// ...and which one is currently selected (typing/paste target)?
	uintptr_t const selref = reinterpret_cast<uintptr_t>(get_selection_ref());
	bool const sel_field = (selref >= ITEMREF_FIRST_FIELD) && (selref < (ITEMREF_FIRST_FIELD + FIELD_COUNT));
	unsigned const selected = sel_field ? unsigned(selref - ITEMREF_FIRST_FIELD) : 0;
	bool const sel_is_host = sel_field && ((FIELD_LISTEN_HOST == selected) || (FIELD_CONNECT_HOST == selected));

	auto const host_chars = [] (char32_t ch) { return ((ch >= '0') && (ch <= '9')) || ('.' == ch); };
	auto const port_chars = [] (char32_t ch) { return (ch >= '0') && (ch <= '9'); };

	switch (ev->iptkey)
	{
	case IPT_UI_SELECT:
		if (on_field)
		{
			// commit this field (validation inside; garbage keeps the old value)
			commit_field(field, false);
			ev->item->set_subtext(m_buffer[field]);
			return true;
		}
		else if (ITEMREF_RESET == ref)
		{
			m_device->lp_reset_defaults();
			refresh_buffers();
			ui().popup_time(3, "%s", _("Link play settings reset to defaults - applies on the next launch"));
			reset(reset_options::REMEMBER_POSITION);
		}
		break;

	case IPT_UI_CLEAR:
		if (on_field)
		{
			m_buffer[field].clear();
			ev->item->set_subtext(m_buffer[field]);
			return true;
		}
		break;

	case IPT_UI_PASTE:
		if (sel_field)
		{
			bool const changed = sel_is_host
					? paste_text(m_buffer[selected], 15, host_chars)
					: paste_text(m_buffer[selected], 5, port_chars);
			if (changed)
			{
				ev->item->set_subtext(m_buffer[selected]);
				return true;
			}
		}
		break;

	case IPT_SPECIAL:
		if (sel_field)
		{
			bool const changed = sel_is_host
					? input_character(m_buffer[selected], 15, ev->unichar, host_chars)
					: input_character(m_buffer[selected], 5, ev->unichar, port_chars);
			if (changed)
			{
				ev->item->set_subtext(m_buffer[selected]);
				return true;
			}
		}
		break;
	}

	return false;
}


//-------------------------------------------------
//  menu_dismissed - commit anything still edited
//  when the menu closes (validated; garbage is
//  dropped with a popup and the old value kept)
//-------------------------------------------------

void menu_linkplay::menu_dismissed()
{
	if (m_device)
		for (unsigned field = 0; FIELD_COUNT > field; ++field)
			commit_field(field, true);
}

} // namespace ui
