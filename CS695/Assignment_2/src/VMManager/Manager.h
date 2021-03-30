//
// Created by pranav on 18/04/20.
//

#ifndef ASSIGNMENT_2_MANAGER_H
#define ASSIGNMENT_2_MANAGER_H

#include <libvirt/libvirt.h>

#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "Graph.h"
#include "VM.h"

using namespace std;

class Manager {
   private:
	static const size_t MAX_UTIL_VEC_LENGTH = 128;

	virConnectPtr conn;
	unordered_map<string, VM*> domains;
	unordered_map<string, mutex*> locks;
	unordered_map<string, list<int>*> utilList;
	unordered_map<string, bool> threadTerminationFlags;
	unordered_map<string, mutex*> threadTerminationLocks;
	vector<thread*> threads;
	const string ipFile = "server.dat";
	mutex fileLock;

	static const size_t LOAD_IDLE_THRESHOLD_TIME_IN_SECS = 15;
	static const size_t LOAD_OVERLOAD_THRESHOLD_TIME_IN_SECS = 10;
	static const size_t LOAD_RESET_THRESHOLD_TIME_IN_SECS = 5;

	static const size_t LOAD_IDLE_THRESHOLD_PERCENT = 15;
	static const size_t LOAD_OVERLOAD_THRESHOLD_PERCENT = 80;
	static const size_t LOAD_RESET_THRESHOLD_PERCENT = 50;

	unordered_map<string, mutex*> loadLocks;
	// tuple of (last consecutive overloaded seconds,last consecutive idle
	// seconds, reset counter, isOverloaded flag,canPowerOff flag)
	unordered_map<string, tuple<int, int, int, bool, bool>> load;
	thread* loadHandlerThread;
	mutex loadHandlerMutex;
	bool loadHandlerTerminationFlag = true;

	void _watch(string nameOfVm);
	bool _writeIpToFile(const string& ip);
	bool _deleteIpFromFile(const string& ip);

   public:
	Manager();
	~Manager();
	string startNewVm();
	void powerOn(const string& nameOfVm);
	void shutdown(const string& nameOfVm);
	void startNewVm(const string& nameOfVm);
	bool isVmPowered(const string& nameOfVm);
	string getIP(const string& nameOfVm);
	void startWatching(const string& nameOfVm);
	vector<int> getUtilVector(const string& nameOfVm);
	vector<string> getAllDefinedDomainNames();
	void notifyAboutServer();
	vector<string> attachToTheRunningVms();
	vector<string> getAllActiveVmNames();
};

#endif	// ASSIGNMENT_2_MANAGER_H
