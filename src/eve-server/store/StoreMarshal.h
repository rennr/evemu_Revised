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

/* the python shapes of the NeX store and the character creator's doll data.
 *  this file is also compiled into eve-test, so it includes nothing from the server.
 *  see doc/code_and_design_notes/planning/PLAN-nex-store-aur.md
 */

#ifndef __STORE_MARSHAL_H_INCL__
#define __STORE_MARSHAL_H_INCL__

#include <cstdint>
#include <vector>

class PyDict;

namespace StoreMarshal {

    struct Offer {
        int32_t offerID;
        int32_t typeID;
        int32_t quantity;       // units per bundle
        int64_t priceAUR;       // per bundle
    };

    /* GetAvailableOffers():  {offerID: util.KeyVal(offerID, goods)}.  goods is a list of util.KeyVal(offered, argumentType,
     *  argumentValue, amount, details1, details2, details3):  the offered type, then its AUR cost (storeSvc.py)
     */
    PyDict* BuildOffersReply(const std::vector<Offer>& offers);

    // the paperdollResourceIDs of a doll's modifiers (CharacterAppearance::Build reads the same rows)
    void GetDollResourceIDs(PyDict* doll, std::vector<int32_t>& into);
}

#endif  // __STORE_MARSHAL_H_INCL__
