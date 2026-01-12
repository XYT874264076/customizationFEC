import argparse
import os
import subprocess
import threading
import time

import pandas as pd
import numpy as np
import pytz
from datetime import datetime

def get_simple_video_name(nowstr):
    res="";
    strs=nowstr.split(",");
    for now in strs:
        if (res==""):
            res=res+now.split('/')[-1];
        else:
            res=res+"_"+now.split('/')[-1];
    return res;

def run_without_error(cmd):
    try:
        subprocess.run(cmd, shell=True, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL);
    except Exception as e:
        pass;

def run_WebRTC_single_exp_main(connect_to, port, playVideo, duration, interval, bridge_interface, network_type,
                               exp_type, base_dir, trace_file, epoch, exe_file, namespace, fec_rate=0.18, fec_num=2):
    cmd = (f"python3 WebRTC_single_exp_main.py -conn {connect_to} -p {port} "
           f"-v {playVideo} -d {duration} -i {interval} "
           f"-bri {bridge_interface} -net {network_type} -t {exp_type} "
           f"-dir {base_dir} -f {trace_file} -n {epoch} "
           f"-exe {exe_file} -ns {namespace} -fr {fec_rate} -fn {fec_num}");
    print(cmd);
    print();
    process=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True);

    try:
        for line in process.stdout:
            print(f"\t{line}");
    except:
        print(f"\tException?");

    process.wait();

def run_static_calc_df(base_dirs, output):
    cmd = (f"python3 ./static/calcDataFrame.py -dirs {base_dirs} -out {output}");
    print(cmd);
    print();
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True);

    for line in process.stdout:
        print(f"\t{line}");

    process.wait();


def print_xlsx(path,output, exp_type, base_dir):
    cmd = (f"python3 ./result_plt/drawTable.py -s {path} -out {output} -t {exp_type} -dir {base_dir}");
    print(cmd);
    print();
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True);

    for line in process.stdout:
        print(f"\t{line}");

    process.wait();

def drawFinalResult(xlsx_file, output):
    cmd = (f"python3 ./result_plt/drawFinalBar.py -f {xlsx_file} -o {output}");
    print(cmd);
    print();
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True);

    for line in process.stdout:
        print(f"\t{line}");

    process.wait();

