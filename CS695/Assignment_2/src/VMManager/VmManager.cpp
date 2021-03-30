//
// Created by pranav on 18/04/20.
//

#include "VmManager.h"

#include <iostream>

#include "Graph.h"

VmManager::VmManager() : sanitizerThreadTerminationFlag(false) {
	
	mgr = new Manager();
	auto activeVms = mgr->attachToTheRunningVms();

	this_thread::sleep_for(chrono::seconds(1));

	sanitiserThread = new thread([&] {
		this_thread::sleep_for(chrono::seconds(10));
		while (true) {
			{
				std::lock_guard lck(sanitizerMutex);
				for (auto it = launchThreads.cbegin();
					 it != launchThreads.cend() /* not hoisted */;
					 /* no increment */) {
					if (*it != nullptr and (*it)->joinable()) {
						(*it)->join();
						std::cout << "VmManager::sanitizerThread: "
									 "launchThread destroyed"
								  << std::endl;
					}
					launchThreads.erase(it++);
				}
			}

			// Check for any VMs that are newly powered On
			{
				auto names = mgr->getAllActiveVmNames();
				for (auto &&name : names) {
					auto it = drawingThreads.find(name);
					if (it == drawingThreads.end()) {
						std::cout
							<< "VmManager::sanitizerThread: new VM powered "
							   "on. Starting threads."
							<< std::endl;
						_resetTerminationFlagForVmThreads(name);
						auto box = _getBoxFromGrid(name);
						if (box != nullptr) {
							_launchVmThreads(box, name);
						} else {
							std::cerr << "VmManager::sanitizerThread: no box "
										 "widget found for "
									  << name << std::endl;
						}
					}
				}
			}

			// Check for any shutdown VMs
			{
				for (auto &&i : drawingThreads) {
					std::cout << "+++++++++++" << i.first << "+++++++++++"
							  << std::endl;
					if (not mgr->isVmPowered(i.first)) {
						auto name = i.first;
						std::cout << "VmManager::sanitizerThread: Cleaning up "
									 "the shutdown VM "
								  << name << std::endl;
						_issueTerminationToVmThreads(name);
						_reclaimMemory(name);
						break;
					}
				}
				std::cout << "========================================="
						  << std::endl;
			}
			{
				std::lock_guard lck(sanitizerMutex);
				if (sanitizerThreadTerminationFlag and launchThreads.empty()) {
					std::cout << "VmManager::sanitizerThread: destroying "
								 "sanitizer thread"
							  << std::endl;
					break;
				}
			}
			this_thread::sleep_for(chrono::seconds(1));
		}
		std::cout << "VmManager::sanitizerThread: Exiting sanitizerThread"
				  << endl;
	});

	add(m_box1);

	m_box1.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);

	auto mLabelMain = Gtk::make_managed<Gtk::Label>("Virtual Manager");
	auto mGrid = Gtk::make_managed<Gtk::Grid>();

	mLabelMain->set_text("Virtual Machine Manager");

	m_box1.pack_start(*mLabelMain, Gtk::PACK_SHRINK);
	m_box1.pack_start(*mGrid, Gtk::PACK_EXPAND_WIDGET, 20);

	mGrid->set_row_spacing(50);
	mGrid->set_column_spacing(50);
	mGrid->set_row_homogeneous(true);
	mGrid->set_column_homogeneous(true);
	mGrid->set_border_width(50);

	_fillViewsInGrid(mGrid);
	show_all_children();

	// std::cout << "VmManager::getBoxFromGrid: Already Active VM count: "
	// 		  << activeVms.size() << endl;

	for (auto &&i : activeVms) {
		auto box = _getBoxFromGrid(i);
		if (box != nullptr) {
			_launchVmThreads(box, i);
		} else {
			std::cout << "VmManager::VmManager(): No box found for " << i
					  << endl;
		}
	}
	// std::cout << "VmManager::VmManager(): exiting" << endl;
}

