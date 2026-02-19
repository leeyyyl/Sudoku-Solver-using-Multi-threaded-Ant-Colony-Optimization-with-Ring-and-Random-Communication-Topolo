#include "sudokuant.h"
#include "sudokuantsystem.h"
#include "constraintpropagation.h"

/*******************************************************************************
 * InitSolution
 *
 * Prepares this ant for a new construction run:
 * - copies the puzzle state
 * - sets the starting cell
 * - resets failure counters
 * - re-allocates roulette buffers for weighted selection
 ******************************************************************************/
void SudokuAnt::InitSolution(const Board &puzzle, int cellIndex )
{
	candidateSolution.Copy(puzzle);
	this->cellIndex = cellIndex;
	failCells = 0;
	if (roulette != nullptr)
	{
		delete[] roulette;
		delete[] rouletteVals;
	}
	roulette = new float[puzzle.GetNumUnits()];
	rouletteVals = new ValueSet[puzzle.GetNumUnits()];
}

/*******************************************************************************
 * StepSolution
 *
 * Advances one cell in this ant's construction path:
 * - skips fixed/empty edge cases
 * - chooses a value using ACS policy:
 *   * greedy with probability q0
 *   * roulette-wheel otherwise
 * - applies local pheromone update after setting the cell
 * - advances current index with wrap-around
 ******************************************************************************/
void SudokuAnt::StepSolution()
{
	if (candidateSolution.GetCell(cellIndex).Empty())
	{
		failCells++;
	}
	else if ( !candidateSolution.GetCell(cellIndex).Fixed() )
	{
		// make a choice from the options
		ValueSet choice = ValueSet(candidateSolution.GetNumUnits(), 1);
		if (parent->random() < parent->Getq0())
		{
			// greedy selection
			ValueSet best;
			float maxPher = -1.0f;

			for (int i = 0; i < candidateSolution.GetNumUnits(); i++)
			{
				if (candidateSolution.GetCell(cellIndex).Contains(choice))
				{
					if (parent->Pher(cellIndex, i) > maxPher)
					{
						maxPher = parent->Pher(cellIndex, i);
						best = choice;
					}
				}
				choice <<= 1;
			}
			SetCellAndPropagate(candidateSolution, cellIndex, best);
			// Apply ACS local update on the selected move.
			parent->LocalPheromoneUpdate(cellIndex, best.Index());
		}
		else
		{
			// weighted selection
			float totPher = 0.0f;
			int numChoices = 0;
			for (int i = 0; i < candidateSolution.GetNumUnits(); i++)
			{
				if (candidateSolution.GetCell(cellIndex).Contains(choice))
				{
					roulette[numChoices] = totPher + parent->Pher(cellIndex, i);
					totPher = roulette[numChoices];
					rouletteVals[numChoices] = choice;
					++numChoices;
				}
				choice <<= 1;
			}
			float rouletteVal = totPher * parent->random();

			for (int i = 0; i < numChoices; i++)
			{
				if (roulette[i] > rouletteVal)
				{
					SetCellAndPropagate(candidateSolution, cellIndex, rouletteVals[i]);
					// Apply ACS local update on the selected move.
					parent->LocalPheromoneUpdate(cellIndex, rouletteVals[i].Index());
					break;
				}
			}
		}
	}
	++cellIndex;
	if (cellIndex == candidateSolution.CellCount()) // wrap around
		cellIndex = 0;
}

