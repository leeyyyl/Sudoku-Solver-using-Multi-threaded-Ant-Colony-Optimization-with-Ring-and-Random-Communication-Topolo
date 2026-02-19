#pragma once
#include "valueset.h"
#include <string>
#include <vector>
using namespace std;

// Forward declarations for constraint propagation functions
bool Rule1_Elimination(class Board& board, int cellIndex);
bool Rule2_HiddenSingle(class Board& board, int cellIndex);
void PropagateConstraints(class Board& board, int cellIndex);
void SetCellAndPropagate(class Board& board, int cellIndex, const ValueSet& value);

class Board
{
public:
	// Lifecycle
	Board(){};
	Board(const string &puzzleString);
	Board(const Board &other);
	~Board();

	// Core accessors/status
	const ValueSet &GetCell(int i) const;
	int FixedCellCount(void) const;
	int InfeasibleCellCount(void) const;
	int CellCount(void) const;
	int GetNumUnits() const;

	// Copy and validation
	void Copy(const Board &other);
	bool CheckSolution(const Board& other) const;

	// Geometric helpers
	int RowCell(int iRow, int iCell) const;
	int ColCell(int iCol, int iCell) const;
	int BoxCell(int iBox, int iCell) const;
	int RowForCell(int iCell) const;
	int ColForCell(int iCell) const;
	int BoxForCell(int iCell) const;

	// Output/visualization
	string AsString(bool useNumbers=false, bool showUnfixed = false);

	// Internal methods for constraint propagation (used by constraintpropagation.cpp)
	void SetCellDirect(int i, const ValueSet &c);
	void IncrementFixedCells();
	void IncrementInfeasible();

private:
	ValueSet *cells = nullptr;

	int order;   // order of puzzle
	int numUnits; // number of units (rows, columns, blocks)
	int numCells; // number of cells
	int numFixedCells; // number of cells with uniquely determined value
	int numInfeasible; // number of cells with no possibilities.
};

