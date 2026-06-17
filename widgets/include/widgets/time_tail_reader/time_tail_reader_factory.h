#pragma once

#include <opendaq/signal_ptr.h>
#include <widgets/time_tail_reader/time_tail_reader_ptr.h>

BEGIN_NAMESPACE_OPENDAQ

TimeTailReaderPtr TimeTailReader(SignalPtr signal,
                                 Float historyDurationSeconds,
                                 SampleType valueReadType = SampleType::Float64,
                                 SampleType domainReadType = SampleType::Int64,
                                 ReadMode mode = ReadMode::Scaled);

TimeTailReaderPtr TimeTailReaderFromPort(InputPortConfigPtr port,
                                         Float historyDurationSeconds,
                                         SampleType valueReadType = SampleType::Float64,
                                         SampleType domainReadType = SampleType::Int64,
                                         ReadMode mode = ReadMode::Scaled);

END_NAMESPACE_OPENDAQ