VmManager::~VmManager() {
	// cout << "VmManager::~VmManager(): called" << endl;
	for (auto &&i : terminationMutexes) {
		_issueTerminationToVmThreads(i.first);
	}

	{
		std::lock_guard lck(sanitizerMutex);
		sanitizerThreadTerminationFlag = true;
	}

	if (sanitiserThread != nullptr) {
		sanitiserThread->join();
		std::cout << "VmManager::~VmManager: sanitizerThread destroyed"
				  << std::endl;
		delete sanitiserThread;
	}

	for (auto it = ipUpdaterThreads.cbegin();
		 it != ipUpdaterThreads.cend() /* not hoisted */
		 ;
		 /* no increment */) {
		if (it->second != nullptr) it->second->join();
		std::cout << "VmManager::~VMManager: ipThread destroyed for "
				  << it->first << std::endl;
		ipUpdaterThreads.erase(it++);
	}

	for (auto it = drawingThreads.cbegin();
		 it != drawingThreads.cend() /* not hoisted */;
		 /* no increment */) {
		if (it->second != nullptr) it->second->join();
		std::cout << "VmManager::~VMManager: drawThread destroyed for "
				  << it->first << std::endl;
		drawingThreads.erase(it++);
	}

	for (auto it = terminationMutexes.cbegin();
		 it != terminationMutexes.cend() /* not hoisted */;
		 /* no increment */) {
		delete it->second;
		std::cout << "VmManager::~VMManager: terminationMutex destroyed for "
				  << it->first << std::endl;
		terminationMutexes.erase(it++);
	}

	if (drawingThreads.empty() and ipUpdaterThreads.empty() and
		terminationMutexes.empty()) {
		std::cout << "VmManager::~VMManager: All threads and objects destroyed"
				  << std::endl;
	}

	delete mgr;
	// cout << "VmManager::~VmManager(): exited" << endl;
}

void VmManager::_fillViewsInGrid(Gtk::Grid *grid) {
	// cout << "VmManager::_fillViewsInGrid(Gtk::Grid *grid) called" << endl;
	auto namesOfVM = mgr->getAllDefinedDomainNames();

	int pos = 0;

	for (auto &name : namesOfVM) {
		terminationFlags.insert(make_pair(name, false));
		terminationMutexes.insert(make_pair(name, new mutex()));

		auto outerBox = Gtk::make_managed<Gtk::Box>();

		_getBoxWithWidgets(outerBox);
		_fillBoxWithName(outerBox, name);
		_fillBoxWithCPUUtil(outerBox,name,true);
		_fillBoxWithIP(outerBox, name, true);
		_setButtonsInBox(outerBox, name);
		_fillBoxWithIP(outerBox, name);
		grid->attach(*outerBox, pos % 2, pos / 2);
		pos++;
	}
	// cout << "VmManager::_fillViewsInGrid(Gtk::Grid *grid) exiting" << endl;
}

void VmManager::_getBoxWithWidgets(Gtk::Box *box) {
	// cout << "VmManager::_getBoxWithWidgets(Gtk::Box *box) called" << endl;
	box->set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
	box->set_spacing(5);

	auto lbl = Gtk::make_managed<Gtk::Label>();
	auto draw = Gtk::make_managed<Graph>();
	auto cpuUtil = Gtk::make_managed<Gtk::Label>();
	auto ip = Gtk::make_managed<Gtk::Label>();
	auto onButton = Gtk::make_managed<Gtk::Button>();
	auto offButton = Gtk::make_managed<Gtk::Button>();

	Gtk::Allocation alloc;
	alloc.set_width(150);
	alloc.set_height(50);
	draw->size_allocate(alloc);

	box->pack_start(*lbl, Gtk::PACK_SHRINK);
	box->pack_start(*draw);
	box->pack_start(*cpuUtil, Gtk::PACK_SHRINK);
	box->pack_start(*ip, Gtk::PACK_SHRINK);
	box->pack_start(*onButton, Gtk::PACK_SHRINK);
	box->pack_start(*offButton, Gtk::PACK_SHRINK);
	// cout << "VmManager::_getBoxWithWidgets(Gtk::Box *box) exited" << endl;
}

void VmManager::_fillBoxWithName(Gtk::Box *box, const string &nameOfVM) {
	// cout << "VmManager::_fillBoxWithName called for " << nameOfVM << endl;
	auto children = box->get_children();
	Gtk::Widget *name = children.at(0);
	if (GTK_IS_LABEL(name->gobj())) {
		auto label = dynamic_cast<Gtk::Label *>(name);
		label->set_text(nameOfVM);
	} else {
		std::cerr << "VmManager::__fillBoxWithName: label not found as first "
					 "element of the box"
				  << std::endl;
	}
	// cout << "VmManager::_fillBoxWithName exited for " << nameOfVM << endl;
}

