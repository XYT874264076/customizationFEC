
#include "examples/customizationFEC/MModule/MModulePyWrapper.h"
#include "examples/customizationFEC/Params.h"

std::unique_ptr<MModuleRLWrapper> MModuleRLWrapper::instance = nullptr;

MModuleRLWrapper::MModuleRLWrapper() {
    if (transV::Params::py_init == 0) {
        py::initialize_interpreter(); // Initialize the Python interpreter
        transV::Params::py_init = 1;
    }
    py::module_ sys = py::module_::import("sys");
    // sys.attr("path").attr("insert")(0, "/home/ubuntu/Desktop/WebRTC_SwiftFEC/src/examples/customizationFEC/"); // Add the Python module to search the path
    sys.attr("path").attr("insert")(0, "/home/data/WebRTC_SwiftFEC/webrtc-checkout/src/examples/customizationFEC/"); // Add the Python module to search the path
    py::module_ rl_model = py::module_::import("MModule.modelV3");
    if (inputV::Params::ifSaveM) {
        // rl_module = rl_model.attr("RLModule")("examples/customizationFEC/MModule/checkpoints/model_weightsV15.pth",true); // Initialize the Python class
        rl_module = rl_model.attr("RLModule")("../examples/customizationFEC/MModule/checkpoints/model_weights_NRV2.pth",true);
    }
    else {
        // rl_module = rl_model.attr("RLModule")("examples/customizationFEC/MModule/checkpoints/model_weightsV15.pth",false); // Initialize the Python class
        rl_module = rl_model.attr("RLModule")("../examples/customizationFEC/MModule/checkpoints/model_weights_NRV2.pth",false);
    }
}

MModuleRLWrapper& MModuleRLWrapper::get_instance() {
    if (!instance) {
        instance = std::make_unique<MModuleRLWrapper>();
    }
    return *instance;
}

int MModuleRLWrapper::predict(const std::vector<double>& state, double reward) {
    // py::gil_scoped_acquire acquire; // obtain GIL lock
    std::cout<<"Do M Module Predict!!"<<std::endl;
    return rl_module.attr("predict")(state, reward).cast<int>() + 1;
}

void MModuleRLWrapper::train_step() {
    // py::gil_scoped_acquire acquire;
    rl_module.attr("train_step")();
}