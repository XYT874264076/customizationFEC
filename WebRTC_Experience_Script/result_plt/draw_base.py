import os;
import argparse;
import matplotlib.pyplot as plt;
import pandas as pd;

def auto_label(ax, bars, c):
    for bar in bars:
        height = bar.get_height()
        if (height !=0):
            ax.annotate(f'{height}% loss',
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', color=c, rotation=90);

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
                rate = float(parts[12].replace('kbit', ''))
                rates.append(rate)
            elif (index %4 ==1) :
                parts = line.split()
                delay = float(parts[13].replace('ms', ''))
                delays.append(delay)
                loss = float(parts[15].replace('%', ''))
                losses.append(loss)

    return timestamps, rates, delays, losses

def plot_graph(path, save_to):
    base_time=0;
    print(os.path.join(path, 'time_base.txt'));

    with open(os.path.join(path, 'time_base.txt')) as f:
        fs=f.readline();
        base_time=eval(fs);

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
    print(timeline1);
    print(sent_video_bps);
    print(sent_nack_bps);
    print(sent_fec_bps);
    print(total_sent_bps);

    # Get inbound.csv data
    data2 = pd.read_csv(os.path.join(path,'peer2/inbound.csv'));
    print(data2.columns);
    timeline2=data2['timestamp'].to_list();
    timeline2=[(i-base_time)/1e6 for i in timeline2];
    recvBytes=data2['bytesReceived'].to_list();
    recvbits=[i*8/1e3 for i in recvBytes];
    jitterBuffer=data2['jitterBufferDelay'].to_list();

    print(timeline2);
    total_recv_bps=[];
    for i in range(0,len(recvbits)):
        if (i==0):total_recv_bps.append(recvbits[i]/timeline2[i]);
        else:total_recv_bps.append((recvbits[i]-recvbits[i-1])/(timeline2[i]-timeline2[i-1]));
    print(total_recv_bps);

    # Get traceInfo data
    trace_path = os.path.join(path, 'traceInfo/peer1_network.log');
    trace_timeline, trace_rate, trace_delay, trace_loss = read_trace_file(trace_path);
    print(trace_timeline);
    print(trace_rate);
    print(trace_delay);
    print(trace_loss);

    # Draw graph
    fig, ax1 = plt.subplots(figsize=(24, 16), dpi=300)
    bps_colors = ['darkgreen', 'darkgoldenrod', 'purple', 'c', 'm', 'y']
    ax1.plot(timeline1, sent_video_bps, linestyle='-', linewidth=2, color=bps_colors[0], label=f'sent_video_rate_bps (Mbps)', zorder=10);
    ax1.plot(timeline1, sent_nack_bps, linestyle='-', linewidth=2, color=bps_colors[1], label=f'sent_nack_bps (Mbps)', zorder=10);
    ax1.plot(timeline1, sent_fec_bps, linestyle='-', linewidth=2, color=bps_colors[2], label=f'sent_fec_bps (Mbps)', zorder=10);
    ax1.plot(timeline1, total_sent_bps, linestyle='-', linewidth=5, color="darkred", alpha=0.5, label="sum throughput (Mbps)", zorder=5);

    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Throughput (Mbit/s)')
    ax1.legend(loc='upper left')

    ax2 = ax1.twinx()
    ax3 = ax1.twinx();

    max_rate = 0;
    max_delay = 0;

    ax1.plot(trace_timeline, trace_rate, linestyle='-', linewidth=0.01, color='darkblue', label=f'bandwidth (Mbps)', zorder=10);
    ax1.fill_between(trace_timeline, trace_rate, color='darkblue', alpha=0.2, zorder=1)  # Fill the area under the bandwidth curve
    ax2.plot(trace_timeline, trace_delay, linestyle='dashed', linewidth=0.5, color='black', label=f'delay (ms)')
    bars = ax3.bar(trace_timeline, trace_loss, color='grey', alpha=0.4, zorder=1, edgecolor='grey', linewidth=1.5);
    ax3.set_yticklabels([]);
    ax3.set_yticks([]);
    ax3.set_ylim(0, 100);
    # auto_label(ax3, bars, 'black');
    max_rate = max(max_rate, max(trace_rate));
    max_delay = max(max_delay, max(trace_delay));
    max_rate = max(max_rate, max(total_sent_bps));

    ax1.set_ylim(0, max_rate * 1.4);
    ax2.set_ylim(0, max_delay * 1.4);

    ax2.set_ylabel('Delay (ms)')
    ax2.legend()

    plt.title('Network Throughput and Bandwidth')

    if save_to:
        os.makedirs(os.path.dirname(save_to), exist_ok=True);
        file_name = os.path.join(save_to, f'throughput.png');
        plt.savefig(file_name)
        print(f"Plot saved to {file_name}");
    else:
        plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot pic1, three bps and FEC bps with jitterBufferDelay.')
    parser.add_argument('--data-dir', required=False, type=str, default='./', help='Directory containing throughput log files.')
    parser.add_argument('--output', required=False, type=str, default='./', help='Output file to save the plot.')
    args = parser.parse_args()

    plot_graph(args.data_dir, args.output);
