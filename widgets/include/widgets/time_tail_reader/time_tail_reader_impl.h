#pragma once

#include <opendaq/data_packet_ptr.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/reader_config_ptr.h>
#include <opendaq/reader_impl.h>
#include <opendaq/tail_reader.h>
#include <opendaq/tail_reader_status_ptr.h>

#include <deque>
#include <optional>

BEGIN_NAMESPACE_OPENDAQ

struct TimeTailReaderInfo;

class TimeTailReaderImpl final : public ReaderImpl<ITailReader>
{
public:
    using Super = ReaderImpl<ITailReader>;

    TimeTailReaderImpl(ISignal* signal,
                       Float historyDurationSeconds,
                       SampleType valueReadType,
                       SampleType domainReadType,
                       ReadMode mode,
                       Bool skipEvents = false);

    TimeTailReaderImpl(IInputPortConfig* port,
                       Float historyDurationSeconds,
                       SampleType valueReadType,
                       SampleType domainReadType,
                       ReadMode mode,
                       Bool skipEvents = false);

    ErrCode INTERFACE_FUNC getAvailableCount(SizeT* count) override;
    ErrCode INTERFACE_FUNC getEmpty(Bool* empty) override;
    ErrCode INTERFACE_FUNC getHistorySize(SizeT* size) override;

    ErrCode INTERFACE_FUNC read(void* values, SizeT* count, ITailReaderStatus** status) override;
    ErrCode INTERFACE_FUNC readWithDomain(void* values,
                                          void* domain,
                                          SizeT* count,
                                          ITailReaderStatus** status) override;

    ErrCode INTERFACE_FUNC packetReceived(IInputPort* port) override;

    Float getHistoryDurationSeconds() const noexcept { return historyDurationSeconds; }
    void setHistoryDurationSeconds(Float seconds);

private:
    ErrCode readPacket(TimeTailReaderInfo& info, const DataPacketPtr& dataPacket);
    TailReaderStatusPtr readData(TimeTailReaderInfo& info);

    void ensureDescriptorParsedLocked();
    SizeT countSamplesInBuffer() const;
    std::optional<Float> getPacketStartSeconds(const PacketPtr& packet) const;
    void trimOldPackets();
    Bool hasSufficientHistory() const;

    Float historyDurationSeconds;
    std::deque<PacketPtr> packets;
    EventPacketPtr pendingDescriptorEvent;
    bool descriptorDirty = true;
};

END_NAMESPACE_OPENDAQ
