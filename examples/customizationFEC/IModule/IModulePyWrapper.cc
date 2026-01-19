
#include "examples/customizationFEC/IModule/IModulePyWrapper.h"
#include "examples/customizationFEC/Params.h"

std::unique_ptr<IModuleRLWrapper> IModuleRLWrapper::instance = nullptr;

IModuleRLWrapper::IModuleRLWrapper() {
    if (transV::Params::py_init == 0) {
        py::initialize_interpreter(); // Initialize the Python interpreter
        transV::Params::py_init = 1;
    }
    // py::initialize_interpreter(); // Initialize the Python interpreter
    py::module_ sys = py::module_::import("sys");
    // sys.attr("path").attr("insert")(0, "/home/ubuntu/Desktop/WebRTC/src/examples/customizationFEC/"); // Add the Python module to search the path
    sys.attr("path").attr("insert")(0, "/home/data/WebRTC_SwiftFEC/webrtc-checkout/src/examples/customizationFEC/"); // Add the Python module to search the path
    py::module_ rl_model = py::module_::import("IModule.modelV3");
    if (inputV::Params::ifSaveI) {
        // rl_module = rl_model.attr("RLModule")("examples/customizationFEC/IModule/checkpoints/model_weightsUNV2_break.pth",true); // Initialize the Python class
        rl_module = rl_model.attr("RLModule")("../examples/customizationFEC/IModule/checkpoints/model_weights_NRV2.pth",true); 
    }
    else {
        // rl_module = rl_model.attr("RLModule")("examples/customizationFEC/IModule/checkpoints/model_weightsUNV2_break.pth",false); // Initialize the Python class
        rl_module = rl_model.attr("RLModule")("../examples/customizationFEC/IModule/checkpoints/model_weights_NRV2.pth",false); 
    }
}

IModuleRLWrapper& IModuleRLWrapper::get_instance() {
    if (!instance) {
        instance = std::make_unique<IModuleRLWrapper>();
    }
    return *instance;
}

int IModuleRLWrapper::predict(const std::vector<double>& state, double reward) {
    // py::gil_scoped_acquire acquire; // obtain GIL lock
    return rl_module.attr("predict")(state, reward).cast<int>() - 1;
}

void IModuleRLWrapper::train_step() {
    // py::gil_scoped_acquire acquire;
    rl_module.attr("train_step")();
}