#include "workload_driver.h"
#include "logging.h"

#include <library/cpp/getopt/last_getopt.h>

int main(int argc, const char* argv[])
{
    TParameters params;

    NLastGetopt::TOpts opts;
    opts.AddLongOption('e', "endpoint")
        .DefaultValue(YDB_ENDPOINT)
        .StoreResult(&params.YdbEndpoint);
    opts.AddLongOption('d', "database")
        .DefaultValue(YDB_DATABASE)
        .StoreResult(&params.YdbDatabase);
    opts.AddLongOption("token")
        .DefaultValue(YDB_TOKEN)
        .StoreResult(&params.YdbToken);
    opts.AddLongOption('t', "threads")
        .DefaultValue(1)
        .StoreResult(&params.ThreadCount);
    opts.AddLongOption("seed")
        .DefaultValue(time(nullptr))
        .StoreResult(&params.RandomSeed);
    opts.AddLongOption("debug")
        .StoreTrue(&params.DebugSdk);
    opts.AddLongOption("topic")
        .DefaultValue(YDB_TOPICPATH)
        .StoreResult(&params.TopicPath);
    opts.AddLongOption('p', "partitions")
        .DefaultValue(2)
        .StoreResult(&params.PartitionCount);
    opts.AddLongOption("transactions")
        .StoreResult(&params.TxCount);
    opts.AddLongOption("log-path")
        .DefaultValue(LOG_PATH)
        .StoreResult(&params.LogPath);
    NLastGetopt::TOptsParseResult(&opts, argc, argv);

    OpenLog(params);
    SetLogPrefix("main");

    TWorkloadDriver d;

    d.StartWorkloadThreads(params);
    d.Run();
    d.StopWorkloadThreads();

    CloseLog();
}