bool VmManager::_fillBoxWithCPUUtil(Gtk::Box *box, const string &nameOfVM,
							   const bool update, const int util) {
	bool flag = false;
	string utilMsg;
	if (util >= 0 and util <= 100) {
		utilMsg = to_string(util) + "%";
	} else {
		utilMsg = "--";
	}

	auto children = box->get_children();
	Gtk::Widget *name = children.at(2);
	if (GTK_IS_LABEL(name->gobj())) {
		auto label = dynamic_cast<Gtk::Label *>(name);
		if (update or flag) {
			lock_guard lck(guiUpdateMutex);
			label->set_text("CPU Util : " + utilMsg);
		}
	} else {
		std::cerr << "VmManager::__fillBoxWithCPUUtil: label not found as "
					 "third element "
					 "of the box"
				  << std::endl;
	}
	return flag;
}


bool VmManager::_fillBoxWithIP(Gtk::Box *box, const string &nameOfVM,
							   bool update) {
	bool flag = false;
	string ip;
	if (mgr->isVmPowered(nameOfVM)) {
		ip = mgr->getIP(nameOfVM);
	} else {
		ip = "--";
	}
	if (ip.empty()) {
		ip = "--";
	}
	if (ip != "--") {
		flag = true;
	}

	auto children = box->get_children();
	Gtk::Widget *name = children.at(3);
	if (GTK_IS_LABEL(name->gobj())) {
		auto label = dynamic_cast<Gtk::Label *>(name);
		if (update or flag) {
			lock_guard lck(guiUpdateMutex);
			label->set_text("IP: " + ip);
		}
	} else {
		std::cerr << "VmManager::__fillBoxWithIP: label not found as "
					 "fourth element "
					 "of the box"
				  << std::endl;
	}
	return flag;
}

Gtk::Box *VmManager::_getBoxFromGrid(const string &nameOfVm) {
	auto widgets = m_box1.get_children();
	Gtk::Widget *wid = widgets.at(1);
	if (GTK_IS_GRID(wid->gobj())) {
		auto grid = dynamic_cast<Gtk::Grid *>(wid);
		auto boxes = grid->get_children();
		for (auto &&i : boxes) {
			if (GTK_IS_BOX(i->gobj())) {
				auto outerBox = dynamic_cast<Gtk::Box *>(i);
				auto children = outerBox->get_children();
				Gtk::Widget *name = children.at(0);
				if (GTK_IS_LABEL(name->gobj())) {
					auto label = dynamic_cast<Gtk::Label *>(name);
					auto name = label->get_text();
					if (nameOfVm == name) {
						// std::cout << "VmManager::_getBoxFromGrid: Box Found!"
						// 		  << endl;
						return outerBox;
					}
				} else {
					std::cerr << "VmManager::_getBoxFromGrid: label not "
								 "found as first "
								 "element of the box"
							  << std::endl;
				}
			} else {
				std::cout << "VmManager::_getBoxFromGrid: Widget inside a "
							 "grid is "
							 "not a box."
						  << endl;
			}
		}

	} else {
		std::cerr << "VmManager::getBoxFromGrid: label not found as first "
					 "element of the box"
				  << std::endl;
		return nullptr;
	}
	return nullptr;
}

void VmManager::_launchVmThreads(Gtk::Box *box, const string &name) {
	_spawnIPThread(name, box);
	_spawnDrawingThread(name, box);
}

void VmManager::_setButtonsInBox(Gtk::Box *box, const string &nameOfVM) {
	// cout << "VmManager::_setButtonsInBox called" << endl;
	auto children = box->get_children();
	Gtk::Widget *startButton = children.at(4);
	Gtk::Widget *shutButton = children.at(5);
	if (GTK_IS_BUTTON(startButton->gobj()) and
		GTK_IS_BUTTON(shutButton->gobj())) {
		auto start = dynamic_cast<Gtk::Button *>(startButton);
		auto shut = dynamic_cast<Gtk::Button *>(shutButton);
		start->add_label("Start Server");
		shut->add_label("Shutdown Server");

		start->set_sensitive(true);
		shut->set_sensitive(false);

		start->signal_clicked().connect(
			sigc::bind<std::string, Gtk::Box *, Gtk::Button *, Gtk::Button *>(
				sigc::mem_fun(*this, &VmManager::on_start_button_clicked),
				nameOfVM, box, start, shut));
		shut->signal_clicked().connect(
			sigc::bind<std::string, Gtk::Button *, Gtk::Button *>(
				sigc::mem_fun(*this, &VmManager::on_shut_button_clicked),
				nameOfVM, start, shut));
	} else {
		std::cerr << "VmManager::_setButtonsInBox: button not found as "
					 "fourth "
					 "element of the box"
				  << std::endl;
	}
	// cout << "VmManager::_setButtonsInBox exited" << endl;
}

