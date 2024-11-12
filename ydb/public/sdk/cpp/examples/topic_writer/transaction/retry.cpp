#include <ydb/public/sdk/cpp/client/ydb_topic/topic.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>

void ThrowOnError(const NYdb::TStatus& status)
{
    if (status.IsSuccess()) {
        return;
    }

    ythrow yexception() << status;
}

NYdb::NTable::TTransaction BeginTransaction(NYdb::NTable::TSession& session)
{
    auto result = session.BeginTransaction().GetValueSync();
    ThrowOnError(result);
    return result.GetTransaction();
}

NYdb::TStatus DoWrite(NYdb::NTopic::TTopicClient& topicClient,
                      const NYdb::NTopic::TWriteSessionSettings& topicSessionSettings,
                      NYdb::NTable::TSession tableSession)
{
    auto transaction = BeginTransaction(tableSession);

    auto topicSession = topicClient.CreateSimpleBlockingWriteSession(topicSessionSettings);

    NYdb::NTopic::TWriteMessage writeMessage("message");

    topicSession->Write(std::move(writeMessage), &transaction);

    return transaction.Commit().GetValueSync();
}

int main()
{
    const TString ENDPOINT = "HOST:PORT";
    const TString DATABASE = "DATABASE";
    const TString TOPIC = "PATH/TO/TOPIC";

    NYdb::TDriverConfig config;
    config.SetEndpoint(ENDPOINT);
    config.SetDatabase(DATABASE);
    NYdb::TDriver driver(config);

    NYdb::NTable::TTableClient tableClient(driver);

    NYdb::NTopic::TTopicClient topicClient(driver);
    auto topicSessionSettings = NYdb::NTopic::TWriteSessionSettings()
        .Path(TOPIC)
        .DeduplicationEnabled(true);

    tableClient.RetryOperationSync([&topicClient, &topicSessionSettings](NYdb::NTable::TSession tableSession){
        return DoWrite(topicClient, topicSessionSettings, tableSession); 
    });
}
