#include <widgets/time_tail_reader/time_tail_reader_impl.h>
#include <widgets/time_tail_reader/time_tail_reader_factory.h>
#include <widgets/time_tail_reader/time_tail_reader_ptr.h>

#include <coretypes/impl.h>
#include <opendaq/event_packet_utils.h>
#include <opendaq/reader_errors.h>
#include <opendaq/reader_factory.h>

#include <algorithm>

BEGIN_NAMESPACE_OPENDAQ

struct TimeTailReaderInfo
{
    void* values{};
    void* domainValues{};
    SizeT remainingToRead{};

    SizeT offset{};
};

namespace
{

TimeTailReaderImpl* asTimeTailReaderImpl(const TailReaderPtr& reader)
{
    if (!reader.assigned())
        return nullptr;

    const auto config = ReaderConfigPtr::Borrow(reader);
    return dynamic_cast<TimeTailReaderImpl*>(config.getObject());
}

}  // namespace

TimeTailReaderImpl* TimeTailReaderPtr::impl() const
{
    return asTimeTailReaderImpl(reader_);
}

Float TimeTailReaderPtr::getHistoryDuration() const
{
    const auto* timeImpl = impl();
    if (timeImpl == nullptr)
        DAQ_THROW_EXCEPTION(InvalidParameterException);
    return timeImpl->getHistoryDurationSeconds();
}

void TimeTailReaderPtr::setHistoryDuration(const Float seconds) const
{
    auto* timeImpl = impl();
    if (timeImpl == nullptr)
        DAQ_THROW_EXCEPTION(InvalidParameterException);
    timeImpl->setHistoryDurationSeconds(seconds);
}

TimeTailReaderImpl::TimeTailReaderImpl(ISignal* signal,
                                       Float historyDurationSeconds,
                                       SampleType valueReadType,
                                       SampleType domainReadType,
                                       ReadMode mode,
                                       Bool skipEvents)
    : Super(SignalPtr(signal), mode, valueReadType, domainReadType, skipEvents)
    , historyDurationSeconds(historyDurationSeconds)
{
    try
    {
        port.setNotificationMethod(PacketReadyNotification::SameThread);
        packetReceived(port.as<IInputPort>(true));
    }
    catch (...)
    {
        this->releaseWeakRefOnException();
        throw;
    }
}

TimeTailReaderImpl::TimeTailReaderImpl(IInputPortConfig* port,
                                       Float historyDurationSeconds,
                                       SampleType valueReadType,
                                       SampleType domainReadType,
                                       ReadMode mode,
                                       Bool skipEvents)
    : Super(InputPortConfigPtr(port), mode, valueReadType, domainReadType, skipEvents)
    , historyDurationSeconds(historyDurationSeconds)
{
    try
    {
        this->port.setNotificationMethod(PacketReadyNotification::Scheduler);
        if (connection.assigned())
            packetReceived(this->port.as<IInputPort>(true));
    }
    catch (...)
    {
        this->releaseWeakRefOnException();
        throw;
    }
}

SizeT TimeTailReaderImpl::countSamplesInBuffer() const
{
    SizeT total = 0;
    for (const auto& packet : packets)
    {
        if (packet.getType() != PacketType::Data)
            continue;

        const auto dataPacket = packet.asPtr<IDataPacket>(true);
        if (dataPacket.assigned())
            total += dataPacket.getSampleCount();
    }
    return total;
}

void TimeTailReaderImpl::ensureDescriptorParsedLocked()
{
    if (!descriptorDirty)
        return;

    if (pendingDescriptorEvent.assigned())
        Super::handleDescriptorChanged(pendingDescriptorEvent);

    descriptorDirty = false;
}

std::optional<Float> TimeTailReaderImpl::getPacketStartSeconds(const PacketPtr& packet) const
{
    if (packet.getType() != PacketType::Data)
        return std::nullopt;

    const auto dataPacket = packet.asPtr<IDataPacket>(true);
    if (!dataPacket.assigned())
        return std::nullopt;

    const auto domainPacket = dataPacket.getDomainPacket();
    if (!domainPacket.assigned())
        return std::nullopt;

    const auto desc = domainPacket.getDataDescriptor();
    auto tickResolution = desc.getTickResolution();
    if (!tickResolution.assigned())
        tickResolution = Ratio(1, 1);

    const auto rule = desc.getRule();
    if (rule.assigned() && rule.getType() == DataRuleType::Explicit)
    {
        Float tick = domainPacket.getValueByIndex(0);
        return Ratio(tick, 1) * tickResolution;
    }
    
    const auto offset = domainPacket.getOffset();
    if (!offset.assigned())
        return std::nullopt;

    return Ratio(offset.getIntValue(), 1) * tickResolution;
}

