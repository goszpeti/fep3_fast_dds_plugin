from pathlib import Path
from conans import ConanFile, CMake
from conan.tools.scm import Git
from conan.tools.files import copy
import yaml

required_conan_version = ">=1.59.0"

class ConanProduct(ConanFile):
    name = "dev_essential"
    version = "1.3.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "txt" 
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}
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
       git.fetch_commit(url=sources["url"], commit=f"v{self.version}")

    def build_requirements(self):
        self.tool_requires(self.conan_data["tool_reqs"]["cmake"])

    def build(self):
        cmake = CMake(self)
        cmake.definitions['CMAKE_POSITION_INDEPENDENT_CODE'] = self.options.fPIC
        cmake.configure()
        cmake.build()
        cmake.install()
