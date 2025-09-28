
**************File Structure*******************

A1/
├─ server.cpp
├─ client.cpp
├─ jokes.txt
├─ Makefile
├─ run_three_clients.sh
└─ README.md   (you can paste the instructions below)

*****************RUN*****************

**how to run on same machine :**
cd A1
make

# Start server for 3 demo clients on localhost:
./server 127.0.0.1 5555 --expected 3

# In three other terminals:
./client 127.0.0.1 5555
./client 127.0.0.1 5555
./client 127.0.0.1 5555



*******************  Run on separate machines (LAN) ***************

# 1. On the server machine:

./server 0.0.0.0 5555


**2. Find server IP: ip addr (Linux) or ifconfig, e.g., 192.168.1.20.**

**3. On each client machine:**

./client 192.168.1.20 5555


