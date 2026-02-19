#pragma once
#include <vector>
#include <random>
#include "antcolonyinterface.h"
#include "sudokuant.h"
#include "board.h"
#include "timer.h"
#include "sudokusolver.h"

class SudokuAntSystem : public SudokuSolver, public IAntColony
{
	// Configuration
	int numAnts;
	float q0;
	float rho;
	float xi;
	float pher0;
	float bestEvap;

	// Best-so-far state
	Board bestSol;
	float bestPher;
	int iterationsCompleted;
	Timer solutionTimer;
	float solTime;

	// Ant population and randomness
	std::vector<SudokuAnt*> antList;
	std::mt19937 randGen; 
	std::uniform_real_distribution<float> randomDist;

	// Pheromone matrix
	float **pher; // pheromone matrix
	int numCells;

	// Internal helpers (called by Solve flow)
	void InitPheromone(int numCells, int valuesPerCell);
	float PherAdd(int numCellsFixed);
	void UpdatePheromone();
	void ClearPheromone();

public:
	// Lifecycle
	SudokuAntSystem(int numAnts, float q0, float rho, float xi, float pher0, float bestEvap) : 
		numAnts(numAnts), q0(q0), rho(rho), xi(xi), pher0(pher0), bestEvap(bestEvap), iterationsCompleted(0)
	{
		for ( int i = 0; i < numAnts; i++ )
			antList.push_back(new SudokuAnt(this));
		randomDist = std::uniform_real_distribution<float>(0.0f, 1.0f);
		std::random_device rd;
		randGen = std::mt19937(rd());
	}
	~SudokuAntSystem()
	{
		for (auto a : antList)
			delete a;
	}

	// Solver API
	virtual bool Solve(const Board& puzzle, float maxTime );
	virtual float GetSolutionTime() { return solTime; }
	virtual const Board& GetSolution() { return bestSol; }
	int GetIterationsCompleted() { return iterationsCompleted; }

	// IAntColony helpers for SudokuAnt
	inline float Getq0() { return q0; }
	inline float random() { return randomDist(randGen); }
	inline float Pher(int i, int j) { return pher[i][j]; }
	void LocalPheromoneUpdate(int cellIndex, int iChoice);
};
