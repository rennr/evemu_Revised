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
    Author:        Zhur
*/

#include "eve-server.h"


#include "StaticDataMgr.h"
#include "account/AccountDB.h"
#include "account/AccountService.h"
#include "EVE_Mail.h"
#include "EntityList.h"
#include "corporation/CorpMgr.h"
#include "corporation/CorporationService.h"
#include "station/StationDataMgr.h"

#include <algorithm>
#include <cstdint>

CorporationService::CorporationService() :
    Service("corporationSvc")
{
    this->Add("GetFactionInfo", &CorporationService::GetFactionInfo);
    this->Add("GetCorpInfo", &CorporationService::GetCorpInfo);
    this->Add("GetNPCDivisions", &CorporationService::GetNPCDivisions);
    this->Add("GetEmploymentRecord", &CorporationService::GetEmploymentRecord);
    this->Add("GetRecruitmentAdsByCriteria", &CorporationService::GetRecruitmentAdsByCriteria);
    this->Add("GetRecruitmentAdRegistryData", &CorporationService::GetRecruitmentAdRegistryData);
    this->Add("GetRecruitmentAdsForCorporation", &CorporationService::GetRecruitmentAdsForCorporation);

    this->Add("GetMedalsReceived", &CorporationService::GetMedalsReceived);
    this->Add("GetMedalDetails", &CorporationService::GetMedalDetails);
    this->Add("GetAllCorpMedals", &CorporationService::GetAllCorpMedals);
    this->Add("CreateMedal", &CorporationService::CreateMedal);
    this->Add("GiveMedalToCharacters", &CorporationService::GiveMedalToCharacters);
    this->Add("SetMedalStatus", &CorporationService::SetMedalStatus);
    this->Add("GetMedalStatuses", &CorporationService::GetMedalStatuses);
    this->Add("GetRecipientsOfMedal", &CorporationService::GetRecipientsOfMedal);
}

/*
 * CORP__ERROR
 * CORP__WARNING
 * CORP__INFO
 * CORP__MESSAGE
 * CORP__TRACE
 * CORP__CALL
 * CORP__CALL_DUMP
 * CORP__RSP_DUMP
 * CORP__DB_ERROR
 * CORP__DB_WARNING
 * CORP__DB_INFO
 * CORP__DB_MESSAGE
 */

PyResult CorporationService::GetNPCDivisions(PyCallArgs &call)
{
    return sDataMgr.GetNPCDivisions();
}

PyResult CorporationService::GetEmploymentRecord(PyCallArgs &call, PyInt* characterID) {
    return m_db.GetEmploymentRecord(characterID->value());
}

PyResult CorporationService::GetFactionInfo(PyCallArgs &call) {
    /*self.factionIDbyNPCCorpID, self.factionRegions, self.factionConstellations, self.factionSolarSystems,
     * self.factionRaces, self.factionStationCount, self.factionSolarSystemCount, self.npcCorpInfo = sm.RemoteSvc('corporationSvc').GetFactionInfo()
     *        for corpID, factionID in self.factionIDbyNPCCorpID.iteritems():
     *            if factionID not in self.corpsByFactionID:
     *                self.corpsByFactionID[factionID] = []
     *            if corpID not in self.corpsByFactionID[factionID]:
     *                self.corpsByFactionID[factionID].append(corpID)
     *
     *        owners = {}
     *        for k, v in self.factionIDbyNPCCorpID.iteritems():
     *            owners[k] = 0
     *            owners[v] = 0
     */

    return sDataMgr.GetFactionInfo();
}

// this wants corp market info
PyResult CorporationService::GetCorpInfo(PyCallArgs &call, PyInt* corporationID) {
    return m_db.GetMktInfo(corporationID->value());
}

PyResult CorporationService::GetRecruitmentAdRegistryData(PyCallArgs& call)
{   // working
    _log(CORP__CALL, "CorporationService::Handle_GetRecruitmentAdRegistryData()");
    call.Dump(CORP__CALL_DUMP);

    PyDict* dict = new PyDict();
        dict->SetItemString("types", m_db.GetAdTypeData());
        dict->SetItemString("groups", m_db.GetAdGroupData());
    PyObject* args = new PyObject("util.KeyVal", dict);
    if (is_log_enabled(CORP__RSP_DUMP))
        args->Dump(CORP__RSP_DUMP, "");
    return args;
}

