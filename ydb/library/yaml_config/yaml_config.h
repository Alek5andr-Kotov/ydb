#pragma once

#include <ydb/library/fyamlcpp/fyamlcpp.h>
#include <ydb/library/actors/core/actor.h>
#include <library/cpp/protobuf/json/json2proto.h>

#include <ydb/core/protos/config.pb.h>
#include <ydb/core/protos/console_config.pb.h>
#include <ydb/library/yaml_config/public/yaml_config.h>
#include <ydb/public/api/protos/ydb_status_codes.pb.h>
#include <yql/essentials/public/issue/yql_issue.h>
#include <ydb/public/api/protos/ydb_config.pb.h>

#include <openssl/sha.h>

#include <util/generic/string.h>
#include <util/generic/vector.h>
#include <util/generic/set.h>
#include <util/generic/map.h>
#include <util/stream/str.h>

#include <unordered_map>
#include <map>
#include <string>

namespace NKikimr::NYamlConfig {

struct TBasicUnknownFieldsCollector : public NProtobufJson::IUnknownFieldsCollector {
    void OnEnterMapItem(const TString& key) override {
        CurrentPath.push_back(key);
    }

    void OnEnterArrayItem(ui64 id) override {
        CurrentPath.push_back(ToString(id));
    }

    void OnLeaveMapItem() override {
        CurrentPath.pop_back();
    }

    void OnLeaveArrayItem() override {
        CurrentPath.pop_back();
    }

    void OnUnknownField(const TString& key, const google::protobuf::Descriptor& value) override {
        TString path;
        for (auto& piece : CurrentPath) {
            path.append("/");
            path.append(piece);
        }
        path.append("/");
        path.append(key);
        UnknownKeys[std::move(path)] = {key, value.full_name()};
    }

    const TMap<TString, std::pair<TString, TString>>& GetUnknownKeys() const {
        return UnknownKeys;
    }

private:
    TVector<TString> CurrentPath;
    TMap<TString, std::pair<TString, TString>> UnknownKeys;
};

/**
 * Converts YAML representation to ProtoBuf
 */
NKikimrConfig::TAppConfig YamlToProto(
    const NFyaml::TNodeRef& node,
    bool allowUnknown = false,
    bool preTransform = true,
    TSimpleSharedPtr<NProtobufJson::IUnknownFieldsCollector> unknownFieldsCollector = nullptr);

/**
 * Resolves config for given labels and stores result to appConfig
 * Stores intermediate resolve data in resolvedYamlConfig and resolvedJsonConfig if given
 */
void ResolveAndParseYamlConfig(
    const TString& mainYamlConfig,
    const TMap<ui64, TString>& volatileYamlConfigs,
    const TMap<TString, TString>& labels,
    NKikimrConfig::TAppConfig& appConfig,
    std::optional<TString> databaseYamlConfig = std::nullopt,
    TString* resolvedYamlConfig = nullptr,
    TString* resolvedJsonConfig = nullptr);

enum class EValidationResult {
    Ok,
    Warn,
    Error,
};

class IConfigValidator {
public:
    virtual ~IConfigValidator() = default;

    virtual EValidationResult ValidateConfig(
        const NKikimrConfig::TAppConfig& config,
        std::vector<TString>& msg) const = 0;
};

/**
 * Replaces kinds not managed by yaml config (e.g. NetClassifierConfig) from config 'from' in config 'to'
 * if corresponding configs are presenet in 'from'
 */
void ReplaceUnmanagedKinds(const NKikimrConfig::TAppConfig& from, NKikimrConfig::TAppConfig& to);

using TValidatorsMap = TMap<TString, TSimpleSharedPtr<IConfigValidator>>;

class IConfigSwissKnife {
public:
    virtual ~IConfigSwissKnife() = default;
    virtual bool VerifyReplaceRequest(const Ydb::Config::ReplaceConfigRequest& request, Ydb::StatusIds::StatusCode& status, NYql::TIssues& issues) const = 0;
    virtual bool VerifyMainConfig(const TString& config) const = 0;
    virtual bool VerifyStorageConfig(const TString& config) const = 0;
    virtual EValidationResult ValidateConfig(
        const NKikimrConfig::TAppConfig& config,
        std::vector<TString>& msg) const;

    const TMap<TString, TSimpleSharedPtr<IConfigValidator>>& GetValidators() const {
        return Validators;
    }
protected:
    TMap<TString, TSimpleSharedPtr<IConfigValidator>> Validators;
};


std::unique_ptr<IConfigSwissKnife> CreateDefaultConfigSwissKnife();

} // namespace NKikimr::NYamlConfig
