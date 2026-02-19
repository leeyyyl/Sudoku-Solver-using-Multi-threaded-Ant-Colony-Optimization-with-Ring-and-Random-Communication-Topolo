// Backtracking search using the same constraint-propagation rules
// as the ant-based solvers.
#include "backtracksearch.h"
#include "constraintpropagation.h"

/*******************************************************************************
 * Solve
 *
 * Initializes search state and starts recursive backtracking.
 ******************************************************************************/
bool BacktrackSearch::Solve(const Board& puzzle, float maxTime)
{
	solved = false;
	timedOut = false;
	timeOut = maxTime;
	solutionTimer.Reset();
	StepSolution(puzzle);
	solTime = solutionTimer.Elapsed();
	return solved;
}

/*******************************************************************************
 * StepSolution
 *
 * Recursive backtracking step with:
 * - timeout guard
 * - minimum-remaining-values (MRV) next-cell choice
 * - constraint propagation after each tentative assignment
 ******************************************************************************/
void BacktrackSearch::StepSolution(const Board &puzzle)
{
	// Timeout guard
	if (timedOut)
		return;
	stepCount++;
	if ( stepCount%5000 == 0 )
	{
		if ( solutionTimer.Elapsed() > timeOut )
		{
			timedOut = true;
			return;
		}
	}
	// Find the cell with the fewest possibilities (MRV heuristic).
	int nextCell = -1;
	int minCount = puzzle.GetNumUnits()+1;

	for (int iCell = 0; iCell < puzzle.CellCount(); iCell++)
	{
		if (!puzzle.GetCell(iCell).Fixed() && puzzle.GetCell(iCell).Count() < minCount)
		{
			nextCell = iCell;
			minCount = puzzle.GetCell(iCell).Count();
			if (minCount == 2)
				break;
		}
	}
	if (nextCell == -1)
	{
		// No unfixed cell left: solved.
		solved = true;
		solution.Copy(puzzle);
		return;
	}

	// Try each candidate value for nextCell.
	ValueSet choice = ValueSet(puzzle.GetNumUnits(), 1);
	for (int i = 0; i < puzzle.GetNumUnits(); i++)
	{
		if (solved)
			return;
		if ( puzzle.GetCell(nextCell).Contains(choice))
		{
			// Copy board and assign a candidate.
			Board newBoard;
			newBoard.Copy(puzzle);
			SetCellAndPropagate(newBoard, nextCell, choice);

			// Check completion.
			if (newBoard.FixedCellCount() == newBoard.CellCount())
			{
				solved = true;
				solution.Copy(newBoard);
				return;
			}

			// Recurse only if board remains feasible.
			if (newBoard.InfeasibleCellCount() == 0)
			{
				StepSolution(newBoard);
			}
		}
		choice <<= 1;
	}
}
