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
#include <string>
#include <vector>

#include "../../eve-common/EVE_Store.h"

#include "EVEServerConfig.h"
#include "account/AccountService.h"
#include "inventory/ItemFactory.h"
#include "store/StoreDB.h"
#include "store/StoreService.h"

// -1 = not set at runtime, follow account.storeEnabled
static int8 s_storeOpen = -1;

StoreService::StoreService() :
    Service("storeServer")
{
    this->Add("AcceptOffer", &StoreService::AcceptOffer);
    this->Add("GetAvailableOffers", &StoreService::GetAvailableOffers);
}

bool StoreService::IsOpen()
{
    if (s_storeOpen < 0)
        return sConfig.account.storeEnabled;
    return (s_storeOpen > 0);
}

void StoreService::SetOpen(bool open)
{
    s_storeOpen = (open ? 1 : 0);
}

void StoreService::CheckOpen()
{
    // the client closes the store windows and keeps them closed for 15 minutes (storeSvc.py)
    if (!IsOpen())
        throw UserError("VG_STORE_CLOSED");
}

PyResult StoreService::GetAvailableOffers(PyCallArgs& call)
{
    // availableOffers = sm.RemoteSvc('storeServer').GetAvailableOffers()   -> {offerID: offerInfo}
    CheckOpen();

    std::vector<StoreMarshal::Offer> offers, known;
    StoreDB::GetOffers(offers);
    // the client raises on an offered type it doesn't know;  leave out what this server doesn't have either
    for (const StoreMarshal::Offer& cur : offers)
        if (sItemFactory.GetType(cur.typeID) != nullptr)
            known.push_back(cur);

    return StoreMarshal::BuildOffersReply(known);
}

PyResult StoreService::AcceptOffer(PyCallArgs& call, PyInt* offerID, PyInt* quantity)
{
    // sm.RemoteSvc('storeServer').AcceptOffer(offerID, quantity)   quantity = bundles.  the return is ignored
    CheckOpen();

    Client* pClient(call.client);
    uint32 stationID(pClient->GetStationID());
    if (!pClient->IsDocked() or (stationID == 0))
        throw UserError("VStoreNotAvailable");

    // the client drops its cached offers on OfferNotAvailableNow and asks again
    StoreMarshal::Offer offer = StoreMarshal::Offer();
    if (!StoreDB::GetOffer(offerID->value(), offer))
        throw UserError("OfferNotAvailableNow");
    const ItemType* pType(sItemFactory.GetType(offer.typeID));
    if (pType == nullptr)
        throw UserError("OfferNotAvailableNow");

    int64 bundles(quantity->value());
    int64 cost(StoreRules::OfferCost(offer.priceAUR, bundles));
    int64 units((int64)offer.quantity * bundles);
    if ((cost < 0) or (units < 1) or (units > INT32_MAX))
        throw UserError("ErrorFulfillingStoreOffer");

    // checked here so the client gets the AUR message, not the wallet's
    double balance(pClient->GetBalance(Account::CreditType::AURUM));
    if (balance < (double)cost)
        throw UserError("NotEnoughAurum")
                .AddFormatValue("amount", new PyLong(cost))
                .AddFormatValue("balance", new PyLong((int64)balance));

    uint32 charID(pClient->GetCharacterID());
    // the journal hint lists the goods (wallet.py _ParseDescription: STOREITEMS [[typeID, qty], ...])
    char reason[64];
    snprintf(reason, sizeof(reason), "STOREITEMS: [[%u, %lli]]", (uint32)offer.typeID, (long long)units);
    if (cost > 0)
        AccountService::TransferFunds(charID, corpCONCORD, (double)cost, reason, Journal::EntryType::StorePurchase,
                                      offer.offerID, Account::KeyType::AUR, Account::KeyType::AUR);

    // goods go into the item hangar of this station (Incarna 1.1 notes)
    ItemData idata(offer.typeID, charID, locTemp, flagHangar, (uint32)units);
    InventoryItemRef iRef(sItemFactory.SpawnItem(idata));
    if (iRef.get() == nullptr) {
        _log(SERVICE__ERROR, "StoreService::AcceptOffer - %s: could not create %lli of type %u for offer %i.  refunded.",
             pClient->GetName(), (long long)units, (uint32)offer.typeID, offer.offerID);
        if (cost > 0)
            AccountService::TransferFunds(corpCONCORD, charID, (double)cost, reason, Journal::EntryType::StoreRefund,
                                          offer.offerID, Account::KeyType::AUR, Account::KeyType::AUR);
        throw UserError("ErrorFulfillingStoreOffer");
    }
    iRef->Move(stationID, flagHangar, true);
    iRef->SaveItem();

    _log(SERVICE__MESSAGE, "StoreService::AcceptOffer - %s bought %lli x %s (offer %i) for %lli AUR.",
         pClient->GetName(), (long long)units, pType->name().c_str(), offer.offerID, (long long)cost);
    return nullptr;
}

