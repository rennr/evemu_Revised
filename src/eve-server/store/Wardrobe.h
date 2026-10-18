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

/* premium apparel ownership (the NeX wardrobe).  a paperdoll resource with a typeID can only be worn while that type is
 *  owned:  in the item hangar of the station the character is docked in, or in its wardrobe (its own items with
 *  flagWardrobe).  saving a look moves one unit of each newly worn type from the hangar into the wardrobe, where it
 *  stays.  ROLE_CONTENT wears anything.  see doc/code_and_design_notes/planning/PLAN-nex-store-aur.md
 */

#ifndef __STORE_WARDROBE_H_INCL__
#define __STORE_WARDROBE_H_INCL__

#include <vector>

class Client;
class PyDict;

namespace Wardrobe {
    /* before a recustomized look is saved (paperDollServer UpdateExistingCharacterLimited/Full):  throws
     *  ItemNotAtStation {item: typeID} for a worn type that isn't owned.  'toWardrobe' gets the types to Wear() after the save
     */
    void CheckWorn(Client* pClient, uint32 charID, PyDict* doll, std::vector<int32>& toWardrobe);
    // moves one unit of each type from the station item hangar into the character's wardrobe
    void Wear(Client* pClient, uint32 charID, const std::vector<int32>& types);
    // a new character (CreateCharacterWithDoll) owns no apparel:  throws for a typed resource
    void CheckNewCharacter(Client* pClient, PyDict* doll);
}

#endif  // __STORE_WARDROBE_H_INCL__
