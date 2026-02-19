/*******************************************************************************
 * SUDOKU ANT SYSTEM - Algorithm 0 (Single-threaded ACS)
 * 
 * This file implements the standard Ant Colony System for solving Sudoku.
 * This is the baseline algorithm (Algorithm 0) that runs in a single thread.
 * 
 * Key Components:
 * - Pheromone matrix: guides ant construction
 * - Ant population: constructs candidate solutions
 * - Best-so-far solution tracking
 * - ACS pheromone update rules (global + local)
 * 
 * Reference: Dorigo & Gambardella (1997) - Ant Colony System
 ******************************************************************************/

#include "sudokuantsystem.h"

// ============================================================================
// SECTION 1: ENTRY POINT (ALGORITHM 0)
// ============================================================================

/*******************************************************************************
 * Solve - Main algorithm loop for single-threaded ACS
 * 
 * Algorithm flow:
 * 1. Initialize pheromone matrix
 * 2. Loop until solution found or timeout:
 *    a. Each ant constructs a solution (probabilistic + greedy choice)
 *    b. Find iteration-best ant
 *    c. Update best-so-far if improved
 *    d. Apply global pheromone update (reinforce best-so-far)
 *    e. Decay best pheromone value
 * 3. Return success/failure
 * 
 * Parameters:
 *   puzzle   - The initial puzzle (after constraint propagation)
 *   maxTime  - Time limit in seconds
 * 
 * Returns: true if a complete solution was found
 ******************************************************************************/
bool SudokuAntSystem::Solve(const Board& puzzle, float maxTime )
{
	solutionTimer.Reset();
	int iter = 0;
	bool solved = false;
	bestPher = 0.0f;
	iterationsCompleted = 0;
	
	// Initialize pheromone matrix
	InitPheromone( puzzle.CellCount(), puzzle.GetNumUnits() );
	
	// Main iteration loop
	while (!solved)
	{
		// === ANT CONSTRUCTION PHASE ===
		// Start each ant on a random cell
		std::uniform_int_distribution<int> dist(0, puzzle.CellCount()-1);
		for (auto a : antList)
		{
			a->InitSolution(puzzle, dist(randGen));
		}
		
		// Fill cells one at a time (all ants step in parallel)
		for (int i = 0; i < puzzle.CellCount(); i++)
		{
			for (auto a : antList)
			{
				a->StepSolution();
			}
		}
		
		// === FIND ITERATION-BEST ANT ===
		int iBest = 0;
		int bestVal = 0;
		for (unsigned int i = 0; i < antList.size(); i++)
		{
			if (antList[i]->NumCellsFilled() > bestVal)
			{
				bestVal = antList[i]->NumCellsFilled();
				iBest = i;
			}
		}
		
		// Calculate pheromone reinforcement value
		float pherToAdd = PherAdd(bestVal);

		// === UPDATE BEST-SO-FAR SOLUTION ===
		if (pherToAdd > bestPher)
		{
			bestSol.Copy(antList[iBest]->GetSolution());
			bestPher = pherToAdd;
			
			// Check if complete solution found
			if (bestVal == numCells)
			{
				solved = true;
				solTime = solutionTimer.Elapsed();
			}
		}
		
		// === PHEROMONE UPDATE ===
		UpdatePheromone();           // Global update (reinforce best-so-far)
		bestPher *= (1.0f - bestEvap); // Decay best pheromone value
		
		++iter;
		
		// === TIMEOUT CHECK (every 100 iterations) ===
		if ((iter % 100) == 0)
		{
			float elapsed = solutionTimer.Elapsed();
			if ( elapsed > maxTime)
			{
				break;
			}
		}
	}
	
	iterationsCompleted = iter;
	ClearPheromone();
	return solved;
}

// ============================================================================
// SECTION 2: HELPER FUNCTIONS (ORDERED BY USAGE FLOW)
// ============================================================================

/*******************************************************************************
 * InitPheromone - Allocate and initialize the pheromone matrix
 *
 * Creates a [cell x value] matrix and initializes all entries to pher0.
 ******************************************************************************/
void SudokuAntSystem::InitPheromone(int numCells, int valuesPerCell )
{
	this->numCells = numCells;
	pher = new float*[numCells];
	for (int i = 0; i < numCells; i++)
	{
		pher[i] = new float[valuesPerCell];
		for (int j = 0; j < valuesPerCell; j++)
			pher[i][j] = pher0;
	}
}

/*******************************************************************************
 * LocalPheromoneUpdate - Local pheromone update (ACS rule)
 *
 * Called by ants during construction to encourage exploration:
 *   τ(i,j) ← (1-ξ)·τ(i,j) + ξ·τ₀
 ******************************************************************************/
void SudokuAntSystem::LocalPheromoneUpdate(int cellIndex, int iChoice)
{
	pher[cellIndex][iChoice] = pher[cellIndex][iChoice] * (1.0f - xi) + pher0 * xi;
}

/*******************************************************************************
 * PherAdd - Calculate pheromone reinforcement from solution quality
 ******************************************************************************/
float SudokuAntSystem::PherAdd(int numCellsFixed)
{
	return numCells / (float)(numCells - numCellsFixed);
}

/*******************************************************************************
 * UpdatePheromone - Global pheromone update (ACS rule)
 * 
 * Reinforces the best-so-far solution by updating pheromone on its path:
 *   τ(i,j) ← (1-ρ)·τ(i,j) + ρ·bestPher
 * 
 * Only cells in the best-so-far solution receive reinforcement.
 * This is the standard ACS global update rule.
 ******************************************************************************/
void SudokuAntSystem::UpdatePheromone()
{
	for (int i = 0; i < numCells; i++)
	{
		if (bestSol.GetCell(i).Fixed())
		{
			pher[i][bestSol.GetCell(i).Index()] = pher[i][bestSol.GetCell(i).Index()] * (1.0f - rho) + rho * bestPher;
		}
	}
}

/*******************************************************************************
 * ClearPheromone - Deallocate the pheromone matrix
 *
 * Called at the end of solving to release matrix memory.
 ******************************************************************************/
void SudokuAntSystem::ClearPheromone()
{
	for (int i = 0; i < numCells; i++)
		delete[] pher[i];
	delete[] pher;
}
