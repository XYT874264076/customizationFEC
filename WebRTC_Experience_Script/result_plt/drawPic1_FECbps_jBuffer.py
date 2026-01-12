import os;
import argparse;
import matplotlib.pyplot as plt;
import pandas as pd;
from matplotlib.lines import lineStyles


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

def avg_smooth(arr,step=10):
    newarr=[];
    for i in range(len(arr)):
        sum=0;
        cnt=0;
        for j in range(max(0,i-step+1),i+1):
            sum+=arr[j];
            cnt+=1;
        newarr.append(sum/cnt);
    return newarr;

def plot_graph(path, save_to):
    base_time=0;

    with open(os.path.join(path, 'time_base.txt')) as f:
        fs=f.readline();
        base_time=eval(fs);

    # Get timeline1 with sent_video_rate_bps,sent_nack_rate_bps, sent_fec_rate_bps
    data1 = pd.read_csv(os.path.join(path,'peer1/rtp_video_sender.csv'));
    timeline1=data1['timestamp'].to_list();
    timeline1=[(i-base_time)/1e6 for i in timeline1];
    sent_video_bps=data1['sent_video_rate_bps'].to_list();
    sent_video_bps=[(i/1e3) for i in sent_video_bps];
    sent_nack_bps=data1['sent_nack_rate_bps'].to_list();
    sent_nack_bps=[(i/1e3) for i in sent_nack_bps];
    sent_fec_bps=data1['sent_fec_rate_bps'].to_list();
    sent_fec_bps=[(i/1e3) for i in sent_fec_bps];
    sent_video_bps=avg_smooth(sent_video_bps,5);
    sent_nack_bps=avg_smooth(sent_nack_bps,5);
    sent_fec_bps=avg_smooth(sent_fec_bps,5);
    total_sent_bps=[i+j+k for (i,j,k) in zip(sent_video_bps,sent_nack_bps,sent_fec_bps)];

    # Get inbound.csv data
    data2 = pd.read_csv(os.path.join(path,'peer2/inbound.csv'));
    timeline2=data2['timestamp'].to_list();
    timeline2=[(i-base_time)/1e6 for i in timeline2];
    recvBytes=data2['bytesReceived'].to_list();
    recvbits=[i*8/1e3 for i in recvBytes];
    jitterBuffer=data2['jitterBufferDelay'].to_list();
    framsReceive=data2['framesReceived'].to_list();
    frameWidth=data2['frameWidth'].to_list();
    frameHeight=data2['frameHeight'].to_list();

    total_recv_bps=[];
    for i in range(0,len(recvbits)):
        if (i==0):total_recv_bps.append(recvbits[i]/timeline2[i]);
        else:total_recv_bps.append((recvbits[i]-recvbits[i-1])/(timeline2[i]-timeline2[i-1]));

    # Get traceInfo data
    trace_path = os.path.join(path, 'traceInfo/peer1_network.log');
    trace_timeline, trace_rate, trace_delay, trace_loss = read_trace_file(trace_path);

    # calc effective FEC rate
    data3 = pd.read_csv(os.path.join(path, 'peer2/forward_error_correction.csv'));
    data4 = pd.read_csv(os.path.join(path, 'peer2/rtp_video_stream_receiver2.csv'));
    ulp_fec_seq_num=data3['FEC_seq_num'].to_list();
    ulp_fec_status_list=data3['status'].to_list();
    ulp_fec_payload_size=[];
    ulp_fec_status=[];
    ulp_fec_timeline=[];
    for now_num_sub,now_status_sub in zip(ulp_fec_seq_num,ulp_fec_status_list):
        for now_num in str(now_num_sub).split('_'):
            sel_data=data4[data4['seq_num']==int(now_num)];
            ulp_fec_timeline.append(sel_data['timestamp'].to_list()[0]);
            ulp_fec_payload_size.append(sel_data['payload_size'].to_list()[0]);
            ulp_fec_status.append(now_status_sub);
    ulp_fec_timeline = [(i - base_time) / 1e6 for i in ulp_fec_timeline];

    effective_ulp_fec_bps=[];
    failed_ulp_fec_bps=[];
    from_time=0;
    nowi=0;
    for to_time in timeline2:
        now_sum_success=0;
        now_sum_fail=0;
        while(nowi<len(ulp_fec_timeline) and ulp_fec_timeline[nowi]<=to_time):
            if (ulp_fec_status[nowi]=='success'):now_sum_success+=ulp_fec_payload_size[nowi];
            elif (ulp_fec_status[nowi][:4]=='fail'):now_sum_fail+=ulp_fec_payload_size[nowi];
            nowi+=1;
        effective_ulp_fec_bps.append(now_sum_success/(to_time-from_time));
        failed_ulp_fec_bps.append(now_sum_fail/(to_time-from_time));
        from_time=to_time;
    effective_ulp_fec_bps = [i*8/1e3 for i in effective_ulp_fec_bps];
    failed_ulp_fec_bps = [i * 8 / 1e3 for i in failed_ulp_fec_bps];
    # effective_ulp_fec_bps = avg_smooth(effective_ulp_fec_bps);
    failed_ulp_fec_bps = avg_smooth(failed_ulp_fec_bps,5);

    # calc avg jitterBuffer per frame
    jitterBufferPerFrame=[];
    for i in range(0, len(jitterBuffer)):
        if (i == 0):
            if (framsReceive[i]!=0):jitterBufferPerFrame.append(jitterBuffer[i] / framsReceive[i]);
            else:jitterBufferPerFrame.append(0);
        else:
            if ((framsReceive[i] - framsReceive[i - 1])!=0):jitterBufferPerFrame.append((jitterBuffer[i] - jitterBuffer[i - 1]) / (framsReceive[i] - framsReceive[i - 1]));
            else:jitterBufferPerFrame.append(0);
    jitterBufferPerFrame = [i*1000 for i in jitterBufferPerFrame];
    jitterBufferPerFrame = avg_smooth(jitterBufferPerFrame);

    # Get pixels
    pixel=[i*j for (i,j)in zip(frameWidth,frameHeight)];
    pixel_labels=sorted(list(set(pixel)));
    if (pixel_labels[0]==0): pixel_labels=pixel_labels[1:];
    width_labels=sorted(list(set(frameWidth)));
    if (width_labels[0] == 0): width_labels = width_labels[1:];
    height_labels=sorted(list(set(frameHeight)));
    if (height_labels[0] == 0): height_labels = height_labels[1:];
    pixel_labels_text=[str(i)+"p" for (i,j) in zip(width_labels,height_labels)];

    # Get availableOutgoingBitrate
    data5 = pd.read_csv(os.path.join(path,"peer1/candidate_pair.csv"));
    print(data5.columns);
    cpTimeline = data5['timestamp'].to_list();
    cpTimeline = [(i - base_time) / 1e6 for i in cpTimeline];
    curRTT = data5['currentRoundTripTime'].to_list();
    curRTT = [i*1000 for i in curRTT];
    avaliable_out_bps = data5['availableOutgoingBitrate'].to_list();
    avaliable_out_bps = [i/1e3 for i in avaliable_out_bps];
    print(curRTT);
    print(avaliable_out_bps);

    # Draw graph
    fig, ax1 = plt.subplots(figsize=(24, 16), dpi=300)
    bps_colors = ['darkcyan', 'darkgoldenrod', 'purple', 'darkgreen', 'darkred', 'y']
    ax1.plot(timeline1, sent_video_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[0], label=f'sent_video_rate_bps (Kbps)', zorder=10);
    ax1.plot(timeline1, sent_nack_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[1], label=f'sent_nack_bps (Kbps)', zorder=10);
    ax1.plot(timeline1, sent_fec_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[2], label=f'sent_fec_bps (Kbps)', zorder=10);
    ax1.plot(timeline2, effective_ulp_fec_bps, linestyle='-', linewidth=2, color=bps_colors[3], label=f'Effective fec_bps (Kbps)', zorder=10);
    ax1.plot(timeline2, failed_ulp_fec_bps, linestyle='-', linewidth=2, color=bps_colors[4], label=f'Waste fec_bps (Kbps)', zorder=12);
    ax1.plot(timeline1, total_sent_bps, linestyle='-', linewidth=5, color="darkred", alpha=0.5, label="sum throughput (Kbps)", zorder=5);
    ax1.plot(cpTimeline, avaliable_out_bps, linestyle='-', linewidth=5, color='black', label='estimate throughput (Kbps)', zorder=5);

    ax1.set_xlabel('Time (s)',fontsize=30, fontweight='bold');
    ax1.set_ylabel('Throughput (Kbit/s)',fontsize=30, labelpad=20, fontweight='bold');
    ax1.legend(loc='upper left',fontsize=25)

    ax2 = ax1.twinx()
    ax3 = ax1.twinx();
    ax4 = ax1.twinx();

    max_rate = 0;
    max_delay = 0;

    # ax1.plot(trace_timeline, trace_rate, linestyle='-', linewidth=0.01, color='darkblue', label=f'bandwidth (Mbps)', zorder=10);
    ax1.fill_between(trace_timeline, trace_rate, color='darkblue', alpha=0.2, zorder=1, label=f'bandwidth (kbps)')  # Fill the area under the bandwidth curve
    ax2.plot(trace_timeline, trace_delay, linestyle='dashed', linewidth=0.5, color='black', label=f'delay (ms)')
    # bars = ax3.bar(trace_timeline, trace_loss, color='grey', alpha=0.4, zorder=1, edgecolor='grey', linewidth=1.5);
    for i in range(1,len(trace_timeline)):
        loss_time=[];
        loss_value=[];
        loss_time.append(trace_timeline[i-1]);
        loss_time.append(trace_timeline[i]);
        loss_value.append(trace_loss[i-1]);
        loss_value.append(trace_loss[i-1]);
        ax3.fill_between(loss_time, loss_value, color='grey', alpha=0.4, zorder=1);
    ax3.set_yticklabels([]);
    ax3.set_yticks([]);
    ax3.set_ylim(0, 100);
    # auto_label(ax3, bars, 'black');

    ax2.plot(timeline2, jitterBufferPerFrame, color='slategrey', linewidth=5, linestyle='-', alpha=0.5, label="JitterBuffer Per Frame (ms)", zorder=5);

    ax4.plot(timeline2, pixel, color='darkgreen', linewidth=5, linestyle='-', alpha=1, label='Video resolution', zorder=5);

    max_rate = max(max_rate, max(trace_rate));
    max_delay = max(max_delay, max(trace_delay));
    max_rate = max(max_rate, max(total_sent_bps));
    max_rate = max(max_rate, max(avaliable_out_bps));
    max_delay = max(max_delay, max(jitterBufferPerFrame));
    max_resoluton = max(pixel);

    ax1.set_ylim(0, max_rate * 1.4);
    ax2.set_ylim(0, max_delay * 1.4);
    ax4.set_ylim(0, max_resoluton * 1.5);

    ax2.set_ylabel('Delay (ms)',fontsize=30, fontweight='bold');
    ax2.legend(loc='upper right', fontsize=25);
    ax4.legend(loc='upper center', fontsize=25);
    ax1.tick_params(axis='x', labelsize=25)
    ax1.tick_params(axis='y', labelsize=25)
    ax2.tick_params(axis='y', labelsize=25)

    ax4.yaxis.set_ticks_position('left')
    ax4.yaxis.set_label_position('left')
    ax4.spines['left'].set_position(('outward', -1340))
    ax4.set_yticks(pixel_labels);
    ax4.set_yticklabels(pixel_labels_text, fontsize=25, rotation=90);

    for label in ax1.get_yticklabels():
        label.set_fontweight('bold')

    for label in ax2.get_yticklabels():
        label.set_fontweight('bold')

    for label in ax4.get_yticklabels():
        label.set_fontweight('bold')

    for label in ax1.get_xticklabels():
        label.set_fontweight('bold')

    plt.title('Network Throughput and Bandwidth', fontsize=35, fontweight='bold', pad=10)

    if save_to:
        os.makedirs(os.path.dirname(save_to), exist_ok=True);
        file_name = os.path.join(save_to, f'Pic1_FECbps_jBuffer.png');
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
