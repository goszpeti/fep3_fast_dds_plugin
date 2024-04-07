#include "fep_fast_dds_simulation_bus.h"

#include "bus_info/bus_info.h"
#include "converter.h"
#include "internal_topic/internal_topic.h"
#include "reader_item_queue.h"
#include "stream_item_topic/stream_item_topic.h"

#include <a_util/filesystem.h>
#include <a_util/result.h>

#include <codecvt>
#include <cstring>
#include <fep3/base/properties/properties.h>
#include <fep3/base/stream_type/default_stream_type.h>
#include <fep3/components/configuration/configuration_service_intf.h>
#include <fep3/components/participant_info/participant_info_intf.h>
#include <fep3/components/simulation_bus/simulation_data_access.h>
#include <fep3/fep3_participant_version.h>
#include <locale>
#include <regex>
#include <vector>

#ifdef WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

using namespace fep3;

a_util::filesystem::Path getFilePath()
{
    a_util::filesystem::Path current_binary_file_path;
#ifdef WIN32
    HMODULE hModule = nullptr;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)getFilePath, &hModule)) {
        std::wstring file_path_buffer;
        DWORD number_of_copied_characters = 0;
        // note: to support paths with length > MAX_PATH we have do trial-and-error
        // because GetModuleFileName does not indicate if the path was truncated
        while (number_of_copied_characters >= file_path_buffer.size()) {
            file_path_buffer.resize(file_path_buffer.size() + MAX_PATH);
            number_of_copied_characters = GetModuleFileNameW(
                hModule, &file_path_buffer[0], static_cast<DWORD>(file_path_buffer.size()));
        }

        file_path_buffer.resize(number_of_copied_characters);
        auto size_needed = WideCharToMultiByte(
            CP_UTF8, 0, &file_path_buffer[0], (int)file_path_buffer.size(), NULL, 0, NULL, NULL);
        std::string str_utf8(size_needed, 0);
        WideCharToMultiByte(CP_UTF8,
                            0,
                            &file_path_buffer[0],
                            (int)file_path_buffer.size(),
                            &str_utf8[0],
                            size_needed,
                            NULL,
                            NULL);
        current_binary_file_path = str_utf8;
    }
#else // WIN32
    Dl_info dl_info;
    dladdr(reinterpret_cast<void*>(getFilePath), &dl_info);
    current_binary_file_path = dl_info.dli_fname;
#endif
    return current_binary_file_path.getParent();
}

class FastDDSSimulationBus::Impl {
public:
    Impl()
    {
    }

    ~Impl()
    {
        _topics.clear();
        _bus_info.reset();
    }

    void createDataAccessCollection()
    {
        _data_access_collection =
            std::make_shared<base::SimulationDataAccessCollection<ReaderItemQueue>>();
    }

    void releaseDataAccessCollection()
    {
        _data_access_collection.reset();
    }

    std::unique_ptr<ISimulationBus::IDataReader> getReader(const std::string& name,
                                                           const IStreamType& stream_type,
                                                           size_t queue_capacity = 0)
    {
        auto topic = getOrCreateTopic(name, stream_type);
        return topic->createDataReader(queue_capacity, _data_access_collection);
    }

    std::unique_ptr<ISimulationBus::IDataReader> getReader(const std::string& name,
                                                           size_t queue_capacity = 0)
    {
        auto topic = getOrCreateTopic(name, fep3::base::StreamTypeRaw());
        return topic->createDataReader(queue_capacity, _data_access_collection);
    }

    std::unique_ptr<ISimulationBus::IDataWriter> getWriter(const std::string& name,
                                                           const IStreamType& stream_type,
                                                           size_t queue_capacity = 0)
    {
        auto topic = getOrCreateTopic(name, stream_type);
        return topic->createDataWriter(queue_capacity);
    }
    std::unique_ptr<ISimulationBus::IDataWriter> getWriter(const std::string& name,
                                                           size_t queue_capacity = 0)
    {
        auto topic = getOrCreateTopic(name, fep3::base::StreamTypeRaw());
        return topic->createDataWriter(queue_capacity);
    }

