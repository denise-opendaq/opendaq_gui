#include "widgets/signal_descriptor_widget.h"
#include "property_object_protected_ptr.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"

#include <QVBoxLayout>
#include <cstdint>
#include <opendaq/opendaq.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <string>

// Property name constants

static constexpr const char* K_UNIT_SYMBOL  = "Symbol";
static constexpr const char* K_UNIT_NAME    = "Name";
static constexpr const char* K_UNIT_QTY     = "Quantity";
static constexpr const char* K_UNIT_ID      = "ID";

static constexpr const char* K_RANGE_LOW    = "Low";
static constexpr const char* K_RANGE_HIGH   = "High";

static constexpr const char* K_NAME         = "Name";
static constexpr const char* K_SAMPLE_TYPE  = "Sample Type";
static constexpr const char* K_SAMPLE_SIZE  = "Sample Size";
static constexpr const char* K_RULE         = "Rule";
static constexpr const char* K_UNIT         = "Unit";
static constexpr const char* K_RESOLUTION   = "Tick Resolution";
static constexpr const char* K_ORIGIN       = "Origin";
static constexpr const char* K_RANGE        = "Range";
static constexpr const char* K_DIMENSIONS   = "Dimensions";
static constexpr const char* K_STRUCT_FIELDS = "Struct Fields";

static daq::PropertyObjectPtr createDescriptorObject()
{
    static const auto ruleValues = daq::Dict<daq::IInteger, daq::IString>({
        {static_cast<int>(daq::DataRuleType::Other), "Other"},
        {static_cast<int>(daq::DataRuleType::Linear), "Linear"},
        {static_cast<int>(daq::DataRuleType::Constant), "Constant"},
        {static_cast<int>(daq::DataRuleType::Explicit), "Explicit"}
    });

    static const auto sampleTypeValues = daq::Dict<daq::IInteger, daq::IString>({
        {static_cast<uint32_t>(daq::SampleType::Invalid), "Invalid"},
        {static_cast<uint32_t>(daq::SampleType::Undefined), "Undefined"},
        {static_cast<uint32_t>(daq::SampleType::Float32), "Float32"},
        {static_cast<uint32_t>(daq::SampleType::Float64), "Float64"},
        {static_cast<uint32_t>(daq::SampleType::UInt8), "UInt8"},
        {static_cast<uint32_t>(daq::SampleType::Int8), "Int8"},
        {static_cast<uint32_t>(daq::SampleType::UInt16), "UInt16"},
        {static_cast<uint32_t>(daq::SampleType::Int16), "Int16"},
        {static_cast<uint32_t>(daq::SampleType::UInt32), "UInt32"},
        {static_cast<uint32_t>(daq::SampleType::Int32), "Int32"},
        {static_cast<uint32_t>(daq::SampleType::UInt64), "UInt64"},
        {static_cast<uint32_t>(daq::SampleType::Int64), "Int64"},
        {static_cast<uint32_t>(daq::SampleType::RangeInt64), "RangeInt64"},
        {static_cast<uint32_t>(daq::SampleType::ComplexFloat32), "ComplexFloat32"},
        {static_cast<uint32_t>(daq::SampleType::ComplexFloat64), "ComplexFloat64"},
        {static_cast<uint32_t>(daq::SampleType::Binary), "Binary"},
        {static_cast<uint32_t>(daq::SampleType::String), "String"},
        {static_cast<uint32_t>(daq::SampleType::Struct), "Struct"},
        {static_cast<uint32_t>(daq::SampleType::Null), "Null"}
    });

    auto unit = daq::PropertyObject();
    unit.addProperty(daq::StringPropertyBuilder(K_UNIT_SYMBOL, "—").setReadOnly(true).build());
    unit.addProperty(daq::StringPropertyBuilder(K_UNIT_NAME, "—").setReadOnly(true).build());
    unit.addProperty(daq::StringPropertyBuilder(K_UNIT_QTY, "—").setReadOnly(true).build());
    unit.addProperty(daq::IntPropertyBuilder(K_UNIT_ID, -1).setReadOnly(true).build());

    auto range = daq::PropertyObject();
    range.addProperty(daq::FloatPropertyBuilder(K_RANGE_LOW, 0.0f).setReadOnly(true).build());
    range.addProperty(daq::FloatPropertyBuilder(K_RANGE_HIGH, 0.0f).setReadOnly(true).build());

    auto obj = daq::PropertyObject();
    obj.addProperty(daq::StringPropertyBuilder(K_NAME, "—").setReadOnly(true).build());
    obj.addProperty(daq::SparseSelectionPropertyBuilder(K_SAMPLE_TYPE, sampleTypeValues, static_cast<int>(daq::SampleType::Undefined)).setReadOnly(true).build());
    obj.addProperty(daq::IntPropertyBuilder(K_SAMPLE_SIZE, 0).setUnit(daq::Unit("B")).setReadOnly(true).build());
    obj.addProperty(daq::SparseSelectionPropertyBuilder(K_RULE, ruleValues, static_cast<int>(daq::DataRuleType::Other)).setReadOnly(true).build());
    obj.addProperty(daq::ObjectPropertyBuilder(K_UNIT, unit).setReadOnly(true).build());
    obj.addProperty(daq::StringPropertyBuilder(K_RESOLUTION, "—").setReadOnly(true).build());
    obj.addProperty(daq::StringPropertyBuilder(K_ORIGIN, "—").setReadOnly(true).build());
    obj.addProperty(daq::ObjectPropertyBuilder(K_RANGE, range).setReadOnly(true).build());
    obj.addProperty(daq::StringPropertyBuilder(K_DIMENSIONS, "—").setReadOnly(true).build());
    obj.addProperty(daq::StringPropertyBuilder(K_STRUCT_FIELDS, "—").setReadOnly(true).build());

    return obj;
}

