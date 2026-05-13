#include "widgets/signal_descriptor_widget.h"
#include "property_object_protected_ptr.h"
#include "widgets/property_object_view.h"
#include "context/AppContext.h"
#include "context/QueuedEventHandler.h"

#include <QVBoxLayout>
#include <opendaq/opendaq.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

static std::string sampleTypeToStr(daq::SampleType t)
{
    switch (t)
    {
        case daq::SampleType::Float32:        return "Float32";
        case daq::SampleType::Float64:        return "Float64";
        case daq::SampleType::UInt8:          return "UInt8";
        case daq::SampleType::Int8:           return "Int8";
        case daq::SampleType::UInt16:         return "UInt16";
        case daq::SampleType::Int16:          return "Int16";
        case daq::SampleType::UInt32:         return "UInt32";
        case daq::SampleType::Int32:          return "Int32";
        case daq::SampleType::UInt64:         return "UInt64";
        case daq::SampleType::Int64:          return "Int64";
        case daq::SampleType::RangeInt64:     return "RangeInt64";
        case daq::SampleType::ComplexFloat32: return "ComplexFloat32";
        case daq::SampleType::ComplexFloat64: return "ComplexFloat64";
        case daq::SampleType::Binary:         return "Binary";
        case daq::SampleType::String:         return "String";
        case daq::SampleType::Struct:         return "Struct";
        case daq::SampleType::Null:           return "Null";
        default:                              return "Unknown";
    }
}

static std::string dataRuleTypeToStr(daq::DataRuleType t)
{
    switch (t)
    {
        case daq::DataRuleType::Linear:   return "Linear";
        case daq::DataRuleType::Constant: return "Constant";
        case daq::DataRuleType::Explicit: return "Explicit";
        default:                          return "Other";
    }
}

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
    auto add = [&](const daq::PropertyObjectPtr& obj, const char* name)
    {
        obj.addProperty(daq::StringPropertyBuilder(name, "—").setReadOnly(true).build());
    };

    auto unit = daq::PropertyObject();
    add(unit, K_UNIT_SYMBOL);
    add(unit, K_UNIT_NAME);
    add(unit, K_UNIT_QTY);
    add(unit, K_UNIT_ID);

    auto range = daq::PropertyObject();
    add(range, K_RANGE_LOW);
    add(range, K_RANGE_HIGH);

    auto obj = daq::PropertyObject();
    add(obj, K_NAME);
    add(obj, K_SAMPLE_TYPE);
    add(obj, K_SAMPLE_SIZE);
    add(obj, K_RULE);
    obj.addProperty(daq::ObjectPropertyBuilder(K_UNIT, unit).setReadOnly(true).build());
    add(obj, K_RESOLUTION);
    add(obj, K_ORIGIN);
    obj.addProperty(daq::ObjectPropertyBuilder(K_RANGE, range).setReadOnly(true).build());
    add(obj, K_DIMENSIONS);
    add(obj, K_STRUCT_FIELDS);

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
    auto prot = descriptorObj.asPtr<daq::IPropertyObjectProtected>(true);
    auto set   = [&](const daq::PropertyObjectProtectedPtr& prot, const char* name, const std::string& value)
    {
        try { prot.setProtectedPropertyValue(name, daq::String(value)); } catch (...) {}
    };

    daq::DataDescriptorPtr d;
    try
    { 
        d = signal.getDescriptor(); 
    } 
    catch (...) 
    {
    }

    descriptorObj.clearPropertyValues();

    if (!d.assigned())
    {
        view->refresh();
        return;
    }

    if (const auto name = d.getName(); name.assigned())
        set(prot, K_NAME, name);

    set(prot, K_SAMPLE_TYPE, sampleTypeToStr(d.getSampleType()));
    set(prot, K_SAMPLE_SIZE, std::to_string(d.getSampleSize()) + " bytes");

    if (const auto rule = d.getRule(); rule.assigned())
        set(prot, K_RULE, dataRuleTypeToStr(rule.getType()));

    if (const auto unit = d.getUnit(); unit.assigned())
    {
        daq::PropertyObjectProtectedPtr unitProt = descriptorObj.getPropertyValue(K_UNIT);

        if (const auto symbol = unit.getSymbol(); symbol.assigned())
            set(unitProt, K_UNIT_SYMBOL, symbol);

        if (const auto name = unit.getName(); name.assigned())
            set(unitProt, K_UNIT_NAME, name);

        if (const auto quantity = unit.getQuantity(); quantity.assigned())
            set(unitProt, K_UNIT_QTY, quantity);

        set(unitProt, K_UNIT_QTY, std::to_string(unit.getId()));
    }

    if (const auto res = d.getTickResolution(); res.assigned())
        set(prot, K_RESOLUTION, res.toString());

    if (const auto origin = d.getOrigin(); origin.assigned())
        set(prot, K_ORIGIN, origin);

    if (const auto range = d.getValueRange(); range.assigned())
    {
        daq::PropertyObjectProtectedPtr rangeProp = descriptorObj.getPropertyValue(K_RANGE);
        if (const auto low = range.getLowValue(); low.assigned())
            set(rangeProp, K_RANGE_LOW, low);

        if (const auto high = range.getLowValue(); high.assigned())
            set(rangeProp, K_RANGE_HIGH, high);
    }

    if (const auto dims = d.getDimensions(); dims.assigned() && dims.getCount() > 0)
    {
        std::string s;
        for (const auto& dim : dims)
        {
            if (!s.empty()) s += ", ";
            try { s += std::to_string(dim.getSize()); } catch (...) { s += "?"; }
        }
        set(prot, K_DIMENSIONS, s);
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
        set(prot, K_STRUCT_FIELDS, s);
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
