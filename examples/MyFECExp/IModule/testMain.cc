#include "IModulePyWrapper.h"

int main() {
    // 初始化单例
    auto& rl = IModuleRLWrapper::get_instance();

    // 模拟状态输入
    std::vector<double> state1 = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    double reward1 = 0.5;

    std::vector<double> state2 = {0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6};
    double reward2 = 0.6;

    std::vector<double> state3 = {0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7};
    double reward3 = 0.7;

    // 预测动作s
    int action = rl.predict(state1, reward1);
    std::cout << "Action: " << action << std::endl;
    rl.train_step();

    action = rl.predict(state2, reward2);
    std::cout << "Action: " << action << std::endl;
    rl.train_step();

    action = rl.predict(state3, reward3);
    std::cout << "Action: " << action << std::endl;
    rl.train_step();

    return 0;
}