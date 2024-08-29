#pragma once

#include <util/generic/string.h>
#include <cstddef>

extern const TString YDB_ENDPOINT;
extern const TString YDB_DATABASE;
extern const TString YDB_TOPICPATH;
extern const TString YDB_TOKEN;
extern const TString LOG_PATH;

struct TParameters {
    TString YdbEndpoint = YDB_ENDPOINT;
    TString YdbDatabase = YDB_DATABASE;
    TString YdbToken = YDB_TOKEN;
    int RandomSeed = 0;
    bool DebugSdk = false;
    TString TopicPath = YDB_TOPICPATH;
    size_t PartitionCount = 2;
    size_t TxCount = Max<size_t>();
    TString LogPath = LOG_PATH;
    size_t ThreadCount = 1;
};