/* the listed stacks of 'typeID' this character has in its current station's item hangar, in the order given.
 *  'notOwned' / 'notHere' are the client's messages for an item of someone else and one elsewhere (empty: CustomError)
 */
static void GetHangarStacks(Client* pClient, PyList* itemIDs, uint16 typeID, const char* notOwned, const char* notHere,
                            std::vector<InventoryItemRef>& into)
{
    uint32 charID(pClient->GetCharacterID()), stationID(pClient->GetStationID());
    if ((itemIDs == nullptr) or itemIDs->empty())
        throw CustomError ("There is nothing to exchange.");

    std::set<uint32> seen;
    for (PyList::const_iterator cur = itemIDs->begin(); cur != itemIDs->end(); ++cur) {
        uint32 itemID(PyRep::IntegerValueU32(*cur));
        if (!seen.insert(itemID).second)
            continue;
        InventoryItemRef iRef(sItemFactory.GetItemRef(itemID));
        if ((iRef.get() == nullptr) or (iRef->typeID() != typeID))
            throw CustomError ("That item can't be exchanged for Aurum.");
        if (iRef->ownerID() != charID) {
            if (*notOwned != '\0')
                throw UserError(notOwned);
            throw CustomError ("You don't own that item.");
        }
        if (!pClient->IsDocked() or (iRef->locationID() != stationID) or (iRef->flag() != flagHangar)) {
            if (*notHere != '\0')
                throw UserError(notHere);
            throw CustomError ("The item must be in the item hangar of the station you are docked in.");
        }
        into.push_back(iRef);
    }
}

// takes 'qty' units of the stacks (in their order) and pays 'rate' AUR a unit from CONCORD
static void Exchange(Client* pClient, std::vector<InventoryItemRef>& stacks, int64 qty, int64 rate, uint8 entryType, const char* what)
{
    int64 payout(StoreRules::ConversionPayout(rate, qty));
    if (payout < 0)
        throw CustomError ("Invalid quantity.");

    std::vector<int64> sizes;
    for (auto cur : stacks)
        sizes.push_back(cur->quantity());
    std::vector<int64> take(StoreRules::PlanTake(sizes, qty));
    if (take.empty())
        throw CustomError ("You don't have that many %s in this station's item hangar.", what);

    for (size_t i = 0; i < stacks.size(); ++i) {
        if (take[i] < 1)
            continue;
        if (take[i] >= stacks[i]->quantity())
            stacks[i]->Delete();
        else
            stacks[i]->AlterQuantity((int32)-take[i], true);
    }

    char reason[96];
    snprintf(reason, sizeof(reason), "DESC: %lli %s exchanged for Aurum", (long long)qty, what);
    AccountService::TransferFunds(corpCONCORD, pClient->GetCharacterID(), (double)payout, reason, entryType, 0,
                                  Account::KeyType::AUR, Account::KeyType::AUR);
    _log(SERVICE__MESSAGE, "StoreService - %s exchanged %lli %s for %lli AUR.", pClient->GetName(), (long long)qty, what, (long long)payout);
}

void StoreService::ConvertPlex(Client* pClient, PyList* itemIDs, int64 qty)
{
    std::vector<InventoryItemRef> stacks;
    GetHangarStacks(pClient, itemIDs, EVEDB::invTypes::PilotLicence, "", "", stacks);
    Exchange(pClient, stacks, qty, StoreRules::PlexToAur, Journal::EntryType::PlexConversion, "PLEX");
}

void StoreService::ConvertAurToken(Client* pClient, PyList* itemIDs, int64 qty)
{
    std::vector<InventoryItemRef> stacks;
    GetHangarStacks(pClient, itemIDs, EVEDB::invTypes::AurumToken, "AurTokenYouDoNotOwnItem", "AurTokenNotInStation", stacks);
    Exchange(pClient, stacks, qty, StoreRules::AurTokenToAur, Journal::EntryType::AurumTokenConversion, "Aurum Tokens");
}
