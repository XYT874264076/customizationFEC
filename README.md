#### Environment Setup

To ensure a stable environment, it is strongly recommended to create a Docker container for setting up the experimental environment. In our evaluation, we use the Ubuntu 22.04 image:

```shell
# Pull the official Ubuntu 22.04 image
sudo docker pull ubuntu:22.04
# Create shared directory
sudo mkdir /data/WebRTC_SwiftFEC
sudo chown username:username /data/WebRTC_SwiftFEC
```

Since Docker images typically do not include the PulseAudio module, and our full WebRTC binary needs to connect to PulseAudio for audio services (audio input or output), we first need to create a virtual audio device `pulse-native` on the host machine so that the WebRTC code can connect properly:

```shell
# Create a PulseAudio virtual device
pactl load-module module-native-protocol-unix auth-anonymous=1 socket=/tmp/pulse-native
```

After creating the virtual device, you can create the Docker container:

```shell
# Create the Docker container
sudo docker run -itd --name WebRTCSwift \
	--privileged \
	--network host \
	-e PULSE_SERVER=unix:/tmp/pulse-native \
  	-v /tmp/pulse-native:/tmp/pulse-native \
  	-v /etc/machine-id:/etc/machine-id:ro \
	-v /data/WebRTC_SwiftFEC:/home/data/WebRTC_SwiftFEC \
	ubuntu:22.04
```

Note: since we use the `ip` tool and `tc` tool to isolate network namespaces and build the network environment, privileged mode `--privileged` must be enabled. The PulseAudio device on the host must be mapped into the Docker container, hence the shared socket mount. Finally, the shared directory is created for convenience when writing or debugging code, allowing the host machine (including VSCode and other IDEs connected via SSH) to access the code.

Next, enter the Docker container, install the basic PulseAudio utilities, and verify that PulseAudio works correctly inside the container (this is important — without it, the code will not run properly):

```shell
# Enter the container
sudo docker exec -u 0 -it WebRTCSwift /bin/bash
# Install pulseaudio-utils
apt-get update && apt-get install -y pulseaudio-utils
# Test the connection; if PulseAudio info is printed, the host PulseAudio device is accessible
PULSE_SERVER=unix:/tmp/pulse-native pactl info
```

After successful verification, continue installing the remaining dependencies required for compiling and running the code:

```shell
# Install common basic development tools
apt-get update
apt-get install -y git build-essential clang cmake ninja-build pkg-config
# Install required build toolchain
apt-get install -y \
    build-essential \
    clang \
    ninja-build \
    cmake \
    pkg-config \
    python3 \
    python3-pip \
    autoconf \
    automake \
    libtool \
    m4 \
    make \
    gcc \
    g++ \
    yasm \
    nasm \
    pkg-config \
    curl \
    wget \
    libgtk-3-dev \
    iproute2 \
    net-tools \
    unzip \
    vim
# X11-related development libraries
apt-get install -y \
    libx11-dev \
    libxcomposite-dev \
    libxext-dev \
    libxrender-dev \
    libxrandr-dev \
    libxdamage-dev \
    libxfixes-dev \
    libxi-dev \
    libxcb1-dev \
    libxcb-randr0-dev \
    libxcb-shape0-dev    
# FFmpeg multimedia libraries
apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libpulse-dev \
    libopus-dev \
    libvpx-dev
# Network and audio/video protocol libraries that WebRTC core depends on
apt-get install -y \
    libssl-dev \
    libevent-dev \
    libsrtp2-dev \
    libnice-dev \
    libusrsctp-dev \
    libcrypto++-dev
# Graphics rendering related libraries
apt-get install -y \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libdrm-dev \
    libgbm-dev \
    libwayland-dev \
    libva-dev \
    libvdpau-dev
# Install Python development environment
apt-get install -y \
    python3.10 \
    python3.10-dev \
    python3-pip \
    python-is-python3 \
    python2
# Install GF-Complete library (must be compiled from source)
cd /tmp
git clone https://github.com/ceph/gf-complete.git
cd gf-complete
./autogen.sh
./configure
make -j$(nproc)
make install
ldconfig
ln -s /usr/local/lib/libgf_complete.so /usr/lib/libgf_complete.so
ln -s /usr/local/lib/libgf_complete.so.1 /usr/lib/libgf_complete.so.1 
# Install pybind11 for Python-C++ interoperability
apt-get install -y pybind11-dev
# Install Xvfb (virtual framebuffer)
apt-get update && apt-get install -y xvfb
```

