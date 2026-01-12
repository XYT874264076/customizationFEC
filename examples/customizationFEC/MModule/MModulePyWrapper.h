#ifndef EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULEWRAPPER_H_
#define EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULEWRAPPER_H_

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <iostream>
#include <vector>
#include <memory>

namespace py = pybind11;

class MModuleRLWrapper {
private:
    py::object rl_module;
    static std::unique_ptr<MModuleRLWrapper> instance;  // singleton pattern

    MModuleRLWrapper();

    friend std::unique_ptr<MModuleRLWrapper> std::make_unique<MModuleRLWrapper>();

public:
    static MModuleRLWrapper& get_instance();

    int predict(const std::vector<double>& state, double reward);
    void train_step();
};

#endif  // EXAMPLES_MYFECEXP_RSFEC_MMODULE_MMODULEWRAPPER_H_