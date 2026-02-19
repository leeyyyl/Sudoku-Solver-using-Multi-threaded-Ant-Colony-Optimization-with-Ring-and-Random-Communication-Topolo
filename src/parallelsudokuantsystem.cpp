/*******************************************************************************
 * PARALLEL ANT COLONY SYSTEM FOR SUDOKU - ALGORITHM 2
 * 
 * This implementation uses multiple sub-colonies running in parallel threads.
 * Each sub-colony maintains its own pheromone matrix and ant population.
 * 
 * Key Features:
 * - Multi-threaded execution (one thread per sub-colony)
 * - Two communication topologies:
 *   1. Ring topology: iteration-best solutions
 *   2. Random topology: best-so-far solutions
 * - Three-source pheromone update (local + 2 received solutions)
 * - Timeout-based termination (default 120 seconds)
 * - Immediate stop upon finding complete solution
 ******************************************************************************/

#include "parallelsudokuantsystem.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <chrono>

// ============================================================================
// SUBCOLONY CLASS - Represents one independent ant colony in a thread
// ============================================================================

// ----------------------------------------------------------------------------
// Constructor: Initialize a sub-colony with its own parameters
// ----------------------------------------------------------------------------
SubColony::SubColony(int id, int numAnts, float q0, float rho, float xi, float pher0, float bestEvap)
	: numAnts(numAnts), q0(q0), rho(rho), xi(xi), pher0(pher0), bestEvap(bestEvap), bestPher(0.0f),
	  iterationBestScore(0), bestSolScore(0), receivedIterationBestScore(0), receivedBestSolScore(0),
	  currentIteration(0), pher(nullptr), numCells(0), numUnits(0),
	  contributions(nullptr), hasContribution(nullptr)
{
	// Initialize random number generator with unique seed per colony
	randomDist = std::uniform_real_distribution<float>(0.0f, 1.0f);
	std::random_device rd;
	randGen = std::mt19937(rd() + id); // Different seed ensures diversity
}

SubColony::~SubColony()
{
	for (auto a : antList)
		delete a;
	if (pher != nullptr)
		ClearPheromone();
	if (contributions != nullptr)
		delete[] contributions;
	if (hasContribution != nullptr)
		delete[] hasContribution;
}

// ----------------------------------------------------------------------------
// Initialize: Prepare sub-colony for a new puzzle
// Called once before starting the parallel algorithm
// ----------------------------------------------------------------------------
void SubColony::Initialize(const Board& puzzle)
{
	numCells = puzzle.CellCount();
	numUnits = puzzle.GetNumUnits();
	
	// Setup random distribution for ant starting positions
	startPosDist = std::uniform_int_distribution<int>(0, numCells - 1);
	
	// === ANT POPULATION SETUP ===
	// Clear old ants if any
	for (auto a : antList)
		delete a;
	antList.clear();
	
	// Create new ant population
	for (int i = 0; i < numAnts; i++)
		antList.push_back(new SudokuAnt(this));
	
	// === PHEROMONE MATRIX INITIALIZATION ===
	InitPheromone(numCells, numUnits);
	
	// Allocate temporary arrays for pheromone updates (performance optimization)
	// These arrays are reused every iteration to avoid repeated allocations
	if (contributions != nullptr)
		delete[] contributions;
	if (hasContribution != nullptr)
		delete[] hasContribution;
	contributions = new float[numUnits];
	hasContribution = new bool[numUnits];
	
	// === SOLUTION TRACKING INITIALIZATION ===
	iterationBest.Copy(puzzle);
	bestSol.Copy(puzzle);
	receivedIterationBest.Copy(puzzle);   // From ring topology neighbor
	receivedBestSol.Copy(puzzle);         // From random topology partner
	
	iterationBestScore = puzzle.FixedCellCount();
	bestSolScore = puzzle.FixedCellCount();
	receivedIterationBestScore = 0;
	receivedBestSolScore = 0;
	currentIteration = 0;
	bestPher = 0.0f;  // Reset best pheromone value
}

void SubColony::InitPheromone(int numCells, int valuesPerCell)
{
	if (pher != nullptr)
		ClearPheromone();
	
	this->numCells = numCells;
	pher = new float*[numCells];
	for (int i = 0; i < numCells; i++)
	{
		pher[i] = new float[valuesPerCell];
		for (int j = 0; j < valuesPerCell; j++)
			pher[i][j] = pher0;
	}
}

