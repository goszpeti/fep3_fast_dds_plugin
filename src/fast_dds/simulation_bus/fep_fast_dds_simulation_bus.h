#pragma once

#include "fast_dds_include.h"

#include <fep3/base/properties/propertynode.h>
#include <fep3/components/base/component.h>
#include <fep3/components/logging/logging_service_intf.h>
#include <fep3/components/simulation_bus/simulation_bus_intf.h>

using namespace fep3;

#if defined _WIN32
#ifdef BUILD_SIMBUS
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif
#else
#define EXPORTED
#endif

class EXPORTED FastDDSSimulationBus : public fep3::base::Component<fep3::ISimulationBus> {
public:
    FastDDSSimulationBus();
    ~FastDDSSimulationBus();
    FastDDSSimulationBus(const FastDDSSimulationBus&) = delete;
    FastDDSSimulationBus(FastDDSSimulationBus&&) = delete;
    FastDDSSimulationBus& operator=(const FastDDSSimulationBus&) = delete;
    FastDDSSimulationBus& operator=(FastDDSSimulationBus&&) = delete;

public: // the base::Component statemachine
    fep3::Result create() override;
    fep3::Result destroy() override;
    fep3::Result initialize() override;
    fep3::Result deinitialize() override;

public: // the arya SimulationBus interface
    bool isSupported(const IStreamType& stream_type) const override;

    std::unique_ptr<IDataReader> getReader(const std::string& name,
                                           const IStreamType& stream_type) override;

    std::unique_ptr<IDataReader> getReader(const std::string& name,
                                           const IStreamType& stream_type,
                                           size_t queue_capacity) override;

    std::unique_ptr<IDataReader> getReader(const std::string& name) override;
    std::unique_ptr<IDataReader> getReader(const std::string& name, size_t queue_capacity) override;
    std::unique_ptr<IDataWriter> getWriter(const std::string& name,
                                           const IStreamType& stream_type) override;
    std::unique_ptr<IDataWriter> getWriter(const std::string& name,
                                           const IStreamType& stream_type,
                                           size_t queue_capacity) override;
    std::unique_ptr<IDataWriter> getWriter(const std::string& name) override;
    std::unique_ptr<IDataWriter> getWriter(const std::string& name, size_t queue_capacity) override;

    void startBlockingReception(
        const std::function<void()>& reception_preparation_done_callback) override;
    void stopBlockingReception() override;

private:
    class FastDDSSimulationBusConfiguration : public fep3::base::Configuration {
    public:
        FastDDSSimulationBusConfiguration();
        ~FastDDSSimulationBusConfiguration() = default;

    public:
        fep3::Result registerPropertyVariables() override;
        fep3::Result unregisterPropertyVariables() override;

    public:
        fep3::base::PropertyVariable<int32_t> _participant_domain{5};
    };

public:
    class Impl;
    std::unique_ptr<Impl> _impl;

    void logError(const fep3::Result& res);

    FastDDSSimulationBusConfiguration _simulation_bus_configuration;
};
