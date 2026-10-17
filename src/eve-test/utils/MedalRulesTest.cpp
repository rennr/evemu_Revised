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

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "EVE_Medals.h"

namespace {
    int failures = 0;

    void Check(bool ok, const char* what)
    {
        ::printf("%s %s\n", (ok ? "  ok  " : "  FAIL"), what);
        if (!ok)
            ++failures;
    }

    MedalRules::Layer L(int32_t part, const char* graphic, int32_t color = -1)
    {
        MedalRules::Layer layer;
        layer.part = part;
        layer.graphic = graphic;
        layer.color = color;
        return layer;
    }
}

int utils_MedalRulesTest( int /*argc*/, char* /*argv*/[] )
{
    using namespace MedalRules;

    ::printf("text:\n");
    std::string desc10(10, 'd');
    Check(CheckText("Valor", "For valour in the field") == TextResult::Ok, "a plain title and description");
    Check(CheckText("ab", desc10) == TextResult::NameInvalid, "a 2 character title is too short");
    Check(CheckText("   ", desc10) == TextResult::NameInvalid, "a blank title");
    Check(CheckText(std::string(100, 'x'), desc10) == TextResult::Ok, "a 100 character title");
    Check(CheckText(std::string(101, 'x'), desc10) == TextResult::NameTooLong, "a 101 character title");
    Check(CheckText("Valor", std::string(9, 'd')) == TextResult::DescriptionTooShort, "a 9 character description");
    Check(CheckText("Valor", std::string(1000, 'd')) == TextResult::Ok, "a 1000 character description");
    Check(CheckText("Valor", std::string(1001, 'd')) == TextResult::DescriptionTooLong, "a 1001 character description");
    // 100 two-byte characters are 200 bytes but 100 characters
    std::string wide;
    for (int i = 0; i < 100; ++i)
        wide += "\xC3\xA9";
    Check(Utf8Length(wide) == 100, "utf-8 characters, not bytes");
    Check(CheckText(wide, desc10) == TextResult::Ok, "100 accented characters fit the title");
    Check(ReasonOk(std::string(200, 'r')) and !ReasonOk(std::string(201, 'r')), "the reason holds 200 characters");

    ::printf("graphics:\n");
    Check(ValidGraphic(PartRibbon, "Caldari.1_1") and ValidGraphic(PartRibbon, "Minmatar.1_2"), "the race ribbon bases");
    Check(ValidGraphic(PartRibbon, "Amarr.3_3") and ValidGraphic(PartRibbon, "gallente.2_1"), "ribbon shapes, any case");
    Check(!ValidGraphic(PartRibbon, "Jove.1_1"), "no jove ribbons");
    Check(!ValidGraphic(PartRibbon, "Caldari.4_1"), "the ribbons have three sheets");
    Check(ValidGraphic(PartMedal, "compass.1_1") and ValidGraphic(PartMedal, "star.7_4"), "medal base shapes");
    Check(!ValidGraphic(PartMedal, "star.8_1") and !ValidGraphic(PartMedal, "pentagon.2_1"), "past a shape's sheets");
    Check(ValidGraphic(PartMedal, "elements.33_4") and !ValidGraphic(PartMedal, "elements.34_1"), "33 element sheets");
    Check(!ValidGraphic(PartMedal, "compass.1_5") and !ValidGraphic(PartMedal, "compass.1_0"), "cells 1..4");
    Check(!ValidGraphic(PartMedal, "compass.1_1.x") and !ValidGraphic(PartMedal, "compass") and !ValidGraphic(PartMedal, "compass.a_1"),
          "malformed graphics");
    Check(!ValidGraphic(PartMedal, "x');DROP TABLE crpMedals;--.1_1"), "no sql in a label");
    Check(!ValidGraphic(3, "compass.1_1") and !ValidGraphic(PartMedal, "Caldari.1_1"), "the part decides the labels");

    ::printf("design:\n");
    std::vector<Layer> design = {L(1, "Caldari.1_1"), L(1, "Caldari.1_2", -330271), L(2, "compass.1_1")};
    Check(ValidDesign(design), "the default design:  two ribbon layers and a medal");
    Check(!ValidDesign({L(1, "Caldari.1_1")}), "no medal part");
    Check(!ValidDesign({L(2, "compass.1_1")}), "no ribbon part");
    std::vector<Layer> ribbons6(6, L(1, "Caldari.2_1"));
    ribbons6.push_back(L(2, "round.1_1"));
    Check(!ValidDesign(ribbons6), "six ribbon layers");
    std::vector<Layer> medals5(5, L(2, "elements.1_1"));
    medals5.push_back(L(1, "Amarr.1_1"));
    Check(!ValidDesign(medals5), "five medal layers");
    std::vector<Layer> full = {L(1, "Amarr.1_1"), L(1, "Amarr.2_1"), L(1, "Amarr.2_2"), L(1, "Amarr.3_1"), L(1, "Amarr.1_2"),
                               L(2, "star.3_2"), L(2, "elements.1_1"), L(2, "elements.10_3"), L(2, "elements.33_4")};
    Check(ValidDesign(full), "five ribbon and four medal layers");

    ::printf("colours, costs, status:\n");
    Check(LayerColor(-330271, true) == -330271, "a negative ARGB int");
    Check(LayerColor(0xFFFAF5E1LL, true) == -330271, "the same colour unsigned");
    Check(LayerColor(0, false) == -1 and LayerColor(1LL << 40, true) == -1, "no colour is white");
    Check(std::fabs(CreateCost - 5000000.0) < 1e-6, "5m ISK to create");
    Check(std::fabs(AwardTotal(3) - 15000000.0) < 1e-6, "5m ISK per recipient");
    Check(CanSetStatus(StatusPrivate, StatusPublic) and CanSetStatus(StatusPublic, StatusPrivate), "private <-> public");
    Check(CanSetStatus(StatusPublic, StatusRemoved), "a recipient removes a medal");
    Check(!CanSetStatus(StatusRemoved, StatusPublic) and !CanSetStatus(StatusRemoved, StatusPrivate), "a removed medal stays removed");
    Check(!CanSetStatus(StatusPrivate, 0) and !CanSetStatus(StatusPrivate, 4), "no other statuses");
    Check(IsCorpWalletKey(1000) and IsCorpWalletKey(1006) and !IsCorpWalletKey(999) and !IsCorpWalletKey(1200), "wallet divisions 1000..1006");

    ::printf("%s\n", (failures ? "MedalRulesTest FAILED" : "MedalRulesTest passed"));
    return failures;
}
