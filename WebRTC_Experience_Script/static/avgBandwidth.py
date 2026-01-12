import os
import argparse
import matplotlib.pyplot as plt

def read_throughput_file(filepath):
    timestamps = []
    throughputs = []
    with open(filepath, 'r') as file:
        for line in file:
            timestamp, throughput = map(float, line.split())
            timestamps.append(timestamp / 1000)  # Convert ms to seconds
            throughputs.append(throughput*8 / 1e6)  # Convert Bytes to Mbits
    return timestamps, throughputs

def read_trace_file(filepath):
    timestamps = []
    rates = []
    delays = []
    losses = []
    with open(filepath, 'r') as file:
        lines = file.readlines();
        for index, line in zip(range(len(lines)), lines):
            if (index % 4 ==0):
                parts = line.split()
                timestamp = float(parts[0].split(':')[1])
                timestamps.append(timestamp)
                rate = float(parts[12].replace('kbit', '')) / 1e3  # Convert kbit to Mbit
                rates.append(rate)
            elif (index %4 ==1) :
                parts = line.split()
                delay = float(parts[13].replace('ms', ''))
                delays.append(delay)
                loss = float(parts[15].replace('%', ''))
                losses.append(loss)

    return timestamps, rates, delays, losses

def trunc_len(timestamps, rates, delays, losses,max_num):
    newtimestamps=[];
    newRates=[];
    lastRate=0;
    newdelays=[];
    lastDelay=0;
    newLosses=[];
    lastLoss=0;
    for i in range(len(timestamps)):
        if timestamps[i]<max_num:
            newtimestamps.append(timestamps[i]);
            newRates.append(rates[i]);
            newdelays.append(delays[i]);
            newLosses.append(losses[i]);
            lastRate=rates[i];
            lastDelay=delays[i];
            lastLoss=losses[i];
        else:
            newtimestamps.append(max_num);
            newRates.append(lastRate);
            newdelays.append(lastDelay);
            newLosses.append(lastLoss);
    return newtimestamps, newRates, newdelays, newLosses;

def calc_avg_bandwidth(tlog_dir, trace_info):

    addr_files = [f for f in os.listdir(tlog_dir) if f.endswith('.down')]
    addr_files.sort()  # Ensure consistent color mapping
    max_num=0;
    cur_rates=[];
    cur_timestamps=[];
    cur_delays=[];
    for i, down_file in enumerate(addr_files):
        local_addr = down_file.split('.')[0]
        down_path = os.path.join(tlog_dir, down_file)
        timestamps, throughputs = read_throughput_file(down_path)
        max_num=max(max_num, max(timestamps));

    for i, down_file in enumerate(addr_files):
        local_addr = down_file.split('.')[0]
        local_addr = ".".join(local_addr.split("_")[:4]);
        trace_path = os.path.join(trace_info, f'network_{local_addr}.log')
        if os.path.exists(trace_path):
            timestamps, rates, delays, losses = read_trace_file(trace_path)
            cur_rates=rates;
            cur_timestamps=timestamps;
            cur_delays=delays;

    minrates=cur_rates[0];
    final_timestamps=[];
    final_rates=[];
    final_delays=[];
    trunc_index=0;
    for ts, rt,delays, index in zip(cur_timestamps, cur_rates,cur_delays,range(len(cur_timestamps))):
        if ts<5 :
            final_timestamps.append(5);
            final_rates.append(rt);
            final_delays.append(delays);
            trunc_index=index;
        else:
            final_timestamps.append(ts);
            final_rates.append(rt);
            final_delays.append(delays);
    final_timestamps=final_timestamps[trunc_index:];
    final_rates=final_rates[trunc_index:];
    final_delays=final_delays[trunc_index:];

    sumrate=0;
    sumdelays=0;
    sumTimes=final_timestamps[-1]-5;
    for i in range(len(final_timestamps)):
        if (i+1<len(final_timestamps)):
            sumrate=sumrate+(final_timestamps[i+1]-final_timestamps[i])*(final_rates[i+1]+final_rates[i])/2;
            sumdelays=sumdelays+(final_timestamps[i+1]-final_timestamps[i])*(final_delays[i+1]+final_delays[i])/2;
    return sumrate/sumTimes,sumdelays/sumTimes;

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Plot network throughput and bandwidth.')
    parser.add_argument('--tlog-dir', required=True, help='Directory containing throughput log files.')
    parser.add_argument('--traceInfo', required=True, help='Directory containing trace info files.')
    args = parser.parse_args()
    avg_bandwidth, avg_delays=calc_avg_bandwidth(args.tlog_dir, args.traceInfo);
    print(f"Average available bandwidth: {avg_bandwidth}Mbps");
    print(f"Average delays: {avg_delays}ms");
