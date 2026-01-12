import csv
import random
import math

# 设置随机种子保证可重复性
random.seed(42)


class NetworkTraceGenerator:
    def __init__(self):
        # 基础LTE参数
        self.base_latency = 30  # 基站基础延迟(ms)
        self.max_bandwidth = 50000  # 最大理论带宽(Kbps)

    def generate_walking_trace(self):
        """生成步行场景网络轨迹 (稳定环境)"""
        trace = []
        current_time = 0

        for _ in range(240):  # 120秒数据点
            time_step = random.randint(450, 550)
            current_time += time_step

            bandwidth = random.gauss(35000, 5000)  # 均值35Mbps
            bandwidth = max(10000, min(self.max_bandwidth, bandwidth))

            latency = random.gauss(self.base_latency + 5, 5)
            latency = max(20, min(100, latency))

            loss_rate = random.uniform(0.1, 0.5)

            trace.append([current_time, bandwidth, latency, loss_rate])

        return trace

    def generate_driving_trace(self):
        """生成乘车场景网络轨迹 (中等波动)"""
        trace = []
        current_time = 0

        signal_state = 0
        state_duration = 0

        for _ in range(240):
            time_step = random.randint(450, 550)
            current_time += time_step

            if state_duration <= 0:
                signal_state = random.choices([0, 1, 2], weights=[0.7, 0.25, 0.05])[0]
                state_duration = random.randint(20, 40)
            state_duration -= 1

            if signal_state == 0:  # 好信号
                bandwidth = random.gauss(30000, 8000)
            elif signal_state == 1:  # 中等信号
                bandwidth = random.gauss(20000, 6000)
            else:  # 差信号 (如隧道)
                bandwidth = random.gauss(8000, 3000)
            bandwidth = max(5000, min(self.max_bandwidth, bandwidth))

            latency_base = self.base_latency + 20
            if signal_state == 2:
                latency_base += random.randint(30, 100)
            latency = random.gauss(latency_base, 15)
            latency = max(30, min(300, latency))

            loss_rate = random.gauss(0.3, 0.5) if signal_state < 2 else random.gauss(2.0, 1.0)
            loss_rate = max(0.05, min(5.0, loss_rate))

            trace.append([current_time, bandwidth, latency, loss_rate])

        return trace

    def generate_hsr_trace(self):
        """生成高铁场景网络轨迹 (快速切换导致高度波动)"""
        trace = []
        current_time = 0

        switch_counter = 0
        switch_duration = random.randint(5, 10)

        for _ in range(240):
            time_step = random.randint(450, 550)
            current_time += time_step

            switch_counter += 1
            is_switching = False
            if switch_counter >= switch_duration:
                is_switching = True
                switch_counter = 0
                switch_duration = random.randint(5, 10)

            if is_switching:
                bandwidth = random.gauss(10000, 5000)
                if random.random() < 0.25:
                    bandwidth = random.gauss(3000, 1000)
            else:
                bandwidth = random.gauss(25000, 10000)
            bandwidth = max(2000, min(self.max_bandwidth, bandwidth))

            if is_switching:
                latency = random.gauss(self.base_latency + 150, 50)
            else:
                latency = random.gauss(self.base_latency + 30, 20)
            latency = max(30, min(500, latency))

            if is_switching:
                loss_rate = random.gauss(8.0, 3.0)
            else:
                loss_rate = random.gauss(1.5, 1.0)
            loss_rate = max(0.1, min(15.0, loss_rate))

            trace.append([current_time, bandwidth, latency, loss_rate])

        return trace

    def generate_indoor_dense_trace(self):
        """生成室内密集场景网络轨迹 (高用户密度环境)"""
        trace = []
        current_time = 0

        # 拥塞模型状态
        congestion_level = 0  # 0=正常, 1=轻度, 2=中度, 3=重度
        congestion_duration = 0

        # 信号空洞区状态
        deadzone_active = False
        deadzone_duration = 0

        for _ in range(240):
            time_step = random.randint(450, 550)
            current_time += time_step

            # 1. 更新拥塞状态 (持续10-30秒)
            if congestion_duration <= 0:
                congestion_level = random.choices([0, 1, 2, 3], weights=[0.4, 0.3, 0.2, 0.1])[0]
                congestion_duration = random.randint(20, 60)  # 10-30秒
            congestion_duration -= 1

            # 2. 更新信号空洞状态 (持续1-3秒)
            if deadzone_active:
                deadzone_duration -= 1
                if deadzone_duration <= 0:
                    deadzone_active = False
            elif random.random() < 0.05:  # 5%概率进入死区
                deadzone_active = True
                deadzone_duration = random.randint(2, 6)  # 1-3秒

            # 3. 带宽模型 (基于拥塞和死区)
            if deadzone_active:
                base_bandwidth = random.gauss(3000, 1000)  # 死区带宽极低
            else:
                # 拥塞级别影响
                if congestion_level == 0:
                    base_bandwidth = random.gauss(25000, 5000)  # 正常
                elif congestion_level == 1:
                    base_bandwidth = random.gauss(18000, 4000)  # 轻度拥塞
                elif congestion_level == 2:
                    base_bandwidth = random.gauss(10000, 3000)  # 中度拥塞
                else:  # 重度拥塞
                    base_bandwidth = random.gauss(5000, 2000)

                # 突发流量影响（30%概率出现10秒流量高峰）
                if random.random() < 0.3:
                    base_bandwidth *= random.uniform(0.4, 0.7)  # 带宽降低30%-60%

            bandwidth = max(1000, min(self.max_bandwidth, base_bandwidth))

            # 4. 延迟模型
            base_latency = self.base_latency
            # 拥塞增加延迟
            base_latency += congestion_level * 20
            # 死区大幅增加延迟
            if deadzone_active:
                base_latency += random.randint(150, 300)
            # 加入随机波动
            latency = random.gauss(base_latency, 10)
            latency = max(30, min(600, latency))

            # 5. 丢包率模型
            base_loss_rate = 0.3  # 基础丢包率
            # 拥塞增加丢包
            base_loss_rate += congestion_level * 1.5
            # 死区大幅增加丢包
            if deadzone_active:
                base_loss_rate += random.uniform(8.0, 15.0)
            # 加入随机波动
            loss_rate = max(0.1, min(30.0, base_loss_rate * random.uniform(0.8, 1.2)))

            trace.append([current_time, bandwidth, latency, loss_rate])

        return trace

    def save_trace(self, trace, filename):
        """保存轨迹到CSV文件"""
        with open(filename, 'w', newline='') as f:
            # 写入标题行
            f.write(",time (ms),bandwidth (Kbps),Latency (ms),Loss rate (%)\n")

            # 写入数据行
            for i, row in enumerate(trace):
                line = f"{i},{row[0]},{row[1]:.1f},{row[2]:.3f},{row[3]:.3f}\n"
                f.write(line)

    def generate_all_traces(self):
        """生成所有场景轨迹"""
        print("生成步行场景轨迹...")
        walking_trace = self.generate_walking_trace()
        self.save_trace(walking_trace, "walking/trace.csv")

        print("生成乘车场景轨迹...")
        driving_trace = self.generate_driving_trace()
        self.save_trace(driving_trace, "driving/trace.csv")

        print("生成高铁场景轨迹...")
        hsr_trace = self.generate_hsr_trace()
        self.save_trace(hsr_trace, "high_speed/trace.csv")

        print("生成室内密集场景轨迹...")
        indoor_trace = self.generate_indoor_dense_trace()
        self.save_trace(indoor_trace, "dense_indoor/trace.csv")

        print("所有轨迹文件已生成成功!")


# 运行生成器
if __name__ == "__main__":
    generator = NetworkTraceGenerator()
    generator.generate_all_traces()