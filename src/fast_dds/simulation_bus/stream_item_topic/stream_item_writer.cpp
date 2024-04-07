#include "../vector_raw_memory.hpp"
#include "../converter.h"

#include "stream_item_writer.h"


using namespace fep3;

StreamItemDataWriter::StreamItemDataWriter(const std::shared_ptr<StreamItemTopic>& topic
    , size_t /*queue_capacity*/
    )
    : _topic(topic)
{
    PublisherQos pqos = PUBLISHER_QOS_DEFAULT;
    //topic->getDomainParticipant().get_publisher_qos_from_profile(topic->getQosProfile(), pqos);

    // We can use the FEP system name as a partition
    std::vector<std::string> partitions = {topic->getSystemName().c_str()};
    pqos.partition().setNames(partitions);

    _publisher = topic->getDomainParticipant().create_publisher(pqos, nullptr);

    createWriter();
    DataWriterQos wqos;
    _publisher->get_datawriter_qos_from_profile(std::string(FEP3_QOS_STREAM_TYPE) + "::writer", wqos);
    auto stw = _publisher->create_datawriter(topic->getStreamTypeTopic(), wqos);

    _stream_type_writer = stw; // <fep3::ddstypes::StreamType>
}

StreamItemDataWriter::~StreamItemDataWriter()
{
    _stream_type_writer->close();
    _sample_writer->close();
    _publisher->delete_contained_entities();
}

struct BytesTopicTypeRawMemory : public fep3::IRawMemory
{

public:
    BytesTopicTypeRawMemory(fep3::ddstypes::BusData& value) : _value(value)
    {
    }
    size_t capacity() const override
    {
        return _value.data().capacity();
    }
    const void* cdata() const override
    {
        return nullptr;
    }
    size_t size() const override
    {
        return _value.data().size();
    }
    size_t set(const void* data, size_t data_size) override
    {
        std::vector<uint8_t> buffer(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + data_size);
        _value.data(buffer);
        return data_size;
    }
    size_t resize(size_t data_size) override
    {
        return data_size;
    }

private:
    fep3::ddstypes::BusData& _value;

};

void StreamItemDataWriter::createWriter()
{
    if (_sample_writer)
    {
        _sample_writer->close();
    }
    DataWriterQos wqos;
    _publisher->get_datawriter_qos_from_profile(_topic->getQosProfile() + "::writer", wqos);
    auto sw = _publisher->create_datawriter(_topic->getSampleTopic(), wqos);

    _sample_writer = sw;
}

fep3::Result StreamItemDataWriter::write(const IDataSample& data_sample)
{
    try
    {
        fep3::ddstypes::BusData sample;
        BytesTopicTypeRawMemory raw_memory(sample);
        data_sample.read(raw_memory);

        auto ret = _sample_writer->write_w_timestamp(&sample, HANDLE_NIL , convertTimestamp(data_sample.getTime()));

        PublicationMatchedStatus status ;
        _sample_writer->get_publication_matched_status(status);
        if (status.total_count == 0 ){
            std::cout << "No FEP writer match" << std::endl;

        }
        if (!ret)
        {
            std::cout << "Error while writing" << std::endl;
        }

        return {};
    }
    catch (eprosima::fastcdr::exception::Exception& exception)
    {
        return convertExceptionToResult(exception);
    }

}

fep3::Result StreamItemDataWriter::write(const IStreamType& stream_type)
{
    try
    {
        fep3::ddstypes::StreamType dds_stream_type;
        dds_stream_type.metatype(stream_type.getMetaTypeName());

        for (auto property_name : stream_type.getPropertyNames())
        {
            auto prop = fep3::ddstypes::Property();
            prop.name(property_name);
            prop.type(stream_type.getPropertyType(property_name));
            prop.value(stream_type.getProperty(property_name));

            dds_stream_type.properties().push_back(prop);
        }

        if (_topic->updateStreamType(stream_type))
        {
            createWriter();
        }

        return {};
    }
    catch (eprosima::fastcdr::exception::Exception& exception)
    {
        return convertExceptionToResult(exception);
    }
}

fep3::Result StreamItemDataWriter::transmit()
{
    try
    {
        return {};
    }
    catch (eprosima::fastcdr::exception::Exception& exception)
    {
        return convertExceptionToResult(exception);
    }

}

void StreamItemDataWriter::on_offered_deadline_missed(
    DataWriter* /*writer*/,
    const OfferedDeadlineMissedStatus& /*status*/)
{
    //std::cout << "           on_offered_deadline_missed" << std::endl;
}
void StreamItemDataWriter::on_offered_incompatible_qos(
    DataWriter* /*writer*/,
    const OfferedIncompatibleQosStatus& /*status*/)
{
    //std::cout << "           on_offered_incompatible_qos" << std::endl;
}
void StreamItemDataWriter::on_liveliness_lost(
    DataWriter* /*writer*/,
    const LivelinessLostStatus& /*status*/)
{
    //std::cout << "           on_liveliness_lost" << std::endl;
}
void StreamItemDataWriter::on_publication_matched(
    DataWriter* /*writer*/,
    const PublicationMatchedStatus& /*status*/)
{
    //std::cout << "           on_publication_matched" << std::endl;
}