PyResult CorporationService::GetRecruitmentAdsByCriteria(PyCallArgs& call, PyInt* typeMask, PyBool* inAlliance, std::optional<PyInt*> minMembers, std::optional<PyInt*> maxMembers)
{    //   return sm.RemoteSvc('corporationSvc').GetRecruitmentAdsByCriteria(typeMask, isInAlliance, minMembers, maxMembers)
    _log(CORP__CALL, "CorporationService::Handle_GetRecruitmentAdsByCriteria()");
    call.Dump(CORP__CALL_DUMP);

    return m_db.GetAdRegistryData(
        typeMask->value(), inAlliance->value(),
        minMembers.has_value () ? minMembers.value ()->value() : 0,
        maxMembers.has_value () ? maxMembers.value ()->value() : 0);
}

PyResult CorporationService::GetRecruitmentAdsForCorporation(PyCallArgs& call)
{
    // recruitments = self.GetCorpRegistry().GetRecruitmentAdsForCorporation()
    _log(CORP__CALL, "CorporationService::Handle_GetRecruitmentAdsForCorporation()");
    call.Dump(CORP__CALL_DUMP);

    return m_db.GetAdRegistryData();
}


/**     ***********************************************************************
 * @note   corporation decorations (medals).  the rules are in EVE_Medals.h
 */

void CorporationService::CheckMedalRoles(Client* pClient)
{
    if (!IsPlayerCorp(pClient->GetCorporationID()) or !CorpMgr::HasRole(pClient->GetCorpRole(), Corp::Role::PersonnelManager))
        throw UserError("CrpAccessDenied")
                .AddFormatValue("reason", new PyString("You must be a Director or Personnel Manager to create and award decorations."));
}

uint16 CorporationService::MedalWalletKey(Client* pClient)
{
    // MedalRibbonPickerWindow.SubmitData:  the active division (corpAccountKey), one the character can take from
    int32 key = pClient->GetCorpAccountKey();
    if (!MedalRules::IsCorpWalletKey(key) or !CorpMgr::HasRole(pClient->GetCorpRole(), Corp::AccountTakeRole(key)))
        throw UserError("CrpAccessDenied")
                .AddFormatValue("reason", new PyString("You can't take ISK from the active corporation wallet division."));
    return (uint16)key;
}

