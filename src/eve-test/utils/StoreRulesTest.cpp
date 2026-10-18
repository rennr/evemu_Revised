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

#include "eve-test.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "EVE_Store.h"
#include "StoreMarshal.h"

namespace {
    int failures = 0;

    void Check(bool ok, const char* what)
    {
        ::printf("%s %s\n", (ok ? "  ok  " : "  FAIL"), what);
        if (!ok)
            ++failures;
    }

    // a util.KeyVal's attribute (nullptr when it isn't one or lacks it)
    PyRep* Attr(PyRep* rep, const char* name)
    {
        if ((rep == nullptr) or !rep->IsObject() or !rep->AsObject()->arguments()->IsDict())
            return nullptr;
        return rep->AsObject()->arguments()->AsDict()->GetItemString(name);
    }

    bool IsKeyVal(PyRep* rep)
    {
        return (rep != nullptr) and rep->IsObject() and (rep->AsObject()->type()->content() == "util.KeyVal");
    }

    int64_t IntAttr(PyRep* rep, const char* name)
    {
        PyRep* v = Attr(rep, name);
        if ((v == nullptr) or !v->IsInt())
            return -999999;
        return v->AsInt()->value();
    }

    // a doll modifier row as the client sends it:  ObjectEx type 2, (token, modifierLocationID, resourceID, variation)
    PyRep* Modifier(int32_t location, int32_t resource, int32_t variation)
    {
        PyTuple* row = new PyTuple(4);
            row->SetItem(0, new PyToken("blue.DBRow"));
            row->SetItem(1, new PyInt(location));
            row->SetItem(2, new PyInt(resource));
            row->SetItem(3, new PyInt(variation));
        return new PyObjectEx_Type2(row, new PyDict());
    }
}

