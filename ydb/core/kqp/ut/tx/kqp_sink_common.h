#pragma once

#include <ydb/core/kqp/ut/common/kqp_ut_common.h>
#include <ydb/core/testlib/common_helper.h>
#include <ydb/core/tx/columnshard/hooks/abstract/abstract.h>
#include <ydb/core/tx/columnshard/hooks/testing/controller.h>


namespace NKikimr {
namespace NKqp {

using namespace NYdb;
using namespace NYdb::NQuery;

class TTableDataModificationTester {
protected:
    std::unique_ptr<TKikimrRunner> Kikimr;
    YDB_ACCESSOR(bool, IsOlap, false);
    YDB_ACCESSOR(bool, FastSnapshotExpiration, false);
    YDB_ACCESSOR(bool, DisableSinks, false);

    virtual void DoExecute() = 0;
public:
    void Execute() {
        auto settings = TKikimrSettings().SetWithSampleTables(false).SetColumnShardReaderClassName("PLAIN");
        settings.AppConfig.MutableTableServiceConfig()->SetEnableOlapSink(!DisableSinks);
        settings.AppConfig.MutableTableServiceConfig()->SetEnableOltpSink(!DisableSinks);
        settings.AppConfig.MutableTableServiceConfig()->SetEnableSnapshotIsolationRW(true);
        if (FastSnapshotExpiration) {
            settings.SetKeepSnapshotTimeout(TDuration::Seconds(1));
        }

        Kikimr = std::make_unique<TKikimrRunner>(settings);
        Tests::NCommon::TLoggerInit(*Kikimr).Initialize();

        auto client = Kikimr->GetQueryClient();

        auto csController = NYDBTest::TControllers::RegisterCSControllerGuard<NYDBTest::NColumnShard::TController>();
        csController->SetOverridePeriodicWakeupActivationPeriod(TDuration::Seconds(1));
        csController->SetOverrideLagForCompactionBeforeTierings(TDuration::Seconds(1));

        {
            auto type = IsOlap ? "COLUMN" : "ROW";
            auto result = client.ExecuteQuery(Sprintf(R"(
                CREATE TABLE `/Root/Test` (
                    Group Uint32 not null,
                    Name String not null,
                    Amount Uint64,
                    Comment String,
                    PRIMARY KEY (Group, Name)
                ) WITH (
                    STORE = %s,
                    AUTO_PARTITIONING_MIN_PARTITIONS_COUNT = 10
                );

                CREATE TABLE `/Root/KV` (
                    Key Uint32 not null,
                    Value String,
                    PRIMARY KEY (Key)
                ) WITH (
                    STORE = %s,
                    AUTO_PARTITIONING_BY_SIZE = DISABLED,
                    AUTO_PARTITIONING_BY_LOAD = DISABLED,
                    AUTO_PARTITIONING_MIN_PARTITIONS_COUNT = 100,
                    UNIFORM_PARTITIONS = 100
                );

                CREATE TABLE `/Root/KV2` (
                    Key Uint32 not null,
                    Value String,
                    PRIMARY KEY (Key)
                ) WITH (
                    STORE = %s,
                    AUTO_PARTITIONING_BY_SIZE = DISABLED,
                    AUTO_PARTITIONING_BY_LOAD = DISABLED,
                    AUTO_PARTITIONING_MIN_PARTITIONS_COUNT = 100,
                    UNIFORM_PARTITIONS = 100
                );
            )", type, type, type), TTxControl::NoTx()).GetValueSync();
            UNIT_ASSERT_C(result.GetStatus() == NYdb::EStatus::SUCCESS, result.GetIssues().ToString());
        }

        {
            auto result = client.ExecuteQuery(R"(
                REPLACE INTO `Test` (Group, Name, Amount, Comment) VALUES
                    (1u, "Anna", 3500ul, "None"),
                    (1u, "Paul", 300ul, "None"),
                    (2u, "Tony", 7200ul, "None");
                REPLACE INTO `KV` (Key, Value) VALUES
                    (1u, "One"),
                    (2u, "Two"),
                    (3u, "Three"),
                    (4000000001u, "BigOne"),
                    (4000000002u, "BigTwo"),
                    (4000000003u, "BigThree");
                )", TTxControl::NoTx()).GetValueSync();
            UNIT_ASSERT_C(result.GetStatus() == NYdb::EStatus::SUCCESS, result.GetIssues().ToString());
        }

        DoExecute();
    }

};

}
}
