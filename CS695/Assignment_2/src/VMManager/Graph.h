//
// Created by pranav on 18/04/20.
//

#ifndef ASSIGNMENT_2_GRAPH_H
#define ASSIGNMENT_2_GRAPH_H

#include <gtkmm/drawingarea.h>

#include <vector>

class Graph : public Gtk::DrawingArea {
   private:
	std::mutex m;
	std::vector<int> mCurrState;
	std::vector<int> mPrevState;
	enum { DRAW_HOLD, DRAW_START, DRAW_CLEAR } mState;

	static const int TIMEOUT_INTERVAL_IN_MILLIS = 1000;

   protected:
	bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
	bool on_timeout();

   public:
   	static const int MAX_TICKS_ON_GRAPH = 50;

	Graph();
	~Graph() override;
	void setVectorToDraw(const std::vector<int>& vec);
	void clear();
};

#endif	// ASSIGNMENT_2_GRAPH_H
