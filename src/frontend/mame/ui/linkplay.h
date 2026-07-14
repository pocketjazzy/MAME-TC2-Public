// license:BSD-3-Clause
// copyright-holders:CrouchCrisis project
/***************************************************************************

    ui/linkplay.h

    "Link Play Config" menu - native UI editor for the Namco C139/C422
    twin-cabinet link settings (CrouchCrisis tc2-linkplay fork, P068).

    Shown from the in-emulation main menu ONLY when the running machine
    carries a namco_c139 device (see find_c139); every other machine's
    menus are untouched.  Edits are stored on the device and persisted in
    the per-machine cfg file (cfg\<system>.cfg <linkplay> node); they are
    consumed once at machine start, so changes apply on the NEXT launch.

    MODEL PROVENANCE: Fable 5.

***************************************************************************/

#ifndef MAME_FRONTEND_UI_LINKPLAY_H
#define MAME_FRONTEND_UI_LINKPLAY_H

#pragma once

#include "ui/menu.h"

#include <string>

class namco_c139_device;

namespace ui {

class menu_linkplay : public menu
{
public:
	menu_linkplay(mame_ui_manager &mui, render_container &container);
	virtual ~menu_linkplay() override;

	// device-presence gate for the main menu (nullptr = machine has no link device)
	static namco_c139_device *find_c139(running_machine &machine);

	enum : unsigned
	{
		FIELD_LISTEN_HOST = 0,  // red/left listener: bind IP
		FIELD_LISTEN_PORT,      // red/left listener: bind port
		FIELD_CONNECT_HOST,     // blue/right connector: target IP
		FIELD_CONNECT_PORT,     // blue/right connector: target port
		FIELD_COUNT
	};

private:
	virtual void populate() override;
	virtual bool handle(event const *ev) override;
	virtual void menu_dismissed() override;

	void refresh_buffers();                             // reload edit buffers from the device
	std::string device_value(unsigned field) const;     // the device's current value, as display text
	bool commit_field(unsigned field, bool quiet);      // validate + apply one buffer (garbage keeps the old value)

	namco_c139_device *const m_device;
	std::string m_buffer[FIELD_COUNT];
};

} // namespace ui

#endif // MAME_FRONTEND_UI_LINKPLAY_H
