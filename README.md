<a href="http://www.eprosima.com"><img src="https://encrypted-tbn3.gstatic.com/images?q=tbn:ANd9GcSd0PDlVz1U_7MgdTe0FRIWD0Jc9_YH-gGi0ZpLkr-qgCI6ZEoJZ5GBqQ" hspace="8" vspace="2" width="100" height="100" ></a>
<a href="https://github.com/cariad-tech"><img src="https://raw.githubusercontent.com/cariad-tech/fep3_sdk/main/doc/images/fep_logo_white.ico" align="left" hspace="8" vspace="2" width="100" height="100" ></a>

# FEP3 eProsima Fast DDS Simulation Bus Plugin

This project aim to add an open source simulation bus to the FEP3 SDK for the purpose of showcasing its capabilities. It does not aim to be a fully featured and fast implementation.

The current source is based on rti_dds_plugin of fep3_sdk_participant under MPL-2 license.

## Features and known limitations

What is working?

* Participant with system name isolation through DDS domain tags
* Metadata as user info on bultin participant topic
* Reading and writing DDS topics

Possible issues with timing where not analized yet. 

### Qos

Fully featured through the USER_QOS_PROFILES.xml.

## Using the Adapter

### For simulations

A *fep3_participant.fep_components* file is provided in the lib folder, so you only need to copy it and the *fast_dds'* folder beside your 
fep3_particpant shared library and you're ready to go!

### Developing participants - CMake macros

There are analogous macros to FEP3 deploy and install:

    fep_fast_dds_plugin_deploy(TARGET)
    fep_fast_dds_plugin_install(TARGET DESTINATION)

They will copy the FEP3 participant, the Fast DDS plugin and its dependecies and
a modified fep_components file.

## Building the project

The project is set up with Conan using the available public packages and building the missing ones with custom conan files in the thirdparty directory.
For linux please use an appropriate profile, like the supplied linux_gcc_9

    conan create thirdparty/fast_dds/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo
    conan create thirdparty/dev_essential/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo
    conan create thirdparty/fep3_participant/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo
    conan create thirdparty/fep3_system/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo
    conan create thirdparty/fep3_base_utilities/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo
    conan create thirdparty/fep3_sdk/conanfile.py /_@_ -pr build_profiles/win_142_relwithdebinfo

    conan create . -pr build_profiles/win_142_relwithdebinfo

## Used products and sources

* [Fast DDS](https://www.eprosima.com/index.php/products-all/eprosima-fast-dds) by eProsima under [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0) 
* [FEP3](https://github.com/cariad-tech) components dev_essential 1.2.0 and fep_sdk 3.1.0 by CARIAD under MPL-2.0
* boost 1.75 under BSL-1.0
* gtest/1.10.0 under BSD-3-Clause

## Used compilers

* Visual Studio MSVC 142
* gcc 9
