 /**
  * @name EVE_Medals.h
  *     corporation decorations (medals):  creation, awarding and the recipient's status (Crucible rules)
  *
  * these are pure functions, so the unit tests (eve-test) can check them
  *
  *  who:  creating and awarding need corpRoleDirector or corpRolePersonnelManager (medals.py CreateMedal and
  *   GiveMedalToCharacters raise CrpAccessDenied 'NeedRolesError' without one of them).  medals go to members of the
  *   corp only ("Decorations are medals which can be created by the CEO or Directors and then awarded to members of
  *   the corporation", EVE University wiki 'Managing corporation members';  eve-search thread 919555, Nov 2008).
  *
  *  cost:  5 million ISK to create a medal and 5 million ISK per recipient to award it, paid from the corp wallet
  *   division active in the session (corpAccountKey;  MedalRibbonPickerWindow.SubmitData asks for one the character
  *   can take from).  eve-search thread 919555 (2008.11.09: "it was changed to 5 million ISK for creation of the medal
  *   and 5 million ISK per person to award it") and thread 995922 (2009.02.11, the same two charges).  the journal
  *   entries are refMedalCreation (87) and refMedalIssuing (88).  both calls are asked twice:  the first raises
  *   ConfirmCreatingMedal / ConfirmGivingMedal {cost} and the client calls again with True when the pilot agrees.
  *
  *  text:  title 3..100 and description 10..1000 characters (appConst medalMin/MaxNameLength,
  *   medalMin/MaxDescriptionLength);  the award reason is a 200 character NamePopup (medals.GiveMedalToCharacters).
  *
  *  design:  the creation window (medalribbonranks.MedalRibbonPickerWindow) sends [[part, 'label.icon', color], ...]:
  *   part 1 is a ribbon layer, up to 5 (the race base '1_1' and '1_2', then the race's shapes '2_1'..'3_3'), part 2 a
  *   medal layer, up to 4 (one base shape, then up to 3 'elements').  icon is 'sheet_cell', cell 1..4 of a 2x2 sheet
  *   (MapLogo);  the label picks the texture res:/UI/Texture/Medals/<Ribbons|Medals>/<label lower>NN.dds.  both parts
  *   must be there:  uix.FormatMedalData indexes the details by part and fails on a missing one.
  *
  *  status (crpMedalStatus):  1 removed by the recipient, 2 private, 3 public.  the character sheet sets 2/3 and 1
  *   ("Remove", confirmed by DeleteMedalConfirmation);  a removed medal is no longer listed (GetMedalsReceivedWithFlag
  *   keeps 2 and 3) and can't be made visible again.  others see only public ones (info window Decorations tab).
  */


#ifndef EVE_MEDALS_H
#define EVE_MEDALS_H

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>


namespace MedalRules {

    // appConst.medalMinNameLength .. medalMaxDescriptionLength
    static constexpr size_t MinNameLength           = 3;
    static constexpr size_t MaxNameLength           = 100;
    static constexpr size_t MinDescriptionLength    = 10;
    static constexpr size_t MaxDescriptionLength    = 1000;
    // the award reason popup (uiutil.NamePopup maxLength=200)
    static constexpr size_t MaxReasonLength         = 200;

    // ISK, paid to SCC from the corp wallet
    static constexpr double CreateCost              = 5000000.0;
    static constexpr double AwardCost               = 5000000.0;   // per recipient

    // crpMedalData.part
    static constexpr int32_t PartRibbon             = 1;
    static constexpr int32_t PartMedal              = 2;
    static constexpr size_t MaxRibbonLayers         = 5;
    static constexpr size_t MaxMedalLayers          = 4;

    // chrMedals.status (crpMedalStatus)
    static constexpr int32_t StatusRemoved          = 1;
    static constexpr int32_t StatusPrivate          = 2;
    static constexpr int32_t StatusPublic           = 3;

    // characters of a UTF-8 string (the client counts unicode characters, the server holds UTF-8)
    inline size_t Utf8Length(const std::string& text)
    {
        size_t count = 0;
        for (unsigned char c : text)
            if ((c & 0xC0) != 0x80)
                ++count;
        return count;
    }

    enum class TextResult : uint8_t {
        Ok,
        NameInvalid,            // MedalNameInvalid:  blank or shorter than MinNameLength
        NameTooLong,            // MedalNameTooLong {maxLength}
        DescriptionTooShort,    // MedalDescriptionTooShort {minLength}
        DescriptionTooLong      // MedalDescriptionTooLong {maxLength}
    };

    inline bool IsBlank(const std::string& text)
    {
        for (unsigned char c : text)
            if (!std::isspace(c))
                return false;
        return true;
    }