PyResult CorporationService::CreateMedal(PyCallArgs &call, PyRep* title, PyRep* description, PyList* graphics, std::optional<PyBool*> confirmed)
{
    // closeParent = sm.StartService('medals').CreateMedal(mName, mDesc, cMedalData)
    //  cMedalData = [[part, 'label.icon', color], ...]  part 1 ribbon, 2 medal (medalribbonranks.MedalRibbonPickerWindow)
    _log(CORP__CALL, "CorporationService::Handle_CreateMedal()");
    call.Dump(CORP__CALL_DUMP);

    Client* pClient = call.client;
    CheckMedalRoles(pClient);
    if (!(title->IsString() or title->IsWString()) or !(description->IsString() or description->IsWString()))
        throw CustomError("The decoration needs a name and a description.");

    std::string sTitle = PyRep::StringContent(title), sDesc = PyRep::StringContent(description);
    switch (MedalRules::CheckText(sTitle, sDesc)) {
        case MedalRules::TextResult::Ok:
            break;
        case MedalRules::TextResult::NameInvalid:
            throw UserError("MedalNameInvalid");
        case MedalRules::TextResult::NameTooLong:
            throw UserError("MedalNameTooLong").AddFormatValue("maxLength", new PyString(std::to_string(MedalRules::MaxNameLength)));
        case MedalRules::TextResult::DescriptionTooShort:
            throw UserError("MedalDescriptionTooShort").AddFormatValue("minLength", new PyString(std::to_string(MedalRules::MinDescriptionLength)));
        case MedalRules::TextResult::DescriptionTooLong:
            throw UserError("MedalDescriptionTooLong").AddFormatValue("maxLength", new PyString(std::to_string(MedalRules::MaxDescriptionLength)));
    }

    std::vector<MedalRules::Layer> layers;
    for (PyList::const_iterator itr = graphics->begin(); itr != graphics->end(); ++itr) {
        PyList* list = (*itr)->IsList() ? (*itr)->AsList() : nullptr;
        if ((list == nullptr) or (list->size() < 3) or !list->GetItem(0)->IsInt()
        or !(list->GetItem(1)->IsString() or list->GetItem(1)->IsWString()))
            throw CustomError("The decoration's design is not valid.");
        MedalRules::Layer layer;
            layer.part = list->GetItem(0)->AsInt()->value();
            layer.graphic = PyRep::StringContent(list->GetItem(1));
            PyRep* color = list->GetItem(2);
            layer.color = MedalRules::LayerColor(PyRep::IntegerValue(color), (color->IsInt() or color->IsLong()));
        layers.push_back(layer);
    }
    if (!MedalRules::ValidDesign(layers))
        throw CustomError("The decoration's design is not valid.");

    uint32 corpID = pClient->GetCorporationID();
    uint16 accountKey = MedalWalletKey(pClient);
    double cost = sConfig.rates.medalCreateCost;
    if (!confirmed.has_value() or !confirmed.value()->value()) {
        double balance = AccountDB::GetCorpBalance(corpID, accountKey);
        if (balance < cost)
            throw UserError("NotEnoughMoney").AddISK("amount", cost).AddISK("balance", balance);
        // the client asks the pilot (medals.CreateMedal formats e.dict['cost']) and calls again with True
        throw UserError("ConfirmCreatingMedal").AddFormatValue("cost", new PyFloat(cost));
    }

    uint32 medalID = m_db.CreateMedal(corpID, pClient->GetCharacterID(), sTitle, sDesc, layers);
    if (medalID == 0)
        throw CustomError("The decoration could not be saved.");

    // paid to SCC from the active division (refMedalCreation, argID1 the creating character)
    std::string reason = "DESC: Medal Creation by ";
    reason += pClient->GetName();
    try {
        AccountService::TransferFunds(corpID, corpSCC, cost, reason.c_str(), Journal::EntryType::MedalCreation,
                                      pClient->GetCharacterID(), accountKey, Account::KeyType::Cash, pClient);
    } catch (...) {
        // not paid (NotEnoughMoneyCorp):  no medal
        m_db.DeleteMedal(medalID);
        throw;
    }

    // corp_ui_member_decorations reloads the corp's medals (medals.OnCorporationMedalAdded invalidates GetAllCorpMedals)
    PyTuple* payload = new PyTuple(1);
        payload->SetItem(0, new PyInt(medalID));
    sEntityList.CorpNotify(corpID, Notify::Types::CorpNews, "OnCorporationMedalAdded", "corpid", payload);

    return nullptr;
}

PyResult CorporationService::GetMedalsReceived(PyCallArgs &call, PyInt* characterID) {
    //  medalInfo, medalGraphics = sm.StartService('medals').GetMedalsReceived(charID)
    //   the character sheet for the character's own, the info window's Decorations tab for anyone (public ones only)
    _log(CORP__CALL, "CorporationService::Handle_GetMedalsReceived()");
    call.Dump(CORP__CALL_DUMP);

    uint32 charID = characterID->value();
    bool publicOnly = (charID != call.client->GetCharacterID());
    PyTuple* res = new PyTuple(2);
        res->SetItem(0, m_db.GetMedalsReceived(charID, publicOnly));
        res->SetItem(1, m_db.GetMedalsReceivedDetails(charID, publicOnly));
    if (is_log_enabled(CORP__RSP_DUMP))
        res->Dump(CORP__RSP_DUMP, "");
    return res;
}

PyResult CorporationService::GetMedalDetails(PyCallArgs &call, PyInt* medalID)
{
    // GetMedalDetails(medalID).info[0]:  the medal info window and the CharMedal notification
    _log(CORP__CALL, "CorporationService::Handle_GetMedalDetails()");
    call.Dump(CORP__CALL_DUMP);

    PyRep* res = m_db.GetMedalDetails(medalID->value());
    if (res == nullptr)
        throw CustomError("That decoration no longer exists.");
    return res;
}

