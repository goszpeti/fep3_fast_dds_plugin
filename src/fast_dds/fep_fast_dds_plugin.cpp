#include <fep3/plugin/cpp/cpp_plugin_impl_base.hpp>
#include <fep3/plugin/cpp/cpp_plugin_component_factory.h>
#include <fep3/components/base/component.h>
#include "simulation_bus/fep_fast_dds_simulation_bus.h"

#include <functional>


void fep3_plugin_getPluginVersion(void(*callback)(void*, const char*), void* destination)
{
    callback(destination, "0.1.0");
}

 fep3::plugin::cpp::catelyn::IComponentFactory* fep3_plugin_cpp_catelyn_getFactory()
 {
     return new fep3::plugin::cpp::ComponentFactory<FastDDSSimulationBus>();
 }


