PROGRAM(topictx)

SRCS(
    topictx.cpp
    application.cpp
    counter_toucher.cpp
    logging.cpp
    parameters.cpp
    workload_driver.cpp
)

PEERDIR(
    ydb/public/sdk/cpp/client/ydb_table
    ydb/public/sdk/cpp/client/ydb_topic
)

END()