def print_line(str):
    n=100;
    line_len=n-len(str);
    for i in range(line_len//2):
        print("=",end='');
    if (line_len%2==1):
        print("=", end='');
    print(str,end='');
    for i in range(line_len//2):
        print("=",end='');
    print();
    print();

def delete_exsit_setting(bri, ns):
    #首先，按顺序清除 tc 重定向配置：
    for dev in bri:
        cmd = f"tc qdisc del dev {dev}-ifb root";
        run_without_error(cmd);
    for dev in bri:
        cmd = f"tc qdisc del dev {dev} ingress";
        run_without_error(cmd);
    for dev in bri:
        cmd = f"ip link del {dev}-ifb";
        run_without_error(cmd);

    #然后，清除桥接设备的 tc 配置：
    for dev in bri:
        cmd = f"tc qdisc del dev {dev} root";
        run_without_error(cmd);

    #然后，清除桥接设备:
    for dev in bri:
        cmd = f"ip link del {dev}";
        run_without_error(cmd);

    #清除网桥：
    cmd = "ip link del br0";
    run_without_error(cmd);

    #清除命名空间:
    for name in ns:
        cmd = f"ip netns del {name}";
        run_without_error(cmd);

def build_network_setting(bri, nsi, ns, pips, sip):

    # 创建虚拟网卡
    for (ns_dev, br_dev) in zip(nsi, bri):
        cmd = f"ip link add {ns_dev} type veth peer {br_dev}";
        run_without_error(cmd);

    #启动网卡
    for ns_dev in nsi:
        cmd = f"ip link set {ns_dev} up";
        run_without_error(cmd);
    for br_dev in bri:
        cmd = f"ip link set {br_dev} up";
        run_without_error(cmd);

    #构建命名空间
    for name in ns:
        cmd = f"ip netns add {name}";
        run_without_error(cmd);

    #网卡 插入到 namespace 中
    for ns_dev,name in zip(nsi,ns):
        cmd = f"ip link set {ns_dev} netns {name}";
        run_without_error(cmd);

    #构建网桥（虚拟交换机）
    cmd = "ip link add name br0 type bridge";
    run_without_error(cmd);

    #启动网桥
    cmd = "ip link set br0 up";
    run_without_error(cmd);

    #网桥卡插入到网桥中
    for br_dev in bri:
        cmd = f"ip link set {br_dev} master br0";
        run_without_error(cmd);

    #分配 client 端 ip 地址
    for ns_dev,name,ip in zip(nsi,ns,pips):
        cmd = f"ip netns exec {name} ifconfig {ns_dev} {ip}/24";
        run_without_error(cmd);

    # 分配 signal server ip 地址
    cmd = f"ip addr add {sip}/24 dev br0";
    run_without_error(cmd);

    return;

def main():
    parser = argparse.ArgumentParser(description="Run WebRTC Experience All!");
    parser.add_argument('-sip', '--signal-server-ip', type=str, required=False, default='192.168.0.1', help='Signal server IP address');
    parser.add_argument('-pips', '--peer-ips', type=str, required=False, default='192.168.0.2,192.168.0.3', help='Peers IP addresses');
    parser.add_argument('-p', '--port', type=str, required=False, default=3000, help='Signal server port');
    parser.add_argument('-vs', '--playVideos', type=str, required=False, default='/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo1.mp4,/home/ubuntu/Desktop/MyFECExp/testVideo/testVideo2.mp4', help='Video contents transmitted by two peers! For two peers, split with ,. For each experience, split with ;')
    # parser.add_argument('-d', '--duration', type=int, required=False, default=400, help='Duration of the network simulation');
    parser.add_argument('-d', '--duration', type=int, required=False, default=120, help='Duration of the network simulation');
    parser.add_argument('-i', '--interval', type=int, required=False, default=500, help='The interval between each json data collection');
    parser.add_argument('-nsi', '--namespace-interface', type=str, required=False, default='veth1,veth2');
    parser.add_argument('-bri', '--bridge-interface', type=str, required=False, default="br-veth1,br-veth2");
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='TestLoss200K,Test200K;TestLoss5M,Test5M', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='TestLossP2_1M,TestLossP2_1M;TestLossP4_1M,TestLossP4_1M;TestLossP6_1M,TestLossP6_1M;TestLossP8_1M,TestLossP8_1M;TestLossP10_1M,TestLossP10_1M', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='TestLossP2_1M_50MS,TestLossP2_1M_50MS;TestLossP4_1M_50MS,TestLossP4_1M_50MS;TestLossP6_1M_50MS,TestLossP6_1M_50MS;TestLossP8_1M_50MS,TestLossP8_1M_50MS;TestLossP10_1M_50MS,TestLossP10_1M_50MS', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='TestLossP10_1M_50MS,TestLossP10_1M_50MS;UnStableBTW,UnStableBTW', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='dense_indoor,dense_indoor;walking,walking;driving,driving;high_speed,high_speed;LTE_2025,LTE_2025;NR_2025,NR_2025', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    parser.add_argument('-net', '--network-type', type=str, required=False, default='LTE_2025_New,LTE_2025_New', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-net', '--network-type', type=str, required=False, default='driving,driving;high_speed,high_speed', help='Type of the network environments for two peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-ts', '--exp-types', type=str, required=False, default='WebRTCSource;RSFECBlock;RSFECStreamStableRate;FECClose', help='Type of the experience types, split with ;');
    parser.add_argument('-ts', '--exp-types', type=str, required=False, default='RSFECStreamStableRate', help='Type of the experience types, split with ;');
    # parser.add_argument('-ts', '--exp-types', type=str, required=False, default='WebRTCSource;RSFECBlock;RSFECStreamStableRate', help='Type of the experience types, split with ;');
    parser.add_argument('-f', '--traceFile', type=str, required=False, default='trace.csv,trace.csv;trace.csv,trace.csv;trace.csv,trace.csv;trace.csv,trace.csv;trace.csv,trace.csv;trace.csv,trace.csv', help='Trace file for network simulation for each peers. For each peers, split with ,. For each experience, split with ;');
    # parser.add_argument('-f', '--traceFile', type=str, required=False, default='trace.csv,trace.csv', help='Trace file for network simulation for each peers. For each peers, split with ,. For each experience, split with ;');

    parser.add_argument('-n', '--epoch', type=int, required=False, default=1, help='Number of experiment groups(1<=epoch<=10)');
    parser.add_argument('-exe', '--executable-file', type=str, required=False, default='../out/customizationFEC/customizationFEC' ,help='executable files path, that is MyFECExp file path!');
    parser.add_argument('-ns', '--namespace', type=str, required=False, default='client1,client2', help='The namespace for each peers, split with ,');
    # parser.add_argument('-fr', '--fec-rate', type=str, required=False, default='0.1,0.12,0.14,0.16,0.18,0.2,0.22,0.24', help='The FECRate, split with ,');
    # parser.add_argument('-fr', '--fec-rate', type=str, required=False, default='0.03,0.06,0.09,0.12', help='The FECRate, split with ,');
    parser.add_argument('-fr', '--fec-rate', type=str, required=False, default='0.18', help='The FECRate, split with ,');
    # parser.add_argument('-fn', '--fec-num', type=str, required=False, default='1,2,3', help='The FECNum, split with ,');
    parser.add_argument('-fn', '--fec-num', type=str, required=False, default='1', help='The FECNum, split with ,');
    args = parser.parse_args();

    if (os.geteuid() != 0):
        print("Please run this script as sudoer!");
        return -1;

    vs=args.playVideos.split(";");
    network_type=args.network_type.split(";");
    ts=args.exp_types.split(";");
    trace_file=args.traceFile.split(";");
    frl=args.fec_rate.split(",");
    fnl=args.fec_num.split(",");

    ns=args.namespace.split(',');
    bri=args.bridge_interface.split(',');
    nsi=args.namespace_interface.split(',');
    pips=args.peer_ips.split(',');
    sip=args.signal_server_ip;

    delete_exsit_setting(bri, ns);
    build_network_setting(bri,nsi,ns,pips,sip);

    print_line(" [Current Experiment Settings] ");
    print("video to be played:");
    print('\t', vs);
    print();
    print("network type:")
    print('\t', network_type);
    print();
    print("experience type:");
    print('\t', ts);
    print();
    print("fec rate:");
    print('\t', frl);
    print();
    print("fec num:");
    print("\t", fnl);
    print();

    print_line(" [Current Network Settings] ");
    print("current namespace interfaces:");
    print('\t', nsi);
    print();
    print("current peer ip address:")
    print('\t', pips);
    print();
    print("current signal server ip address:")
    print('\t', sip);
    print();

    vs_name = [get_simple_video_name(strn) for strn in vs];
    network_name = [stri.replace(',', '_') for stri in network_type];
    ts_name = ts;

    timestamp = datetime.now(pytz.timezone('Asia/Shanghai')).strftime('%Y%m%d_%H%M%S');
    vs_join="+".join(vs_name);
    network_join="+".join(network_name);
    ts_join="+".join(ts_name);
    fn_join="+".join(fnl);
    fr_join="+".join(frl);
    base_dir_name = f"WebRTC-{timestamp}-[{ts_join}]";
    print(f"Results save to \'{base_dir_name}\'");
    print();
    os.makedirs(base_dir_name,exist_ok=True);
    sub_dirs=[];

    for net_i,now_net,now_tf in zip(network_name,network_type,trace_file):
        for vs_i,now_vs in zip(vs_name, vs):
            for ts_i in ts_name:
                # if ts_i == 'RSFECStreamStableRate' :
                #     fec_rate_list = args.fec_rate.split(',');
                #     fec_num_list = args.fec_num.split(',');
                #     for fr in fec_rate_list:
                #         for fn in fec_num_list:
                #             print_line(f" running [{vs_i}]-[{ts_i}]-[{net_i}]-[{fr}]-[{fn}] ")
                #             sub_dir_name = f"{base_dir_name}/[{vs_i}]-[{ts_i}]-[{net_i}]-[{fr}]-[{fn}]";
                #             sub_dirs.append(sub_dir_name);
                #             os.makedirs(sub_dir_name, exist_ok=True);
                #
                #             # for motivation evaluation!
                #             # curLr = now_net.split(",")[0].split("_")[0].split("P")[1];
                #             # temp_fr = eval(curLr)/100*3;
                #             temp_fr = 0.09;
                #             run_WebRTC_single_exp_main(sip, args.port, now_vs, args.duration, args.interval,
                #                                        args.bridge_interface, now_net, ts_i, sub_dir_name, now_tf,
                #                                        args.epoch, args.executable_file, args.namespace, temp_fr, fn);
                #
                #             # run_WebRTC_single_exp_main(sip, args.port, now_vs, args.duration, args.interval,
                #             #                            args.bridge_interface, now_net, ts_i, sub_dir_name, now_tf,
                #             #                            args.epoch, args.executable_file, args.namespace, fr, fn);
                # else:
                #     print_line(f" running [{vs_i}]-[{ts_i}]-[{net_i}] ");
                #     sub_dir_name=f"{base_dir_name}/[{vs_i}]-[{ts_i}]-[{net_i}]";
                #     sub_dirs.append(sub_dir_name);
                #     os.makedirs(sub_dir_name,exist_ok=True);
                #     run_WebRTC_single_exp_main(sip, args.port, now_vs, args.duration, args.interval,
                #                                args.bridge_interface, now_net, ts_i, sub_dir_name, now_tf,
                #                                args.epoch, args.executable_file, args.namespace);

                if ts_i == 'RSFECStreamStableRate':
                    for fr in [2]:
                        trace_file_path = os.path.join("./trace_log", now_net.split(",")[0], now_tf.split(",")[0]);
                        data = pd.read_csv(trace_file_path);
                        temp_fr = np.mean(data['Loss rate (%)'].to_list())*fr/100;
                        print_line(f" running [{vs_i}]-[{ts_i}{fr}]-[{net_i}] ");
                        sub_dir_name = f"{base_dir_name}/[{vs_i}]-[{ts_i}{fr}]-[{net_i}]";
                        sub_dirs.append(sub_dir_name);
                        os.makedirs(sub_dir_name, exist_ok=True);
                        run_WebRTC_single_exp_main(sip, args.port, now_vs, args.duration, args.interval,
                                                   args.bridge_interface, now_net, ts_i, sub_dir_name, now_tf,
                                                   args.epoch, args.executable_file, args.namespace, temp_fr, 1);
                else:
                    temp_fr = 0.135;
                    print_line(f" running [{vs_i}]-[{ts_i}]-[{net_i}] ");
                    sub_dir_name = f"{base_dir_name}/[{vs_i}]-[{ts_i}]-[{net_i}]";
                    sub_dirs.append(sub_dir_name);
                    os.makedirs(sub_dir_name, exist_ok=True);
                    run_WebRTC_single_exp_main(sip, args.port, now_vs, args.duration, args.interval,
                                               args.bridge_interface, now_net, ts_i, sub_dir_name, now_tf,
                                               args.epoch, args.executable_file, args.namespace, temp_fr, 1);


    # all_sub_dirs = ",".join(sub_dirs);
    # df_res_dir=f"{base_dir_name}/result.csv";
    # print_line(f" runing result dataFrame calculate! ");
    # run_static_calc_df(all_sub_dirs, df_res_dir);
    #
    # df_xlsx_dir=f"{base_dir_name}/final.xlsx";
    # print_line(f" saving final excel file ");
    # print_xlsx(df_res_dir, df_xlsx_dir, args.experience_type, args.base_dir);

    # drawFinalResult(df_xlsx_dir, f"{base_dir_name}/");

    delete_exsit_setting(bri, ns);

if __name__ == "__main__":
    main();