import csv
import random
import numpy as np

# 设置随机种子保证可重复性
random.seed(42)
np.random.seed(42)

# 网络类型配置 (单位: Kbps)
NETWORK_PROFILES = {
    "3G": {"target_bw": 3500, "min_bw": 500, "max_bw": 7000},
    "4G": {"target_bw": 15000, "min_bw": 2000, "max_bw": 35000},
    "5G": {"target_bw": 45000, "min_bw": 5000, "max_bw": 100000}
}


# 读取原始数据
def read_trace(filename):
    times, bws, lats, losses = [], [], [], []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            times.append(float(row['time (ms)']))
            bws.append(float(row['bandwidth (Kbps)']))
            lats.append(float(row['Latency (ms)']))
            losses.append(float(row['Loss rate (%)']))
    return times, bws, lats, losses


# 生成新trace
def generate_trace(orig_times, orig_bws, orig_lats, orig_losses, profile_name):
    profile = NETWORK_PROFILES[profile_name]

    # 计算原始平均带宽
    orig_avg_bw = np.mean(orig_bws)

    # 1. 带宽缩放和限制
    scale_factor = profile["target_bw"] / orig_avg_bw
    scaled_bws = [min(max(bw * scale_factor, profile["min_bw"]), profile["max_bw"])
                  for bw in orig_bws]

    # 2. 添加带宽噪声 (5%波动)
    noisy_bws = [bw * (1 + random.uniform(-0.05, 0.05)) for bw in scaled_bws]

    # 3. 延迟调整 (增加固定延迟+随机波动)
    base_latency = 50  # 固定增加的基准延迟
    adjusted_lats = [lat + base_latency + random.uniform(-10, 10)
                     for lat in orig_lats]

    # 4. 均匀随机丢包率 (0% ~ 3%)
    uniform_losses = [random.uniform(0.0, 3.0) for _ in orig_losses]

    # 5. 扩展数据到120s
    final_times = list(orig_times)
    final_bws = list(noisy_bws)
    final_lats = list(adjusted_lats)
    final_losses = list(uniform_losses)

    # 计算需要添加的数据点
    time_interval = 500  # 平均时间间隔(ms)
    last_time = orig_times[-1]
    end_time = 120000  # 120秒

    while last_time < end_time:
        # 随机选取一个时间点作为模板
        idx = random.randint(0, len(orig_times) - 1)

        # 更新时间
        last_time += time_interval * random.uniform(0.8, 1.2)
        final_times.append(last_time)

        # 复制并略微修改模板点
        final_bws.append(noisy_bws[idx] * random.uniform(0.9, 1.1))
        final_lats.append(adjusted_lats[idx] * random.uniform(0.9, 1.1))
        final_losses.append(min(5.0, uniform_losses[idx] * random.uniform(0.8, 1.2)))

    return final_times, final_bws, final_lats, final_losses


# 保存新trace
def save_trace(filename, times, bws, lats, losses):
    with open(filename, 'w', newline='') as f:
        # 写入标题行
        f.write(",time (ms),bandwidth (Kbps),Latency (ms),Loss rate (%)\n")

        # 写入数据行
        for i, (t, b, l, loss) in enumerate(zip(times, bws, lats, losses)):
            # 使用字符串格式化确保精度，不添加额外引号
            line = f"{i},{t:.1f},{b:.1f},{l:.3f},{loss:.3f}\n"
            f.write(line)


# 主处理流程
def process_trace(input_file):
    # 读取原始数据
    orig_times, orig_bws, orig_lats, orig_losses = read_trace(input_file)

    # 生成3G/4G/5G trace
    for profile in NETWORK_PROFILES.keys():
        new_times, new_bws, new_lats, new_losses = generate_trace(
            orig_times, orig_bws, orig_lats, orig_losses, profile)
        save_trace(f"network_trace_{profile}.csv",
                   new_times, new_bws, new_lats, new_losses)


# 运行处理
process_trace("LTE_CT/trace.csv")