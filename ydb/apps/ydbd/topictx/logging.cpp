#include "logging.h"
#include <library/cpp/logger/file.h>
#include <library/cpp/logger/filter.h>
#include <library/cpp/logger/log.h>
#include <util/datetime/base.h>

static TMaybe<TLog> Logger;
thread_local static TString LogPrefix;
static THolder<TLogBackend> LogBackend;

ELogPriority GetLogPriority(const TParameters& params)
{
    return params.DebugSdk ? TLOG_DEBUG : TLOG_INFO;
}

void OpenLog(const TParameters& params)
{
    ELogPriority priority = GetLogPriority(params);
    LogBackend = MakeHolder<TFilteredLogBackend>(MakeHolder<TFileLogBackend>(params.LogPath),
                                                 priority);

    Logger = TLog(GetLogBackend());
    Logger->SetDefaultPriority(priority);
}

void CloseLog()
{
    Logger = Nothing();
    LogBackend = nullptr;
}

THolder<TLogBackend> GetLogBackend()
{
    Y_ABORT_UNLESS(LogBackend);

    class TLogBackendWrapper : public TLogBackend {
    public:
        explicit TLogBackendWrapper(TLogBackend* slave) :
            Slave(slave)
        {
        }

        void WriteData(const TLogRecord& rec) override
        {
            Slave->WriteData(rec);
        }
            
        void ReopenLog() override
        {
            Slave->ReopenLog();
        }

    private:
        TLogBackend* Slave;
    };

    THolder<TLogBackend> logBackend(new TLogBackendWrapper(LogBackend.Get()));

    return logBackend;
}

void SetLogPrefix(TString logPrefix)
{
    LogPrefix = std::move(logPrefix);
}

TTempBufOutput& OutLogPrefix(TTempBufOutput& b)
{
    b << TInstant::Now().ToString() << " [" << LogPrefix << "] ";
    return b;
}

void LogEntry(ELogPriority priority, const TTempBuf& b)
{
    Logger->Write(priority, b.Data(), b.Filled());
}
