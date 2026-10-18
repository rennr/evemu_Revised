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

#include "../../eve-common/EVE_Store.h"

#include "EVEServerConfig.h"
#include "account/AccountService.h"
#include "store/AurumStipend.h"
#include "store/StoreDB.h"

int64 AurumStipend::Check(uint32 charID)
{
    if ((sConfig.account.aurStipendAmount == 0) or (sConfig.account.aurStipendIntervalHours == 0))
        return 0;

    int64 now(GetFileTimeNow());
    int64 interval((int64)sConfig.account.aurStipendIntervalHours * StoreRules::TicksPerHour);
    int64 lastPaid(0);
    bool hasRow(StoreDB::GetStipendLastPaid(charID, lastPaid));
    StoreRules::StipendDue due(StoreRules::GetStipendDue(hasRow, lastPaid, now, interval, sConfig.account.aurStipendMaxPeriods));
    if (due.nextDue == 0)
        return 0;
    if (due.periods < 1)
        return due.nextDue;

    // claim the periods first:  only the check that moves the timestamp pays
    if (!StoreDB::AdvanceStipend(charID, hasRow, lastPaid, due.newLastPaid))
        return now + 60 * EvE::Time::Second;

    double amount((double)sConfig.account.aurStipendAmount * due.periods);
    try {
        AccountService::TransferFunds(corpCONCORD, charID, amount, "DESC: Aurum stipend", Journal::EntryType::AurumGiveAway,
                                      0, Account::KeyType::AUR, Account::KeyType::AUR);
        _log(ACCOUNT__MESSAGE, "AurumStipend - paid %.0f AUR (%lli period(s)) to %u.", amount, (long long)due.periods, charID);
    } catch (...) {
        _log(ACCOUNT__ERROR, "AurumStipend - paying %.0f AUR to %u failed.", amount, charID);
    }
    return due.nextDue;
}
