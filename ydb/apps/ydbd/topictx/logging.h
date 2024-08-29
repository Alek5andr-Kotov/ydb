#pragma once

#include "parameters.h"

#include <library/cpp/logger/backend.h>
#include <library/cpp/logger/priority.h>
#include <util/memory/tempbuf.h>
#include <util/stream/tempbuf.h>

void OpenLog(const TParameters& params);
void CloseLog();

THolder<TLogBackend> GetLogBackend();
ELogPriority GetLogPriority(const TParameters& params);

void SetLogPrefix(TString v);
TTempBufOutput& OutLogPrefix(TTempBufOutput& b);

void LogEntry(ELogPriority priority, const TTempBuf& b);

#define LOG_E(e) { TTempBufOutput b; OutLogPrefix(b) << e << "\n"; LogEntry(TLOG_ERR,     b); }
#define LOG_W(e) { TTempBufOutput b; OutLogPrefix(b) << e << "\n"; LogEntry(TLOG_WARNING, b); }
#define LOG_I(e) { TTempBufOutput b; OutLogPrefix(b) << e << "\n"; LogEntry(TLOG_INFO,    b); }
#define LOG_D(e) { TTempBufOutput b; OutLogPrefix(b) << e << "\n"; LogEntry(TLOG_DEBUG,   b); }