PyResult CorporationService::GetAllCorpMedals(PyCallArgs& call, PyInt* corporationID)
{
    // medals, medalDetails = sm.RemoteSvc('corporationSvc').GetAllCorpMedals(session.corpid)
    //  the corp's decorations tab and the award menu.  another corp's medals (and their designs) are not shown
    _log(CORP__CALL, "CorporationService::Handle_GetAllCorpMedals()");
    call.Dump(CORP__CALL_DUMP);

    if ((uint32)corporationID->value() != call.client->GetCorporationID())
        throw UserError("CrpAccessDenied").AddFormatValue("reason", new PyString("You can only view your own corporation's decorations."));

    PyTuple* res = new PyTuple(2);
        res->SetItem(0, m_db.GetAllCorpMedals(corporationID->value()));
        res->SetItem(1, m_db.GetCorpMedalData(corporationID->value()));
    if (is_log_enabled(CORP__RSP_DUMP))
        res->Dump(CORP__RSP_DUMP, "");
    return res;
}

PyResult CorporationService::GetRecipientsOfMedal(PyCallArgs &call, PyInt* medalID)
{
    //   recipients = sm.RemoteSvc('corporationSvc').GetRecipientsOfMedal(medalID)
    //    corp_ui_member_decorations.GetMedalSubContent, for the corp's own medals:  the awards and their reasons
    _log(CORP__CALL, "CorporationService::Handle_GetRecipientsOfMedal()");
    call.Dump(CORP__CALL_DUMP);

    if (m_db.GetMedalOwner(medalID->value()) != call.client->GetCorporationID())
        throw UserError("CrpAccessDenied").AddFormatValue("reason", new PyString("You can only view the recipients of your own corporation's decorations."));

    return m_db.GetRecipientsOfMedal(medalID->value());
}

PyResult CorporationService::GiveMedalToCharacters(PyCallArgs &call, PyInt* medalID, PyRep* recipientIDs, PyRep* reason, std::optional<PyBool*> confirmed)
{
    //  sm.RemoteSvc('corporationSvc').GiveMedalToCharacters(medalID, recipientID, reason[, True])
    //   menusvc.AwardDecoration for selected corp members, recipientID a list of charIDs
    _log(CORP__CALL, "CorporationService::Handle_GiveMedalToCharacters()");
    call.Dump(CORP__CALL_DUMP);

    Client* pClient = call.client;
    CheckMedalRoles(pClient);
    uint32 corpID = pClient->GetCorporationID();
    if (m_db.GetMedalOwner(medalID->value()) != corpID)
        throw UserError("CrpAccessDenied").AddFormatValue("reason", new PyString("You can only award your own corporation's decorations."));

    if (!(reason->IsString() or reason->IsWString() or reason->IsNone()))
        throw CustomError("The reason for the award is not valid.");
    std::string sReason = PyRep::StringContent(reason);
    if (!MedalRules::ReasonOk(sReason))
        throw CustomError("The reason for the award can be at most %u characters.", (uint32)MedalRules::MaxReasonLength);

    // the recipients:  characters, once each, who are members of the corp now
    std::vector<PyRep*> ids;
    if (recipientIDs->IsList()) {
        for (PyList::const_iterator itr = recipientIDs->AsList()->begin(); itr != recipientIDs->AsList()->end(); ++itr)
            ids.push_back(*itr);
    } else if (recipientIDs->IsTuple()) {
        for (PyTuple::const_iterator itr = recipientIDs->AsTuple()->begin(); itr != recipientIDs->AsTuple()->end(); ++itr)
            ids.push_back(*itr);
    } else if (recipientIDs->IsInt()) {
        ids.push_back(recipientIDs);
    }
    std::vector<uint32> charVec;
    for (auto cur : ids) {
        if (!cur->IsInt())
            continue;
        int32 charID = cur->AsInt()->value();
        if (!IsCharacterID(charID) or (std::find(charVec.begin(), charVec.end(), (uint32)charID) != charVec.end()))
            continue;
        if ((uint32)m_db.GetCorpIDforChar(charID) != corpID)
            continue;
        charVec.push_back(charID);
    }
    if (charVec.empty())
        throw UserError("MedalNoValidRecipients");

    uint16 accountKey = MedalWalletKey(pClient);
    double cost = MedalRules::AwardTotal(charVec.size(), sConfig.rates.medalAwardCost);
    if (!confirmed.has_value() or !confirmed.value()->value()) {
        double balance = AccountDB::GetCorpBalance(corpID, accountKey);
        if (balance < cost)
            throw UserError("NotEnoughMoney").AddISK("amount", cost).AddISK("balance", balance);
        throw UserError("ConfirmGivingMedal").AddFormatValue("cost", new PyFloat(cost));
    }

    // paid to SCC from the active division (refMedalIssuing, argID1 the issuing character), then the awards
    std::string finalReason = "DESC: Awarding Medal by ";
    finalReason += pClient->GetName();
    AccountService::TransferFunds(corpID, corpSCC, cost, finalReason, Journal::EntryType::MedalIssuing,
                                  pClient->GetCharacterID(), accountKey, Account::KeyType::Cash, pClient);

    m_db.GiveMedalToCharacters(pClient->GetCharacterID(), corpID, medalID->value(), charVec, sReason);

    // CharMedal {medalID, corpID, reason} (notificationUtil.ParamCharacterMedalNotification) and OnMedalIssued() to
    //  the recipients, OnCorporationMedalAdded(medalID) to the corp (the award count and recipients changed)
    for (auto cur : charVec) {
        PyDict* data = new PyDict();
            data->SetItemString("medalID", new PyInt(medalID->value()));
            data->SetItemString("corpID", new PyInt(corpID));
            data->SetItemString("reason", new PyWString(sReason));
        sCorpMgr.NotifyChar(cur, Notify::Types::CharMedal, pClient->GetCharacterID(), data);
        Client* pRecipient = sEntityList.FindClientByCharID(cur);
        if (pRecipient != nullptr) {
            PyTuple* payload = new PyTuple(0);
            pRecipient->SendNotification("OnMedalIssued", "charid", &payload);
        }
    }
    PyTuple* payload = new PyTuple(1);
        payload->SetItem(0, new PyInt(medalID->value()));
    sEntityList.CorpNotify(corpID, Notify::Types::CorpNews, "OnCorporationMedalAdded", "corpid", payload);

    return nullptr;
}

