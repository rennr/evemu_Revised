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

#ifndef __STORE_DB_H_INCL__
#define __STORE_DB_H_INCL__

#include <set>
#include <vector>

#include "ServiceDB.h"
#include "store/StoreMarshal.h"

class StoreDB
{
public:
    // the enabled offers (storeOffers), by offerID
    static void GetOffers(std::vector<StoreMarshal::Offer>& into);
    // false when there is no such enabled offer
    static bool GetOffer(int32 offerID, StoreMarshal::Offer& into);

    // the typeIDs of these paperdoll resources (paperdollResources.typeID), the untyped ones left out
    static void GetResourceTypeIDs(const std::vector<int32>& resourceIDs, std::vector<int32>& typeIDs);
    // the types in the character's wardrobe (its own items with flagWardrobe), read from the db
    static void GetWardrobeTypes(uint32 charID, std::set<int32>& into);

    // the AUR balance stored for a character (chrCharacters.aurBalance).  false when there is no such character
    static bool GetAurBalance(uint32 charID, double& balance);

    // the stipend's last payment (chrAurumStipend).  false when the character was never paid
    static bool GetStipendLastPaid(uint32 charID, int64& lastPaid);
    /* stores newLastPaid only if the stored value is still oldLastPaid (or there was no row and hadRow is false).
     *  false when another check got there first, so the payment must not be made
     */
    static bool AdvanceStipend(uint32 charID, bool hadRow, int64 oldLastPaid, int64 newLastPaid);
};

#endif  // __STORE_DB_H_INCL__
