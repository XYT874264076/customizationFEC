import argparse
import os
import signal
import subprocess
import threading
import time
import pytz
from datetime import datetime

def run_trace_replay(trace_file, net_type, net_device, duration, trace_log_file):
    trace_file_path = os.path.join("./trace_log",net_type,trace_file);
    cmd = f"python3 ./trace_log/traceReplay.py -f {trace_file_path} -d {duration} -i {net_device}";
    print(cmd);
    result=subprocess.run(cmd, shell=True, capture_output=True, text=True);
    with open(trace_log_file, "a") as f:
        f.write(result.stdout);

def run_WebRTC_peer(namespace, exec_file, connect_to, port, playVideo, duration, interval, output_dir, exp_type, output_std, output_log, fec_rate, fec_num):
    cmd = f"xvfb-run -a ip netns exec {namespace} bash -c 'export PULSE_SERVER=unix:/tmp/pulse-native; \
            {exec_file} --playVideo {playVideo} --duration {duration} --interval {interval} --output {output_dir} \
            --type {exp_type} --connect {connect_to} --port {port} --FECRate {fec_rate} --FECNum {fec_num} --ifTrainI true --ifSaveI true --ifTrainM true --ifSaveM true'";

    # cmd = f"ip netns exec {namespace} bash -c 'export PULSE_SERVER=unix:/run/user/1000/pulse/native; export PULSE_COOKIE=/run/user/1000/pulse/cookie; \
    #         {exec_file} --playVideo {playVideo} --duration {duration} --interval {interval} --output {output_dir} \
    #         --type {exp_type} --connect {connect_to} --port {port} --FECRate {fec_rate} --FECNum {fec_num} --ifTrainI true --ifSaveI true --ifTrainM true --ifSaveM true'";

    print(cmd);
    result=subprocess.run(cmd, shell=True, capture_output=True, text=True);
    with open(output_std, "a") as f:
        f.write(result.stdout);

    with open(output_log, "a") as f:
        f.write(result.stderr);

def run_plot_graph_1(epoch_dir, output):
    cmd = f"python3 ./result_plt/drawPic1_FECbps_jBuffer.py --data-dir {epoch_dir} --output {output}";
    print(cmd);
    result=subprocess.run(cmd, shell=True, capture_output=True, text=True);

def run_plot_graph_2(epoch_dir, output):
    cmd = f"python3 ./result_plt/drawPic2_VideoPackets_pauseTime.py --data-dir {epoch_dir} --output {output}";
    print(cmd);
    result=subprocess.run(cmd, shell=True, capture_output=True, text=True);

def run_calc_avg_bandwidth(tlog_dir, traceInfo,output):
    cmd = f"python3 ./static/avgBandwidth.py --tlog-dir {tlog_dir} --traceInfo {traceInfo}";
    print(cmd);
    result=subprocess.run(cmd, shell=True, capture_output=True, text=True);
    with open(output,"a") as f:
        f.write(result.stdout);

def force_terminate_ns_thread(namespace):
    pids = subprocess.check_output(["ip", "netns", "pids", f"{namespace}"]).decode().split();
    for pid in pids:
        try:
            os.kill(int(pid), signal.SIGINT)
        except ProcessLookupError:
            # Maybe the process has finished!
            pass

def force_terminate_port_thread(port):
    # Use lsof to find the specified process ID which own this port!
    cmd = f"lsof -t -i:{port}"
    result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if result.returncode != 0 or not result.stdout.strip():
        return

    # Get PID
    pid = int(result.stdout.strip())

    # Terminate the process!
    os.kill(pid, signal.SIGKILL)

def run_signal_server(ip_addr, port):
    cmd = f"node ./SignalServer/signal.js";
    print(cmd);
    process=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True);
    process.wait();

