#pragma once

#include <coretypes/coretypes.h>
#include <opendaq/tail_reader_ptr.h>

BEGIN_NAMESPACE_OPENDAQ

class TimeTailReaderImpl;

/// Smart pointer for a time-window tail reader (implemented as ITailReader).
class TimeTailReaderPtr
{
public:
    TimeTailReaderPtr() = default;

    explicit TimeTailReaderPtr(const TailReaderPtr& reader)
        : reader_(reader)
    {
    }

    bool assigned() const noexcept { return reader_.assigned(); }

    SizeT getAvailableCount() const { return reader_.getAvailableCount(); }

    TailReaderStatusPtr read(void* values, SizeT* count) const { return reader_.read(values, count); }

    TailReaderStatusPtr readWithDomain(void* values, void* domain, SizeT* count) const
    {
        return reader_.readWithDomain(values, domain, count);
    }

    Float getHistoryDuration() const;
    void setHistoryDuration(Float seconds) const;

    const TailReaderPtr& tailReader() const noexcept { return reader_; }

private:
    TimeTailReaderImpl* impl() const;

    TailReaderPtr reader_;
};

END_NAMESPACE_OPENDAQ