void SubColony::ClearPheromone()
{
	for (int i = 0; i < numCells; i++)
		delete[] pher[i];
	delete[] pher;
	pher = nullptr;
}

float SubColony::PherAdd(int numCellsFixed)
{
	return numCells / (float)(numCells - numCellsFixed);
}

// ============================================================================
// STANDARD ALGORITHM 0 GLOBAL PHEROMONE UPDATE
// ============================================================================
// This is the standard ACS global update that happens EVERY iteration.
// Updates only the edges in the best-so-far solution.
//
// EQUATION: τ_ij(t+1) = (1-ρ)·τ_ij(t) + ρ·Δτ_best
//           where ρ = 0.9 (standard ACS parameter)
// ============================================================================
void SubColony::UpdatePheromone()
{
	for (int i = 0; i < numCells; i++)
	{
		if (bestSol.GetCell(i).Fixed())
		{
			pher[i][bestSol.GetCell(i).Index()] = pher[i][bestSol.GetCell(i).Index()] * (1.0f - rho) + rho * bestPher;
		}
	}
}

// ============================================================================
// THREE-SOURCE PHEROMONE UPDATE FOR COMMUNICATION
// ============================================================================
// This implements the parallel ACS pheromone update with THREE sources.
// Called ONLY after communication exchanges.
//
// EQUATION: τ_ij(t+1) = (1-ρ)·τ_ij(t) + Δτ_ij
//           where ρ is the rate of pheromone evaporation (same as standard ACS)
//           and Δτ_ij = Δτ_ij^1 + Δτ_ij^2 + Δτ_ij^3
//
// SOURCE 1 (Δτ_ij^1): Local iteration-best (this colony's best this iteration)
// SOURCE 2 (Δτ_ij^2): Received iteration-best (from ring topology neighbor)
// SOURCE 3 (Δτ_ij^3): Received best-so-far (from random topology partner)
//
// SELECTIVE EVAPORATION: Only applies evaporation to [cell,digit] pairs that
//                        receive pheromone deposits (not all cells)
// ============================================================================
void SubColony::UpdatePheromoneWithCommunication()
{
	// --- STEP 1: Calculate pheromone deposit amounts for each source ---
	float pherValue1 = (iterationBestScore > 0) ? PherAdd(iterationBestScore) : 0.0f;
	float pherValue2 = (receivedIterationBestScore > 0) ? PherAdd(receivedIterationBestScore) : 0.0f;
	float pherValue3 = (receivedBestSolScore > 0) ? PherAdd(receivedBestSolScore) : 0.0f;
	
	// --- STEP 2: Process each cell ---
	for (int i = 0; i < numCells; i++)
	{
		// Clear temporary arrays for this cell
		std::fill(contributions, contributions + numUnits, 0.0f);
		std::fill(hasContribution, hasContribution + numUnits, false);
		
		// --- STEP 3: Collect contributions from all three sources ---
		
		// Source 1: Local iteration-best solution
		if (iterationBestScore > 0 && iterationBest.GetCell(i).Fixed())
		{
			int digit = iterationBest.GetCell(i).Index();
			contributions[digit] += pherValue1;
			hasContribution[digit] = true;
		}
		
		// Source 2: Iteration-best from ring topology neighbor
		if (receivedIterationBestScore > 0 && receivedIterationBest.GetCell(i).Fixed())
		{
			int digit = receivedIterationBest.GetCell(i).Index();
			contributions[digit] += pherValue2;
			hasContribution[digit] = true;
		}
		
		// Source 3: Best-so-far from random topology partner
		if (receivedBestSolScore > 0 && receivedBestSol.GetCell(i).Fixed())
		{
			int digit = receivedBestSol.GetCell(i).Index();
			contributions[digit] += pherValue3;
			hasContribution[digit] = true;
		}
		
		// --- STEP 4: Apply evaporation + reinforcement ---
		// Only for [cell, digit] pairs that received contributions
		for (int j = 0; j < numUnits; j++)
		{
			if (hasContribution[j])
			{
				// Evaporate old pheromone, add new contribution
				// Using rho for communication update
				pher[i][j] = pher[i][j] * (1.0f - rho) + rho * contributions[j];
			}
		}
	}
}

