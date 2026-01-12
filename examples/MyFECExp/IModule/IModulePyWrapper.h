
#ifndef EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULEPYWRAPPER_H_
#define EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULEPYWRAPPER_H_

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <iostream>
#include <vector>
#include <memory>

namespace py = pybind11;

//__attribute__((visibility("default"))) 
class IModuleRLWrapper {
private:
    py::object rl_module;  
    static std::unique_ptr<IModuleRLWrapper> instance;  // singleton pattern

    IModuleRLWrapper();

    friend std::unique_ptr<IModuleRLWrapper> std::make_unique<IModuleRLWrapper>();

public:
    static IModuleRLWrapper& get_instance();

    int predict(const std::vector<double>& state, double reward);
    void train_step();
};

#endif  // EXAMPLES_MYFECEXP_RSFEC_IMODULE_IMODULEPYWRAPPER_H_