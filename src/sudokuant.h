#pragma once
#include "board.h"
#include "antcolonyinterface.h"

class SudokuAnt
{
	// Working state for one ant
	Board candidateSolution;	// current working solution
	int cellIndex;	// current cell
	IAntColony *parent;	// parent ant colony (can be SudokuAntSystem or SubColony)
	int failCells;	// no of cells on this attempt which were unsettable

	// Temporary buffers for roulette-wheel selection
	float *roulette; // working array for the roulette wheel selection
	ValueSet *rouletteVals; // working array for the roulette wheel selection

public:	
	SudokuAnt(IAntColony *parent) : parent(parent), cellIndex(0), roulette(nullptr), rouletteVals(nullptr) {}

	// Construction flow
	void InitSolution(const Board &puzzle, int cellIndex);
	void StepSolution();

	// Result accessors
	const Board& GetSolution() { return candidateSolution; }
	int NumCellsFilled() { return candidateSolution.CellCount() - failCells; }
};
