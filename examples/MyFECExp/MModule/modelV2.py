import torch
import torch.nn as nn
import numpy as np
import os
import json


class A2CModel(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(A2CModel, self).__init__()
        self.shared_net = nn.Sequential(
            nn.Linear(state_dim, 64),
            nn.ReLU(),
            nn.Linear(64, 128),
            nn.ReLU(),
        )
        self.actor_head = nn.Sequential(
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, action_dim)
        )
        self.critic_head = nn.Sequential(
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, 1)
        )

    def forward(self, x):
        x = self.shared_net(x)
        action_logits = self.actor_head(x)
        action_probs = torch.softmax(action_logits, dim=-1)
        state_value = self.critic_head(x)
        return action_probs, state_value


class RLModule:
    def __init__(self, model_path, auto_save=True, save_interval=10):
        self.model = A2CModel(state_dim=8, action_dim=5)  # 输入维度12，输出维度31,分别表示 I=1~5
        self.optimizer = torch.optim.Adam(self.model.parameters(), lr=0.001)
        self.last_state = None
        self.last_reward = None
        self.curr_state = None
        self.curr_reward = None
        self.last_action_idx = None
        self.buffer = []  # 存储 (state, reward) 对
        self.path = model_path

        self.auto_save = auto_save
        self.save_interval = save_interval
        self.train_step_count = 0

        file_path = self.path

        # 加载已有参数
        if model_path and os.path.exists(file_path):
            self.model.load_state_dict(torch.load(file_path))
            print(f"Loaded model from {file_path}")

    def predict(self, state, reward):

        # 将state转换为Tensor
        state_tensor = torch.FloatTensor(np.array(state)).unsqueeze(0)

        # 前向传播
        with torch.no_grad():
            action_probs, _ = self.model(state_tensor)

        print("state:", state);
        print("action_probs:", action_probs);

        # 选项1：确定性选择（取最大概率对应的动作索引）
        # action_idx = torch.argmax(action_probs).item()
        # action = action_idx + 1

        # 选项2：按概率分布随机采样（探索性选择）
        action_dist = torch.distributions.Categorical(probs=action_probs)
        print('action_dist:',action_dist);
        action_idx = action_dist.sample().item()
        action = action_idx + 1

        # 保存当前状态和动作用于后续训练
        self.last_state = self.curr_state
        self.last_reward = self.curr_reward
        self.curr_state = state_tensor
        self.curr_reward = reward

        self.last_action_idx = action_idx;

        print("Current M:",action);

        return action

    def train_step(self, gamma=0.99):
        if self.last_state is None or self.curr_state is None or self.curr_reward is None:
            return

        # 计算损失（简化示例，实际需根据A2C算法实现）
        _, last_v = self.model(self.last_state)
        with torch.no_grad():
            _, curr_v = self.model(self.curr_state)

        print("last_v:",last_v);
        print("curr_v:",curr_v);

        advantage = self.curr_reward + gamma * curr_v - last_v;

        print("advantage:",advantage);

        action_probs, _ = self.model(self.last_state)
        action_dist = torch.distributions.Categorical(probs = action_probs)
        log_probs = action_dist.log_prob(torch.tensor(self.last_action_idx))

        actor_loss = -torch.mean(log_probs * advantage.detach())

        print("actor_loss:",actor_loss);

        critic_loss = torch.mean(advantage ** 2)

        print(critic_loss);

        # 反向传播
        total_loss = actor_loss + critic_loss
        self.optimizer.zero_grad()
        total_loss.backward()
        self.optimizer.step()

        self.train_step_count += 1
        if self.auto_save and (self.train_step_count % self.save_interval == 0):
            save_path = self.path;
            self.save_model(save_path)

    def save_model(self, path):
        with open(path, 'wb') as f:
            torch.save(self.model.state_dict(), f)
        print(f"Model saved to {path}")


if __name__ == "__main__":
    # 示例用法
    rl = RLModule("checkpoints/model_weightsV2.pth")
    # state1 = [32,0,0,0,0,0,0.123,0.123,0.123,0.123,0.122,1.7e+06]  # 模拟状态输入
    # reward1 = 1

    state1 = [0.5] *8  # 模拟状态输入
    reward1 = 1

    state2 = [0.6] * 8 # 模拟状态输入
    reward2 = 0.6

    state3 = [0.7] * 8  # 模拟状态输入
    reward3 = 0.7

    action = rl.predict(state1, reward1)
    rl.train_step()
    print(f"Action: {action}")

    action = rl.predict(state2, reward2)
    rl.train_step()
    print(f"Action: {action}")

    action = rl.predict(state3, reward3)
    rl.train_step()
    print(f"Action: {action}")