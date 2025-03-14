#pragma once

#include "restore_impl.h"

namespace NYdb::NDump {

NPrivate::IDataAccumulator* CreateCompatAccumulator(
    const TString& path,
    const NTable::TTableDescription& desc,
    const TRestoreSettings& settings);

NPrivate::IDataWriter* CreateCompatWriter(
    const TString& path,
    NTable::TTableClient& tableClient,
    const NPrivate::IDataAccumulator* accumulator,
    const TRestoreSettings& settings);

} // NYdb::NDump
