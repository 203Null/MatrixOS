#pragma once

#include "MatrixOS.h"
#include "Application.h"

class Influence : public Application {
public:
  inline static Application_Info info = {
      .name = "Influence",
      .author = "203 Systems",
      .color = Color(0xFF0000),
      .version = 1,
      .visibility = true,
  };

  void Setup(const vector<string>& args) override;
  void Loop() override;
  void End() override;

private:
  static constexpr const char* TAG = "Influence";
  static constexpr uint8_t BOARD_WIDTH = 8;
  static constexpr uint8_t BOARD_HEIGHT = 8;
  static constexpr uint8_t CELL_COUNT = BOARD_WIDTH * BOARD_HEIGHT;
  static constexpr uint8_t MAX_FORCES = CELL_COUNT;
  static constexpr uint8_t NO_FORCE = 255;
  static constexpr uint8_t MAX_WEIGHT = 4;
  static constexpr uint16_t OPERATION_INTERVAL_MS = 120;
  static constexpr uint16_t FADE_DURATION_MS = 90;

  enum class OperationType : uint8_t {
    Attack,
    Capture,
    Reinforce,
  };

  struct Cell {
    uint8_t owner = NO_FORCE;
    uint8_t weight = 0;
  };

  struct Force {
    bool active = false;
    Color color = Color::Black;
  };

  struct Operation {
    OperationType type = OperationType::Reinforce;
    uint8_t source = 0;
    uint8_t target = 0;
  };

  array<Cell, CELL_COUNT> board;
  array<Force, MAX_FORCES> forces;
  array<uint8_t, MAX_FORCES> forceQueue;
  uint8_t forceCount = 0;
  uint8_t currentForceIndex = 0;
  uint8_t currentActionsRemaining = 0;
  bool turnActive = false;
  Timer operationTimer;

  void ResetSimulation();
  void StepSimulation();
  void ProcessInputEvents();
  void BoostCell(uint8_t cellIndex);
  bool BeginNextTurn();
  void TrySpawnAtRoundStart();

  bool SpawnForce(uint8_t cellIndex, bool addToFront);
  int16_t FindFreeForceSlot() const;
  void RemoveForce(uint8_t forceId);
  void RemoveForceIfEmpty(uint8_t forceId);

  uint8_t CountOwnedCells(uint8_t forceId) const;
  uint16_t CountOperations(uint8_t forceId) const;
  bool GetOperation(uint8_t forceId, uint16_t operationIndex, Operation* operation) const;
  void ExecuteOperation(uint8_t forceId, const Operation& operation);

  void Render();
  void Render(bool fade);
  Color GetCellColor(const Cell& cell) const;
  Color GetDominantForceColor() const;
  Color RandomForceColor() const;

  uint8_t RandomSpawnOrigin() const;
  uint8_t RandomEmptySpawnOrigin() const;
  bool CanSpawnAt(uint8_t cellIndex, bool requireEmpty) const;
  float RandomUnit() const;
  bool IsValidCell(int16_t x, int16_t y) const;
  uint8_t CellIndex(int16_t x, int16_t y) const;
  Point CellPoint(uint8_t cellIndex) const;
};