def main():
    parser = argparse.ArgumentParser(description="Run WebRTC Experience Single!");
    parser.add_argument('-conn', '--connect-to', type=str, required=False, default='192.168.0.1', help='Signal server IP address');
    parser.add_argument('-p', '--port', type=int, required=False, default=3000, help='Signal server Port');
    parser.add_argument('-v', '--playVideo', type=str, required=False, default='/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo1.mp4,/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo2.mp4', help='Video contents transmitted by two peer!');
    parser.add_argument('-d', '--duration', type=int, required=False, default=100, help='Duration of the network simulation');
    parser.add_argument('-i', '--interval', type=int, required=False, default=500, help='The interval between each json data collection');
    parser.add_argument('-bri', '--bridge-interface', type=str, required=False, default="br-veth1,br-veth2");
    parser.add_argument('-net', '--network-type', type=str, required=False, default='Test100M,Test100M', help='Type of the network environments for two peers, split with ,');
    parser.add_argument('-t', '--exp-type', type=str, required=False, default='WebRTCSource', help='Type of the experience type');
    parser.add_argument('-dir', '--base-dir', type=str, required=False, default='./', help='Base dictionary (default ./WebRTC-[timestamp])');
    parser.add_argument('-f', '--traceFile', type=str, required=False, default='trace.csv,trace.csv', help='Trace file for network simulation for each peer, split with ,');
    parser.add_argument('-n', '--epoch', type=int, required=False, default=1, help='Number of experiment groups(1<=epoch<=100)');
    parser.add_argument('-exe', '--executable-file', type=str, required=False, default='../out/customizationFEC/customizationFEC' ,help='executable files path, that is customizationFEC file path!');
    parser.add_argument('-ns', '--namespace', type=str, required=False, default='client1,client2', help='The namespace for each peers, split with ,');
    parser.add_argument('-fr', '--fec-rate', type=str, required=False, default='0.16', help='The FECRate, split with ,');
    parser.add_argument('-fn', '--fec-num', type=str, required=False, default='2', help='The FECNum, split with ,');
    args = parser.parse_args();

    if (os.geteuid() != 0):
        print("Please run this script as sudoer!");
        return -1;

    tFLen=len(args.traceFile.split(","));
    ifLen=len(args.bridge_interface.split(","));
    ntLen=len(args.network_type.split(","));
    if (tFLen != ifLen or tFLen!=ntLen):
        print("Invalid number of traceFile, bridge-interface and network-type");
        print("Please make sure the number of this three argument are the same");

    if (args.epoch<1 or args.epoch>100):
        print ("Invalid number of epochs");
        print("Please make sure 1<=epoch<=100");

    timestamp = datetime.now(pytz.timezone('Asia/Shanghai')).strftime('%Y%m%d_%H%M%S');
    base_dir = "[default]";
    if (args.base_dir == "./"):
        base_dir = f"{args.base_dir}WebRTC-{timestamp}";
    else:
        base_dir = f"{args.base_dir}";
    os.makedirs(base_dir, exist_ok=True);

    for i in range(1, args.epoch + 1):
        print("============ Epoch {} ============".format(i));
        epoch_dir = os.path.join(base_dir, str(i));
        os.makedirs(epoch_dir, exist_ok=True);

        peer1_dir = os.path.join(epoch_dir, "peer1");
        peer2_dir = os.path.join(epoch_dir, "peer2");
        os.makedirs(peer1_dir, exist_ok=True);
        os.makedirs(peer2_dir, exist_ok=True);
        peer1_output_std = os.path.join(epoch_dir, "peer1_output_std.txt");
        peer2_output_std = os.path.join(epoch_dir, "peer2_output_std.txt");
        peer1_output_log = os.path.join(epoch_dir, "peer1_output_log.txt");
        peer2_output_log = os.path.join(epoch_dir, "peer2_output_log.txt");
        time_base = os.path.join(epoch_dir, "time_base.txt");
        traceInfo = os.path.join(epoch_dir, "traceInfo");
        os.makedirs(traceInfo, exist_ok=True);
        graph_output = os.path.join(epoch_dir, "graphs");
        os.makedirs(graph_output, exist_ok=True);
        # static_output = os.path.join(epoch_dir, "static");
        # os.makedirs(static_output, exist_ok=True);
        # static_file = os.path.join(static_output, "static.txt");

        threads=[];

        #run tquic_server
        print("[run SignalServer]");
        run_signal_server_thread = threading.Thread(target=run_signal_server, args=(args.connect_to, args.port));
        run_signal_server_thread.start();

        # replay network trace for each peer
        print("[run trace replay]");
        networkTypeList=args.network_type.split(",");
        interfaceList=args.bridge_interface.split(",");
        traceFileList=args.traceFile.split(",");
        for traceFile,networkType,interface,peerIndex in zip(traceFileList,networkTypeList,interfaceList,range(1,len(interfaceList)+1)):
            trace_log_file = os.path.join(traceInfo, f"peer{peerIndex}_network.log");
            trace_replay_thread = threading.Thread(target=run_trace_replay, args=(traceFile, networkType, interface, args.duration, trace_log_file));
            trace_replay_thread.start();
            threads.append(trace_replay_thread);
        with open(time_base, 'a') as file:
            timestamp_in_seconds = time.time()
            zero_time = int(timestamp_in_seconds * 1_000_000)
            file.write(str(zero_time));
        time.sleep(5);

        # run WebRTC peers
        print("[run WebRTC peers]");
        playVideoList=args.playVideo.split(",");
        namespaceList=args.namespace.split(",");
        run_WebRTC_peer_thread1 = threading.Thread(target=run_WebRTC_peer,
                                  args=(namespaceList[0], args.executable_file, args.connect_to, args.port, playVideoList[0],
                                        args.duration-10, args.interval, peer1_dir, args.exp_type, peer1_output_std, peer1_output_log, args.fec_rate, args.fec_num));
        run_WebRTC_peer_thread1.start();

        run_WebRTC_peer_thread2 = threading.Thread(target=run_WebRTC_peer,
                                  args=(namespaceList[1], args.executable_file, args.connect_to, args.port, playVideoList[1],
                                        args.duration-10, args.interval, peer2_dir, args.exp_type, peer2_output_std, peer2_output_log, args.fec_rate, args.fec_num));
        run_WebRTC_peer_thread2.start();

        # wait for network trace replay finish!
        for thread in threads:
            thread.join();

        # Now we try to terminate signal.js and two peers!
        force_terminate_ns_thread(namespaceList[0]);
        force_terminate_ns_thread(namespaceList[1]);
        force_terminate_port_thread(args.port);

        print("[run draw graphs]");
        threads.clear();
        run_plot_thread=threading.Thread(target=run_plot_graph_1, args=(epoch_dir,graph_output));
        run_plot_thread.start();
        threads.append(run_plot_thread);

        run_plot_thread=threading.Thread(target=run_plot_graph_2, args=(epoch_dir,graph_output));
        run_plot_thread.start();
        threads.append(run_plot_thread);

        # print("[run calc static]");
        # threads.clear();
        # calc_static_thread=threading.Thread(target=run_calc_avg_bandwidth, args=(tlog_dir,traceInfo,static_file));
        # calc_static_thread.start();
        # threads.append(calc_static_thread);

        for thread in threads:
            thread.join();

        print();

if __name__ == "__main__":
    main();