    inline TextResult CheckText(const std::string& title, const std::string& description)
    {
        if (IsBlank(title) or (Utf8Length(title) < MinNameLength))
            return TextResult::NameInvalid;
        if (Utf8Length(title) > MaxNameLength)
            return TextResult::NameTooLong;
        if (IsBlank(description) or (Utf8Length(description) < MinDescriptionLength))
            return TextResult::DescriptionTooShort;
        if (Utf8Length(description) > MaxDescriptionLength)
            return TextResult::DescriptionTooLong;
        return TextResult::Ok;
    }

    inline bool ReasonOk(const std::string& reason)
    {
        return Utf8Length(reason) <= MaxReasonLength;
    }

    // 'sheet_cell' with sheet 1..maxSheet and cell 1..4
    inline bool ValidIcon(const std::string& icon, int maxSheet)
    {
        size_t sep = icon.find('_');
        if ((sep == std::string::npos) or (sep == 0) or (sep + 1 >= icon.size()) or (sep > 3) or (icon.size() - sep - 1 > 1))
            return false;
        for (size_t i = 0; i < icon.size(); ++i)
            if ((i != sep) and !std::isdigit(static_cast<unsigned char>(icon[i])))
                return false;
        int sheet = std::atoi(icon.substr(0, sep).c_str());
        int cell = std::atoi(icon.substr(sep + 1).c_str());
        return (sheet >= 1) and (sheet <= maxSheet) and (cell >= 1) and (cell <= 4);
    }

    inline std::string Lower(const std::string& text)
    {
        std::string out(text);
        for (char& c : out)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    // one 'label.icon' graphic of the creation window's sliders
    inline bool ValidGraphic(int32_t part, const std::string& graphic)
    {
        size_t dot = graphic.find('.');
        if ((dot == std::string::npos) or (graphic.find('.', dot + 1) != std::string::npos))
            return false;
        std::string label = Lower(graphic.substr(0, dot));
        std::string icon = graphic.substr(dot + 1);
        if (part == PartRibbon) {
            // the four empire races (cfg.races raceName), base sheet 1 and the shape sheets 2..3 (GetRibbonShapeData)
            if ((label != "amarr") and (label != "caldari") and (label != "gallente") and (label != "minmatar"))
                return false;
            return ValidIcon(icon, 3);
        }
        if (part == PartMedal) {
            // AddSliders:  the base shapes and their sheet counts, then the elements
            static const struct { const char* label; int sheets; } shapes[] = {
                {"compass", 2}, {"imperial", 2}, {"pentagon", 1}, {"rombus", 1}, {"round", 6},
                {"shield", 1}, {"square", 2}, {"star", 7}, {"elements", 33}
            };
            for (const auto& cur : shapes)
                if (label == cur.label)
                    return ValidIcon(icon, cur.sheets);
            return false;
        }
        return false;
    }

    struct Layer {
        int32_t part = 0;
        std::string graphic;
        int32_t color = -1;
    };

    // the whole design:  every layer valid, 1..5 ribbon and 1..4 medal layers
    inline bool ValidDesign(const std::vector<Layer>& layers)
    {
        size_t ribbon = 0, medal = 0;
        for (const auto& cur : layers) {
            if (!ValidGraphic(cur.part, cur.graphic))
                return false;
            if (cur.part == PartRibbon)
                ++ribbon;
            else
                ++medal;
        }
        return (ribbon >= 1) and (ribbon <= MaxRibbonLayers) and (medal >= 1) and (medal <= MaxMedalLayers);
    }

    // the layer colour is a 32 bit ARGB int (trinity.TriColor.AsInt);  anything else is plain white (-1)
    inline int32_t LayerColor(int64_t value, bool isInteger)
    {
        if (!isInteger or (value < INT32_MIN) or (value > static_cast<int64_t>(UINT32_MAX)))
            return -1;
        return static_cast<int32_t>(static_cast<uint32_t>(value & 0xFFFFFFFF));
    }

    inline double AwardTotal(size_t recipients, double perRecipient = AwardCost)
    {
        return perRecipient * static_cast<double>(recipients);
    }

    // SetMedalStatus:  the statuses a recipient may set, and a removed medal stays removed
    inline bool CanSetStatus(int32_t current, int32_t wanted)
    {
        if ((wanted != StatusRemoved) and (wanted != StatusPrivate) and (wanted != StatusPublic))
            return false;
        return (current == StatusPrivate) or (current == StatusPublic);
    }

    // a corp wallet division the session can pay from (1000..1006)
    inline bool IsCorpWalletKey(int32_t key)
    {
        return (key >= 1000) and (key <= 1006);
    }
}

#endif  // EVE_MEDALS_H