void VmManager::_drawGraphInBox(Gtk::Box *box, const string &nameOfVm,
								bool clear) {
	// cout << "VmManager::_drawGraphInBox called for " << nameOfVm <<
	// endl;
	auto vec = mgr->getUtilVector(nameOfVm);

	auto children = box->get_children();
	Gtk::Widget *name = children.at(1);
	if (GTK_IS_DRAWING_AREA(name->gobj())) {
		auto graph = dynamic_cast<Graph *>(name);
		if (clear) {
			graph->clear();
		} else {
			graph->setVectorToDraw(vec);
		}
	} else {
		std::cerr << "VmManager::_drawGraphInBox: label not found as "
					 "third element "
					 "of the box"
				  << std::endl;
	}
	auto it = vec.rbegin();
	if(it!=vec.rend()){
		_fillBoxWithCPUUtil(box, nameOfVm, true, *it);
	}
	// cout << "VmManager::_drawGraphInBox exited " << nameOfVm << endl;
}

// Signal Handlers

void VmManager::on_start_button_clicked(const std::string &name, Gtk::Box *box,
										Gtk::Button *startButton,
										Gtk::Button *shutButton) {
	// cout << "VmManager::on_start_button_clicked called" << endl;
	startButton->set_sensitive(false);
	shutButton->set_sensitive(true);
	_powerOnImpl(name, box);
	// std::cout << "VmManager::on_start_button_clicked exiting" << endl;
}

void VmManager::on_shut_button_clicked(const std::string &name,
									   Gtk::Button *startButton,
									   Gtk::Button *shutButton) {
	// std::cout << "VmManager::on_shut_button_clicked called" << endl;
	startButton->set_sensitive(true);
	shutButton->set_sensitive(false);
	if (mgr->isVmPowered(name)) {
		_shutdownImpl(name);
	}
	// cout << "VmManager::on_shut_button_clicked exiting" << endl;
}

void VmManager::_powerOnImpl(const std::string &name, Gtk::Box *box) {
	// cout << "VmManager::_powerOnImpl called" << endl;
	auto launcherThread = new thread([this, name]() {
		mgr->startNewVm(name);
		if (not mgr->isVmPowered(name)) {
			_resetTerminationFlagForVmThreads(name);
			mgr->powerOn(name);
			mgr->startWatching(name);
			auto box = _getBoxFromGrid(name);
			if (box != nullptr) _launchVmThreads(box, name);
		}
	});

	{
		std::lock_guard lck(sanitizerMutex);
		launchThreads.push_back(launcherThread);
	}
	// cout << "VmManager::_powerOnImpl called" << endl;
}

void VmManager::_shutdownImpl(const string &name) {
	_issueTerminationToVmThreads(name);
	_reclaimMemory(name);
	auto shutdownThread = new thread([&] {
		if (mgr->isVmPowered(name)) mgr->shutdown(name);
	});
	{
		std::lock_guard lck(sanitizerMutex);
		launchThreads.push_back(shutdownThread);
	}
}

void VmManager::_spawnDrawingThread(const std::string &name, Gtk::Box *box) {
	// std::cout << "VmManager::_spawnDrawingThread called" << endl;
	auto drawingThread = new thread([this, box, name] {
		bool terminationFlag = false;
		while (not terminationFlag) {
			{
				auto m = terminationMutexes.find(name);
				if (m == terminationMutexes.end()) {
					std::cerr << "VmManager::drawingThread: no mutex "
								 "found for "
							  << name << std::endl;
					break;
				}
				auto f = terminationFlags.find(name);
				if (f == terminationFlags.end()) {
					std::cerr << "VmManager::drawingThread: no flag "
								 "found for "
							  << name << std::endl;
					break;
				}

				{
					std::lock_guard lck(*m->second);
					terminationFlag = f->second;
				}
				if ((not mgr->isVmPowered(name)) or terminationFlag) {
					std::cout << "VmManager::drawingThread: exiting thread"
							  << std::endl;
					break;
				} else {
					_drawGraphInBox(box, name, false);
				}
			}
			this_thread::sleep_for(chrono::seconds(1));
		}
		_drawGraphInBox(box, name, true);
	});
	drawingThreads.insert(std::make_pair(name, drawingThread));
	// cout << "VmManager::_spawnDrawingThread exiting" << endl;
}