After integrating TamburFEC, additional dependencies for Tambur need to be installed. Tambur uses the C++ version of Torch, i.e., libtorch, so we need to install libtorch:

```shell
# Go to the WebRTC_SwiftFEC directory
cd /home/data/WebRTC_SwiftFEC/
mkdir download
cd download
# Download libtorch
wget "https://download.pytorch.org/libtorch/lts/1.8/cpu/libtorch-cxx11-abi-shared-with-deps-1.8.2%2Bcpu.zip"
# Extract libtorch
unzip libtorch-cxx11-abi-shared-with-deps-1.8.2+cpu.zip
# Copy Torch headers and libraries to system paths (manual install)
cp -r libtorch/include/torch /usr/include/
cp -r libtorch/include/ATen /usr/include/
cp -r libtorch/include/c10 /usr/include/
cp -r libtorch/include/caffe2 /usr/include/
cp -r libtorch/lib/* /usr/lib/
# Create symlinks to ensure correct versioning
ln -sf /usr/lib/libtorch.so /usr/lib/libtorch.so.1
ln -sf /usr/lib/libtorch.so /usr/lib/libtorch.so.1.8.2
```

Test code:

```c++
#include <iostream>
#include <torch/script.h>

int main() {
    try {
        std::cout << "Testing PyTorch C++ integration..." << std::endl;
        
        // Create a simple tensor
        auto tensor = torch::tensor({1.0, 2.0, 3.0});
        std::cout << "Tensor created successfully: " << tensor << std::endl;
        
        // Test basic operations
        auto result = tensor * 2.0;
        std::cout << "Tensor multiplied by 2: " << result << std::endl;
        
        std::cout << "✓ PyTorch C++ integration test passed!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

Compile test:

```shell
g++ test_torch.cpp -o test_torch -ltorch -ltorch_cpu -lc10
```

If this code compiles and runs successfully, the Torch installation is correct. Then install four additional third-party libraries:

```shell
# Create a temporary working directory
mkdir -p /tmp/erasure_libs_install
cd /tmp/erasure_libs_install

# Download Jerasure
git clone https://github.com/tsuraan/Jerasure.git
cd Jerasure

# Compile and install Jerasure
autoreconf -i
./configure --prefix=/usr/local --enable-shared
make -j$(nproc)
make install
ln -sv /usr/local/include/jerasure/galois.h      /usr/local/include/galois.h
ln -sv /usr/local/include/jerasure/cauchy.h       /usr/local/include/cauchy.h
ln -sv /usr/local/include/jerasure/liberation.h   /usr/local/include/liberation.h
ln -sv /usr/local/include/jerasure/reed_sol.h     /usr/local/include/reed_s
ln -sf /usr/local/lib/libJerasure.so.2.0.0 /lib/libJerasure.so.2.0.0
ln -sf /lib/libJerasure.so.2.0.0 /lib/libJerasure.so.2
ln -sf /lib/libJerasure.so.2.0.0 /lib/libJerasure.so
cd ..

# Download maxflow
git clone https://github.com/gerddie/maxflow.git
cd maxflow

# Compile and install maxflow
mkdir -p build && cd build
cmake ..
make -j$(nproc)
make install
cd ../../..
ln -sv /usr/local/include/maxflow-3.0/maxflow.h     /usr/local/include/maxflow.h
cp -r /usr/local/include/maxflow-3.0/maxflow /usr/local/include/
ln -sf /usr/local/lib/libmaxflow.so /usr/lib/libmaxflow.so
ln -sf /usr/local/lib/libmaxflow.so.0 /usr/lib/libmaxflow.so.0

# Install nlohmann-json directly via apt
apt install nlohmann-json3-dev
```

#### Fetch WebRTC Code & Compile

WebRTC is a large project with numerous dependencies, multiple sub-modules, and third-party libraries. It requires specialized tools such as `depot_tools`, so the official source code must be obtained first:

```shell
# Enter the shared directory
cd /home/data/WebRTC_SwiftFEC
# 1. Fetch depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
cd depot_tools
# Ensure depot_tools version matches the WebRTC snapshot version we are using
git checkout 9dd0755b
cd ..

