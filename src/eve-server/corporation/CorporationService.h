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


#ifndef __CORPORATION_SERVICE_H_INCL__
#define __CORPORATION_SERVICE_H_INCL__

#include "corporation/CorporationDB.h"
#include "services/Service.h"

class CorporationService : public Service<CorporationService>
{
public:
    CorporationService();

protected:
    CorporationDB m_db;

    PyResult GetNPCDivisions(PyCallArgs& call);
    PyResult GetEmploymentRecord(PyCallArgs& call, PyInt* characterID);
    PyResult GetFactionInfo(PyCallArgs& call);
    PyResult GetCorpInfo(PyCallArgs& call, PyInt* corporationID);
    PyResult GetRecruitmentAdRegistryData(PyCallArgs& call);
    PyResult GetRecruitmentAdsByCriteria(PyCallArgs& call, PyInt* typeMask, PyBool* inAlliance, std::optional<PyInt*> minMembers, std::optional<PyInt*> maxMembers);
    PyResult GetRecruitmentAdsForCorporation(PyCallArgs& call);
    /* medals.py:  CreateMedal(title, description, graphics) raises ConfirmCreatingMedal {cost}, then
     *  CreateMedal(title, description, graphics, True) pays and creates.  the strings are str or unicode */
    PyResult CreateMedal(PyCallArgs& call, PyRep* title, PyRep* description, PyList* graphics, std::optional<PyBool*> confirmed);
    PyResult GetMedalsReceived(PyCallArgs& call, PyInt* characterID);
    PyResult GetMedalDetails(PyCallArgs& call, PyInt* medalID);
    PyResult GetAllCorpMedals(PyCallArgs& call, PyInt* corporationID);
    PyResult GetRecipientsOfMedal(PyCallArgs& call, PyInt* medalID);
    // GiveMedalToCharacters(medalID, [charID], reason) raises ConfirmGivingMedal {cost}, then the same with True
    PyResult GiveMedalToCharacters(PyCallArgs& call, PyInt* medalID, PyRep* recipientIDs, PyRep* reason, std::optional<PyBool*> confirmed);
    PyResult GetMedalStatuses(PyCallArgs& call);
    PyResult SetMedalStatus(PyCallArgs& call, PyDict* newStatus);

private:
    // Director or Personnel Manager of a player corp (medals.py NeedRolesError), else CrpAccessDenied
    void CheckMedalRoles(Client* pClient);
    // the session's corp wallet division, which the character must be able to take from
    uint16 MedalWalletKey(Client* pClient);
};

#endif