void SubColony::LocalPheromoneUpdate(int cellIndex, int iChoice)
{
	pher[cellIndex][iChoice] = pher[cellIndex][iChoice] * (1.0f - xi) + pher0 * xi;
}

// ----------------------------------------------------------------------------
// RunIteration: Execute one complete iteration of the ant colony
// ----------------------------------------------------------------------------
void SubColony::RunIteration(const Board& puzzle)
{
	// === PHASE 1: SOLUTION CONSTRUCTION ===
	// Start each ant on a random cell (for diversity)
	for (auto a : antList)
	{
		a->InitSolution(puzzle, startPosDist(randGen));
	}
	
	// Each ant constructs a solution by visiting all cells
	for (int i = 0; i < numCells; i++)
	{
		// All ants take one step (fill one cell)
		for (auto a : antList)
		{
			a->StepSolution();
		}
	}
	
	// === PHASE 2: SOLUTION EVALUATION ===
	// Find the best ant in this iteration
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
	
	// === PHASE 3: SOLUTION TRACKING ===
	// Update iteration-best (best in this iteration)
	iterationBest.Copy(antList[iBest]->GetSolution());
	iterationBestScore = bestVal;
	
	// Calculate pheromone value for this iteration's best
	float pherToAdd = PherAdd(bestVal);
	
	// Update best-so-far (best ever found by this colony)
	// EXACTLY like Algorithm 0: compare by pheromone value, update BOTH together
	if (pherToAdd > bestPher)
	{
		bestSol.Copy(iterationBest);
		bestSolScore = iterationBestScore;
		bestPher = pherToAdd;
	}
}

void SubColony::ReceiveIterationBest(const Board& solution)
{
	// Store received iteration-best from ring topology
	// This solution is ONLY used for the three-source pheromone update
	// It does NOT update our own bestSol (which stays independent, like Algorithm 0)
	receivedIterationBest.Copy(solution);
	receivedIterationBestScore = solution.FixedCellCount();
}

void SubColony::ReceiveBestSol(const Board& solution)
{
	// Store received best-so-far from random topology
	// This solution is ONLY used for the three-source pheromone update
	// It does NOT update our own bestSol (which stays independent, like Algorithm 0)
	receivedBestSol.Copy(solution);
	receivedBestSolScore = solution.FixedCellCount();
}

// ============================================================================
// PARALLEL ANT COLONY SYSTEM - Master Coordinator
// ============================================================================
// This class manages multiple sub-colonies running in parallel threads.
// It handles thread creation, synchronization, and communication coordination.
// ============================================================================

// ----------------------------------------------------------------------------
// Constructor: Create the parallel system with N sub-colonies
// ----------------------------------------------------------------------------
ParallelSudokuAntSystem::ParallelSudokuAntSystem(int nSubColonies, int numAntsPerColony,
	float q0, float rho, float xi, float pher0, float bestEvap)
	: numSubColonies(nSubColonies), maxTime(120.0f),
	  globalBestScore(0), iterationsCompleted(0), communicationOccurred(false), communicationTime(0.0f),
	  solTime(0.0f), barrier(0), stopFlag(false),
	  communicationPhaseActive(false), communicationPhaseDone(0)
{
	// Create N independent sub-colonies
	// Note: rho is used for both standard ACS global update and communication update
	for (int i = 0; i < numSubColonies; i++)
	{
		subColonies.push_back(new SubColony(i, numAntsPerColony, q0, rho, xi, pher0, bestEvap));
	}
	
	// Initialize master random generator (for random topology matching)
	std::random_device rd;
	masterRandGen = std::mt19937(rd());
}

ParallelSudokuAntSystem::~ParallelSudokuAntSystem()
{
	for (auto colony : subColonies)
		delete colony;
}