PyResult CorporationService::GetMedalStatuses(PyCallArgs &call)
{
    //  return sm.RemoteSvc('corporationSvc').GetMedalStatuses()
    //  statusID, statusName = self.GetStatus(theyareallthesame.status)
    _log(CORP__CALL, "CorporationService::Handle_GetMedalStatuses()");
    call.Dump(CORP__CALL_DUMP);

    // statusID, statusName (crpMedalStatus)
    PyRep* res = m_db.GetMedalStatuses();
    if (is_log_enabled(CORP__RSP_DUMP))
        res->Dump(CORP__RSP_DUMP, "");
    return res;
}

PyResult CorporationService::SetMedalStatus(PyCallArgs &call, PyDict* newStatus)
{
    //  sm.RemoteSvc('corporationSvc').SetMedalStatus({medalID: status})
    //   the character sheet's decoration permissions:  2 private, 3 public, 1 remove (DeleteMedalConfirmation).
    //   a removed award stays in chrMedals with status 1:  the corp keeps its award history, the character sheet no
    //   longer lists it (GetMedalsReceivedWithFlag keeps 2 and 3) and it can't be shown again
    _log(CORP__CALL, "CorporationService::Handle_SetMedalStatus()");
    call.Dump(CORP__CALL_DUMP);

    uint32 charID = call.client->GetCharacterID();
    uint32 changed(0);
    for (PyDict::const_iterator itr = newStatus->begin(); itr != newStatus->end(); ++itr) {
        if (!(itr->first->IsInt() or itr->first->IsLong()) or !(itr->second->IsInt() or itr->second->IsLong()))
            continue;
        int64 medalID = PyRep::IntegerValue(itr->first);
        int64 status = PyRep::IntegerValue(itr->second);
        if ((medalID <= 0) or (medalID > INT32_MAX))
            continue;
        // the current status is checked in the update (only private and public awards change)
        if (!MedalRules::CanSetStatus(MedalRules::StatusPrivate, (int32)status))
            continue;
        changed += m_db.SetMedalStatus(charID, (uint32)medalID, (uint8)status);
    }

    if (changed > 0) {
        PyTuple* payload = new PyTuple(0);
        call.client->SendNotification("OnMedalStatusChanged", "charid", &payload);
    }
    return nullptr;
}