void TimeTailReaderImpl::trimOldPackets()
{
    if (packets.size() < 2)
        return;

    const auto tLatest = getPacketStartSeconds(packets.back());
    const auto tFirst = getPacketStartSeconds(packets.front());

    if (!tLatest.has_value())
        return;

    const Float cutoff = *tLatest - historyDurationSeconds;

    const auto firstKeep = std::lower_bound(packets.begin(),
                                            packets.end(),
                                            cutoff,
                                            [this](const PacketPtr& packet, Float cut) {
                                                const auto t = getPacketStartSeconds(packet);
                                                return t.has_value() && *t < cut;
                                            });

    if (firstKeep != packets.begin())
        packets.erase(packets.begin(), firstKeep);
}

Bool TimeTailReaderImpl::hasSufficientHistory() const
{
    if (packets.empty())
        return false;

    const auto tFront = getPacketStartSeconds(packets.front());
    const auto tBack = getPacketStartSeconds(packets.back());
    if (!tFront.has_value() || !tBack.has_value())
        return !packets.empty();

    return (*tBack - *tFront) >= historyDurationSeconds;
}

void TimeTailReaderImpl::setHistoryDurationSeconds(const Float seconds)
{
    if (seconds <= 0.0)
        DAQ_THROW_EXCEPTION(InvalidParameterException, "History duration must be positive");

    std::unique_lock lock(mutex);
    historyDurationSeconds = seconds;
    trimOldPackets();
}

ErrCode TimeTailReaderImpl::getAvailableCount(SizeT* count)
{
    OPENDAQ_PARAM_NOT_NULL(count);

    std::unique_lock lock(mutex);
    *count = countSamplesInBuffer();
    return OPENDAQ_SUCCESS;
}

ErrCode TimeTailReaderImpl::getEmpty(Bool* empty)
{
    OPENDAQ_PARAM_NOT_NULL(empty);

    SizeT count = 0;
    getAvailableCount(&count);
    *empty = count == 0;
    return OPENDAQ_SUCCESS;
}

ErrCode TimeTailReaderImpl::getHistorySize(SizeT* size)
{
    OPENDAQ_PARAM_NOT_NULL(size);

    std::unique_lock lock(mutex);
    *size = countSamplesInBuffer();
    return OPENDAQ_SUCCESS;
}

ErrCode TimeTailReaderImpl::readPacket(TimeTailReaderInfo& info, const DataPacketPtr& dataPacket)
{
    const SizeT sampleCount = dataPacket.getSampleCount();
    if (info.offset > sampleCount)
    {
        info.offset -= sampleCount;
        return OPENDAQ_SUCCESS;
    }

    const auto remainingSampleCount = sampleCount - info.offset;
    const SizeT toRead = std::min(info.remainingToRead, remainingSampleCount);

    ErrCode errCode = valueReader->readData(getValuePacketData(dataPacket), info.offset, &info.values, toRead);
    OPENDAQ_RETURN_IF_FAILED(errCode);

    if (info.domainValues != nullptr)
    {
        auto domainPacket = dataPacket.getDomainPacket();
        errCode = domainReader->readData(domainPacket.getData(), info.offset, &info.domainValues, toRead);
        if (errCode == OPENDAQ_ERR_INVALIDSTATE)
        {
            if (!trySetDomainSampleType(domainPacket))
                return DAQ_EXTEND_ERROR_INFO(errCode, "Failed to set domain sample type for packet");

            daqClearErrorInfo();
            errCode = domainReader->readData(domainPacket.getData(), info.offset, &info.domainValues, toRead);
        }

        OPENDAQ_RETURN_IF_FAILED(errCode);
    }

    info.offset = 0;
    info.remainingToRead -= toRead;
    return OPENDAQ_SUCCESS;
}

TailReaderStatusPtr TimeTailReaderImpl::readData(TimeTailReaderInfo& info)
{
    std::unique_lock lock(mutex);
    ensureDescriptorParsedLocked();

    const SizeT available = countSamplesInBuffer();
    if (info.remainingToRead > available)
        return TailReaderStatus(nullptr, !invalid, 0, false);

    if (available > info.remainingToRead)
        info.offset = available - info.remainingToRead;

    NumberPtr offset;
    for (const auto& packet : packets)
    {
        const auto dataPacket = packet.asPtrOrNull<IDataPacket>(true);
        if (!dataPacket.assigned())
            continue;

        if (!offset.assigned() && info.offset < dataPacket.getSampleCount())
            offset = calculateOffset(dataPacket, info.offset);

        readPacket(info, packet);
    }

    return TailReaderStatus(nullptr, !invalid, offset, hasSufficientHistory());
}

ErrCode TimeTailReaderImpl::read(void* values, SizeT* count, ITailReaderStatus** status)
{
    OPENDAQ_PARAM_NOT_NULL(count);
    if (*count != 0)
        OPENDAQ_PARAM_NOT_NULL(values);

    if (invalid)
    {
        if (status != nullptr)
            *status = TailReaderStatus(nullptr, false).detach();
        *count = 0;
        return OPENDAQ_IGNORED;
    }

    TimeTailReaderInfo info{values, nullptr, *count};

    auto statusPtr = readData(info);
    if (status != nullptr)
        *status = statusPtr.detach();

    *count = *count - info.remainingToRead;
    return OPENDAQ_SUCCESS;
}