// ============================================================================
// MAIN SOLVE METHOD - Entry point for parallel algorithm
// ============================================================================
// This method orchestrates the entire parallel solving process:
// 1. Initializes synchronization structures
// 2. Spawns N worker threads (one per sub-colony)
// 3. Waits for completion (solution found or timeout)
// 4. Collects results from all threads
// ============================================================================
bool ParallelSudokuAntSystem::Solve(const Board& puzzle, float timeLimit)
{
	// === INITIALIZATION ===
	maxTime = (timeLimit > 0) ? timeLimit : 120.0f;
	
	solutionTimer.Reset();
	communicationOccurred = false;
	communicationTime = 0.0f;
	communicationPhaseActive.store(false);
	communicationPhaseDone.store(0);
	stopFlag.store(false);   // Shared stop signal (atomic)
	barrier.store(0);        // Synchronization counter (atomic)
	
	globalBest.Copy(puzzle);
	globalBestScore = puzzle.FixedCellCount();
	
	// === THREAD CREATION ===
	std::vector<std::thread> threads;
	for (int i = 0; i < numSubColonies; i++)
	{
		threads.emplace_back(&ParallelSudokuAntSystem::SubColonyWorker, this, i, std::ref(puzzle));
	}
	
	// === THREAD SYNCHRONIZATION ===
	for (auto& thread : threads)
	{
		thread.join();
	}
	
	// === RESULT COLLECTION ===
	iterationsCompleted = 0;
	for (int i = 0; i < numSubColonies; i++)
	{
		if (subColonies[i]->GetBestSolScore() > globalBestScore)
		{
			globalBest.Copy(subColonies[i]->GetBestSol());
			globalBestScore = subColonies[i]->GetBestSolScore();
		}
		
		if (subColonies[i]->GetCurrentIteration() > iterationsCompleted)
		{
			iterationsCompleted = subColonies[i]->GetCurrentIteration();
		}
	}
	
	solTime = solutionTimer.Elapsed();
	return (globalBestScore == puzzle.CellCount());
}

// ============================================================================
// SUBCOLONY WORKER THREAD - Main parallel execution loop
// ============================================================================
// This is the entry point for each thread. Each sub-colony runs independently
// in its own thread, periodically synchronizing for solution exchange.
//
// TERMINATION CONDITIONS:
// 1. Any colony finds a complete solution (all cells filled correctly)
// 2. Global timeout exceeded (default: 120 seconds)
//
// SYNCHRONIZATION:
// - Threads run independently most of the time (parallel execution)
// - Periodic barrier synchronization for solution communication
// - Master thread performs communication while others wait
// ============================================================================
void ParallelSudokuAntSystem::SubColonyWorker(int colonyId, const Board& puzzle)
{
	SubColony* colony = subColonies[colonyId];
	colony->Initialize(puzzle);
	
	int iter = 0;
	
	// Optimization: for a single thread, avoid repeated atomic loads.
	bool shouldStop = false;
	bool useLocalStop = (numSubColonies == 1);
	
	while (useLocalStop ? !shouldStop : !stopFlag.load())
	{
		if (CheckTimeout())
		{
			if (useLocalStop)
				shouldStop = true;
			break;
		}
		
		iter++;
		colony->currentIteration = iter;
		colony->RunIteration(puzzle);
		
		// Either standard Algorithm 0 update OR communication update.
		bool shouldCommunicate = false;
		if (numSubColonies > 1)
		{
			// Before iter 200: every 100 iterations. After: every 10 iterations.
			if (iter < 200)
				shouldCommunicate = (iter % 100 == 0);
			else
				shouldCommunicate = (iter % 10 == 0);
		}
		
		if (shouldCommunicate)
		{
			PerformBarrierSynchronization();
			colony->UpdatePheromoneWithCommunication();
			CompleteCommunicationPhase();
			
			if (useLocalStop && stopFlag.load())
				shouldStop = true;
			if (useLocalStop ? shouldStop : stopFlag.load())
				break;
		}
		else
		{
			colony->UpdatePheromone();
			colony->bestPher *= (1.0f - colony->bestEvap);
		}
		
		ReportProgress(colonyId, iter, puzzle);
		if (CheckSolutionFound(colony))
		{
			if (useLocalStop)
				shouldStop = true;
			break;
		}
	}
}

std::vector<int> ParallelSudokuAntSystem::GenerateMatchArray()
{
	// Generate a random permutation of colony IDs
	std::vector<int> matchArray(numSubColonies);
	std::iota(matchArray.begin(), matchArray.end(), 0); // Fill with 0, 1, 2, ..., n-1
	std::shuffle(matchArray.begin(), matchArray.end(), masterRandGen);
	return matchArray;
}