// ============================================================================
// SignalDescriptorWidget
// ============================================================================

SignalDescriptorWidget::SignalDescriptorWidget(const daq::SignalPtr& sig, QWidget* parent)
    : QWidget(parent)
    , signal(sig)
    , descriptorObj(createDescriptorObject())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    view = new PropertyObjectView(descriptorObj, this);
    layout->addWidget(view);

    update();

    if (signal.assigned())
        *AppContext::DaqEvent() += daq::event(this, &SignalDescriptorWidget::onCoreEvent);
}

SignalDescriptorWidget::~SignalDescriptorWidget()
{
    if (signal.assigned())
        *AppContext::DaqEvent() -= daq::event(this, &SignalDescriptorWidget::onCoreEvent);
}

void SignalDescriptorWidget::update()
{
    daq::DataDescriptorPtr d;
    try
    { 
        d = signal.getDescriptor(); 
    } 
    catch (...) 
    {
    }

    auto prot = descriptorObj.asPtr<daq::IPropertyObjectProtected>(true);
    prot.clearProtectedPropertyValues();

    if (!d.assigned())
    {
        view->refresh();
        return;
    }

    if (const auto name = d.getName(); name.assigned())
        prot.setProtectedPropertyValue(K_NAME, name);

    prot.setProtectedPropertyValue(K_SAMPLE_TYPE, static_cast<uint32_t>(d.getSampleType()));
    prot.setProtectedPropertyValue(K_SAMPLE_SIZE, d.getSampleSize());

    if (const auto rule = d.getRule(); rule.assigned())
        prot.setProtectedPropertyValue(K_RULE, static_cast<int>(rule.getType()));

    if (const auto unit = d.getUnit(); unit.assigned())
    {
        daq::PropertyObjectProtectedPtr unitProt = descriptorObj.getPropertyValue(K_UNIT);
        unitProt.setProtectedPropertyValue(K_UNIT_SYMBOL, unit.getSymbol());
        unitProt.setProtectedPropertyValue(K_UNIT_NAME, unit.getName());
        unitProt.setProtectedPropertyValue(K_UNIT_QTY, unit.getQuantity());
        unitProt.setProtectedPropertyValue(K_UNIT_ID, unit.getId());
    }

    if (const auto res = d.getTickResolution(); res.assigned())
        prot.setProtectedPropertyValue(K_RESOLUTION, res.toString());

    if (const auto origin = d.getOrigin(); origin.assigned() && origin.getLength() > 0)
        prot.setProtectedPropertyValue(K_ORIGIN, origin);

    if (const auto range = d.getValueRange(); range.assigned())
    {
        daq::PropertyObjectProtectedPtr rangeProt = descriptorObj.getPropertyValue(K_RANGE);
        if (const auto low = range.getLowValue(); low.assigned())
            rangeProt.setProtectedPropertyValue(K_RANGE_LOW, low);

        if (const auto high = range.getHighValue(); high.assigned())
            rangeProt.setProtectedPropertyValue(K_RANGE_HIGH, high);
    }

    if (const auto dims = d.getDimensions(); dims.assigned() && dims.getCount() > 0)
    {
        std::string s;
        for (const auto& dim : dims)
        {
            if (!s.empty()) s += ", ";
            try { s += std::to_string(dim.getSize()); } catch (...) { s += "?"; }
        }
        prot.setProtectedPropertyValue(K_DIMENSIONS, s);
    }

    if (const auto fields = d.getStructFields(); fields.assigned())
    {
        std::string s;
        for (const daq::DataDescriptorPtr& f : fields)
        {
            if (!s.empty()) s += ", ";
            if (f.assigned() && f.getName().assigned())
                s += f.getName().toStdString();;
        }
        prot.setProtectedPropertyValue(K_STRUCT_FIELDS, s);
    }

    view->refresh();
}

void SignalDescriptorWidget::onCoreEvent(daq::ComponentPtr& sender, daq::CoreEventArgsPtr& args)
{
    if (sender != signal)
        return;

    try
    {
        if (static_cast<daq::CoreEventId>(args.getEventId()) == daq::CoreEventId::DataDescriptorChanged)
            update();
    }
    catch (...) {}
}
