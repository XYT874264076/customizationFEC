import os;
import argparse;
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt;
import numpy as np
import pandas as pd;
from matplotlib.lines import lineStyles
from functools import cmp_to_key

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

def get_delta_arr(arr):
    newarr=[];
    if (len(arr)>0):
        newarr.append(arr[0]);
    for i in range(1,len(arr)):
        newarr.append(arr[i]-arr[i-1]);
    return newarr;

def get_div_arr(arr1, arr2):
    newarr=[];
    for (i,j) in zip(arr1,arr2):
        if (j!=0):
            newarr.append(i/j);
        else:
            newarr.append(0);
    return newarr;

def auto_label(ax, bars, c):
    for bar in bars:
        height = bar.get_height()
        if (height !=0):
            ax.annotate(f'{height}% loss',
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', color=c, rotation=90);

def read_trace_file(path):
    timestamps = []
    rates = []
    delays = []
    losses = []
    with open(os.path.join(path, 'traceInfo/peer1_network.log'), 'r') as file:
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

def F1_GetQuantizationParameter(path, base_time):
    # Get timeline with Quantization Parameters
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    qpSum = curdata['qpSum'].to_list();
    framesDecoded = curdata['framesDecoded'].to_list();
    qpSumDelta = get_delta_arr(qpSum);
    framesDecodedDelta = get_delta_arr(framesDecoded);
    QP = get_div_arr(qpSumDelta, framesDecodedDelta);
    return curtimeline, QP;

def F2_GetTargetBitrate(path, base_time):
    # Get timeline with target bitrate for sender
    curdata = pd.read_csv(os.path.join(path, 'peer2/outbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    targetBitrate = curdata['targetBitrate'].to_list();
    targetBitrate = [i/1e3 for i in targetBitrate];
    return curtimeline, targetBitrate;

def F3_GetAvgFrameAssemblyDelay(path, base_time):
    # Get timeline with average frame assembly delay, Only avilable for high resolution
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    totAssemTime = curdata['totalAssemblyTime'].to_list();
    framesFromMultiPkt = curdata['framesAssembledFromMultiplePackets'].to_list();
    totAssemTimeDelta = get_delta_arr(totAssemTime);
    framesFromMultiPktDelta = get_delta_arr(framesFromMultiPkt);
    AFAD = get_div_arr(totAssemTimeDelta, framesFromMultiPktDelta);
    return curtimeline, AFAD;

def F4_GetInterFrameDelay(path, base_time):
    # Get timeline with average Inter-Frame Delay
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    totInterFrameDelay = curdata['totalInterFrameDelay'].to_list();
    framesReceived = curdata['framesReceived'].to_list();
    totInterFrameDelayDelta = get_delta_arr(totInterFrameDelay);
    framesReceivedDelta = get_delta_arr(framesReceived);
    IFD = get_div_arr(totInterFrameDelayDelta, framesReceivedDelta);
    return curtimeline, IFD;

def F5_GetInterFrameDelayVariance(path, base_time):
    # Get timeline with average Inter-Frame Delay Variance
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    totInterFrameDelay = curdata['totalInterFrameDelay'].to_list();
    framesReceived = curdata['framesReceived'].to_list();
    squaredInterFrameDelay = curdata['totalSquaredInterFrameDelay'].to_list();
    totInterFrameDelayDelta = get_delta_arr(totInterFrameDelay);
    framesReceivedDelta = get_delta_arr(framesReceived);
    squaredInterFrameDelayDelta = get_delta_arr(squaredInterFrameDelay);
    IFD = get_div_arr(totInterFrameDelayDelta, framesReceivedDelta);
    SIFD = get_div_arr(squaredInterFrameDelayDelta, framesReceivedDelta);
    IFDV = [i-j*j for (i,j) in zip(IFD, SIFD)];
    return curtimeline, IFDV;

def F6_GetFreezeCount(path, base_time):
    # Get timeline with freeze count
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    freezeCount = curdata['freezeCount'].to_list();
    freezeCountDelta = get_delta_arr(freezeCount);
    return curtimeline, freezeCountDelta;

def F7_GetAvgFreezeDuration(path, base_time):
    # Get timeline with average freeze duration
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    freezeDuration = curdata['totalFreezesDuration'].to_list();
    freezeCount = curdata['freezeCount'].to_list();
    freezeDurationDelta = get_delta_arr(freezeDuration);
    freezeCountDelta = get_delta_arr(freezeCount);
    FD = get_div_arr(freezeDurationDelta, freezeCountDelta);
    return curtimeline, FD;

def F8_GetAvgPktTransTimePerPkt(path, base_time):
    # Get packet transmission time for all packets
    peer2data = pd.read_csv(os.path.join(path, 'peer2/rtp_sender_egress.csv'));
    peer2data = peer2data[peer2data['packet_type'] == 'VideoPacket'];
    peer2data = peer2data[['timestamp', 'seq_num']];
    peer2data['timestamp2'] = peer2data['timestamp'];
    peer1data = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    peer1data = peer1data[peer1data['packet_type'] != 'FECPacket'];
    peer1data = peer1data.loc[peer1data.groupby('seq_num')['timestamp'].idxmin()];
    peer1data = peer1data[['timestamp','seq_num']];
    peer1data = peer1data.rename(columns={'timestamp':'timestamp3'});
    mergeData = peer2data.merge(peer1data, on='seq_num', how='inner');
    mergeData['transTime'] = mergeData['timestamp3'] - mergeData['timestamp2'];
    mergeData = mergeData[['timestamp','transTime']];
    curtimeline = mergeData['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    transTime = mergeData['transTime'].to_list();
    transTime = [i/1e3 for i in transTime];
    return curtimeline[:-2],transTime[:-2];

def F8_GetAvgPktTransTimeTimeline(path, base_time, timestep=0.5):
    # Get timeline with avg packet transmission time
    peer2data = pd.read_csv(os.path.join(path, 'peer2/rtp_sender_egress.csv'));
    peer2data = peer2data[peer2data['packet_type'] == 'VideoPacket'];
    peer2data = peer2data[['timestamp', 'seq_num']];
    peer2data['timestamp2'] = peer2data['timestamp'];
    peer1data = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    peer1data = peer1data[peer1data['packet_type'] != 'FECPacket'];
    peer1data = peer1data.loc[peer1data.groupby('seq_num')['timestamp'].idxmin()];
    peer1data = peer1data[['timestamp','seq_num']];
    peer1data = peer1data.rename(columns={'timestamp':'timestamp3'});
    mergeData = peer2data.merge(peer1data, on='seq_num', how='inner');
    mergeData['transTime'] = mergeData['timestamp3'] - mergeData['timestamp2'];
    mergeData = mergeData[['timestamp','transTime']];
    curtimeline = mergeData['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    transTime = mergeData['transTime'].to_list();
    transTime = [i/1e3 for i in transTime];
    curtimeline = curtimeline[:-2];
    transTime = transTime[:-2];
    if len(transTime)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    avgTransTime = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        suml=0;
        cntl=0;
        while (idx<len(curtimeline) and curtimeline[idx]<timelinebase[i]):
            suml+=transTime[idx];
            cntl+=1;
            idx+=1;
        if (cntl!=0):
            avgTransTime.append(suml/cntl);
        else:
            avgTransTime.append(0);
    return timelinebase[:-1],avgTransTime;

def F9_GetFramesPerSecond(path, base_time):
    # Get timeline with frames per second
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    AFR = curdata['framesPerSecond'].to_list();
    return curtimeline, AFR;

def F10_GetFramesPerSecondVariance(path, base_time, step=10):
    # Get timeline with frames per second variance
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    AFRV = curdata['framesPerSecond'].rolling(window=step, min_periods=1).var().fillna(0).to_list();
    return curtimeline, AFRV;

def F11_GetPktLost(path, base_time):
    # Get timeline with packet lost number
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    lostPktNum = curdata['packetLost'].to_list();
    lostPktNumDelta = get_delta_arr(lostPktNum);
    return curtimeline, lostPktNumDelta;

def F12_GetPktLostRate(path, base_time):
    # Get timeline with packet lost rate
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    lostPktNum = curdata['packetLost'].to_list();
    recvPktNum = curdata['packetsReceived'].to_list();
    lostPktNumDelta = get_delta_arr(lostPktNum);
    recvPktNumDelta = get_delta_arr(recvPktNum);
    lostRate = get_div_arr(lostPktNumDelta, recvPktNumDelta);
    return curtimeline, lostRate;

def F13_GetReTransPktNum(path, base_time):
    # Get timeline with packet retransmission num
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    recvTransPktNum = curdata['retransmittedPacketsReceived'].to_list();
    recvTransPktNumDelta = get_delta_arr(recvTransPktNum);
    return curtimeline, recvTransPktNumDelta;

def F13_GetReTransPktRate(path, base_time):
    # Get timeline with packet retransmission rate
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    recvReTransPktNum = curdata['retransmittedPacketsReceived'].to_list();
    recvPktNum = curdata['packetsReceived'].to_list();
    recvReTransPktNumDelta = get_delta_arr(recvReTransPktNum);
    recvPktNumDelta = get_delta_arr(recvPktNum);
    recvReTransRate = get_div_arr(recvReTransPktNumDelta, recvPktNumDelta);
    return curtimeline, recvReTransRate;

def F14_GetAvgJitterBufferDelay(path, base_time):
    # Get timeline with average jitter buffer delay
    curdata = pd.read_csv(os.path.join(path, 'peer1/inbound.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    curJBD = curdata['jitterBufferDelay'].to_list();
    curJBDEC = curdata['jitterBufferEmittedCount'].to_list();
    JBDDelta = get_delta_arr(curJBD);
    JBDECDelta = get_delta_arr(curJBDEC);
    AJB = get_div_arr(JBDDelta, JBDECDelta)
    return curtimeline, AJB;

def F15_GetSenderRate(path, base_time):
    # Get timeline with sent_video_rate_bps,sent_nack_rate_bps, sent_fec_rate_bps
    curdata = pd.read_csv(os.path.join(path, 'peer2/rtp_video_sender.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    sent_video_bps = curdata['sent_video_rate_bps'].to_list();
    sent_video_bps = [(i / 1e3) for i in sent_video_bps];
    sent_nack_bps = curdata['sent_nack_rate_bps'].to_list();
    sent_nack_bps = [(i / 1e3) for i in sent_nack_bps];
    sent_fec_bps = curdata['sent_fec_rate_bps'].to_list();
    sent_fec_bps = [(i / 1e3) for i in sent_fec_bps];
    sent_video_bps = avg_smooth(sent_video_bps, 5);
    sent_nack_bps = avg_smooth(sent_nack_bps, 5);
    sent_fec_bps = avg_smooth(sent_fec_bps, 5);
    total_sent_bps = [i + j + k for (i, j, k) in zip(sent_video_bps, sent_nack_bps, sent_fec_bps)];
    return curtimeline, sent_video_bps, sent_nack_bps, sent_fec_bps, total_sent_bps;

def F15_GetSenderRatePercent(path, base_time):
    # Get timeline with sent_video_rate_bps,sent_nack_rate_bps, sent_fec_rate_bps transmission percent!
    curdata = pd.read_csv(os.path.join(path, 'peer2/rtp_video_sender.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    sent_video_bps = curdata['sent_video_rate_bps'].to_list();
    sent_video_bps = [(i / 1e3) for i in sent_video_bps];
    sent_nack_bps = curdata['sent_nack_rate_bps'].to_list();
    sent_nack_bps = [(i / 1e3) for i in sent_nack_bps];
    sent_fec_bps = curdata['sent_fec_rate_bps'].to_list();
    sent_fec_bps = [(i / 1e3) for i in sent_fec_bps];
    sent_video_bps = avg_smooth(sent_video_bps, 5);
    sent_nack_bps = avg_smooth(sent_nack_bps, 5);
    sent_fec_bps = avg_smooth(sent_fec_bps, 5);
    total_sent_bps = [i + j + k for (i, j, k) in zip(sent_video_bps, sent_nack_bps, sent_fec_bps)];
    sent_video_percent = [];
    sent_nack_percent = [];
    sent_fec_percent = [];
    for (i,j,k,tot) in zip(sent_video_bps, sent_nack_bps, sent_fec_bps, total_sent_bps):
        if (tot != 0):
            sent_video_percent.append(i/tot);
            sent_nack_percent.append(j/tot);
            sent_fec_percent.append(k/tot);
        else:
            sent_video_percent.append(0);
            sent_nack_percent.append(0);
            sent_fec_percent.append(0);
    return curtimeline, sent_video_percent, sent_nack_percent, sent_fec_percent;

def F15_GetSenderRateAvgPercent(path, base_time):
    # Get timeline with sent_video_rate_bps,sent_nack_rate_bps, sent_fec_rate_bps average percent
    curdata = pd.read_csv(os.path.join(path, 'peer2/rtp_video_sender.csv'));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    sent_video_bps = curdata['sent_video_rate_bps'].to_list();
    sent_video_bps = [(i / 1e3) for i in sent_video_bps];
    sent_nack_bps = curdata['sent_nack_rate_bps'].to_list();
    sent_nack_bps = [(i / 1e3) for i in sent_nack_bps];
    sent_fec_bps = curdata['sent_fec_rate_bps'].to_list();
    sent_fec_bps = [(i / 1e3) for i in sent_fec_bps];
    sent_video_bps = avg_smooth(sent_video_bps, 5);
    sent_nack_bps = avg_smooth(sent_nack_bps, 5);
    sent_fec_bps = avg_smooth(sent_fec_bps, 5);
    total_sent_bps = [i + j + k for (i, j, k) in zip(sent_video_bps, sent_nack_bps, sent_fec_bps)];
    sent_video_avg = 0;
    sent_nack_avg = 0;
    sent_fec_avg = 0;
    if (sum(total_sent_bps)!=0):
        sent_video_avg = sum(sent_video_bps) / sum(total_sent_bps);
        sent_nack_avg = sum(sent_nack_bps) / sum(total_sent_bps);
        sent_fec_avg = sum(sent_fec_bps) / sum(total_sent_bps);
    else :
        sent_video_avg = 0;
        sent_nack_avg = 0;
        sent_fec_avg = 0;
    return sent_video_avg, sent_nack_avg, sent_fec_avg;

def F16_GetFECRecoverSuccAndFailList(path, base_time):
    # Get timestamp with Lost Packet recover success&fail list!
    def is_newer(value, prev_value, bit_size=16):
        k_breakpoint = (1 << (bit_size - 1)) + 1
        diff = value - prev_value
        if diff == k_breakpoint:
            if (value > prev_value):
                return 1
            else:
                return -1;

        if ((value - prev_value) & ((1 << bit_size) - 1) < k_breakpoint):
            return 1;
        else :
            return -1;

    def MaybeLostSeqNum(value, prev_value, bit_size=16):
        newarr=[];
        if (value>prev_value):
            for i in range(prev_value+1,value):
                newarr.append(i);
        else:
            maxNum = (1 << bit_size);
            print(maxNum);
            for i in range(prev_value+1,maxNum):
                newarr.append(i);
            for i in range(0,value):
                newarr.append(i);
        return newarr;

    curdata = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    videoData = curdata[(curdata['packet_type']=='VideoPacket') | (curdata['packet_type']=='FECPacket')];
    recoverData = curdata[curdata['packet_type']=='RecoverVideoPacket'];
    recoverSeqNumSet = set(recoverData['seq_num'].to_list());
    videoSeqNum = videoData['seq_num'].to_list();
    print(videoSeqNum);
    videoSeqNum = sorted(videoSeqNum, key=cmp_to_key(is_newer));
    print(videoSeqNum);
    videoTS = videoData['timestamp'].to_list();
    videoTS = [(i-base_time)/1e6 for i in videoTS];
    maybeLostSN = [];
    maybeLostTS = [];
    maybeLostState = [];
    for i in range(1,len(videoSeqNum)):
        MLSeqNum = MaybeLostSeqNum(videoSeqNum[i],videoSeqNum[i-1]);
        if (len(MLSeqNum)>0):
            TRange = videoTS[i]-videoTS[i-1];
            TInterval = TRange/(len(MLSeqNum)+1);
            for j in range(0,len(MLSeqNum)):
                maybeLostSN.append(MLSeqNum[j]);
                maybeLostTS.append(videoTS[i]+TInterval*(j+1));
                if (MLSeqNum[j] in recoverSeqNumSet):
                    maybeLostState.append(True);
                else:
                    maybeLostState.append(False);
    return maybeLostTS, maybeLostSN, maybeLostState;

def F16_GetFECRecoverSuccAndFail(path, base_time, timestep=0.5):
    # Get timeline with Lost Packet recover success num and fail num!
    def is_newer(value, prev_value, bit_size=16):
        k_breakpoint = (1 << (bit_size - 1)) + 1
        diff = value - prev_value
        if diff == k_breakpoint:
            if (value > prev_value):
                return 1
            else:
                return -1;

        if ((value - prev_value) & ((1 << bit_size) - 1) < k_breakpoint):
            return 1;
        else :
            return -1;

    def MaybeLostSeqNum(value, prev_value, bit_size=16):
        newarr=[];
        if (value>prev_value):
            for i in range(prev_value+1,value):
                newarr.append(i);
        else:
            maxNum = (1 << bit_size);
            print(maxNum);
            for i in range(prev_value+1,maxNum):
                newarr.append(i);
            for i in range(0,value):
                newarr.append(i);
        return newarr;

    curdata = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    videoData = curdata[(curdata['packet_type']=='VideoPacket') | (curdata['packet_type']=='FECPacket')];
    recoverData = curdata[curdata['packet_type']=='RecoverVideoPacket'];
    recoverSeqNumSet = set(recoverData['seq_num'].to_list());
    videoSeqNum = videoData['seq_num'].to_list();
    videoSeqNum = sorted(videoSeqNum, key=cmp_to_key(is_newer));
    videoTS = videoData['timestamp'].to_list();
    videoTS = [(i-base_time)/1e6 for i in videoTS];
    maybeLostSN = [];
    maybeLostTS = [];
    maybeLostState = [];
    for i in range(1,len(videoSeqNum)):
        MLSeqNum = MaybeLostSeqNum(videoSeqNum[i],videoSeqNum[i-1]);
        if (len(MLSeqNum)>0):
            TRange = videoTS[i]-videoTS[i-1];
            TInterval = TRange/(len(MLSeqNum)+1);
            for j in range(0,len(MLSeqNum)):
                maybeLostSN.append(MLSeqNum[j]);
                maybeLostTS.append(videoTS[i]+TInterval*(j+1));
                if (MLSeqNum[j] in recoverSeqNumSet):
                    maybeLostState.append(True);
                else:
                    maybeLostState.append(False);

    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    if len(curtimeline)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    SuccessNum = [];
    FailNum = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        succ=0;
        fail=0;
        while (idx<len(maybeLostTS) and maybeLostTS[idx]<timelinebase[i]):
            if (maybeLostState[idx]):
                succ+=1;
            else:
                fail+=1;
            idx+=1;
        SuccessNum.append(succ);
        FailNum.append(fail);
    return timelinebase[:-1], SuccessNum, FailNum;

def F16_GetFECRecoverSuccAndFailRate(path, base_time, timestep=0.5):
    # Get timeline with Lost Packet recover success rate and fail rate!
    def is_newer(value, prev_value, bit_size=16):
        k_breakpoint = (1 << (bit_size - 1)) + 1
        diff = value - prev_value
        if diff == k_breakpoint:
            if (value > prev_value):
                return 1
            else:
                return -1;

        if ((value - prev_value) & ((1 << bit_size) - 1) < k_breakpoint):
            return 1;
        else :
            return -1;

    def MaybeLostSeqNum(value, prev_value, bit_size=16):
        newarr=[];
        if (value>prev_value):
            for i in range(prev_value+1,value):
                newarr.append(i);
        else:
            maxNum = (1 << bit_size);
            print(maxNum);
            for i in range(prev_value+1,maxNum):
                newarr.append(i);
            for i in range(0,value):
                newarr.append(i);
        return newarr;

    curdata = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    videoData = curdata[(curdata['packet_type']=='VideoPacket') | (curdata['packet_type']=='FECPacket')];
    recoverData = curdata[curdata['packet_type']=='RecoverVideoPacket'];
    recoverSeqNumSet = set(recoverData['seq_num'].to_list());
    videoSeqNum = videoData['seq_num'].to_list();
    videoSeqNum = sorted(videoSeqNum, key=cmp_to_key(is_newer));
    videoTS = videoData['timestamp'].to_list();
    videoTS = [(i-base_time)/1e6 for i in videoTS];
    maybeLostSN = [];
    maybeLostTS = [];
    maybeLostState = [];
    for i in range(1,len(videoSeqNum)):
        MLSeqNum = MaybeLostSeqNum(videoSeqNum[i],videoSeqNum[i-1]);
        if (len(MLSeqNum)>0):
            TRange = videoTS[i]-videoTS[i-1];
            TInterval = TRange/(len(MLSeqNum)+1);
            for j in range(0,len(MLSeqNum)):
                maybeLostSN.append(MLSeqNum[j]);
                maybeLostTS.append(videoTS[i]+TInterval*(j+1));
                if (MLSeqNum[j] in recoverSeqNumSet):
                    maybeLostState.append(True);
                else:
                    maybeLostState.append(False);

    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    if len(curtimeline)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    SuccessNum = [];
    FailNum = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        succ=0;
        fail=0;
        while (idx<len(maybeLostTS) and maybeLostTS[idx]<timelinebase[i]):
            if (maybeLostState[idx]):
                succ+=1;
            else:
                fail+=1;
            idx+=1;
        SuccessNum.append(succ);
        FailNum.append(fail);
    sumLostNum = [i+j for (i,j) in zip(SuccessNum, FailNum)];
    SuccessRate = get_div_arr(SuccessNum, sumLostNum);
    FailRate = get_div_arr(FailNum, sumLostNum);
    return timelinebase[:-1],SuccessRate, FailRate;

def F17_GetFECTimeAheadList(path, base_time):
    # Get timestamp with Time Ahead for FEC Recover Packet compare with Retransmited packet
    curdata = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    retransData = curdata[curdata['packet_type']=='RetransmissionPacket'];
    recoverData = curdata[curdata['packet_type']=='RecoverVideoPacket'];
    retransData = retransData.loc[retransData.groupby('seq_num')['timestamp'].idxmin()];
    recoverData = recoverData.loc[recoverData.groupby('seq_num')['timestamp'].idxmin()];
    retransData = retransData[['timestamp','seq_num']];
    recoverData = recoverData[['timestamp','seq_num']];
    retransData = retransData.rename(columns={"timestamp":"timestamp2"});
    recoverData["timestamp3"]=recoverData["timestamp"];
    mergeData = recoverData.merge(retransData, on='seq_num', how='inner');
    mergeData["timeAhead"]=mergeData["timestamp2"]-mergeData["timestamp3"];
    timeAheadList = mergeData["timeAhead"].to_list();
    timeAheadList = [i/1e3 for i in timeAheadList];
    curtimeline = mergeData['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    SN = mergeData['seq_num'].to_list();
    return curtimeline,timeAheadList,SN;

def F17_GetFECTimeAhead(path, base_time, timestep=0.5):
    # Get timeline with recover packets ahead time compare to retransmitted packets.
    curdata = pd.read_csv(os.path.join(path, 'peer1/rtp_video_stream_receiver2.csv'));
    retransData = curdata[curdata['packet_type']=='RetransmissionPacket'];
    recoverData = curdata[curdata['packet_type']=='RecoverVideoPacket'];
    retransData = retransData.loc[retransData.groupby('seq_num')['timestamp'].idxmin()];
    recoverData = recoverData.loc[recoverData.groupby('seq_num')['timestamp'].idxmin()];
    retransData = retransData[['timestamp','seq_num']];
    recoverData = recoverData[['timestamp','seq_num']];
    retransData = retransData.rename(columns={"timestamp":"timestamp2"});
    recoverData["timestamp3"]=recoverData["timestamp"];
    mergeData = recoverData.merge(retransData, on='seq_num', how='inner');
    mergeData["timeAhead"]=mergeData["timestamp2"]-mergeData["timestamp3"];
    timeAheadList = mergeData["timeAhead"].to_list();
    timeAheadList = [i/1e3 for i in timeAheadList];
    curtimeline = mergeData['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];

    if len(curtimeline) > 0:
        timest = curtimeline[0];
        timeed = curtimeline[-1] + 0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest // timestep) * timestep;
    timelinebase = np.arange(timest, timeed + timestep, timestep);
    AvgAheadTime = [];
    idx = 0;
    for i in range(1,len(timelinebase)):
        suml=0;
        cntl=0;
        while (idx<len(curtimeline) and curtimeline[idx]<timelinebase[i]):
            suml+=timeAheadList[idx];
            cntl+=1;
            idx+=1;
        if (cntl!=0):
            AvgAheadTime.append(suml/cntl);
        else:
            AvgAheadTime.append(0);
    return timelinebase[:-1],AvgAheadTime;

def F18_GetFECEffectiveNum(path, base_time, timestep=0.5):
    # Get timeline with fec packets success num and fail num
    curdata = pd.read_csv(os.path.join(path,"peer1/forward_error_correction.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    curFECSNStr = curdata['FEC_seq_num'].to_list();
    curFECSNStr = [str(i) for i in curFECSNStr];
    curFECState = curdata['status'].to_list();
    FECDelTS = [];
    FECDelState = [];
    FECSN = [];
    for (TS, SNStr, state) in zip(curtimeline, curFECSNStr,curFECState):
        for SN in SNStr.split("_"):
            FECDelTS.append(TS);
            FECDelState.append(state);
            FECSN.append(eval(SN));
    if len(curtimeline)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    SuccessNum = [];
    FailNum = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        succ=0;
        fail=0;
        while (idx<len(FECDelTS) and FECDelTS[idx]<timelinebase[i]):
            if (FECDelState[idx]=='success'):
                succ+=1;
            else:
                fail+=1;
            idx+=1;
        SuccessNum.append(succ);
        FailNum.append(fail);
    return timelinebase[:-1], SuccessNum, FailNum;


def F18_GetFECEffectiveRate(path, base_time, timestep=0.5):
    # Get timeline with fec packets success rate and fail rate
    curdata = pd.read_csv(os.path.join(path,"peer1/forward_error_correction.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    curFECSNStr = curdata['FEC_seq_num'].to_list();
    curFECSNStr = [str(i) for i in curFECSNStr];
    curFECState = curdata['status'].to_list();
    FECDelTS = [];
    FECDelState = [];
    FECSN = [];
    for (TS, SNStr, state) in zip(curtimeline, curFECSNStr,curFECState):
        for SN in SNStr.split("_"):
            FECDelTS.append(TS);
            FECDelState.append(state);
            FECSN.append(eval(SN));
    if len(curtimeline)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    SuccessNum = [];
    FailNum = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        succ=0;
        fail=0;
        while (idx<len(FECDelTS) and FECDelTS[idx]<timelinebase[i]):
            if (FECDelState[idx]=='success'):
                succ+=1;
            else:
                fail+=1;
            idx+=1;
        SuccessNum.append(succ);
        FailNum.append(fail);
    sumLostNum = [i+j for (i,j) in zip(SuccessNum, FailNum)];
    SuccessRate = get_div_arr(SuccessNum, sumLostNum);
    FailRate = get_div_arr(FailNum, sumLostNum);
    return timelinebase[:-1], SuccessRate, FailRate;

def F19_GetFECEffectiveBps(path, base_time, timestep=0.5):
    # Get timeline with fec bps and effective bps
    curdata = pd.read_csv(os.path.join(path,"peer1/forward_error_correction.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    curFECSNStr = curdata['FEC_seq_num'].to_list();
    curFECSNStr = [str(i) for i in curFECSNStr];
    curFECState = curdata['status'].to_list();
    FECDelTS = [];
    FECDelState = [];
    FECSN = [];
    for (TS, SNStr, state) in zip(curtimeline, curFECSNStr,curFECState):
        for SN in SNStr.split("_"):
            FECDelTS.append(TS);
            FECDelState.append(state);
            FECSN.append(eval(SN));
    FECData = pd.DataFrame({"timestamp":FECDelTS, "seq_num":FECSN, "status":FECDelState});
    print(FECData);
    recvData = pd.read_csv(os.path.join(path,"peer1/rtp_video_stream_receiver2.csv"));
    recvData = recvData[recvData["packet_type"] == "FECPacket"];
    recvData = recvData[["seq_num","payload_size"]];
    mergeData = FECData.merge(recvData, on="seq_num", how="left");
    FECPayloadSize = mergeData["payload_size"].to_list();
    print(mergeData);
    if len(curtimeline)>0:
        timest = curtimeline[0];
        timeed = curtimeline[-1]+0.1;
    else:
        timest = 0;
        timeed = 0;
    timest = (timest//timestep)*timestep;
    timelinebase = np.arange(timest,timeed+timestep,timestep);
    SuccessBps = [];
    FailBps = [];
    idx=0;
    for i in range(1,len(timelinebase)):
        succSum=0;
        failSum=0;
        while (idx<len(FECDelTS) and FECDelTS[idx]<timelinebase[i]):
            if (FECDelState[idx]=='success'):
                succSum+=FECPayloadSize[idx];
            else:
                failSum+=FECPayloadSize[idx];
            idx+=1;
        SuccessBps.append(succSum/timestep);
        FailBps.append(failSum/timestep);
    return timelinebase[:-1], SuccessBps, FailBps;

def F20_GetVideoResolution(path, base_time):
    # Get Timeline with frame width and frame height
    curdata = pd.read_csv(os.path.join(path, "peer1/inbound.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i-base_time)/1e6 for i in curtimeline];
    frameWidth = curdata['frameWidth'].to_list();
    frameHeight = curdata['frameHeight'].to_list();
    return curtimeline, frameWidth, frameHeight;

def F21_GetRTT(path, base_time):
    # Get timeline with Round Trip Time
    curdata = pd.read_csv(os.path.join(path, "peer2/candidate_pair.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    RTT = curdata['currentRoundTripTime'].to_list();
    return curtimeline, RTT;

def F22_GetEstimateBtw(path, base_time):
    # Get timeline with Estimate bandwidth
    curdata = pd.read_csv(os.path.join(path, "peer2/candidate_pair.csv"));
    curtimeline = curdata['timestamp'].to_list();
    curtimeline = [(i - base_time) / 1e6 for i in curtimeline];
    EstimateBtw = curdata['availableOutgoingBitrate'].to_list();
    return curtimeline, EstimateBtw;

def plot_graph(path, save_to):

    # Get base_time!
    base_time=0;
    with open(os.path.join(path, 'time_base.txt')) as f:
        fs=f.readline();
        base_time=eval(fs);

    # Get Data
    timeline1, QP = F1_GetQuantizationParameter(path, base_time);
    timeline2, targetBitrate = F2_GetTargetBitrate(path, base_time);
    timeline3, AFAD = F3_GetAvgFrameAssemblyDelay(path, base_time);
    timeline4, IFD = F4_GetInterFrameDelay(path,base_time);
    timeline5, IFDV = F5_GetInterFrameDelayVariance(path, base_time);
    timeline6, FC = F6_GetFreezeCount(path,base_time);
    timeline7, FD = F7_GetAvgFreezeDuration(path, base_time);
    timeline8, APTD = F8_GetAvgPktTransTimeTimeline(path,base_time);
    timeline9, AFR = F9_GetFramesPerSecond(path, base_time);
    timeline10, AFRV = F10_GetFramesPerSecondVariance(path, base_time);
    timeline11, pktLost = F11_GetPktLost(path, base_time);
    timeline12, PktLostRate = F12_GetPktLostRate(path, base_time);
    timeline13, rePkt = F13_GetReTransPktNum(path, base_time);
    timeline14, AJB = F14_GetAvgJitterBufferDelay(path, base_time);
    timeline15, video, nack, fec, total = F15_GetSenderRate(path, base_time);
    timeline16, recSuccess, recFail = F16_GetFECRecoverSuccAndFailRate(path,base_time);
    timeline17, timeAhead = F17_GetFECTimeAhead(path, base_time);
    timeline18, succRate, failRate = F18_GetFECEffectiveRate(path, base_time);
    timeline19, succbps, failbps = F19_GetFECEffectiveBps(path, base_time);
    timeline20, width, height = F20_GetVideoResolution(path, base_time);
    timeline21, RTT = F21_GetRTT(path, base_time);
    timeline22, estimateBtw = F22_GetEstimateBtw(path, base_time);

    # Draw picture settings!
    fig, ax1 = plt.subplots(figsize=(24, 16), dpi=300)
    ax2 = ax1.twinx();
    #淡蓝色、淡绿色、淡橙色、淡紫色、淡灰色
    myFavoriteColor = ['#B9CDE5', '#D7E4BD', '#FDEADA', '#DDC3DD', '#D9D9D9']
    #深红色、深绿色、深蓝色
    lineUsualColor = ['#C00000', '#00501F', '#215968']
    #淡灰蓝色、淡紫蓝色、浅粉色、浅绿色、淡橙色、淡红色
    StandardColors = ['lightsteelblue', 'slateblue', 'thistle', 'lightgreen', 'moccasin','lightcoral'];
    #实线、虚线、点划线、点线
    StandardLineStyle = ['-', '--', '-.', ':'];
    #柱状图阴影
    StandardHatch = ['/','x','\\','|', '-', 'o', '*'];
    #标记形状
    StandardMarker = ['o', '^', 's', 'D', '*', 'v', '<', '>', 'p', 'H'];

    # Draw graph prototype
    # 1.折线图
    # ax1.plot(timeline, values, linestyle=StandardLineStyle[0], linewidth=0.5, color=bps_colors[0], alpha=0.5, label=f'Your legend', zorder=10);
    # 2.柱状图
    # ax1.bar(timeline, values, width=np.average(get_delta_arr(timeline))*0.9, color=myFavoriteColor[0], alpha=0.4, zorder=1, edgecolor='grey', linewidth=1.5, hatch=StandardHatch[0]*2, label=f'frame freeze time');
    # 3.背景填充
    # ax1.fill_between(timeline, values, color=myFavoriteColor[3], alpha=0.8, zorder=1, label=f'bandwidth (kbps)')
    # 4.绘制散点图
    # ax1.scatter(timeline, values, color=myFavoriteColor[3], s=50, alpha=0.8, label="Group A", marker="o")


    # Draw graph
    ax1.plot(timeline2, targetBitrate, linestyle=StandardLineStyle[0], linewidth=0.5, color=lineUsualColor[1], alpha=1, label=f'Success', zorder=1);
    ax2.fill_between(timeline22, estimateBtw, color=myFavoriteColor[3], alpha=0.8, zorder=10, label=f'bandwidth (kbps)')

    # Set Figure axis
    ax1.set_xlabel('Time (s)', fontsize=10, fontweight='bold');
    ax1.set_ylabel('values', fontsize=10, labelpad=20, fontweight='bold');
    ax1.legend(loc='upper left', fontsize=10);

    # Show picture!
    plt.show();


    # ax1.plot(timeline1, sent_video_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[0], label=f'sent_video_rate_bps (Kbps)', zorder=10);
    # ax1.plot(timeline1, sent_nack_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[1], label=f'sent_nack_bps (Kbps)', zorder=10);
    # ax1.plot(timeline1, sent_fec_bps, linestyle='dashed', linewidth=0.5, color=bps_colors[2], label=f'sent_fec_bps (Kbps)', zorder=10);
    # ax1.plot(timeline2, effective_ulp_fec_bps, linestyle='-', linewidth=2, color=bps_colors[3], label=f'Effective fec_bps (Kbps)', zorder=10);
    # ax1.plot(timeline2, failed_ulp_fec_bps, linestyle='-', linewidth=2, color=bps_colors[4], label=f'Waste fec_bps (Kbps)', zorder=12);
    # ax1.plot(timeline1, total_sent_bps, linestyle='-', linewidth=5, color="darkred", alpha=0.5, label="sum throughput (Kbps)", zorder=5);
    # ax1.plot(cpTimeline, avaliable_out_bps, linestyle='-', linewidth=5, color='black', label='estimate throughput (Kbps)', zorder=5);
    #
    # ax1.set_xlabel('Time (s)',fontsize=30, fontweight='bold');
    # ax1.set_ylabel('Throughput (Kbit/s)',fontsize=30, labelpad=20, fontweight='bold');
    # ax1.legend(loc='upper left',fontsize=25)
    #
    # ax2 = ax1.twinx()
    # ax3 = ax1.twinx();
    # ax4 = ax1.twinx();
    #
    # max_rate = 0;
    # max_delay = 0;
    #
    # # ax1.plot(trace_timeline, trace_rate, linestyle='-', linewidth=0.01, color='darkblue', label=f'bandwidth (Mbps)', zorder=10);
    # ax1.fill_between(trace_timeline, trace_rate, color='darkblue', alpha=0.2, zorder=1, label=f'bandwidth (kbps)')  # Fill the area under the bandwidth curve
    # ax2.plot(trace_timeline, trace_delay, linestyle='dashed', linewidth=0.5, color='black', label=f'delay (ms)')
    # # bars = ax3.bar(trace_timeline, trace_loss, color='grey', alpha=0.4, zorder=1, edgecolor='grey', linewidth=1.5);
    # for i in range(1,len(trace_timeline)):
    #     loss_time=[];
    #     loss_value=[];
    #     loss_time.append(trace_timeline[i-1]);
    #     loss_time.append(trace_timeline[i]);
    #     loss_value.append(trace_loss[i-1]);
    #     loss_value.append(trace_loss[i-1]);
    #     ax3.fill_between(loss_time, loss_value, color='grey', alpha=0.4, zorder=1);
    # ax3.set_yticklabels([]);
    # ax3.set_yticks([]);
    # ax3.set_ylim(0, 100);
    # # auto_label(ax3, bars, 'black');
    #
    # ax2.plot(timeline2, jitterBufferPerFrame, color='slategrey', linewidth=5, linestyle='-', alpha=0.5, label="JitterBuffer Per Frame (ms)", zorder=5);
    #
    # ax4.plot(timeline2, pixel, color='darkgreen', linewidth=5, linestyle='-', alpha=1, label='Video resolution', zorder=5);
    #
    # max_rate = max(max_rate, max(trace_rate));
    # max_delay = max(max_delay, max(trace_delay));
    # max_rate = max(max_rate, max(total_sent_bps));
    # max_rate = max(max_rate, max(avaliable_out_bps));
    # max_delay = max(max_delay, max(jitterBufferPerFrame));
    # max_resoluton = max(pixel);
    #
    # ax1.set_ylim(0, max_rate * 1.4);
    # ax2.set_ylim(0, max_delay * 1.4);
    # ax4.set_ylim(0, max_resoluton * 1.5);
    #
    # ax2.set_ylabel('Delay (ms)',fontsize=30, fontweight='bold');
    # ax2.legend(loc='upper right', fontsize=25);
    # ax4.legend(loc='upper center', fontsize=25);
    # ax1.tick_params(axis='x', labelsize=25)
    # ax1.tick_params(axis='y', labelsize=25)
    # ax2.tick_params(axis='y', labelsize=25)
    #
    # ax4.yaxis.set_ticks_position('left')
    # ax4.yaxis.set_label_position('left')
    # ax4.spines['left'].set_position(('outward', -1340))
    # ax4.set_yticks(pixel_labels);
    # ax4.set_yticklabels(pixel_labels_text, fontsize=25, rotation=90);
    #
    # for label in ax1.get_yticklabels():
    #     label.set_fontweight('bold')
    #
    # for label in ax2.get_yticklabels():
    #     label.set_fontweight('bold')
    #
    # for label in ax4.get_yticklabels():
    #     label.set_fontweight('bold')
    #
    # for label in ax1.get_xticklabels():
    #     label.set_fontweight('bold')
    #
    # plt.title('Network Throughput and Bandwidth', fontsize=35, fontweight='bold', pad=10)
    #
    # if save_to:
    #     os.makedirs(os.path.dirname(save_to), exist_ok=True);
    #     file_name = os.path.join(save_to, f'Pic1_FECbps_jBuffer.png');
    #     plt.savefig(file_name)
    #     print(f"Plot saved to {file_name}");
    # else:
    #     plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot pic1, three bps and FEC bps with jitterBufferDelay.')
    parser.add_argument('--data-dir', required=False, type=str, default='/home/ubuntu/Desktop/WebRTC-20250314_132119-[testVideo1.mp4_testVideo2.mp4]-[WebRTCSource+RSFECBlock+RSFECStreamStableRate+FECClose]-[1]-[0.03+0.06+0.09+0.12]-[TestLoss200K2P_TestLoss200K2P+TestLoss300K2P_TestLoss300K2P+TestLoss400K2P_TestLoss400K2P]/[testVideo1.mp4_testVideo2.mp4]-[RSFECStreamStableRate]-[TestLoss300K2P_TestLoss300K2P]-[0.12]-[1]/2/', help='Directory containing throughput log files.')
    parser.add_argument('--output', required=False, type=str, default='/home/ubuntu/Desktop/WebRTC-20250314_132119-[testVideo1.mp4_testVideo2.mp4]-[WebRTCSource+RSFECBlock+RSFECStreamStableRate+FECClose]-[1]-[0.03+0.06+0.09+0.12]-[TestLoss200K2P_TestLoss200K2P+TestLoss300K2P_TestLoss300K2P+TestLoss400K2P_TestLoss400K2P]/[testVideo1.mp4_testVideo2.mp4]-[RSFECStreamStableRate]-[TestLoss300K2P_TestLoss300K2P]-[0.12]-[1]/2/graphs', help='Output file to save the plot.')
    args = parser.parse_args()

    plot_graph(args.data_dir, args.output);
