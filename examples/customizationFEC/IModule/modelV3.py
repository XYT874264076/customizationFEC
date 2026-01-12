import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from collections import deque
import os
import random


# 定义神经网络结构
class ActorCritic(nn.Module):
    def __init__(self, state_dim, action_dim):
        super().__init__()
        self.shared = nn.Sequential(
            nn.Linear(state_dim, 32),
            nn.LayerNorm(32),
            nn.ReLU(),
            nn.Linear(32,64),
            nn.LayerNorm(64),
            nn.ReLU(),
        )
        self.actor = nn.Sequential(
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, action_dim)
        )
        self.critic = nn.Sequential(
            nn.Linear(64, 32),
            nn.ReLU(),
            nn.Linear(32, 1)
        )

    def forward(self, x):
        x = self.shared(x)
        return torch.softmax(self.actor(x), dim=-1), self.critic(x)


# 经验缓冲区（处理延迟）
class DelayBuffer:
    def __init__(self, agent, delay_steps=5, gamma=0.99, gae_lambda=0.95):
        self.agent = agent
        self.buffer = deque(maxlen=delay_steps)
        self.delay = delay_steps
        self.gamma = gamma
        self.gae_lambda = gae_lambda

    def add(self, state, action, reward, next_state):
        self.buffer.append((state, action, reward, next_state))

    def get_batch(self):
        if len(self.buffer) < self.delay:
            return None

        # 获取延迟训练的数据
        train_idx = 0
        state, action, _, _ = self.buffer[train_idx]

        # 计算n-step回报和GAE优势
        returns = []
        advantages = []
        with torch.no_grad():
            next_value = self._get_value(self.buffer[-1][3])

        # 逆向计算GAE
        gae = 0
        for t in reversed(range(train_idx, len(self.buffer))):
            _, _, r, next_s = self.buffer[t]
            value = self._get_value(self.buffer[t][0])
            next_value = next_value
            delta = r + self.gamma * next_value - value
            gae = delta + self.gamma * self.gae_lambda * gae
            returns.insert(0, gae + value)
            advantages.insert(0, gae)
            next_value = value

        return (
            torch.FloatTensor(state),
            torch.LongTensor([action]),
            torch.FloatTensor(returns),
            torch.FloatTensor(advantages)
        )

    def _get_value(self, state):
        with torch.no_grad():
            return self.agent.model(torch.FloatTensor(state))[1].item()

class RLModule:
    def __init__(self, model_path, auto_save=True, save_interval=10,
                 state_dim=6, action_dim=3, delay_steps=1,
                 lr=1e-3, gamma=0.99, gae_lambda=0.95):
        self.model = ActorCritic(state_dim, action_dim)
        self.optimizer = optim.Adam(self.model.parameters(), lr=lr)
        self.buffer = DelayBuffer(self, delay_steps, gamma, gae_lambda)

        self.last_state = None
        self.last_action = None
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

        self.path = model_path
        self.auto_save = auto_save
        self.save_interval = save_interval
        self.train_step_count = 0

        file_path = self.path
        # 加载已有参数
        if model_path and os.path.exists(file_path):
            self.model.load_state_dict(torch.load(file_path))
            print(f"Loaded model from {file_path}")

        self.model.to(self.device)

    def predict(self, current_state, reward=None):
        """返回当前state对应的action，并维护经验缓冲区"""
        # 存储上一步的经验
        if self.last_state is not None and self.last_action is not None and reward is not None:
            self.buffer.add(self.last_state, self.last_action, reward, current_state)

        # 选择当前动作
        with torch.no_grad():
            state_tensor = torch.FloatTensor(current_state).to(self.device)
            prob, _ = self.model(state_tensor)
            print("Current prob:",prob);
            action = torch.multinomial(prob, 1).item()

        # 更新历史记录
        self.last_state = current_state.copy() if isinstance(current_state, np.ndarray) else current_state
        self.last_action = action

        return action

    def train_step(self):
        """执行一步训练，使用延迟的旧数据"""
        batch = self.buffer.get_batch()
        if not batch:
            return 0.0

        states, actions, returns, advantages = batch

        print("Now train states: ",states);
        print("Now train actions: ",actions);

        # 数据转移到设备
        states = states.to(self.device)
        actions = actions.to(self.device)
        returns = returns.to(self.device)
        advantages = advantages.to(self.device)

        # 计算损失
        probs, values = self.model(states)
        dist = torch.distributions.Categorical(probs)
        log_probs = dist.log_prob(actions.squeeze())

        entropy = dist.entropy().mean()
        actor_loss = -(log_probs * advantages).mean() - 0.01 * entropy

        critic_loss = 0.5 * (returns - values.squeeze()).pow(2).mean()
        loss = actor_loss + critic_loss

        # 反向传播
        self.optimizer.zero_grad()
        loss.backward()
        self.optimizer.step()

        self.train_step_count += 1
        if self.auto_save and (self.train_step_count % self.save_interval == 0):
            save_path = self.path;
            self.save_model(save_path)

        return loss.item()

    def reset(self):
        """重置智能体状态用于新episode"""
        self.last_state = None
        self.last_action = None

    def save_model(self, path):
        with open(path, 'wb') as f:
            torch.save(self.model.state_dict(), f)
        print(f"Model saved to {path}")

if __name__ == "__main__":

    # 初始化智能体
    agent = RLModule("checkpoints/model_weightsV11.pth")

    # 模拟环境交互
    episode_loss = []
    total_steps = 10000
    reward = 0;
    last_i = 0;
    last_action = 0;
    testState = [[0.01, 0.01, 0.01, 0.8, 0.01, 0.2],
                 [-0.01, -0.01, -0.01, 1.2, -0.01, 0.2]];
    testReward = [[0.5, 0.8, 1.0], [1.0, 0.8, 0.5]];
    bl_reward = 0;

    for t in range(total_steps):

        cur_i = random.randint(0,1)
        state = testState[cur_i];
        reward = testReward[last_i][last_action];

        print();
        print("================== step : ", t, " ==================");
        # 获得动作（首次调用reward为None）
        print("Current state:", state);
        print("Current reward:", reward);
        print("Baseline reward:", reward - bl_reward);
        action = agent.predict(state, reward=reward - bl_reward);
        last_action = action;
        last_i = cur_i;
        bl_reward = 0.9*bl_reward + 0.1*reward;

        print("Current action:",action-1);

        # 执行训练
        loss = agent.train_step()
        if loss:
            episode_loss.append(loss)

        # 定期输出
        if t % 500 == 0 and t > 0:
            print(f"Step {t}, Avg Loss: {np.mean(episode_loss[-50:])}")