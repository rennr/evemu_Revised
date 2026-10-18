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

#include "eve-server.h"

#include <set>
#include <vector>

#include "../../eve-common/EVE_Store.h"

#include "Client.h"
#include "character/Character.h"
#include "inventory/Inventory.h"
#include "store/StoreDB.h"
#include "store/StoreMarshal.h"
#include "store/Wardrobe.h"

static bool HasContentRole(Client* pClient)
{
    return ((pClient->GetAccountRole() & Acct::Role::CONTENT) == Acct::Role::CONTENT);
}

// the typeIDs of the doll's resources that have one (any repeats kept)
static void GetWornTypes(PyDict* doll, std::vector<int32>& into)
{
    std::vector<int32_t> resourceIDs;
    StoreMarshal::GetDollResourceIDs(doll, resourceIDs);
    std::vector<int32> ids(resourceIDs.begin(), resourceIDs.end());
    StoreDB::GetResourceTypeIDs(ids, into);
}

// this character's apparel in the item hangar of the station it is docked in (none at character selection or in space)
static void GetHangarApparel(Client* pClient, uint32 charID, std::vector<InventoryItemRef>& into)
{
    if ((pClient->GetCharacterID() != charID) or !pClient->IsDocked())
        return;
    Inventory* pHangar(pClient->GetStationHangar(pClient->GetStationID()));
    if (pHangar == nullptr)
        return;
    std::vector<InventoryItemRef> items;
    pHangar->GetItemsByFlag(flagHangar, items);
    for (auto cur : items)
        if ((cur->ownerID() == charID) and (cur->categoryID() == EVEDB::invCategories::Apparel) and (cur->quantity() > 0))
            into.push_back(cur);
}

static void GetWardrobe(Client* pClient, uint32 charID, std::set<int32>& into)
{
    CharacterRef cRef(pClient->GetChar());
    if ((pClient->GetCharacterID() == charID) and (cRef.get() != nullptr) and (cRef->GetMyInventory() != nullptr)) {
        std::vector<InventoryItemRef> items;
        cRef->GetMyInventory()->GetItemsByFlag(flagWardrobe, items);
        for (auto cur : items)
            into.insert(cur->typeID());
        return;
    }
    // character selection (forced recustomization):  the character isn't loaded
    StoreDB::GetWardrobeTypes(charID, into);
}

void Wardrobe::CheckWorn(Client* pClient, uint32 charID, PyDict* doll, std::vector<int32>& toWardrobe)
{
    if (HasContentRole(pClient))
        return;

    std::vector<int32> worn;
    GetWornTypes(doll, worn);
    if (worn.empty())
        return;

    std::vector<InventoryItemRef> apparel;
    GetHangarApparel(pClient, charID, apparel);
    std::set<int32> hangar, wardrobe;
    for (auto cur : apparel)
        hangar.insert(cur->typeID());
    GetWardrobe(pClient, charID, wardrobe);

    // the client checks this too (characterCreationLayer.py UpdateExistingCharacter) and raises the same
    int32 missing(StoreRules::FirstUnownedType(worn, hangar, wardrobe));
    if (missing != 0)
        throw UserError("ItemNotAtStation").AddFormatValue("item", new PyInt(missing));

    toWardrobe = StoreRules::TypesToWardrobe(worn, wardrobe);
}

void Wardrobe::Wear(Client* pClient, uint32 charID, const std::vector<int32>& types)
{
    if (types.empty())
        return;
    std::vector<InventoryItemRef> apparel;
    GetHangarApparel(pClient, charID, apparel);

    for (int32 typeID : types) {
        InventoryItemRef iRef(nullptr);
        for (auto cur : apparel)
            if ((cur->typeID() == (uint32)typeID) and (cur->locationID() != charID)) {
                iRef = cur;
                break;
            }
        if (iRef.get() == nullptr) {
            // CheckWorn found it a moment ago
            _log(ITEM__ERROR, "Wardrobe::Wear - %s(%u): no type %i left in the hangar to wear.", pClient->GetName(), charID, typeID);
            continue;
        }
        if (iRef->quantity() > 1) {
            iRef = iRef->Split(1, true);
            if (iRef.get() == nullptr) {
                _log(ITEM__ERROR, "Wardrobe::Wear - %s(%u): could not split one unit of type %i.", pClient->GetName(), charID, typeID);
                continue;
            }
        }
        iRef->Move(charID, flagWardrobe, true);
        iRef->SaveItem();
        _log(ITEM__MESSAGE, "Wardrobe::Wear - %s(%u) wears %s(%u):  moved into the wardrobe.", pClient->GetName(), charID, iRef->name(), iRef->itemID());
    }
}

void Wardrobe::CheckNewCharacter(Client* pClient, PyDict* doll)
{
    if (HasContentRole(pClient))
        return;
    // the creator hides typed resources at initial creation (ui/services/character.py, eveDollRandomizer.py)
    std::vector<int32> worn;
    GetWornTypes(doll, worn);
    for (int32 typeID : worn)
        if (StoreRules::IsOwnedType(typeID))
            throw CustomError ("A new character can't wear Noble Exchange apparel.  Buy it in the store once you are docked.");
}
