//
// Created by pranav on 18/04/20.
//

#ifndef ASSIGNMENT_2_VMMANAGER_H
#define ASSIGNMENT_2_VMMANAGER_H

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include <thread>
#include <unordered_map>

#include "Manager.h"

class VmManager : public Gtk::Window {
   private:
	Manager *mgr;

	list<thread *> launchThreads;
	unordered_map<string, thread *> ipUpdaterThreads;
	unordered_map<string, thread *> drawingThreads;

	unordered_map<string, bool> terminationFlags;
	unordered_map<string, mutex *> terminationMutexes;

	mutex guiUpdateMutex;

	thread *sanitiserThread;
	mutex sanitizerMutex;
	bool sanitizerThreadTerminationFlag;

	void _fillViewsInGrid(Gtk::Grid *grid);
	bool _fillBoxWithCPUUtil(Gtk::Box *box, const string &nameOfVM,
						const bool update = false, const int util = -1);
	bool _fillBoxWithIP(Gtk::Box *box, const string &nameOfVM,
						const bool update = false);
	void _drawGraphInBox(Gtk::Box *box, const string &nameOfVm,const bool clear);
	void _setButtonsInBox(Gtk::Box *box, const string &nameOfVM);
	void _powerOnImpl(const string &name, Gtk::Box *box);
	void _shutdownImpl(const string &name);

	// static functions
	static void _getBoxWithWidgets(Gtk::Box *box);
	static void _fillBoxWithName(Gtk::Box *box, const string &nameOfVM);

   public:
	VmManager();
	~VmManager() override;

   protected:
	Gtk::Box m_box1;
	unordered_map<std::string, Gtk::Button> startButtons;
	unordered_map<std::string, Gtk::Button> shutButtons;

	// Signal handlers:
	void on_start_button_clicked(const std::string &name, Gtk::Box *box,
								 Gtk::Button *start, Gtk::Button *shut);
	void on_shut_button_clicked(const std::string &name, Gtk::Button *start,
								Gtk::Button *shut);

	Gtk::Box *_getBoxFromGrid(const string &name);
	void _spawnIPThread(const std::string &name, Gtk::Box *box);
	void _spawnDrawingThread(const std::string &name, Gtk::Box *box);
	void _issueTerminationToVmThreads(const std::string &name);
	void _reclaimMemory(const string &name);
	void _resetTerminationFlagForVmThreads(const string &name);
	void _launchVmThreads(Gtk::Box *pBox, const string &basicString);
};

#endif	// ASSIGNMENT_2_VMMANAGER_H
