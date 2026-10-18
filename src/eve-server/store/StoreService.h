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

/* the Noble Exchange (NeX) virtual goods store (storeServer) and the Aurum conversions.
 *  see doc/code_and_design_notes/planning/PLAN-nex-store-aur.md
 */

#ifndef __STORE_SERVICE_H_INCL__
#define __STORE_SERVICE_H_INCL__

#include "services/Service.h"

class Client;

class StoreService : public Service<StoreService>
{
public:
    StoreService();

    // the store switch:  account.storeEnabled at startup, /store open|close at runtime
    static bool IsOpen();
    static void SetOpen(bool open);

    /* invbroker ConvertPlexToCurrency(plexItemIDs, qty):  'qty' PLEX from the listed stacks in this station's item
     *  hangar, 3500 AUR each from CONCORD (refPlexConversion)
     */
    static void ConvertPlex(Client* pClient, PyList* itemIDs, int64 qty);
    /* invbroker ConvertAurTokenToCurrency([itemID], qty):  'qty' Aurum Tokens of that stack in this station's item
     *  hangar, 1000 AUR each from CONCORD (refAurumTokenConversion)
     */
    static void ConvertAurToken(Client* pClient, PyList* itemIDs, int64 qty);

protected:
    PyResult AcceptOffer(PyCallArgs& call, PyInt* offerID, PyInt* quantity);
    PyResult GetAvailableOffers(PyCallArgs& call);

    // VG_STORE_CLOSED when the store is switched off
    static void CheckOpen();
};

#endif /* !__STORE_SERVICE_H_INCL__ */
