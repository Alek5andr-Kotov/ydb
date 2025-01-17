#pragma once
#include "common.h"

#include <ydb/core/base/path.h>
#include <ydb/core/formats/arrow/arrow_helpers.h>
#include <ydb/core/formats/arrow/serializer/abstract.h>
#include <ydb/core/tx/columnshard/common/scalars.h>

#include <ydb/library/formats/arrow/common/validation.h>

#include <contrib/libs/apache/arrow/cpp/src/arrow/util/compression.h>
#include <util/generic/hash_set.h>
#include <util/generic/set.h>

namespace NKikimr::NOlap {

class TTierInfo {
private:
    YDB_READONLY_DEF(TString, Name);
    YDB_READONLY_DEF(TString, EvictColumnName);
    YDB_READONLY_DEF(TDuration, EvictDuration);

    ui32 TtlUnitsInSecond;
    YDB_READONLY_DEF(std::optional<NArrow::NSerialization::TSerializerContainer>, Serializer);
public:
    static TString GetTtlTierName() {
        return NTiering::NCommon::DeleteTierName;
    }

    TTierInfo(const TString& tierName, TDuration evictDuration, const TString& column, ui32 unitsInSecond = 0)
        : Name(tierName)
        , EvictColumnName(column)
        , EvictDuration(evictDuration)
        , TtlUnitsInSecond(unitsInSecond) {
        Y_ABORT_UNLESS(!!Name);
        Y_ABORT_UNLESS(!!EvictColumnName);
    }

    TInstant GetEvictInstant(const TInstant now) const {
        return now - EvictDuration;
    }

    TTierInfo& SetSerializer(const NArrow::NSerialization::TSerializerContainer& value) {
        Serializer = value;
        return *this;
    }

    std::shared_ptr<arrow::Field> GetEvictColumn(const std::shared_ptr<arrow::Schema>& schema) const {
        return schema->GetFieldByName(EvictColumnName);
    }

    std::optional<TInstant> ScalarToInstant(const std::shared_ptr<arrow::Scalar>& scalar) const;

    static std::shared_ptr<TTierInfo> MakeTtl(const TDuration evictDuration, const TString& ttlColumn, ui32 unitsInSecond = 0) {
        return std::make_shared<TTierInfo>(NTiering::NCommon::DeleteTierName, evictDuration, ttlColumn, unitsInSecond);
    }

    static ui32 GetUnitsInSecond(const NKikimrSchemeOp::TTTLSettings::EUnit timeUnit) {
        switch (timeUnit) {
            case NKikimrSchemeOp::TTTLSettings::UNIT_SECONDS:
                return 1;
            case NKikimrSchemeOp::TTTLSettings::UNIT_MILLISECONDS:
                return 1000;
            case NKikimrSchemeOp::TTTLSettings::UNIT_MICROSECONDS:
                return 1000 * 1000;
            case NKikimrSchemeOp::TTTLSettings::UNIT_NANOSECONDS:
                return 1000 * 1000 * 1000;
            case NKikimrSchemeOp::TTTLSettings::UNIT_AUTO:
                return 0;
        }
    }

    TString GetDebugString() const {
        TStringBuilder sb;
        sb << "name=" << Name << ";duration=" << EvictDuration << ";column=" << EvictColumnName << ";serializer=";
        if (Serializer) {
            sb << Serializer->DebugString();
        } else {
            sb << "NOT_SPECIFIED(Default)";
        }
        sb << ";";
        return sb;
    }
};

class TTierRef {
public:
    TTierRef(const std::shared_ptr<TTierInfo>& tierInfo)
        : Info(tierInfo)
    {
        Y_ABORT_UNLESS(tierInfo);
    }

    bool operator < (const TTierRef& b) const {
        if (Info->GetEvictDuration() > b.Info->GetEvictDuration()) {
            return true;
        } else if (Info->GetEvictDuration() == b.Info->GetEvictDuration()) {
            if (Info->GetName() == NTiering::NCommon::DeleteTierName) {
                return true;
            } else if (b.Info->GetName() == NTiering::NCommon::DeleteTierName) {
                return false;
            }
            return Info->GetName() > b.Info->GetName(); // add stability: smaller name is hotter
        }
        return false;
    }

    bool operator == (const TTierRef& b) const {
        return Info->GetEvictDuration() == b.Info->GetEvictDuration()
            && Info->GetName() == b.Info->GetName();
    }

    const TTierInfo& Get() const {
        return *Info;
    }

    std::shared_ptr<TTierInfo> GetPtr() const {
        return Info;
    }

private:
    std::shared_ptr<TTierInfo> Info;
};

class TTiering {
    using TProto = NKikimrSchemeOp::TColumnDataLifeCycle::TTtl;
    using TTiersMap = THashMap<TString, std::shared_ptr<TTierInfo>>;
    TTiersMap TierByName;
    TSet<TTierRef> OrderedTiers;
    std::optional<TString> TTLColumnName;
public:

    class TTieringContext {
    private:
        YDB_READONLY_DEF(TString, CurrentTierName);
        YDB_READONLY_DEF(TDuration, CurrentTierLag);

        YDB_READONLY_DEF(std::optional<TString>, NextTierName);
        YDB_READONLY_DEF(std::optional<TDuration>, NextTierWaiting);
    public:
        TString DebugString() const {
            TStringBuilder sb;
            sb << CurrentTierName << "/" << CurrentTierLag << ";";
            if (NextTierName) {
                sb << *NextTierName << "/" << *NextTierWaiting << ";";
            }
            return sb;
        }