// ============================================================================
// COMMUNICATION TOPOLOGY 1: Ring Network
// ============================================================================
// Each colony i sends its iteration-best solution to colony (i+1) mod n
// This ensures every colony receives fresh information from a neighbor
// 
// Example with 4 colonies:  0 → 1 → 2 → 3 → 0 (ring)
// ============================================================================
void ParallelSudokuAntSystem::CommunicateRingTopology()
{
	std::vector<Board> iterationBests(numSubColonies);
	
	// --- STEP 1: Collect all iteration-best solutions ---
	for (int i = 0; i < numSubColonies; i++)
	{
		iterationBests[i].Copy(subColonies[i]->GetIterationBest());
	}
	
	// --- STEP 2: Send to next colony in ring ---
	for (int i = 0; i < numSubColonies; i++)
	{
		int nextId = (i + 1) % numSubColonies;  // Ring wraparound
		subColonies[nextId]->ReceiveIterationBest(iterationBests[i]);
	}
}

// ============================================================================
// COMMUNICATION TOPOLOGY 2: Random Network
// ============================================================================
// Colonies are randomly shuffled each communication cycle, then each colony
// receives the best-so-far solution from its predecessor in the shuffled order.
//
// This provides diversity - different pairings each time!
//
// Example: matchArray = [2, 0, 3, 1] means:
//   Colony 2 receives from Colony 1
//   Colony 0 receives from Colony 2  
//   Colony 3 receives from Colony 0
//   Colony 1 receives from Colony 3
// ============================================================================
void ParallelSudokuAntSystem::CommunicateRandomTopology(const std::vector<int>& matchArray)
{
	std::vector<Board> bestSols(numSubColonies);
	
	// --- STEP 1: Collect all best-so-far solutions ---
	for (int i = 0; i < numSubColonies; i++)
	{
		bestSols[i].Copy(subColonies[i]->GetBestSol());
	}
	
	// --- STEP 2: Send according to random matching ---
	for (int i = 0; i < numSubColonies; i++)
	{
		int colonyId = matchArray[i];
		int fromPos = (i + numSubColonies - 1) % numSubColonies;  // Previous in shuffled order
		int fromColonyId = matchArray[fromPos];
		
		subColonies[colonyId]->ReceiveBestSol(bestSols[fromColonyId]);
	}
}

// ============================================================================
// HELPER METHODS FOR MODULAR WORKER THREAD
// ============================================================================
// These methods break down the complex SubColonyWorker into manageable pieces,
// making the code easier to understand, test, and maintain.
// ============================================================================

