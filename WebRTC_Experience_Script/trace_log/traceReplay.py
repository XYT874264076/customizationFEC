import csv
import time
import argparse

import subprocess

def run_without_error(cmd):
    try:
        subprocess.run(cmd, shell=True, check=True);
    except Exception as e:
        pass;

def apply_tc_settings(interface, bandwidth, delay, loss, st_time, limit):
    curTime=time.time();
    cmd = f"tc qdisc replace dev {interface} root handle 1: tbf rate {bandwidth}kbit burst 256kbit latency 0.1ms";
    print("cur:{} set:".format(curTime - st_time), cmd);
    run_without_error(cmd);
    cmd = f"tc qdisc replace dev {interface} parent 1:1 handle 10: netem delay {delay}ms loss 0% limit {limit}";
    print("cur:{} set:".format(curTime-st_time),cmd);
    run_without_error(cmd);
    cmd = f"tc qdisc replace dev {interface}_ifb root handle 1: tbf rate {bandwidth}kbit burst 256kbit latency 0.1ms";
    print("cur:{} set:".format(curTime - st_time), cmd);
    run_without_error(cmd);
    cmd = f"tc qdisc replace dev {interface}_ifb parent 1:1 handle 10: netem delay {delay}ms loss {loss}% limit {limit}";
    print("cur:{} set:".format(curTime-st_time),cmd);
    run_without_error(cmd);

def main(interface, inputFile, duration,limit):
    # 读取 CSV 文件
    with open(inputFile, 'r') as file:
        reader = csv.DictReader(file)
        network_conditions = list(reader)

    # 清除已有的 netem qdisc 和 过滤器
    run_without_error(f"tc qdisc del dev {interface} root");
    run_without_error(f"tc qdisc del dev {interface} ingress");
    run_without_error(f"ip link delete {interface}_ifb");

    # 创建初始的 netem qdisc 和 过滤器
    run_without_error(f"ip link add {interface}_ifb type ifb");
    run_without_error(f"ip link set {interface}_ifb up");
    run_without_error(f"tc qdisc add dev {interface} root handle 1: tbf rate 1000kbit burst 2566kbit latency 0.1ms");
    run_without_error(f"tc qdisc add dev {interface} parent 1:1 handle 10: netem delay 30ms loss 0% limit 1000");
    run_without_error(f"tc qdisc add dev {interface} handle ffff: ingress");
    run_without_error(f"tc filter add dev {interface} parent ffff: protocol ip u32 match u32 0 0 action mirred egress redirect dev {interface}_ifb");
    run_without_error(f"tc qdisc add dev {interface}_ifb root handle 1: tbf rate 1000kbit burst 256kbit latency 0.1ms");
    run_without_error(f"tc qdisc add dev {interface}_ifb parent 1:1 handle 10: netem delay 30ms loss 0% limit 1000");

    start_time = time.time()
    base_time = start_time
    end_time = start_time + duration
    max_time = 1;       #最小模拟单位为1ms

    while time.time() < end_time:
        for condition in network_conditions:
            current_time = time.time()
            elapsed_time = (current_time - base_time) * 1000  # 转换为毫秒
            max_time=max(max_time, float(condition['time (ms)']));
            while (elapsed_time > max_time):
                base_time=base_time+max_time/1000;
                elapsed_time = (current_time - base_time) * 1000  # 转换为毫秒
            # 计算需要等待的时间
            wait_time = float(condition['time (ms)']) - elapsed_time
            while wait_time>0:
                if wait_time > 0 and wait_time<500:
                    time.sleep(wait_time / 1000.0)
                else:
                    time.sleep(0.5);
                current_time = time.time()
                if (current_time > end_time): break;
                elapsed_time = (current_time - base_time) * 1000  # 转换为毫秒
                wait_time = float(condition['time (ms)']) - elapsed_time
            bandwidth = condition['bandwidth (Kbps)']
            delay = condition['Latency (ms)']
            loss = condition['Loss rate (%)']
            apply_tc_settings(interface, bandwidth, delay, loss ,start_time,limit)
            if (time.time()>end_time):break;

    # 删除 netem qdisc
    run_without_error(f"tc qdisc del dev {interface} root");
    run_without_error(f"tc qdisc del dev {interface} ingress");
    run_without_error(f"ip link delete {interface}_ifb");

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Simulate network conditions using tc.')
    parser.add_argument('-i', '--interface', required=True, help='Network interface to apply tc settings')
    parser.add_argument('-f', '--inputFile', required=True, help='CSV file with network conditions')
    parser.add_argument('-d', '--duration', type=int, required=True, help='Duration to run the script in seconds')
    parser.add_argument('-l', '--limit', type=int, default=1000, help='Queue limit in packets (default: 1000)')

    args = parser.parse_args()
    main(args.interface, args.inputFile, args.duration,args.limit)