void VmManager::_spawnIPThread(const string &name, Gtk::Box *box) {
	// cout << "VmManager::_spawnIPThread called" << endl;
	auto ipUpdaterThread = new thread([this, box, name] {
		bool terminationFlag = false;
		while (not terminationFlag) {
			{
				auto m = terminationMutexes.find(name);
				if (m == terminationMutexes.end()) {
					std::cerr << "VmManager::ipUpdaterThread: no mutex "
								 "found for "
							  << name << std::endl;
					break;
				}
				auto f = terminationFlags.find(name);
				if (f == terminationFlags.end()) {
					std::cerr << "VmManager::ipUpdaterThread: no flag "
								 "found for "
							  << name << std::endl;
					break;
				}

				{
					std::lock_guard lck(*m->second);
					terminationFlag = f->second;
				}
				if ((not mgr->isVmPowered(name)) or terminationFlag) {
					std::cout << "VmManager::ipUpdaterThread: exiting thread"
							  << std::endl;
					break;
				} else {
					if (_fillBoxWithIP(box, name)) {
						std::cout << "VmManager:ipUpdaterThread: IP updated. "
									 "Terminating thread for "
								  << name << "." << std::endl;
						mgr->notifyAboutServer();
						break;
					}
				}
			}
			this_thread::sleep_for(chrono::seconds(1));
		}
	});
	ipUpdaterThreads.insert(std::make_pair(name, ipUpdaterThread));
	// cout << "VmManager::_spawnIPThread exiting" << endl;
}

void VmManager::_resetTerminationFlagForVmThreads(const std::string &name) {
	// cout << "VmManager::_resetTerminationFlagForVmThreads called" << endl;
	auto itm = terminationMutexes.find(name);
	if (itm == terminationMutexes.end()) {
		std::cerr << "VmManager::_resetTerminationFlagForVmThreads: no mutex "
					 "found for "
				  << name << std::endl;
		return;
	}

	std::lock_guard lck(*itm->second);

	auto it = terminationFlags.find(name);
	if (it != terminationFlags.end()) {
		it->second = false;
	} else {
		std::cerr << "VmManager::_resetTerminationFlagForVmThreads: no flag "
					 "found for "
				  << name << std::endl;
	}
	// cout << "VmManager::_resetTerminationFlagForVmThreads exiting" << endl;
}

void VmManager::_issueTerminationToVmThreads(const std::string &name) {
	// cout << "VmManager::_issueTerminationToVmThreads called" << endl;
	auto itm = terminationMutexes.find(name);
	if (itm == terminationMutexes.end()) {
		std::cerr << "VmManager::terminationIssueThread: no mutex found for "
				  << name << std::endl;
		return;
	}

	std::lock_guard lck(*itm->second);

	auto it = terminationFlags.find(name);
	if (it != terminationFlags.end()) {
		it->second = true;
	} else {
		std::cerr << "VmManager::terminationIssueThread: no flag found for "
				  << name << std::endl;
	}
	// cout << "VmManager::_issueTerminationToVmThreads exiting" << endl;
}

void VmManager::_reclaimMemory(const std::string &name) {
	// cout << "VmManager::_reclaimMemory called" << endl;
	auto ipThread = ipUpdaterThreads.find(name);
	if (ipThread != ipUpdaterThreads.end()) {
		if (ipThread->second != nullptr and ipThread->second->joinable()) {
			ipThread->second->join();
			delete ipThread->second;
			ipUpdaterThreads.erase(name);
			std::cout << "VmManager::_reclaimMemory: ip thread for " << name
					  << " terminated" << std::endl;
		}
	} else {
		std::cerr << "VmManager::_reclaimMemory no ipUpdater thread "
					 "found for "
				  << name << std::endl;
	}

	auto drawThread = drawingThreads.find(name);
	if (drawThread != drawingThreads.end()) {
		if (drawThread->second != nullptr and drawThread->second->joinable()) {
			drawThread->second->join();
			delete drawThread->second;
			drawingThreads.erase(name);
			std::cout << "VmManager::_reclaimMemory: draw thread for " << name
					  << " terminated" << std::endl;
		}
	} else {
		std::cerr << "VmManager::_reclaimMemory: no draw thread "
					 "found for "
				  << name << std::endl;
	}
	// cout << "VmManager::_reclaimMemory exiting" << endl;
}
