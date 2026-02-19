#pragma once
#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include "antcolonyinterface.h"
#include "sudokuant.h"
#include "board.h"
#include "timer.h"
#include "sudokusolver.h"

// Forward declaration
class ParallelSudokuAntSystem;

// SubColony: one independent ant colony executed by a worker thread.
class SubColony : public IAntColony
{
private:
	// Configuration
	int numAnts;
	float q0;
	float rho;        // ACS evaporation parameter (used for both standard and communication updates)
	float xi;         // ACS local pheromone update parameter
	float pher0;
	
	// Local and received solution state
	Board iterationBest;      // Best solution in current iteration (ΔT_ij^1 - local)
	Board bestSol;            // Best solution found so far (used in standard Algorithm 0 update)
	Board receivedIterationBest;  // Received iteration-best from ring topology (ΔT_ij^2)
	Board receivedBestSol;        // Received best-so-far from random topology (ΔT_ij^3)
	
	int iterationBestScore;   // Number of cells filled in iteration best
	int bestSolScore;         // Number of cells filled in best so far
	int receivedIterationBestScore;  // Score of received iteration-best
	int receivedBestSolScore;        // Score of received best-so-far
	
	std::vector<SudokuAnt*> antList;
	std::mt19937 randGen;
	std::uniform_real_distribution<float> randomDist;
	std::uniform_int_distribution<int> startPosDist;  // For ant starting positions (reused)
	
	// Pheromone matrix and dimensions
	float **pher;
	int numCells;
	int numUnits;
	
	// Temporary arrays for pheromone updates (allocated once, reused every iteration)
	float* contributions;
	bool* hasContribution;
	
	void InitPheromone(int numCells, int valuesPerCell);
	void ClearPheromone();
	float PherAdd(int numCellsFixed);
	
public:
	int currentIteration;     // Current iteration number (public for access by worker)
	float bestPher;           // Best pheromone value (for Algorithm 0 standard update)
	float bestEvap;           // Best pheromone evaporation parameter
	
	// Lifecycle
	SubColony(int id, int numAnts, float q0, float rho, float xi, float pher0, float bestEvap);
	~SubColony();
	
	// Iteration flow
	void Initialize(const Board& puzzle);
	void RunIteration(const Board& puzzle);
	
	// Pheromone update rules
	void UpdatePheromone();
	void UpdatePheromoneWithCommunication();
	
	// Get results
	const Board& GetIterationBest() const { return iterationBest; }
	const Board& GetBestSol() const { return bestSol; }
	int GetBestSolScore() const { return bestSolScore; }
	int GetCurrentIteration() const { return currentIteration; }
	
	// Communication inputs
	void ReceiveIterationBest(const Board& solution);
	void ReceiveBestSol(const Board& solution);
	
	// IAntColony helpers for SudokuAnt
	inline float Getq0() { return q0; }
	inline float random() { return randomDist(randGen); }
	inline float Pher(int i, int j) { return pher[i][j]; }
	void LocalPheromoneUpdate(int cellIndex, int iChoice);
};

// Parallel Ant Colony System with multiple sub-colonies
class ParallelSudokuAntSystem : public SudokuSolver
{
private:
	// Configuration and colony collection
	int numSubColonies;
	float maxTime;  // Maximum time in seconds
	std::vector<SubColony*> subColonies;
	
	// Global best/result metrics
	Board globalBest;
	int globalBestScore;
	int iterationsCompleted;
	bool communicationOccurred;
	float communicationTime;
	float solTime;
	Timer solutionTimer;

	// Communication interval timing state
	std::atomic<bool> communicationPhaseActive;
	std::atomic<int> communicationPhaseDone;
	std::chrono::high_resolution_clock::time_point communicationPhaseStart;
	
	std::mt19937 masterRandGen;
	
	// Synchronization primitives
	std::mutex commMutex;
	std::condition_variable commCV;
	std::atomic<int> barrier;
	std::atomic<bool> stopFlag;
	
	// Communication helpers
	std::vector<int> GenerateMatchArray();
	void CommunicateRingTopology();
	void CommunicateRandomTopology(const std::vector<int>& matchArray);
	
	// Worker entry point
	void SubColonyWorker(int colonyId, const Board& puzzle);
	
	// Worker/coordination helpers
	bool CheckTimeout();
	void ReportProgress(int colonyId, int iteration, const Board& puzzle);
	bool CheckSolutionFound(SubColony* colony);
	void PerformBarrierSynchronization();
	void ExecuteMasterThreadTasks();
	void ExecuteWorkerThreadWait(std::unique_lock<std::mutex>& lock);
	void CompleteCommunicationPhase();
	
public:
	ParallelSudokuAntSystem(int numSubColonies, int numAntsPerColony, 
	                        float q0, float rho, float xi, float pher0, float bestEvap);
	~ParallelSudokuAntSystem();
	
	virtual bool Solve(const Board& puzzle, float maxTime);
	virtual float GetSolutionTime() { return solTime; }
	virtual const Board& GetSolution() { return globalBest; }
	int GetIterationsCompleted() { return iterationsCompleted; }
	bool GetCommunicationOccurred() { return communicationOccurred; }
	float GetCommunicationTime() { return communicationTime; }
};

