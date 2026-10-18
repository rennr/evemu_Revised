 /**
  * @name EVE_Store.h
  *     the Noble Exchange (NeX) store, Aurum (AUR) conversions and stipend, and the apparel wardrobe (Crucible rules)
  *
  * these are pure functions, so the unit tests (eve-test) can check them
  *
  *  offers:  the client (ui/services/storeSvc.py GetPreparedOffers) takes an offer as one offered good (GOODTYPE_EVETYPEID,
  *   argumentValue a typeID, amount the units per bundle) and costs that are all GOODTYPE_CREDITS in creditsAURUM, summed
  *   into the price.  an ISK cost, a non-credit cost or a second offered good makes it drop the offer.
  *   AcceptOffer(offerID, quantity) buys 'quantity' bundles.
  *
  *  conversions:  appConst Plex2AurExchangeRatio 3500 AUR per PLEX (typePilotLicence 29668), AurumToken2AurExchangeRatio
  *   1000 AUR per Aurum Token (typeAurumToken 2833), both from items in the station item hangar.
  *
  *  stipend:  a server option (no Crucible counterpart), paid per character every interval, journaled refAurumGiveAway.
  *   the last payment time is stored;  a payment advances it by whole intervals, so the schedule keeps its phase, and a
  *   character away for several intervals is paid at most maxPeriods of them.  a character never paid is due at once.
  *
  *  wardrobe:  the character creator lists as owned the category 30 (Apparel) types in the current station's item
  *   hangar and the character's own items with flagWardrobe (ui/services/ccSvc.py GetMyApparel).  a paperdoll resource
  *   with a typeID needs that type owned (characterCreationLayer.py raises ItemNotAtStation {item: typeID});  typeID -1
  *   (six black monocle variants) and None are not sellable types.  wearing a piece moves one unit from the hangar into
  *   the wardrobe, where it stays (taking it off doesn't move it back).
  */


#ifndef EVE_STORE_H
#define EVE_STORE_H

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>


namespace StoreRules {

    // storeConst
    static constexpr int32_t GoodTypeCredits        = 1;
    static constexpr int32_t GoodTypeEveTypeID      = 2;
    // appConst creditsISK, creditsAURUM
    static constexpr int32_t CreditsISK             = 0;
    static constexpr int32_t CreditsAurum           = 1;

    // appConst exchange rates, AUR per item
    static constexpr int64_t PlexToAur              = 3500;
    static constexpr int64_t AurTokenToAur          = 1000;

    // bundles per purchase (the buy window has no limit of its own)
    static constexpr int64_t MaxBundles             = 1000;
    // more AUR than this in one operation is refused (the client draws AUR as an int)
    static constexpr int64_t MaxAur                 = 2000000000LL;

    // filetime ticks (100 ns) per hour
    static constexpr int64_t TicksPerHour           = 36000000000LL;

    // the AUR cost of 'bundles' of an offer priced 'price' a bundle.  -1 when the quantity or price isn't allowed
    inline int64_t OfferCost(int64_t price, int64_t bundles)
    {
        if ((price < 0) or (bundles < 1) or (bundles > MaxBundles))
            return -1;
        if ((price > 0) and (bundles > MaxAur / price))
            return -1;
        return price * bundles;
    }

    // the AUR paid for 'qty' items at 'rate' AUR each.  -1 when qty isn't allowed
    inline int64_t ConversionPayout(int64_t rate, int64_t qty)
    {
        if ((rate < 1) or (qty < 1) or (qty > MaxAur / rate))
            return -1;
        return rate * qty;
    }

    /* how much to take from each stack, in the order given, to make up 'qty'.  empty when they hold less than that
     *  (the client sends the PLEX stacks largest first, so that is the order used up)
     */
    inline std::vector<int64_t> PlanTake(const std::vector<int64_t>& stacks, int64_t qty)
    {
        std::vector<int64_t> take(stacks.size(), 0);
        if (qty < 1)
            return std::vector<int64_t>();
        int64_t remaining(qty);
        for (size_t i = 0; (i < stacks.size()) and (remaining > 0); ++i) {
            if (stacks[i] < 1)
                continue;
            take[i] = std::min<int64_t>(stacks[i], remaining);
            remaining -= take[i];
        }
        if (remaining > 0)
            return std::vector<int64_t>();
        return take;
    }

    struct StipendDue {
        int64_t periods;        // payments to make now
        int64_t newLastPaid;    // the last-paid time to store with them
        int64_t nextDue;        // when the next one is due (0 when the stipend is off)
    };

    /* the stipend due at 'now' (filetimes).  hasLastPaid false: never paid, one payment is due at once.
     *  intervalTicks <= 0 or maxPeriods <= 0 turn the stipend off
     */
    inline StipendDue GetStipendDue(bool hasLastPaid, int64_t lastPaid, int64_t now, int64_t intervalTicks, int64_t maxPeriods)
    {
        StipendDue due = StipendDue();
        due.newLastPaid = lastPaid;
        if ((intervalTicks <= 0) or (maxPeriods <= 0))
            return due;
        if (!hasLastPaid) {
            due.periods = 1;
            due.newLastPaid = now;
            due.nextDue = now + intervalTicks;
            return due;
        }
        if (now < lastPaid) {
            // the clock went back:  wait for it to pass the last payment again
            due.nextDue = lastPaid + intervalTicks;
            return due;
        }
        int64_t elapsed(now - lastPaid);
        int64_t whole(elapsed / intervalTicks);
        due.periods = std::min<int64_t>(whole, maxPeriods);
        due.newLastPaid = lastPaid + whole * intervalTicks;
        due.nextDue = due.newLastPaid + intervalTicks;
        return due;
    }

    // a paperdoll resource's typeID that has to be owned to wear it (none, -1 are not sellable types)
    inline bool IsOwnedType(int32_t typeID)
    {
        return (typeID > 0);
    }

    // the first worn type that is in neither the hangar nor the wardrobe, 0 when all are owned
    inline int32_t FirstUnownedType(const std::vector<int32_t>& worn, const std::set<int32_t>& hangar, const std::set<int32_t>& wardrobe)
    {
        for (int32_t typeID : worn) {
            if (!IsOwnedType(typeID))
                continue;
            if ((hangar.find(typeID) == hangar.end()) and (wardrobe.find(typeID) == wardrobe.end()))
                return typeID;
        }
        return 0;
    }

    // the worn types not yet in the wardrobe (one unit of each moves there from the hangar), each once, ascending
    inline std::vector<int32_t> TypesToWardrobe(const std::vector<int32_t>& worn, const std::set<int32_t>& wardrobe)
    {
        std::set<int32_t> types;
        for (int32_t typeID : worn)
            if (IsOwnedType(typeID) and (wardrobe.find(typeID) == wardrobe.end()))
                types.insert(typeID);
        return std::vector<int32_t>(types.begin(), types.end());
    }
}

#endif  // EVE_STORE_H
