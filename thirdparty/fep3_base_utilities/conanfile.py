from pathlib import Path
from conans import ConanFile, CMake
from conan.tools.files import copy, patch
from conan.tools.scm import Git
import yaml

class ConanProduct(ConanFile):
    name = "fep_sdk_base_utilities"
    version = "3.2.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "txt"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True, 
                       "boost:without_date_time":False,
                       "boost:without_filesystem":False}
    no_copy_source = True
    short_paths = True
    exports_sources = "patch.diff"
    no_copy_export_sources = True
    no_copy_source = True

    def init(self):
        if not self.conan_data:
            # copy single source of truth conan_data from repo root
            copy(self, "conandata.yml", src=str(Path(__file__).parents[2]), 
                 dst=str(Path(__file__).parents[0]))
            self.conan_data = yaml.safe_load(Path("conandata.yml").read_text())

    def source(self):
       git = Git(self)
       sources = self.conan_data["sources"][self.name]
       # shallow checkout
       git.fetch_commit(url=sources["url"], commit="v3.1.0") # no newer version available

    def build_requirements(self):
        self.tool_requires(self.conan_data["tool_reqs"]["cmake"])

    def requirements(self):
        self.requires(f"fep_sdk_system/{self.version}@{self.user}/{self.channel}", private=True)
        self.requires(self.conan_data["reqs"]["boost"], private=True)


    def build(self):
        #patch_str = (Path(self.source_folder) / "patch.diff").read_text()
        #patch(self, patch_string=patch_str)

        cmake = CMake(self)
        cmake.definitions['CMAKE_POSITION_INDEPENDENT_CODE'] = self.options.fPIC
        cmake.configure(defs={"fep_base_utilities_cmake_enable_documentation": "OFF", 
                              "fep_base_utilities_cmake_enable_tests": "OFF",
                              "fep_base_utilities_cmake_enable_functional_tests": "OFF",
                              "fep_base_utilities_cmake_enable_private_tests": "OFF",
                              "Boost_USE_STATIC_LIBS": "On",
                             }
                        )
        cmake.build()
        cmake.install()
