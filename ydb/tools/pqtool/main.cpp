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
    for (int i = 0; i < o.issues().size(); ++i) {
        sb << " issues[" << i << "] " << o.issues()[i];
    }
    return sb;
}

using TGrpcChannelInterfacePtr = std::shared_ptr<grpc::ChannelInterface>;

Ydb::KeyValue::CreateVolumeResult CreateVolume(TGrpcChannelInterfacePtr channel, const TString& path)
{
    auto stub = Ydb::KeyValue::V1::KeyValueService::NewStub(channel);

    Ydb::KeyValue::CreateVolumeRequest request;
    request.set_path(path);
    request.set_partition_count(1);
    request.mutable_storage_config()->add_channel()->set_media("ssd");

    Ydb::KeyValue::CreateVolumeResponse response;
    Ydb::KeyValue::CreateVolumeResult result;

    grpc::ClientContext ctx;
    MakeRequestContext(ctx);

    stub->CreateVolume(&ctx, request, &response);
    Y_ABORT_UNLESS(response.operation().status() == Ydb::StatusIds::SUCCESS,
                   "operation: %s", GetOperationDescription(response.operation()).data());

    response.operation().result().UnpackTo(&result);

    return result;
}

Ydb::KeyValue::DropVolumeResult DropVolume(TGrpcChannelInterfacePtr channel, const TString& path)
{
    auto stub = Ydb::KeyValue::V1::KeyValueService::NewStub(channel);

    Ydb::KeyValue::DropVolumeRequest request;
    request.set_path(path);

    Ydb::KeyValue::DropVolumeResponse response;
    Ydb::KeyValue::DropVolumeResult result;

    grpc::ClientContext ctx;
    MakeRequestContext(ctx);

    stub->DropVolume(&ctx, request, &response);
    Y_ABORT_UNLESS(response.operation().status() == Ydb::StatusIds::SUCCESS,
                   "operation: %s", GetOperationDescription(response.operation()).data());

    response.operation().result().UnpackTo(&result);

    return result;
}

Ydb::KeyValue::DescribeVolumeResult DescribeVolume(TGrpcChannelInterfacePtr channel, const TString &path)
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
    TString topicPath = database + "/" + "kvtable";

    //using namespace NLastGetopt;
    //TOpts opts = TOpts::Default();
    //opts.AddLongOption('e', "endpoint", "endpoint").RequiredArgument("hostname");
    //opts.AddLongOption('d', "database", "database").RequiredArgument("path");
    //opts.AddLongOption('t', "topic", "topic path").RequiredArgument("path");
    //opts.AddLongOption('p', "partition", "partition").RequiredArgument("number");
    //TOptsParseResult res(&opts, argc, argv);

    TGrpcChannelInterfacePtr channel;
    channel = grpc::CreateChannel(endpoint + ":" + ToString(port), grpc::InsecureChannelCredentials());

    //CreateVolume(channel, topicPath);
    DropVolume(channel, topicPath);

    //auto result = DescribeVolume(channel, topicPath);

    //Cout << result.path() << Endl;
    //Cout << result.partition_count() << Endl;
}
