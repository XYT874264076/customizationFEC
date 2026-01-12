#include <pybind11/embed.h>
#include <iostream>

namespace py = pybind11;

int main() {
    // 初始化Python解释器
    py::scoped_interpreter guard{};

    try {
        // 导入Python模块
        py::module my_module = py::module::import("PyBindExample");

        // 获取Python函数
        py::function add_func = my_module.attr("add");

        // 调用Python函数并传递参数
        int result = add_func(3, 4).cast<int>();

        // 输出结果
        std::cout << "Result from Python function: " << result << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error calling Python function: " << e.what() << std::endl;
    }

    return 0;
}
