## How to perform this DPDK test?

### Install test utilities
```shell
sudo apt install -y python3-scapy netsniff-ng
```
`scapy` is a python-based packect manipulation tool. You can use it to generate any kind of packets you want. `netsniff-ng` is a network analyzer tool set. We will only use its `trafgen` to generate traffic and `ifpps` to watch the traffic's speed.

### Create a tap device to connect DPDK port
```shell
sudo ip tuntap add dev tap0 mode tap
sudo ip link set tap0 up
sudo ip addr add 10.10.1.1/24 dev tap0
```
This will create a `tap` device called `tap0` within your system. You can use `ip a` to check it. After test, you can use this command to delete the `tap0` device:
```shell
sudo ip tuntap del dev tap0 mode tap
```

### Begin the bench test
1. Compile this test program
```shell
./cos compose composition_scripts/bench_dpdk.toml bench_dpdk_test
```
2. Open a separate terminal and then use `ifpps` to monitor the `tap0` deivce
```shell
sudo ifpps --dev tap0 --promisc
```
3. Use `trafgen` to generate traffic before running the program
Open another terminal to run this command:
```shell
sudo trafgen --cpp --out tap0 --conf ./src/components/implementation/tests/bench_dpdk/traffic_template.trafgen --verbose --cpu 1 -b 1000MiB
```
4. Start the Composite networking system
```shell
sudo ./cos run bench_dpdk_test enable-nic
```
5. Watch the `ifpps` monitor
Now you will see in the terminal the rx/tx speed of the `tap0` device. Then you can use `Ctrl+C` to stop the `trafgen`. (At this pointed, If the `trafgen` cannot be stoped, you need to start again the Composite dpdk system and then stop it to make sure the `trafgen` is stopped.) Finally, use `Ctrl+C` to stop the `ifpps`.

6. Watch DPDK stats
After you stop `trafgen`, wait a few seconds, the DPDK test will prints out stats including `rx bytes`, `rx packets`, `tx bytes`, `tx packets`. You can use these stats to compare the traffic stats above.


### Simple ping-pong test
1. Start the program
```shell
sudo ./cos run bench_dpdk_test enable-nic
```

2. After DPDK is ready, start the scapy script and then it will print out if the ping-pong test succeed
```shell
sudo python3 ./src/components/implementation/tests/bench_dpdk/ping-pong.py
```