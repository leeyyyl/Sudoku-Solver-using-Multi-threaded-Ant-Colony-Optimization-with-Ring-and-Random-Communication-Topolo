#pragma once
#include "board.h"
#include "timer.h"
#include "sudokusolver.h"

class BacktrackSearch : public SudokuSolver
{
private:
	// Recursion worker
	void StepSolution(const Board& board);

	// State/metrics
	Timer solutionTimer;
	float solTime;
	Board solution;
	bool solved;
	int stepCount;
	bool timedOut;
	float timeOut;

public:
	BacktrackSearch() : solTime(0.0f), stepCount(0), timedOut(false) {}
	virtual bool Solve(const Board& puzzle, float maxTime);
	virtual float GetSolutionTime() { return solTime; }
	virtual const Board& GetSolution() { return solution; }
};