int utils_StoreRulesTest( int /*argc*/, char* /*argv*/[] )
{
    using namespace StoreRules;

    ::printf("offer cost:\n");
    Check(OfferCost(3200, 1) == 3200, "one bundle costs the price");
    Check(OfferCost(3200, 3) == 9600, "three bundles cost three times the price");
    Check(OfferCost(0, 5) == 0, "a free offer costs nothing");
    Check(OfferCost(3200, 0) == -1, "zero bundles are refused");
    Check(OfferCost(3200, -2) == -1, "a negative quantity is refused");
    Check(OfferCost(3200, MaxBundles + 1) == -1, "more than MaxBundles is refused");
    Check(OfferCost(MaxAur, 2) == -1, "a total above MaxAur is refused");
    Check(OfferCost(-1, 1) == -1, "a negative price is refused");

    ::printf("conversions:\n");
    Check(ConversionPayout(PlexToAur, 1) == 3500, "one PLEX is 3500 AUR (Plex2AurExchangeRatio)");
    Check(ConversionPayout(PlexToAur, 4) == 14000, "four PLEX are 14000 AUR");
    Check(ConversionPayout(AurTokenToAur, 3) == 3000, "three Aurum Tokens are 3000 AUR (AurumToken2AurExchangeRatio)");
    Check(ConversionPayout(PlexToAur, 0) == -1, "zero PLEX is refused");
    Check(ConversionPayout(PlexToAur, MaxAur) == -1, "an overflowing payout is refused");

    ::printf("taking from stacks:\n");
    std::vector<int64_t> take = PlanTake({5, 2, 1}, 6);
    Check((take.size() == 3) and (take[0] == 5) and (take[1] == 1) and (take[2] == 0), "6 from 5,2,1 takes 5 then 1");
    take = PlanTake({5, 2, 1}, 8);
    Check((take.size() == 3) and (take[0] == 5) and (take[1] == 2) and (take[2] == 1), "8 from 5,2,1 takes everything");
    Check(PlanTake({5, 2, 1}, 9).empty(), "9 from 5,2,1 is short");
    Check(PlanTake({5}, 0).empty(), "taking nothing plans nothing");
    take = PlanTake({0, 3}, 2);
    Check((take.size() == 2) and (take[0] == 0) and (take[1] == 2), "an empty stack is skipped");

    ::printf("stipend:\n");
    const int64_t week(168 * TicksPerHour), t0(130000000000000000LL);
    StipendDue due = GetStipendDue(false, 0, t0, week, 1);
    Check((due.periods == 1) and (due.newLastPaid == t0) and (due.nextDue == t0 + week), "never paid: one payment now");
    due = GetStipendDue(true, t0, t0 + week - 1, week, 1);
    Check((due.periods == 0) and (due.newLastPaid == t0) and (due.nextDue == t0 + week), "just short of a week: nothing yet");
    due = GetStipendDue(true, t0, t0 + week, week, 1);
    Check((due.periods == 1) and (due.newLastPaid == t0 + week), "a week on: one payment, the schedule keeps its phase");
    due = GetStipendDue(true, t0, t0 + week + week / 2, week, 1);
    Check((due.periods == 1) and (due.newLastPaid == t0 + week) and (due.nextDue == t0 + 2 * week), "a week and a half: paid up to the week");
    due = GetStipendDue(true, t0, t0 + 5 * week, week, 1);
    Check((due.periods == 1) and (due.newLastPaid == t0 + 5 * week), "five weeks away, max 1: one payment, the missed ones dropped");
    due = GetStipendDue(true, t0, t0 + 5 * week, week, 3);
    Check((due.periods == 3) and (due.newLastPaid == t0 + 5 * week), "five weeks away, max 3: three payments");
    due = GetStipendDue(true, t0 + week, t0, week, 1);
    Check((due.periods == 0) and (due.newLastPaid == t0 + week), "the clock went back: nothing");
    due = GetStipendDue(true, t0, t0 + 2 * week, 0, 1);
    Check((due.periods == 0) and (due.nextDue == 0), "no interval: the stipend is off");
    due = GetStipendDue(false, 0, t0, week, 0);
    Check((due.periods == 0) and (due.nextDue == 0), "max periods 0: the stipend is off");
    // the second check of the same period (another session, or after a restart) finds it paid
    due = GetStipendDue(true, t0 + week, t0 + week + 10, week, 1);
    Check(due.periods == 0, "after a payment the same period isn't due again");

    ::printf("wardrobe:\n");
    std::set<int32_t> hangar = {4067, 4165}, wardrobe = {4008};
    Check(FirstUnownedType({4067, 4008}, hangar, wardrobe) == 0, "worn types in the hangar and wardrobe are owned");
    Check(FirstUnownedType({4067, 4009}, hangar, wardrobe) == 4009, "a worn type in neither is reported");
    Check(FirstUnownedType({-1, 0}, hangar, wardrobe) == 0, "typeID -1 and none aren't checked");
    Check(FirstUnownedType({4055}, hangar, wardrobe) == 4055, "4055 (in no invTypes) can't be owned");
    Check(FirstUnownedType({}, {}, {}) == 0, "nothing worn, nothing owned is fine");
    std::vector<int32_t> moves = TypesToWardrobe({4165, 4008, 4067, 4165, -1}, wardrobe);
    Check((moves.size() == 2) and (moves[0] == 4067) and (moves[1] == 4165), "new pieces move once each, the wardrobe's don't");
    Check(TypesToWardrobe({4008}, wardrobe).empty(), "a piece already in the wardrobe moves nothing");

    ::printf("offers reply:\n");
    std::vector<StoreMarshal::Offer> offers;
    offers.push_back({4067, 4067, 1, 3200});
    offers.push_back({9001, 3958, 2, 8000});
    PyDict* reply = StoreMarshal::BuildOffersReply(offers);
    Check(reply->size() == 2, "a dict with one entry per offer");
    PyInt* key = new PyInt(9001);
    PyRep* info = reply->GetItem(key);
    PyDecRef(key);
    Check(IsKeyVal(info), "keyed by offerID, the value a util.KeyVal");
    Check(IntAttr(info, "offerID") == 9001, "offerInfo.offerID");
    PyRep* goods = Attr(info, "goods");
    Check((goods != nullptr) and goods->IsList() and (goods->AsList()->size() == 2), "offerInfo.goods: the item and its cost");
    if ((goods != nullptr) and goods->IsList() and (goods->AsList()->size() == 2)) {
        PyRep* item = goods->AsList()->GetItem(0);
        PyRep* cost = goods->AsList()->GetItem(1);
        PyRep* offered = Attr(item, "offered");
        Check(IsKeyVal(item) and (offered != nullptr) and offered->IsBool() and offered->AsBool()->value(), "the item is offered");
        Check(IntAttr(item, "argumentType") == GoodTypeEveTypeID, "the item is GOODTYPE_EVETYPEID");
        Check(IntAttr(item, "argumentValue") == 3958, "its argumentValue is the typeID");
        Check(IntAttr(item, "amount") == 2, "its amount is the units per bundle");
        Check((Attr(item, "details1") != nullptr) and Attr(item, "details1")->IsNone()
              and (Attr(item, "details2") != nullptr) and (Attr(item, "details3") != nullptr), "details1-3 are there (None)");
        offered = Attr(cost, "offered");
        Check(IsKeyVal(cost) and (offered != nullptr) and offered->IsBool() and !offered->AsBool()->value(), "the cost isn't offered");
        Check(IntAttr(cost, "argumentType") == GoodTypeCredits, "the cost is GOODTYPE_CREDITS");
        Check(IntAttr(cost, "argumentValue") == CreditsAurum, "in creditsAURUM, not ISK");
        Check(IntAttr(cost, "amount") == 8000, "for the price");
        Check(Attr(cost, "details3") != nullptr, "costs carry details1-3 too");
    }
    PyDecRef(reply);
    reply = StoreMarshal::BuildOffersReply(std::vector<StoreMarshal::Offer>());
    Check(reply->IsDict() and (reply->size() == 0), "no offers: an empty dict (not a list)");
    PyDecRef(reply);

    ::printf("doll resources:\n");
    PyDict* doll = new PyDict();
    PyList* mods = new PyList();
        mods->AddItem(Modifier(10, 4012, 0));
        mods->AddItem(Modifier(11, 1072, 2));
        mods->AddItem(new PyInt(5));    // not a row
    doll->SetItemString("modifiers", mods);
    std::vector<int32_t> ids;
    StoreMarshal::GetDollResourceIDs(doll, ids);
    Check((ids.size() == 2) and (ids[0] == 4012) and (ids[1] == 1072), "modifier rows give their paperdollResourceIDs");
    PyDecRef(doll);
    doll = new PyDict();
    ids.clear();
    StoreMarshal::GetDollResourceIDs(doll, ids);
    Check(ids.empty(), "no modifiers: no resources");
    StoreMarshal::GetDollResourceIDs(nullptr, ids);
    Check(ids.empty(), "no doll: no resources");
    PyDecRef(doll);

    ::printf("%s\n", (failures == 0 ? "all store rule checks passed" : "store rule checks FAILED"));
    return failures;
}