ErrCode TimeTailReaderImpl::readWithDomain(void* values, void* domain, SizeT* count, ITailReaderStatus** status)
{
    OPENDAQ_PARAM_NOT_NULL(count);
    if (*count != 0)
    {
        OPENDAQ_PARAM_NOT_NULL(values);
        OPENDAQ_PARAM_NOT_NULL(domain);
    }

    if (invalid)
    {
        if (status != nullptr)
            *status = TailReaderStatus(nullptr, false).detach();
        *count = 0;
        return OPENDAQ_IGNORED;
    }

    TimeTailReaderInfo info{values, domain, *count};

    auto statusPtr = readData(info);
    if (status != nullptr)
        *status = statusPtr.detach();

    *count = *count - info.remainingToRead;
    return OPENDAQ_SUCCESS;
}

ErrCode TimeTailReaderImpl::packetReceived(IInputPort* port)
{
    std::unique_lock lock(mutex);

    const auto incoming = connection.dequeueAll();
    if (incoming.assigned() && incoming.getCount() > 0)
    {
        std::optional<Float> tLatest;
        std::vector<PacketPtr> collected;
        collected.reserve(static_cast<size_t>(incoming.getCount()));

        bool done = false;
        for (SizeT i = incoming.getCount(); i > 0 && !done; --i)
        {
            const PacketPtr packet = incoming[i - 1];

            switch (packet.getType())
            {
                case PacketType::Data:
                {
                    if (!tLatest.has_value())
                        tLatest = getPacketStartSeconds(packet);

                    if (tLatest.has_value())
                    {
                        const auto t = getPacketStartSeconds(packet);
                        if (t.has_value() && *t < *tLatest - historyDurationSeconds)
                        {
                            packets.clear();
                            done = true;
                        }
                        else
                        {
                            collected.push_back(packet);
                        }
                    }
                    else
                    {
                        collected.push_back(packet);
                    }
                    break;
                }
                case PacketType::Event:
                {
                    const auto eventPacket = packet.asPtr<IEventPacket>(true);
                    if (eventPacket.assigned()
                        && eventPacket.getEventId() == event_packet_id::DATA_DESCRIPTOR_CHANGED)
                    {
                        pendingDescriptorEvent = eventPacket;
                        descriptorDirty = true;
                    }
                    break;
                }
                case PacketType::None:
                    break;
            }
        }

        for (auto it = collected.rbegin(); it != collected.rend(); ++it)
            packets.push_back(*it);

        if (!done)
            trimOldPackets();
    }

    const auto callback = readCallback;
    const Bool sufficientHistory = hasSufficientHistory();
    lock.unlock();

    if (callback.assigned() && sufficientHistory)
        OPENDAQ_RETURN_IF_FAILED(wrapHandler(callback));

    if (externalListener.assigned() && externalListener.getRef().assigned())
        return externalListener.getRef()->packetReceived(port);

    return OPENDAQ_SUCCESS;
}

namespace
{

TailReaderPtr makeTimeTailReaderImpl(SignalPtr signal,
                                     Float historyDurationSeconds,
                                     SampleType valueReadType,
                                     SampleType domainReadType,
                                     ReadMode mode,
                                     Bool skipEvents)
{
    ITailReader* reader = nullptr;
    const auto err = createObject<ITailReader, TimeTailReaderImpl>(&reader,
                                                                 *signal,
                                                                 historyDurationSeconds,
                                                                 valueReadType,
                                                                 domainReadType,
                                                                 mode,
                                                                 skipEvents);
    checkErrorInfo(err);
    return TailReaderPtr(reader);
}

TailReaderPtr makeTimeTailReaderFromPortImpl(InputPortConfigPtr port,
                                             Float historyDurationSeconds,
                                             SampleType valueReadType,
                                             SampleType domainReadType,
                                             ReadMode mode,
                                             Bool skipEvents)
{
    ITailReader* reader = nullptr;
    const auto err = createObject<ITailReader, TimeTailReaderImpl>(&reader,
                                                                 *port,
                                                                 historyDurationSeconds,
                                                                 valueReadType,
                                                                 domainReadType,
                                                                 mode,
                                                                 skipEvents);
    checkErrorInfo(err);
    return TailReaderPtr(reader);
}

}  // namespace

TimeTailReaderPtr TimeTailReader(SignalPtr signal,
                                 Float historyDurationSeconds,
                                 SampleType valueReadType,
                                 SampleType domainReadType,
                                 ReadMode mode)
{
    return TimeTailReaderPtr(
        makeTimeTailReaderImpl(signal, historyDurationSeconds, valueReadType, domainReadType, mode, false));
}

TimeTailReaderPtr TimeTailReaderFromPort(InputPortConfigPtr port,
                                         Float historyDurationSeconds,
                                         SampleType valueReadType,
                                         SampleType domainReadType,
                                         ReadMode mode)
{
    return TimeTailReaderPtr(
        makeTimeTailReaderFromPortImpl(port, historyDurationSeconds, valueReadType, domainReadType, mode, false));
}

END_NAMESPACE_OPENDAQ
