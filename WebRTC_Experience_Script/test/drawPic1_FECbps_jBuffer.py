import os;
import argparse;
import matplotlib.pyplot as plt;
import pandas as pd;

def plot_graph(path, save_to):
    base_time=1734078056408733;
    # with open(os.path.join(path, 'time_base.txt')) as f:
    #     fs=f.readline();
    #     base_time=eval(fs);

    # Get timeline1 with sent_video_rate_bps,sent_nack_rate_bps, sent_fec_rate_bps
    data1 = pd.read_csv(os.path.join(path,'peer1/rtp_video_sender.csv'));
    print(data1.columns);
    timeline1=data1['timestamp'].to_list();
    timeline1=[(i-base_time)/1e6 for i in timeline1];
    sent_video_bps=data1['sent_video_rate_bps'].to_list();
    sent_video_bps=[(i/1e3) for i in sent_video_bps];
    sent_nack_bps=data1['sent_nack_rate_bps'].to_list();
    sent_nack_bps=[(i/1e3) for i in sent_nack_bps];
    sent_fec_bps=data1['sent_fec_rate_bps'].to_list();
    sent_fec_bps=[(i/1e3) for i in sent_fec_bps];
    total_sent_bps=[i+j+k for (i,j,k) in zip(sent_video_bps,sent_nack_bps,sent_fec_bps)];
    # print(timeline1);
    # print(sent_video_bps);
    # print(sent_nack_bps);
    # print(sent_fec_bps);
    print(total_sent_bps);

    # Get inbound.csv data
    data2 = pd.read_csv(os.path.join(path,'peer2/inbound.csv'));
    timeline2=data2['timestamp'].to_list();
    timeline2=[(i-base_time)/1e6 for i in timeline2];
    recvBytes=data2['bytesReceived'].to_list();
    recvbits=[i*8/1e3 for i in recvBytes];
    # print(timeline2);
    total_recv_bps=[];
    for i in range(0,len(recvbits)):
        if (i==0):total_recv_bps.append(recvbits[i]/timeline2[i]);
        else:total_recv_bps.append((recvbits[i]-recvbits[i-1])/(timeline2[i]-timeline2[i-1]));
    print(total_recv_bps);
    for (i,j) in zip(total_sent_bps,total_recv_bps):
        print(int(i),int(j));


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot pic1, three bps and FEC bps with jitterBufferDelay.')
    parser.add_argument('--data-dir', required=False, type=str, default='./', help='Directory containing throughput log files.')
    parser.add_argument('--output', required=False, type=str, default='./', help='Output file to save the plot.')
    args = parser.parse_args()

    plot_graph(args.data_dir, args.output);
