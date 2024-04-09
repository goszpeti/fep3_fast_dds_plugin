from pathlib import Path

import yaml
from conan.tools.files import copy, patch
from conan.tools.scm import Git
from conans import CMake, ConanFile
from conan.tools.microsoft import is_msvc

class ConanProduct(ConanFile):
    name = "fep_sdk_system"
    version = "3.2.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "txt", "CMakeDeps"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True, 
                       "cpython:shared": False,
                       "boost:without_date_time": False,
                       "boost:without_filesystem": False,
                        "cpython:with_bz2": False,
                        "cpython:with_curses": False,
                        "cpython:with_gdbm": False,
                        "cpython:with_lzma": False,
                        "cpython:with_nis": False,
                        "cpython:with_sqlite3": False,
                        "cpython:with_tkinter": False,
                       }
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

    def config_options(self):
        if is_msvc(self):
            del self.options.deps_package_values["cpython"].with_curses
            del self.options.deps_package_values["cpython"].with_gdbm
            del self.options.deps_package_values["cpython"].with_nis

        return super().configure()

    def source(self):
       git = Git(self)
       sources = self.conan_data["sources"][self.name]
       # shallow checkout
       git.fetch_commit(url=sources["url"], commit=f"v{self.version}")
    
    def build_requirements(self):
        self.tool_requires(self.conan_data["tool_reqs"]["cmake"])
        self.build_requires(self.conan_data["tool_reqs"]["python3.10"])

    def requirements(self):
        self.requires(f"fep_sdk_participant/{self.version}@{self.user}/{self.channel}")
        self.requires(self.conan_data["tool_reqs"]["pybind11"], private=True)

    def build(self):
        patch_str = (Path(self.source_folder) / "patch.diff").read_text()
        patch(self, patch_string=patch_str)
        cmake = CMake(self)
        cmake.definitions['CMAKE_POSITION_INDEPENDENT_CODE'] = self.options.fPIC
        cmake.configure(defs={"fep3_system_cmake_enable_documentation": "OFF", 
                              "fep3_system_cmake_enable_tests": "OFF",
                              "fep3_system_cmake_enable_functional_tests": "OFF",
                              "fep3_system_cmake_enable_private_tests": "OFF",
                              "Boost_USE_STATIC_LIBS": "On",
                              "fep3_participant_use_rtidds": "Off",
                              "fep3_system_cmake_python_version": "3.10",
                             }
                        )
        cmake.build()
        cmake.install()
