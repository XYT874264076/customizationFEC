import os
import subprocess
import time
import re
import threading
import datetime;
from queue import Queue
import pandas as pd;
import argparse;

def get_ping_stats(target,curData,timestamp, count=10):
        result = subprocess.run(['ping', target, '-c', str(count), '-i', '0.21'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        output = result.stdout
        print(output);
        if result.returncode == 0:
            # Extract latency (average)
            latency = re.findall(r'rtt min/avg/max/mdev = [\d\.]+/[\d\.]+/[\d\.]+/[\d\.]+ ms', output)
            latency = float(latency[0].split(" ")[3].split("/")[1]) if latency else None
            print("time",timestamp,"letency",latency);
            curData.loc[curData["time (ms)"]==timestamp,"Latency (ms)"] = latency;
            # Extract packet loss
            packet_loss = re.findall(r'(\d+)% packet loss', output)
            packet_loss = float(packet_loss[0]);
            print("time",timestamp,"packet_loss", packet_loss);
            curData.loc[curData["time (ms)"]==timestamp,"Loss rate (%)"]=packet_loss;

def get_bandwidth(target,curData,duration,interval,timestamps):
        result = subprocess.run([r'iperf3', '-c', target, '-t', str(duration),'-i',str(interval)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        output = result.stdout
        if result.returncode == 0:
            # Extract bandwidth
            bandwidth = re.findall(r'(\d+\.\d+ \w+/sec|\d+ \w+/sec)', output)
            print("bandwidth",bandwidth);
            for (curTime,curBandwidth) in zip(timestamps,bandwidth):
                curBandwidthData=float(curBandwidth.split(" ")[0])*1000;
                curData.loc[curData["time (ms)"]==curTime,"bandwidth (Kbps)"]=curBandwidthData;

def measure_and_log(target, curData, curTime,timestamps):
    timestamp=round(curTime*1000,0);
    timestamps.append(timestamp);
    curData.loc[len(curData)]=[timestamp,None,None,None];
    subthread1=threading.Thread(target=get_ping_stats,args=(target, curData, timestamp));
    threads.append(subthread1);
    subthread1.start();

def traceMain(target, curData,duration,interval):
    # start iperf3 to record bandwidth!
    timestamps=[];
    thread=threading.Thread(target=get_bandwidth,args=(target,curData,duration,interval,timestamps))
    threads.append(thread);
    thread.start();

    # start ping to record delay and packet loss rate
    times=duration//interval;
    while times:
        times-=1;
        curTime=time.time()-start_time;
        thread = threading.Thread(target=measure_and_log, args=(target, curData, curTime,timestamps))
        threads.append(thread);
        thread.start()
        time.sleep(interval);

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run traceMain with specified parameters.")
    parser.add_argument("-d", "--duration", type=float, required=False, help="Duration of the test in seconds.", default=100)
    parser.add_argument("-i", "--interval", type=float, required=False, help="Interval for periodic reports in seconds.", default=0.5)
    parser.add_argument("-t", "--target", type=str, required=False, help="Target IP address or hostname.", default="212.64.18.128");
    parser.add_argument("-f", "--file", type=str, required=False, help="Output file to save the results.", default="trace.csv");
    parser.add_argument("-o", "--output-dir", type=str, required=False, help="Output dictionary to save the results", default="default");

    args = parser.parse_args()

    threads=[];
    duration=args.duration;
    interval=args.interval;
    start_time = time.time();
    target = args.target;
    os.mkdir(args.output_dir);
    output_file = args.output_dir+"/"+args.file;
    curData=pd.DataFrame({'time (ms)':[], 'bandwidth (Kbps)':[], 'Latency (ms)':[], 'Loss rate (%)':[]});
    traceMain(target, curData,duration,interval);
    for thread in threads:
        thread.join();
    print(curData);
    curData.to_csv(output_file);
