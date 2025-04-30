#include <ydb/public/api/grpc/ydb_keyvalue_v1.grpc.pb.h>
#include <ydb/public/sdk/cpp/client/resources/ydb_resources.h>

#include <contrib/libs/grpc/include/grpcpp/create_channel.h>
#include <contrib/libs/grpc/include/grpcpp/security/credentials.h>

//#include <library/cpp/getopt/last_getopt.h>
#include <util/string/builder.h>

void MakeRequestContext(grpc::ClientContext& ctx)
{
    ctx.AddMetadata(NYdb::YDB_AUTH_TICKET_HEADER, "root@builtin");
}

TString GetOperationDescription(const Ydb::Operations::Operation& o)
{
    TStringBuilder sb;
    sb << "status " << Ydb::StatusIds::StatusCode_Name(o.status());
    //sb << " issues " << o.issues();
    return sb;
}

Ydb::KeyValue::DescribeVolumeResult DescribeVolume(std::shared_ptr<grpc::ChannelInterface> channel, const TString &path)
{
    auto stub = Ydb::KeyValue::V1::KeyValueService::NewStub(channel);

    Ydb::KeyValue::DescribeVolumeRequest request;
    request.set_path(path);

    Ydb::KeyValue::DescribeVolumeResponse response;
    Ydb::KeyValue::DescribeVolumeResult result;

    grpc::ClientContext ctx;
    MakeRequestContext(ctx);

    stub->DescribeVolume(&ctx, request, &response);
    Y_ABORT_UNLESS(response.operation().status() == Ydb::StatusIds::SUCCESS,
                   "operation: %s", GetOperationDescription(response.operation()).data());

    response.operation().result().UnpackTo(&result);

    return result;
}

int main(int argc, char* argv[])
{
    Y_UNUSED(argc);
    Y_UNUSED(argv);

    TString endpoint = "lbk-devslice-1.logbroker.yandex.net";
    TString database = "/lbk-devslice-1/db1";
    ui16 port = 2135;
    TString topicPath = database + "/" + "my_topic";

    //using namespace NLastGetopt;
    //TOpts opts = TOpts::Default();
    //opts.AddLongOption('e', "endpoint", "endpoint").RequiredArgument("hostname");
    //opts.AddLongOption('d', "database", "database").RequiredArgument("path");
    //opts.AddLongOption('t', "topic", "topic path").RequiredArgument("path");
    //opts.AddLongOption('p', "partition", "partition").RequiredArgument("number");
    //TOptsParseResult res(&opts, argc, argv);

    std::shared_ptr<grpc::ChannelInterface> channel;
    channel = grpc::CreateChannel(endpoint + ":" + ToString(port), grpc::InsecureChannelCredentials());

    auto result = DescribeVolume(channel, topicPath);

    Cout << result.path() << Endl;
    Cout << result.partition_count() << Endl;
}