# 2. Set environment variable
export PATH=$PATH:/home/data/WebRTC_SwiftFEC/depot_tools

# 3. Fetch WebRTC source code
mkdir webrtc-checkout
cd webrtc-checkout
fetch --nohooks webrtc
cd src
# We use the WebRTC snapshot version dated 2024-11-07T04:07:11
git checkout 757d6c3992 
gclient sync
```

Next, compile the WebRTC code once to verify its integrity:

```shell
mkdir out/Default
gn gen out/Default --args='use_sysroot=false use_rtti=true'
ninja -C out/Default/ peerconnection_client
```

A successful build confirms that the base WebRTC code has been built correctly. You can then switch to the current repository and pull the `customizationFEC` code:

```shell
# Remove the current origin remote
git remote remove origin
# Add the current repository address
git remote add origin https://github.com/XYT874264076/customizationFEC.git
# Switch to the docker-env branch
git checkout -b docker-env
# Pull remote code
git pull origin docker-env
```

Now you can compile the `customizationFEC` code:

```shell
mkdir out/customizationFEC
gn gen out/customizationFEC --args='use_sysroot=false use_rtti=true'
ninja -C out/customizationFEC/ customizationFEC
```

#### Run the Evaluation

Before running the code, install the necessary Python runtime libraries (SwiftFEC uses Torch to build neural networks):

```shell
pip install pytz pandas numpy torch matplotlib argparse
```

Since the signaling server uses Node.js, Node must also be installed:

```shell
apt-get install -y nodejs npm
```

At this point, we can start running the code. To verify the environment, we will first skip the full experiment script (located at `WebRTC_Experience_Script/WebRTC_all_exp_main.py`) and instead set up an experiment environment directly from the command line.

This experimental environment requires two network namespaces: `client1` and `client2`, which run the two clients for WebRTC end-to-end transmission respectively. The signaling server runs in the main namespace. The namespaces are connected via a bridge (`br0`), so all three (client1, client2, and the main namespace) are on the same internal network and can address each other. Finally, an IP address must be assigned to each endpoint.

First, clear any existing configuration (you can also use this to clean up after experiments):

```
ip netns del client1
ip netns del client2
ip link del br0
ip link del br-ifb1
ip link del br-ifb2
```

Then run the following commands:

```shell
ip link add veth1 type veth peer br-veth1
ip link add veth2 type veth peer br-veth2

ip link set veth1 up
ip link set veth2 up
ip link set br-veth1 up
ip link set br-veth2 up

ip netns add client1
ip netns add client2

ip link set veth1 netns client1
ip link set veth2 netns client2

ip link add name br0 type bridge
ip link set br0 up

ip link set br-veth1 master br0
ip link set br-veth2 master br0

ip netns exec client1 ifconfig veth1 192.168.0.2/24
ip netns exec client2 ifconfig veth2 192.168.0.3/24

ip addr add 192.168.0.1/24 dev br0
```

Next, set up a simple network environment with bandwidth limited to 1000 Kbps, end-to-end delay of 60 ms (30+30), and a one-way link packet loss rate of 5%. Note that end-to-end communication involves two links — client1 to the main namespace and main namespace to client2 — each with a 5% packet loss rate. Use the `tc` tool to configure this:

```shell
tc qdisc add dev br-veth1 root handle 1: tbf rate 1000kbit burst 256kbit latency 0.1ms
tc qdisc add dev br-veth1 parent 1:1 handle 10: netem delay 30ms loss 0% limit 1000
tc qdisc add dev br-veth2 root handle 1: tbf rate 1000kbit burst 256kbit latency 0.1ms
tc qdisc add dev br-veth2 parent 1:1 handle 10: netem delay 30ms loss 0% limit 1000

