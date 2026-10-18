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

#include "store/StoreDB.h"

static void ReadOffer(DBResultRow& row, StoreMarshal::Offer& into)
{
    into.offerID  = row.GetInt(0);
    into.typeID   = row.GetInt(1);
    into.quantity = row.GetInt(2);
    into.priceAUR = row.GetInt64(3);
}

void StoreDB::GetOffers(std::vector<StoreMarshal::Offer>& into)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT offerID, typeID, quantity, priceAUR FROM storeOffers"
        " WHERE enabled = 1 AND quantity > 0 AND priceAUR >= 0 ORDER BY offerID"))
    {
        _log(DATABASE__ERROR, "StoreDB::GetOffers - %s", res.error.c_str());
        return;
    }
    DBResultRow row;
    while (res.GetRow(row)) {
        StoreMarshal::Offer offer = StoreMarshal::Offer();
        ReadOffer(row, offer);
        into.push_back(offer);
    }
}

bool StoreDB::GetOffer(int32 offerID, StoreMarshal::Offer& into)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT offerID, typeID, quantity, priceAUR FROM storeOffers"
        " WHERE offerID = %i AND enabled = 1 AND quantity > 0 AND priceAUR >= 0", offerID))
    {
        _log(DATABASE__ERROR, "StoreDB::GetOffer - %s", res.error.c_str());
        return false;
    }
    DBResultRow row;
    if (!res.GetRow(row))
        return false;
    ReadOffer(row, into);
    return true;
}

void StoreDB::GetResourceTypeIDs(const std::vector<int32>& resourceIDs, std::vector<int32>& typeIDs)
{
    if (resourceIDs.empty())
        return;
    std::string inList;
    ListToINString(resourceIDs, inList, "0");
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT typeID FROM paperdollResources WHERE paperdollResourceID IN (%s) AND typeID IS NOT NULL", inList.c_str()))
    {
        // without the typeID column (migration 20261025100100) nothing is checked
        _log(DATABASE__ERROR, "StoreDB::GetResourceTypeIDs - %s", res.error.c_str());
        return;
    }
    DBResultRow row;
    while (res.GetRow(row))
        typeIDs.push_back(row.GetInt(0));
}

void StoreDB::GetWardrobeTypes(uint32 charID, std::set<int32>& into)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT DISTINCT typeID FROM entity WHERE locationID = %u AND ownerID = %u AND flag = %u",
        charID, charID, (uint32)flagWardrobe))
    {
        _log(DATABASE__ERROR, "StoreDB::GetWardrobeTypes - %s", res.error.c_str());
        return;
    }
    DBResultRow row;
    while (res.GetRow(row))
        into.insert(row.GetInt(0));
}

bool StoreDB::GetAurBalance(uint32 charID, double& balance)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, "SELECT aurBalance FROM chrCharacters WHERE characterID = %u", charID)) {
        _log(DATABASE__ERROR, "StoreDB::GetAurBalance - %s", res.error.c_str());
        return false;
    }
    DBResultRow row;
    if (!res.GetRow(row))
        return false;
    balance = row.GetDouble(0);
    return true;
}

bool StoreDB::GetStipendLastPaid(uint32 charID, int64& lastPaid)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, "SELECT lastPaid FROM chrAurumStipend WHERE characterID = %u", charID)) {
        _log(DATABASE__ERROR, "StoreDB::GetStipendLastPaid - %s", res.error.c_str());
        // as if just paid:  a broken table pays nothing
        lastPaid = GetFileTimeNow();
        return true;
    }
    DBResultRow row;
    if (!res.GetRow(row))
        return false;
    lastPaid = row.GetInt64(0);
    return true;
}

bool StoreDB::AdvanceStipend(uint32 charID, bool hadRow, int64 oldLastPaid, int64 newLastPaid)
{
    DBerror err;
    uint32 affected(0);
    if (hadRow) {
        if (!sDatabase.RunQuery(err, affected,
            "UPDATE chrAurumStipend SET lastPaid = %lli WHERE characterID = %u AND lastPaid = %lli",
            newLastPaid, charID, oldLastPaid))
        {
            _log(DATABASE__ERROR, "StoreDB::AdvanceStipend - %s", err.c_str());
            return false;
        }
    } else {
        if (!sDatabase.RunQuery(err, affected,
            "INSERT IGNORE INTO chrAurumStipend (characterID, lastPaid) VALUES (%u, %lli)", charID, newLastPaid))
        {
            _log(DATABASE__ERROR, "StoreDB::AdvanceStipend - %s", err.c_str());
            return false;
        }
    }
    return (affected == 1);
}