    void initBusInfo(DomainParticipantQos& participant_qos, const std::string participant_name)
    {
        // Create BusInfo to collect for bus informations
        _bus_info = std::make_unique<BusInfo>();
        _bus_info->getOwnParticipantInfo()->setParticipantName(participant_name);

        BusInfo::Version version;
        version.major = FEP3_PARTICIPANT_LIBRARY_VERSION_MAJOR;
        version.minor = FEP3_PARTICIPANT_LIBRARY_VERSION_MINOR;
        version.patch = FEP3_PARTICIPANT_LIBRARY_VERSION_PATCH;
        _bus_info->getOwnParticipantInfo()->setFepVersion(version);
        _bus_info->registerUserData(&participant_qos);

        // Create built in topic to make bus informations available via ISimulationBus
        auto built_in_topic_businfo = std::make_shared<InternalTopic>("_built_in_topic_businfo");
        _topics["_built_in_topic_businfo"] = built_in_topic_businfo;
        _bus_info->setUpdateCallback([this, built_in_topic_businfo]() {
            if (_bus_info) {
                built_in_topic_businfo->write(_bus_info->asJson());
            }
        });
    }

    void startBlockingReception(const std::function<void()>& reception_preparation_done_callback)
    {
        // RAII ensuring invocation of callback exactly once (even in case exception is thrown)
        std::unique_ptr<bool, std::function<void(bool*)>>
            reception_preparation_done_callback_caller(
                nullptr, [reception_preparation_done_callback](bool* p) {
                    reception_preparation_done_callback();
                    delete p;
                });
        if (reception_preparation_done_callback) {
            reception_preparation_done_callback_caller.reset(new bool);
        }

        if (_data_access_collection) {
            const auto& data_access_collection = *_data_access_collection.get();
            {
                size_t last_size_of_data_access_collection = 0;

                WaitSet* waitset = nullptr;

                // Run until ::stop was called
                _receiving = true;
                while (_receiving) {
                    // Recreate WaitSet if:
                    // * waitset was never created
                    // * a new reader was created (_data_access_collection->size() changed)
                    // * a reader recreated internal dds reader
                    if (last_size_of_data_access_collection != data_access_collection.size() ||
                        waitset == nullptr) {
                        waitset = new WaitSet();
                        for (auto data_access_iterator = data_access_collection.cbegin();
                             data_access_iterator != data_access_collection.cend();
                             ++data_access_iterator) {
                            auto condition =
                                data_access_iterator->_item_queue->createSampleReadCondition(
                                    data_access_iterator->_receiver);
                            waitset->attach_condition(*condition);
                            waitset->attach_condition(*data_access_iterator->_item_queue->createStreamTypeReadCondition(
                                    data_access_iterator->_receiver));

                            data_access_iterator->_item_queue->setRecreateWaitSetCondition(
                                [&waitset]() { waitset = nullptr; });
                        }
                        waitset->attach_condition(_guard_condition);

                        _guard_condition.set_trigger_value(false);

                        // the Simulation Bus is now prepared for the reception of data and for a
                        // call to stopBlockingReception
                        reception_preparation_done_callback_caller.reset();

                        last_size_of_data_access_collection = _data_access_collection->size();
                    }
                    try {
                        // Block until one condition was emited:
                        // * ReadCondition for sample or stream
                        // * _guard_condition
                        // * or timeout 100ms
                        while (!_guard_condition.get_trigger_value()) {
                            ConditionSeq conditions;
                            auto ret = waitset->wait(conditions, Duration_t(0, 100000000));
                            for (auto const& condition: conditions) {
                                for (auto data_access_iterator = data_access_collection.cbegin();
                                     data_access_iterator != data_access_collection.cend();
                                     ++data_access_iterator) {
                                    // check, that the condition if not the guard condition
                                    if (condition != &_guard_condition) {
                                        if (data_access_iterator->_item_queue->popFrom(
                                                data_access_iterator->_receiver)) {
                                            while (data_access_iterator->_item_queue->popFrom(
                                                data_access_iterator->_receiver))
                                                ;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    catch (const std::exception& exception) {
                        if (_logger) {
                            if (_logger->isWarningEnabled()) {
                                _logger->logWarning(a_util::strings::format(
                                    "Caught Fast DDS exception during reception of data: '%s'",
                                    exception.what()));
                            }
                        }
                    }
                    waitset = nullptr;
                    return;
                }
            }
        }
    }

    void stopBlockingReception()
    {
        _receiving = false;
        _guard_condition.set_trigger_value(true);
    }

private:
    std::shared_ptr<ITopic> getOrCreateTopic(const std::string& topic_name,
                                             const IStreamType& stream_type)
    {
        auto entity = _topics.find(topic_name);
        if (entity != _topics.end()) {
            //@TODO Check IStreamType
            return entity->second;
        }

        if (!_participant) {
            throw std::runtime_error(
                "Fast DDS Participant pointer empty. Simulation Bus not initialized.");
        }

        auto topic =
            std::make_shared<StreamItemTopic>(*_participant, topic_name, stream_type, _logger);
        _topics[topic_name] = topic;
        return topic;
    }

public:
    DomainParticipant* _participant = nullptr;
    std::map<std::string, std::shared_ptr<ITopic>> _topics;
    std::shared_ptr<fep3::ILogger> _logger;

    std::unique_ptr<BusInfo> _bus_info;
    IParticipantInfo* _participant_info = nullptr;

private:
    // We need a sub object for collection data access (data triggered behavior)
    // because the simulation bus cannot be passed to the reader, as lifetime of simulation bus
    // cannot be controlled.
    std::shared_ptr<base::SimulationDataAccessCollection<ReaderItemQueue>> _data_access_collection;
    GuardCondition _guard_condition;
    std::atomic<bool> _receiving = {false};
};
//
FastDDSSimulationBus::FastDDSSimulationBus() : _impl(std::make_unique<FastDDSSimulationBus::Impl>())
{
}

FastDDSSimulationBus::~FastDDSSimulationBus()
{
}

fep3::Result FastDDSSimulationBus::create()
{
    std::shared_ptr<const IComponents> components = _components.lock();
    if (components) {
        auto logging_service = components->getComponent<ILoggingService>();
        if (logging_service) {
            _impl->_logger = logging_service->createLogger("connext_dds_simulation_bus.component");
        }

        auto configuration_service = components->getComponent<IConfigurationService>();
        if (configuration_service) {
            _simulation_bus_configuration.initConfiguration(*configuration_service);
        }
        else {
            RETURN_ERROR_DESCRIPTION(ERR_INVALID_STATE,
                                     "Can not get configuration service interface");
        }

        auto participant_info = components->getComponent<IParticipantInfo>();
        if (participant_info) {
            _impl->_participant_info = participant_info;
        }
        else {
            RETURN_ERROR_DESCRIPTION(ERR_INVALID_STATE, "Can not get participant info interface");
        }
    }
    return {};
}

fep3::Result FastDDSSimulationBus::destroy()
{
    _simulation_bus_configuration.deinitConfiguration();
    return {};
}
//
fep3::Result FastDDSSimulationBus::initialize()
{
    // the mutex ensures that qos profile is not read while the working dir is changed and
    // ensures there are not parallel calls to the non thread safe constructor of DomainParticipant
    static std::mutex domain_participant_constructor_mutex;
    a_util::filesystem::Path qos_file_beside_the_binary;
    std::string qos_profile_content{};
    {
        std::unique_lock<std::mutex> constrLock(domain_participant_constructor_mutex);
        qos_file_beside_the_binary = getFilePath().append("USER_QOS_PROFILES.xml");
        a_util::filesystem::readTextFile(qos_file_beside_the_binary, qos_profile_content);
    }
    if (ReturnCode_t::RETCODE_OK == DomainParticipantFactory::get_instance()->load_XML_profiles_file(
            qos_file_beside_the_binary.toString())) {
        DomainParticipantQos dqos;
        auto ret = DomainParticipantFactory::get_instance()->get_participant_qos_from_profile(
            "fep3", dqos);
        if (ret != ReturnCode_t::RETCODE_OK) {
            RETURN_ERROR_DESCRIPTION(fep3::ERR_NOT_FOUND,
                                     "Could not find fep3 library in USER_QOS_PROFILES.xml. \n"
                                     "Please make sure your application has access to the "
                                     "predefined USER_QOS_PROFILES.xml from fep3. \n"
                                     "See documentation for more information");
        }

        _simulation_bus_configuration.updatePropertyVariables();
        uint32_t domain_id = _simulation_bus_configuration._participant_domain;

        auto participant_name = _impl->_participant_info->getName();
        if (participant_name.empty()) {
            RETURN_ERROR_DESCRIPTION(fep3::ERR_NOT_SUPPORTED,
                                     "Participant name from participant info is empty");
        }
        dqos.name(participant_name);
        DomainParticipantFactory::get_instance()->set_default_participant_qos(dqos);
        auto system_name = _impl->_participant_info->getSystemName();
        if (system_name.empty()) {
            RETURN_ERROR_DESCRIPTION(fep3::ERR_NOT_SUPPORTED,
                                     "Participant name from participant info is empty");
        }
        if (system_name.size() >= 255) {
            RETURN_ERROR_DESCRIPTION(fep3::ERR_NOT_SUPPORTED,
                                     "Fast DDS doesn't support system name longer than 255 byte");
        }

        {
            // call DomainParticipant thread safe and not LoadQosProfile while changing the working
            // directories
            std::unique_lock<std::mutex> constr_lock(domain_participant_constructor_mutex);

#ifdef WIN32
            // in windows the rtimonitoring is loaded lazy ... so we need to change working dir here
            // for creating time
            auto orig_wd = a_util::filesystem::getWorkingDirectory();
            auto res = a_util::filesystem::setWorkingDirectory(getFilePath());
            if (isFailed(res)) {
                orig_wd = getFilePath();
            }
#endif
            try {
                DomainParticipantQos pqos;
                pqos.wire_protocol().builtin.discovery_config.leaseDuration =
                    eprosima::fastrtps::c_TimeInfinite;
                pqos.name(participant_name);
                _impl->initBusInfo(dqos, participant_name);
                auto participant = DomainParticipantFactory::get_instance()->create_participant(
                    domain_id, dqos); // _with_profile "fep3"
                _impl->_participant = (participant);
                eprosima::fastdds::dds::TypeSupport type(new fep3::ddstypes::BusDataPubSubType());
                type.register_type(participant);
                eprosima::fastdds::dds::TypeSupport type_streamtype(
                    new fep3::ddstypes::StreamTypePubSubType());
                type_streamtype.register_type(participant);
                eprosima::fastdds::dds::TypeSupport type_prop(
                    new fep3::ddstypes::PropertyPubSubType());
                type_prop.register_type(participant);
                _impl->_bus_info->registerParticipant(*_impl->_participant);
            }
            catch (const std::exception& ex) {
#ifdef WIN32
                a_util::filesystem::setWorkingDirectory(orig_wd);
#endif
                RETURN_ERROR_DESCRIPTION(ERR_UNEXPECTED, ex.what());
            }

#ifdef WIN32
            a_util::filesystem::setWorkingDirectory(orig_wd);
#endif
        }
        _impl->createDataAccessCollection();
    }
    else {
        RETURN_ERROR_DESCRIPTION(fep3::ERR_INVALID_FILE,
                "Error while reading USER_QOS_PROFILES.xml. \n"
                "Please make sure the file is valid against"
                "https:// github.com/eProsima/Fast-DDS/blob/master/resources/xsd/fastRTPS_profiles.xsd"
             );
    }
}
//
fep3::Result FastDDSSimulationBus::deinitialize()
{
    _impl->releaseDataAccessCollection();
    _impl->_topics.clear();

    if (_impl->_participant) {
        _impl->_participant->close();
    }

    _impl->_bus_info->unregisterParticipant();
    // DomainParticipantFactory::get_instance()->delete_participant(_impl->_participant);

    _impl->_participant = nullptr;
    _impl->_bus_info = nullptr;

    return {};
}

bool FastDDSSimulationBus::isSupported(const IStreamType& stream_type) const
{
    using namespace fep3::base::arya;
    return (meta_type_raw == stream_type || (meta_type_audio == stream_type) ||
            (meta_type_ddl == stream_type) || (meta_type_plain == stream_type) ||
            (meta_type_string == stream_type) || (meta_type_plain == stream_type));
}

std::unique_ptr<ISimulationBus::IDataReader> FastDDSSimulationBus::getReader(
    const std::string& name, const IStreamType& stream_type)
{
    try {
        return _impl->getReader(name, stream_type);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}

std::unique_ptr<ISimulationBus::IDataReader> FastDDSSimulationBus::getReader(
    const std::string& name, const IStreamType& stream_type, size_t queue_capacity)
{
    try {
        return _impl->getReader(name, stream_type, queue_capacity);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    catch (std::exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}

std::unique_ptr<ISimulationBus::IDataReader> FastDDSSimulationBus::getReader(
    const std::string& name)
{
    try {
        return _impl->getReader(name);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
std::unique_ptr<ISimulationBus::IDataReader> FastDDSSimulationBus::getReader(
    const std::string& name, size_t queue_capacity)
{
    try {
        return _impl->getReader(name, queue_capacity);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
std::unique_ptr<ISimulationBus::IDataWriter> FastDDSSimulationBus::getWriter(
    const std::string& name, const IStreamType& stream_type)
{
    try {
        return _impl->getWriter(name, stream_type, 0);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
std::unique_ptr<ISimulationBus::IDataWriter> FastDDSSimulationBus::getWriter(
    const std::string& name, const IStreamType& stream_type, size_t queue_capacity)
{
    try {
        return _impl->getWriter(name, stream_type, queue_capacity);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
std::unique_ptr<ISimulationBus::IDataWriter> FastDDSSimulationBus::getWriter(
    const std::string& name)
{
    try {
        return _impl->getWriter(name);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
std::unique_ptr<ISimulationBus::IDataWriter> FastDDSSimulationBus::getWriter(
    const std::string& name, size_t queue_capacity)
{
    try {
        auto topic = _impl->getWriter(name, queue_capacity);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
    return nullptr;
}
void FastDDSSimulationBus::startBlockingReception(
    const std::function<void()>& reception_preparation_done_callback)
{
    try {
        _impl->startBlockingReception(reception_preparation_done_callback);
    }
    catch (eprosima::fastcdr::exception::Exception& exception) {
        logError(convertExceptionToResult(exception));
    }
}

void FastDDSSimulationBus::stopBlockingReception()
{
    _impl->stopBlockingReception();
}

void(FastDDSSimulationBus::logError)(const fep3::Result& res)
{
    if (_impl->_logger) {
        if (_impl->_logger->isErrorEnabled()) {
            _impl->_logger->logError(a_util::result::toString(res));
        }
    }
}

FastDDSSimulationBus::FastDDSSimulationBusConfiguration::FastDDSSimulationBusConfiguration()
    : Configuration("fast_dds_simulation_bus")
{
}

fep3::Result FastDDSSimulationBus::FastDDSSimulationBusConfiguration::registerPropertyVariables()
{
    FEP3_RETURN_IF_FAILED(registerPropertyVariable(_participant_domain, "participant_domain"));

    return {};
}

fep3::Result FastDDSSimulationBus::FastDDSSimulationBusConfiguration::
    unregisterPropertyVariables()
{
    FEP3_RETURN_IF_FAILED(unregisterPropertyVariable(_participant_domain, "participant_domain"));

    return {};
}
