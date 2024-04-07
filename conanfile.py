import os
from conans import ConanFile, CMake

class ConanProduct(ConanFile):
    name = "fep3_fast_dds_plugin"
    version = "0.1.0"
    description = "FEP3 eProsima Fast DDS Simulation Bus Plugin"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "txt", "CMakeDeps"
    default_user = "local"
    default_channel = "testing"
    short_paths=True
    options = []
    default_options = {
        "fast-dds:shared": False
    }

    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto"
    }

    def configure(self):
        import json
        testing_internal = json.loads(self.env.get("enable_testing", "false"))
        self.enable_testing  =  testing_internal if testing_internal else json.loads(os.environ.get("enable_testing", "false"))
        return super().configure()

    def build_requirements(self):
        self.tool_requires("cmake/3.25.0")

    def requirements(self):
        self.requires("fep_sdk_participant/3.2.0@fep_oss/testing", private=True)
        self.requires("fast-dds/2.10.1@fep_oss/testing", private=True)
        if self.enable_testing:
            self.requires("gtest/1.10.0", private=True)

    def build(self):
        cmake = CMake(self)
        cmake.definitions["CMAKE_BUILD_TYPE"] = str(self.settings.build_type).upper()
        cmake.configure(defs={"FEP_FAST_DDS_VERSION": self.version, 
                              "ENABLE_TESTS": str(self.enable_testing),
                            }
                        )
        cmake.build()
        if self.enable_testing:
             cmake.test()
        cmake.install()