// ----------------------------------------------------------------------------
// CheckTimeout: Check if global timeout has been exceeded
// Returns true if timeout occurred and signals all threads to stop
// ----------------------------------------------------------------------------
bool ParallelSudokuAntSystem::CheckTimeout()
{
	if (solutionTimer.Elapsed() >= maxTime)
	{
		stopFlag.store(true);
		if (numSubColonies > 1)
			commCV.notify_all();  // Wake up waiting threads (only needed for multiple threads)
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------------
// ReportProgress: Display progress information (Colony 0 only)
// Shows the global best across ALL colonies, not just Colony 0
// ----------------------------------------------------------------------------
void ParallelSudokuAntSystem::ReportProgress(int colonyId, int iteration, const Board& puzzle)
{
	if (colonyId == 0 && iteration % 50 == 0)
	{
		std::lock_guard<std::mutex> lock(commMutex);
		
		// Find global best across all colonies
		int globalBest = 0;
		for (int i = 0; i < numSubColonies; i++)
		{
			int score = subColonies[i]->GetBestSolScore();
			if (score > globalBest)
				globalBest = score;
		}
		
		std::cerr << "Progress: iteration " << iteration << " (Global best-so-far: " 
		          << globalBest << "/" << puzzle.CellCount() << ")" << std::endl;
	}
}

// ----------------------------------------------------------------------------
// CheckSolutionFound: Check if colony found a complete solution
// Returns true if solution found and signals all threads to stop
// ----------------------------------------------------------------------------
bool ParallelSudokuAntSystem::CheckSolutionFound(SubColony* colony)
{
	if (colony->GetBestSolScore() == colony->GetBestSol().CellCount())
	{
		stopFlag.store(true);
		if (numSubColonies > 1)
			commCV.notify_all();  // Notify all threads to stop (only needed for multiple threads)
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------------
// ExecuteMasterThreadTasks: Tasks performed by the last thread to arrive
// This thread becomes the "master" and performs communication
// ----------------------------------------------------------------------------
void ParallelSudokuAntSystem::ExecuteMasterThreadTasks()
{
	// Mark that communication occurred
	communicationOccurred = true;

	// Generate random matching for topology 2
	std::vector<int> matchArray = GenerateMatchArray();
	
	// --- COMMUNICATION TOPOLOGY 1: Ring (iteration-best) ---
	CommunicateRingTopology();
	
	// --- COMMUNICATION TOPOLOGY 2: Random (best-so-far) ---
	CommunicateRandomTopology(matchArray);
	
	// Check if any solution is complete
	for (int i = 0; i < numSubColonies; i++)
	{
		if (subColonies[i]->GetBestSolScore() == subColonies[i]->GetBestSol().CellCount())
		{
			stopFlag.store(true);
			break;
		}
	}
	
	// Reset barrier for next communication cycle
	barrier.store(0);
	
	// Release all waiting worker threads
	commCV.notify_all();
}

// ----------------------------------------------------------------------------
// ExecuteWorkerThreadWait: Worker threads wait for master to complete
// Uses timed wait to prevent deadlocks
// ----------------------------------------------------------------------------
void ParallelSudokuAntSystem::ExecuteWorkerThreadWait(std::unique_lock<std::mutex>& lock)
{
	auto predicate = [this]() { return barrier.load() == 0 || stopFlag.load(); };
	
	while (!predicate())
	{
		// Timed wait (prevents deadlock if something goes wrong)
		commCV.wait_for(lock, std::chrono::milliseconds(100), predicate);
		
		// Timeout check while waiting
		if (solutionTimer.Elapsed() >= maxTime && !stopFlag.load())
		{
			stopFlag.store(true);
			barrier.store(0);
			commCV.notify_all();
		}
	}
}

// ----------------------------------------------------------------------------
// CompleteCommunicationPhase: Record total comm interval duration
// ----------------------------------------------------------------------------
void ParallelSudokuAntSystem::CompleteCommunicationPhase()
{
	int finished = communicationPhaseDone.fetch_add(1) + 1;
	if (finished == numSubColonies)
	{
		auto commEnd = std::chrono::high_resolution_clock::now();
		std::lock_guard<std::mutex> lock(commMutex);
		if (communicationPhaseActive.load())
		{
			communicationTime += std::chrono::duration<float>(commEnd - communicationPhaseStart).count();
			communicationPhaseActive.store(false);
		}
		communicationPhaseDone.store(0);
	}
}

// ----------------------------------------------------------------------------
// PerformBarrierSynchronization: Coordinate all threads for communication
// Implements barrier pattern with master/worker roles
// ----------------------------------------------------------------------------
void ParallelSudokuAntSystem::PerformBarrierSynchronization()
{
	// Pre-check: Don't enter barrier if stop signal already set
	if (stopFlag.load())
		return;
	
	// === ENTER CRITICAL SECTION ===
	std::unique_lock<std::mutex> lock(commMutex);
	
	// Double-check stopFlag (race condition prevention)
	if (stopFlag.load())
	{
		barrier.store(0);
		commCV.notify_all();
		return;
	}
	
	// Increment barrier counter (atomic operation)
	int arrived = barrier.fetch_add(1) + 1;
	if (arrived == 1)
	{
		communicationPhaseActive.store(true);
		communicationPhaseStart = std::chrono::high_resolution_clock::now();
	}
	
	// === ROLE ASSIGNMENT ===
	if (arrived == numSubColonies)
	{
		// Last thread to arrive becomes MASTER
		ExecuteMasterThreadTasks();
	}
	else
	{
		// Other threads become WORKERS and wait
		ExecuteWorkerThreadWait(lock);
	}
	
	// === EXIT CRITICAL SECTION ===
	// Lock released automatically by unique_lock destructor
}

