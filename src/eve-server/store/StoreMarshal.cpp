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

// this file is also compiled into eve-test, so it includes nothing from the server

#include "eve-common.h"

#include "EVE_Store.h"
#include "python/PyRep.h"

#include "StoreMarshal.h"

static PyObject* NewGood(bool offered, int32_t argumentType, int32_t argumentValue, int32_t amount)
{
    PyDict* good = new PyDict();
        good->SetItemString("offered", new PyBool(offered));
        good->SetItemString("argumentType", new PyInt(argumentType));
        good->SetItemString("argumentValue", new PyInt(argumentValue));
        good->SetItemString("amount", new PyInt(amount));
        // blueprint ME, PE and runs of an offered blueprint (none are sold)
        good->SetItemString("details1", PyStatic.NewNone());
        good->SetItemString("details2", PyStatic.NewNone());
        good->SetItemString("details3", PyStatic.NewNone());
    return new PyObject("util.KeyVal", good);
}

PyDict* StoreMarshal::BuildOffersReply(const std::vector<Offer>& offers)
{
    PyDict* reply = new PyDict();
    for (const Offer& cur : offers) {
        PyList* goods = new PyList();
            goods->AddItem(NewGood(true, StoreRules::GoodTypeEveTypeID, cur.typeID, cur.quantity));
            goods->AddItem(NewGood(false, StoreRules::GoodTypeCredits, StoreRules::CreditsAurum, (int32_t)cur.priceAUR));
        PyDict* info = new PyDict();
            info->SetItemString("offerID", new PyInt(cur.offerID));
            info->SetItemString("goods", goods);
        reply->SetItem(new PyInt(cur.offerID), new PyObject("util.KeyVal", info));
    }
    return reply;
}

void StoreMarshal::GetDollResourceIDs(PyDict* doll, std::vector<int32_t>& into)
{
    if (doll == nullptr)
        return;
    PyRep* rep = doll->GetItemString("modifiers");
    if ((rep == nullptr) or !rep->IsList())
        return;
    for (PyList::const_iterator cur = rep->AsList()->begin(); cur != rep->AsList()->end(); ++cur) {
        if (((*cur) == nullptr) or !(*cur)->IsObjectEx() or !(*cur)->AsObjectEx()->isType2())
            continue;
        PyObjectEx_Type2* obj = (PyObjectEx_Type2*)(*cur)->AsObjectEx();
        PyTuple* row = obj->GetArgs();
        // [0] token, [1] modifierLocationID, [2] paperdollResourceID, [3] paperdollResourceVariation
        if ((row == nullptr) or (row->size() < 3) or (row->GetItem(2) == nullptr) or !row->GetItem(2)->IsInt())
            continue;
        into.push_back(row->GetItem(2)->AsInt()->value());
    }
}
