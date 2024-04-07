#pragma once

#include <fep3/base/sample/data_sample_intf.h>
#include <fep3/base/stream_type/stream_type_intf.h>
#include <fep3/components/simulation_bus/simulation_bus_intf.h>
#include <fep3/components/logging/logger_intf.h>

#include "fast_dds_include.h"

std::shared_ptr<fep3::IStreamType> createStreamType(const fep3::ddstypes::StreamType& dds_streamtype, const SampleInfo& sample_info);
std::shared_ptr<fep3::IDataSample> createSample(const fep3::ddstypes::BusData& dds_sample, const SampleInfo& sample_info);

class StreamItemTopic;

class ReaderItemQueue final
    : public DataReaderListener
{
public:
    ReaderItemQueue
    (const std::shared_ptr<fep3::ILogger>& logger
        , const std::shared_ptr<StreamItemTopic>& topic
    );
    ~ReaderItemQueue();
    size_t size() const;
    size_t capacity() const;
    bool pop(fep3::ISimulationBus::IDataReceiver& receiver);

    void setRecreateWaitSetCondition(const std::function<void()>& release_reader_conditions);

    fep3::Optional<fep3::Timestamp> getFrontTime() const;

    void createReader();
    ReadCondition* createSampleReadCondition(const std::shared_ptr<fep3::ISimulationBus::IDataReceiver>& receiver);
    ReadCondition* createStreamTypeReadCondition(const std::shared_ptr<fep3::ISimulationBus::IDataReceiver>& receiver);
    void ReaderItemQueue::wait_for_reader_connected(std::chrono::milliseconds timeout);

protected:
    void on_data_available(
        DataReader& reader);

    void on_requested_deadline_missed(
        DataReader& reader,
        const RequestedDeadlineMissedStatus& status);

    void on_requested_incompatible_qos(
        DataReader& reader,
        const RequestedIncompatibleQosStatus& status);

    void on_sample_rejected(
        DataReader& reader,
        const SampleRejectedStatus& status);

    void on_liveliness_changed(
        DataReader& reader,
        const LivelinessChangedStatus& status);

    void on_subscription_matched(
        DataReader& reader,
        const SubscriptionMatchedStatus& status);

    void on_sample_lost(
        DataReader& reader,
        const SampleLostStatus& status);

//private:
    public:
    bool popFrom
    (DataReader& sample_reader
        , DataReader& streamtype_reader
        , const Subscriber& subscriber
        , fep3::ISimulationBus::IDataReceiver& receiver
    );
    bool popFrom(const std::shared_ptr<fep3::ISimulationBus::IDataReceiver>& receiver);
    void logError(const fep3::Result& res) const;

private:
    std::shared_ptr<fep3::ILogger> _logger;
    std::shared_ptr<StreamItemTopic> _topic;
    eprosima::fastdds::dds::Subscriber* _subscriber = nullptr;

    std::function<void()> _release_reader_conditions;
    DataReader* _sample_reader = nullptr;
    DataReader* _streamtype_reader = nullptr;

};