        TTieringContext(const TString& tierName, const TDuration waiting, const std::optional<TString>& nextTierName = {}, const std::optional<TDuration>& nextTierDuration = {})
            : CurrentTierName(tierName)
            , CurrentTierLag(waiting)
            , NextTierName(nextTierName)
            , NextTierWaiting(nextTierDuration)
        {
            AFL_VERIFY(!nextTierName == !nextTierDuration);
        }

        TString GetNextTierNameVerified() const {
            AFL_VERIFY(NextTierName);
            return *NextTierName;
        }

        TDuration GetNextTierWaitingVerified() const {
            AFL_VERIFY(NextTierWaiting);
            return *NextTierWaiting;
        }
    };

    TTieringContext GetTierToMove(const std::shared_ptr<arrow::Scalar>& max, const TInstant now, const bool skipEviction) const;

    const TTiersMap& GetTierByName() const {
        return TierByName;
    }

    std::shared_ptr<TTierInfo> GetTierByName(const TString& name) const {
        auto it = TierByName.find(name);
        if (it == TierByName.end()) {
            return nullptr;
        }
        return it->second;
    }

    const TSet<TTierRef>& GetOrderedTiers() const {
        return OrderedTiers;
    }

    bool HasTiers() const {
        return !OrderedTiers.empty();
    }

    [[nodiscard]] bool Add(const std::shared_ptr<TTierInfo>& tier) {
        AFL_VERIFY(tier);
        if (!TTLColumnName) {
            if (tier->GetEvictColumnName().empty()) {
                AFL_ERROR(NKikimrServices::TX_COLUMNSHARD)("problem", "empty_evict_column_name");
                return false;
            }
            TTLColumnName = tier->GetEvictColumnName();
        } else if (*TTLColumnName != tier->GetEvictColumnName()) {
            AFL_ERROR(NKikimrServices::TX_COLUMNSHARD)("problem", "incorrect_tiering_metadata")("column_before", *TTLColumnName)
                ("column_new", tier->GetEvictColumnName());
            return false;
        }

        TierByName.emplace(tier->GetName(), tier);
        OrderedTiers.emplace(tier);
        return true;
    }

    std::optional<NArrow::NSerialization::TSerializerContainer> GetSerializer(const TString& name) const {
        auto it = TierByName.find(name);
        if (it != TierByName.end()) {
            Y_ABORT_UNLESS(!name.empty());
            return it->second->GetSerializer();
        }
        return {};
    }

    TConclusionStatus DeserializeFromProto(const TProto& serialized) {
        if (serialized.HasExpireAfterBytes()) {
            return TConclusionStatus::Fail("TTL by size is not supported.");
        }
        if (!serialized.HasColumnName()) {
            return TConclusionStatus::Fail("Missing column name in TTL settings");
        }

        const TString ttlColumnName = serialized.GetColumnName();
        const ui32 unitsInSecond = TTierInfo::GetUnitsInSecond(serialized.GetColumnUnit());

        if (!serialized.TiersSize()) {
            // legacy schema
            if (!Add(TTierInfo::MakeTtl(TDuration::Seconds(serialized.GetExpireAfterSeconds()), ttlColumnName, unitsInSecond))) {
                return TConclusionStatus::Fail("Invalid ttl settings");
            }
        }
        for (const auto& tier : serialized.GetTiers()) {
            if (!tier.HasApplyAfterSeconds()) {
                return TConclusionStatus::Fail("Missing eviction delay in tier description");
            }
            std::shared_ptr<TTierInfo> tierInfo;
            switch (tier.GetActionCase()) {
                case NKikimrSchemeOp::TTTLSettings_TTier::kDelete:
                    tierInfo = TTierInfo::MakeTtl(TDuration::Seconds(tier.GetApplyAfterSeconds()), ttlColumnName, unitsInSecond);
                    break;
                case NKikimrSchemeOp::TTTLSettings_TTier::kEvictToExternalStorage:
                    tierInfo = std::make_shared<TTierInfo>(CanonizePath(tier.GetEvictToExternalStorage().GetStorage()),
                        TDuration::Seconds(tier.GetApplyAfterSeconds()), ttlColumnName, unitsInSecond);
                    break;
                case NKikimrSchemeOp::TTTLSettings_TTier::ACTION_NOT_SET:
                    return TConclusionStatus::Fail("No action in tier");
            }
            if (!Add(tierInfo)) {
                return TConclusionStatus::Fail("Invalid tier settings");
            }
        }
        return TConclusionStatus::Success();
    }

    const TString& GetEvictColumnName() const {
        AFL_VERIFY(TTLColumnName);
        return *TTLColumnName;
    }

    TString GetDebugString() const {
        TStringBuilder sb;
        for (auto&& i : OrderedTiers) {
            sb << i.Get().GetDebugString() << "; ";
        }
        return sb;
    }

    THashSet<TString> GetUsedTiers() const {
        THashSet<TString> tiers;
        for (const auto& [name, info] : TierByName) {
            if (name != NTiering::NCommon::DeleteTierName) {
                tiers.emplace(name);
            }
        }
        return tiers;
    }

    static THashSet<TString> GetUsedTiers(const TProto& ttlSettings) {
        THashSet<TString> usedTiers;
        for (const auto& tier : ttlSettings.GetTiers()) {
            if (tier.HasEvictToExternalStorage()) {
                usedTiers.emplace(CanonizePath(tier.GetEvictToExternalStorage().GetStorage()));
            }
        }
        return usedTiers;
    }
};

}