ip link add br-ifb1 type ifb
ip link add br-ifb2 type ifb
ip link set br-ifb1 up
ip link set br-ifb2 up
tc qdisc add dev br-veth1 handle ffff: ingress
tc filter add dev br-veth1 parent ffff: protocol ip u32 match u32 0 0 action mirred egress redirect dev br-ifb1
tc qdisc add dev br-veth2 handle ffff: ingress
tc filter add dev br-veth2 parent ffff: protocol ip u32 match u32 0 0 action mirred egress redirect dev br-ifb2
tc qdisc add dev br-ifb1 root handle 1: tbf rate 1000kbit burst 256kbit latency 0.1ms
tc qdisc add dev br-ifb1 parent 1:1 handle 10: netem delay 30ms loss 5% limit 1000
tc qdisc add dev br-ifb2 root handle 1: tbf rate 1000kbit burst 256kbit latency 0.1ms
tc qdisc add dev br-ifb2 parent 1:1 handle 10: netem delay 30ms loss 5% limit 1000
```

In subsequent experiments, the network environment setup is essentially the same: namespace + bridge for network topology, and `tc` token bucket for bandwidth, delay, and packet loss control.

Now, start the signaling server in the main namespace:

```shell
node WebRTC_Experience_Script/SignalServer/signal.js
```

Next, open two new terminals, enter the Docker container in each, and navigate to `/home/data/WebRTC_SwiftFEC/webrtc-checkout/src`.

**Terminal 1:**

```shell
xvfb-run -a ip netns exec client1 bash -c 'export PULSE_SERVER=unix:/tmp/pulse-native; ./out/customizationFEC/customizationFEC --playVideo /path/to/testVideo.mp4 --duration 400 --interval 100 --output ./outputFile/peer1 --type WebRTCSource'
```

**Terminal 2:**

```shell
xvfb-run -a ip netns exec client2 bash -c 'export PULSE_SERVER=unix:/tmp/pulse-native; ./out/customizationFEC/customizationFEC --playVideo /path/to/testVideo.mp4 --duration 400 --interval 100 --output ./outputFile/peer2 --type WebRTCSource'
```

Here, `--playVideo` should be replaced with the path to the MP4 file you want to play (a concrete path is not given here — it is just an example). `--duration` is the experiment duration in seconds (400s here; default is 120s in the script). `--interval` is the logging interval (100ms here; default is 500ms in the script). `--output` is the log output directory. `--type` is the FEC strategy to use; `WebRTCSource` is WebRTC's native FEC strategy.

You can also try the SwiftFEC strategy:

**Terminal 1:**

```shell
xvfb-run -a ip netns exec client1 bash -c 'export PULSE_SERVER=unix:/tmp/pulse-native; ./out/customizationFEC/customizationFEC --playVideo /path/to/testVideo.mp4 --duration 400 --interval 100 --output ./outputFile/peer1 --type RLSRSFEC'
```

**Terminal 2:**

```shell
xvfb-run -a ip netns exec client2 bash -c 'export PULSE_SERVER=unix:/tmp/pulse-native; ./out/customizationFEC/customizationFEC --playVideo /path/to/testVideo.mp4 --duration 400 --interval 100 --output ./outputFile/peer2 --type RLSRSFEC'
```

If both examples run successfully, the WebRTC experimental environment has been set up correctly. You can then try running the full experiment script:

```shell
cd WebRTC_Experience_Script/
python WebRTC_all_exp_main.py 
```

After executing this script, a series of experiments will run automatically. The specific configuration options can be viewed with `python WebRTC_all_exp_main.py -h`.

#### Get more traces

In the experimental directory, we provide some traces we created ourselves as well as traces captured via tools, placed in the  `WebRTC_Experience_Script/trace_log`  directory. Also in this directory, we provide a trace creation tool  trace_generate.py , which relies on two command-line tools —  `iperf3`  and  `ping`  — to capture network fluctuations (throughput and latency) from the current terminal to a target server. The process is as follows:
First, install ` iperf3`  and  `ping` :

```shell
apt install iperf3 iputils-ping
```

Next, start  `iperf3`  in server mode on the **target machine**:

```shell
# Run on the target machine (machine that IP address is <target_ip>)
iperf3 -s
```

Finally, run the following on the local machine:

```shell
# Run locally
python3 trace_generate.py -t <target_ip> -o <target_filename>
```

The captured trace will be automatically saved to the folder named  `target_filename`, using the default format and time interval. To modify the time interval or access more features, you can use  `python3 trace_generator.py -h`  for help.
