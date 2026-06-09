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
  static constexpr int16_t BOARD_ORIGIN_X = -1;
  static constexpr int16_t BOARD_ORIGIN_Y = -1;
  static constexpr uint8_t BOARD_WIDTH = 10;
  static constexpr uint8_t BOARD_HEIGHT = 10;
  static constexpr uint8_t CELL_COUNT = BOARD_WIDTH * BOARD_HEIGHT;
  static constexpr uint8_t MAX_FORCES = 6;
  static constexpr uint8_t NO_FORCE = 255;
  static constexpr uint8_t MAX_WEIGHT = 4;
  static constexpr float MIN_HUE_DISTANCE = 45.0f / 360.0f;
  static constexpr float MIN_SATURATION = 0.42f;
  static constexpr float MAX_SATURATION = 0.56f;
  static constexpr uint16_t OPERATION_INTERVAL_MS = 120;
  static constexpr uint16_t OPERATION_FADE_MS = OPERATION_INTERVAL_MS;

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
    float hue = 0.0f;
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
  bool renderPending = false;
  bool fadePending = false;
  Timer operationTimer;

  void ResetSimulation();
  void StepSimulation();
  void ProcessInputEvents();
  void BoostCell(uint8_t cellIndex);
  bool BeginNextTurn();
  void TrySpawnAtRoundStart();

  bool SpawnForce(uint8_t cellIndex, bool addToFront);
  bool SplitForce(uint8_t sourceForceId);
  bool SelectSplitCells(uint8_t sourceForceId, uint8_t targetCount, array<uint8_t, CELL_COUNT>* selectedCells,
                        uint8_t* selectedCount) const;
  bool IsCellSelected(uint8_t cellIndex, const array<uint8_t, CELL_COUNT>& selectedCells, uint8_t selectedCount) const;
  int16_t FindFreeForceSlot() const;
  void RemoveForce(uint8_t forceId);
  void RemoveForceIfEmpty(uint8_t forceId);

  uint8_t CountOwnedCells(uint8_t forceId) const;
  uint16_t CountOperations(uint8_t forceId) const;
  bool GetOperation(uint8_t forceId, uint16_t operationIndex, Operation* operation) const;
  void ExecuteOperation(uint8_t forceId, const Operation& operation);

  void Render();
  Color GetCellColor(const Cell& cell) const;
  Color RandomForceColor(float hue) const;
  float RandomForceHue() const;
  bool IsHueAvailable(float hue) const;
  float HueDistance(float a, float b) const;

  uint8_t RandomSpawnOrigin() const;
  uint8_t RandomEmptySpawnOrigin() const;
  bool CanSpawnAt(uint8_t cellIndex, bool requireEmpty) const;
  float RandomUnit() const;
  bool IsValidCell(int16_t x, int16_t y) const;
  uint8_t CellIndex(int16_t x, int16_t y) const;
  Point CellPoint(uint8_t cellIndex) const;
};
