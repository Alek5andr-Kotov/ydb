PROGRAM(simple_topic_writer)

SRCS(
    retry.cpp
)

PEERDIR(
    ydb/public/sdk/cpp/client/ydb_topic
    ydb/public/sdk/cpp/client/ydb_table
)

END()
