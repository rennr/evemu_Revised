/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
*/

/* the AUR stipend (a server option):  account.aurStipendAmount AUR every account.aurStipendIntervalHours for each
 *  character, journaled refAurumGiveAway from CONCORD.  the last payment time is kept in chrAurumStipend and advanced
 *  with a compare-and-swap, so a restart or a second check can't pay twice.  checked for online characters
 *  (Client::ProcessClient).  see doc/code_and_design_notes/planning/PLAN-nex-store-aur.md
 */

#ifndef __STORE_AURUM_STIPEND_H_INCL__
#define __STORE_AURUM_STIPEND_H_INCL__

namespace AurumStipend {
    // pays what is due now.  returns the filetime to check again (0 = the stipend is off)
    int64 Check(uint32 charID);
}

#endif  // __STORE_AURUM_STIPEND_H_INCL__
