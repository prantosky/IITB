#!/bin/bash
./PeerNode -ip=127.0.0.1 -port=10000 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10001 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10002 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10003 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10004 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10005 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10006 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10007 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./PeerNode -ip=127.0.0.1 -port=10008 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 &
sleep 2
./../../Flooder/build/PeerNode -ip=127.0.0.1 -port=10009 -seedfile=../seeds.txt -interArrivalTime=$1 -nodeHashPower=0.1 -floodFile=./../../Flooder/flood.txt &

