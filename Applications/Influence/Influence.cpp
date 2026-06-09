#include "Influence.h"
#include <cstdlib>

void Influence::Setup(const vector<string>& args) {
  (void)args;
  MLOGI(TAG, "Influence started");
  srand((unsigned int)MatrixOS::SYS::Micros());
  ResetSimulation();
  Render(false);
}

void Influence::Loop() {
  ProcessInputEvents();

  if (!operationTimer.Tick(OPERATION_INTERVAL_MS))
  {
    return;
  }

  StepSimulation();
}

void Influence::End() {
  MLOGI(TAG, "Influence exited");
}

void Influence::ResetSimulation() {
  for (Cell& cell : board)
  {
    cell = Cell();
  }

  for (Force& force : forces)
  {
    force = Force();
  }

  forceCount = 0;
  currentForceIndex = 0;
  currentActionsRemaining = 0;
  turnActive = false;

  for (uint8_t i = 0; i < 4; i++)
  {
    SpawnForce(RandomEmptySpawnOrigin(), false);
  }
}

void Influence::StepSimulation() {
  if (!turnActive && !BeginNextTurn())
  {
    return;
  }

  uint8_t forceId = forceQueue[currentForceIndex];
  uint16_t operationCount = CountOperations(forceId);
  if (operationCount > 0)
  {
    Operation operation;
    if (GetOperation(forceId, rand() % operationCount, &operation))
    {
      ExecuteOperation(forceId, operation);
    }
  }

  Render(true);

  if (currentActionsRemaining > 0)
  {
    currentActionsRemaining--;
  }

  if (currentActionsRemaining == 0)
  {
    turnActive = false;
    currentForceIndex++;
  }
}

bool Influence::BeginNextTurn() {
  while (forceCount > 0)
  {
    if (currentForceIndex >= forceCount)
    {
      currentForceIndex = 0;
      TrySpawnAtRoundStart();
    }

    if (forceCount == 0)
    {
      return false;
    }

    uint8_t forceId = forceQueue[currentForceIndex];
    uint8_t ownedCells = CountOwnedCells(forceId);
    if (ownedCells == 0)
    {
      RemoveForce(forceId);
      continue;
    }

    currentActionsRemaining = ownedCells;
    turnActive = true;
    return true;
  }

  SpawnForce(RandomSpawnOrigin(), true);
  return BeginNextTurn();
}

void Influence::TrySpawnAtRoundStart() {
  if (forceCount < 4 && (rand() % 2) == 0)
  {
    SpawnForce(RandomSpawnOrigin(), true);
  }
}

void Influence::ProcessInputEvents() {
  InputEvent inputEvent;
  while (MatrixOS::Input::Get(&inputEvent))
  {
    if (inputEvent.inputClass != InputClass::Keypad || inputEvent.keypad.state != KeypadState::Pressed)
    {
      continue;
    }

    Point point;
    if (!MatrixOS::Input::GetPosition(inputEvent.id, &point) || !IsValidCell(point.x, point.y))
    {
      continue;
    }

    BoostCell(CellIndex(point.x, point.y));
  }
}

void Influence::BoostCell(uint8_t cellIndex) {
  if (cellIndex >= CELL_COUNT)
  {
    return;
  }

  Cell& cell = board[cellIndex];
  if (cell.owner == NO_FORCE || cell.weight == 0 || cell.owner >= MAX_FORCES || !forces[cell.owner].active || cell.weight >= MAX_WEIGHT)
  {
    return;
  }

  cell.weight++;
  Render(true);
}

bool Influence::SpawnForce(uint8_t cellIndex, bool addToFront) {
  if (forceCount >= MAX_FORCES || cellIndex >= CELL_COUNT || !CanSpawnAt(cellIndex, false))
  {
    return false;
  }

  int16_t forceSlot = FindFreeForceSlot();
  if (forceSlot < 0)
  {
    return false;
  }

  uint8_t forceId = (uint8_t)forceSlot;
  uint8_t previousOwners[4] = {NO_FORCE, NO_FORCE, NO_FORCE, NO_FORCE};
  Point origin = CellPoint(cellIndex);

  forces[forceId].active = true;
  forces[forceId].color = RandomForceColor();

  uint8_t ownerCount = 0;
  for (uint8_t y = 0; y < 2; y++)
  {
    for (uint8_t x = 0; x < 2; x++)
    {
      uint8_t targetIndex = CellIndex(origin.x + x, origin.y + y);
      uint8_t previousOwner = board[targetIndex].owner;
      bool ownerRecorded = false;

      for (uint8_t i = 0; i < ownerCount; i++)
      {
        if (previousOwners[i] == previousOwner)
        {
          ownerRecorded = true;
          break;
        }
      }

      if (!ownerRecorded && previousOwner != NO_FORCE)
      {
        previousOwners[ownerCount] = previousOwner;
        ownerCount++;
      }

      board[targetIndex].owner = forceId;
      board[targetIndex].weight = MAX_WEIGHT;
    }
  }

  if (addToFront)
  {
    for (int16_t i = forceCount; i > 0; i--)
    {
      forceQueue[i] = forceQueue[i - 1];
    }
    forceQueue[0] = forceId;
    currentForceIndex = 0;
  }
  else
  {
    forceQueue[forceCount] = forceId;
  }
  forceCount++;

  for (uint8_t i = 0; i < ownerCount; i++)
  {
    RemoveForceIfEmpty(previousOwners[i]);
  }

  return true;
}

int16_t Influence::FindFreeForceSlot() const {
  for (uint8_t i = 0; i < MAX_FORCES; i++)
  {
    if (!forces[i].active)
    {
      return i;
    }
  }
  return -1;
}

void Influence::RemoveForce(uint8_t forceId) {
  if (forceId >= MAX_FORCES)
  {
    return;
  }

  forces[forceId].active = false;

  for (Cell& cell : board)
  {
    if (cell.owner == forceId)
    {
      cell.owner = NO_FORCE;
      cell.weight = 0;
    }
  }

  for (uint8_t i = 0; i < forceCount; i++)
  {
    if (forceQueue[i] != forceId)
    {
      continue;
    }

    for (uint8_t j = i; j + 1 < forceCount; j++)
    {
      forceQueue[j] = forceQueue[j + 1];
    }

    forceCount--;
    if (i < currentForceIndex && currentForceIndex > 0)
    {
      currentForceIndex--;
    }
    return;
  }
}

void Influence::RemoveForceIfEmpty(uint8_t forceId) {
  if (forceId != NO_FORCE && CountOwnedCells(forceId) == 0)
  {
    RemoveForce(forceId);
  }
}

uint8_t Influence::CountOwnedCells(uint8_t forceId) const {
  uint8_t count = 0;
  for (const Cell& cell : board)
  {
    if (cell.owner == forceId && cell.weight > 0)
    {
      count++;
    }
  }
  return count;
}

uint16_t Influence::CountOperations(uint8_t forceId) const {
  static constexpr int8_t neighborOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
  uint16_t count = 0;

  for (uint8_t cellIndex = 0; cellIndex < CELL_COUNT; cellIndex++)
  {
    const Cell& sourceCell = board[cellIndex];
    if (sourceCell.owner != forceId || sourceCell.weight == 0)
    {
      continue;
    }

    count++;

    Point sourcePoint = CellPoint(cellIndex);
    for (uint8_t i = 0; i < 4; i++)
    {
      int16_t x = sourcePoint.x + neighborOffsets[i][0];
      int16_t y = sourcePoint.y + neighborOffsets[i][1];
      if (!IsValidCell(x, y))
      {
        continue;
      }

      const Cell& targetCell = board[CellIndex(x, y)];
      if (targetCell.owner != forceId)
      {
        count++;
      }
    }
  }

  return count;
}

bool Influence::GetOperation(uint8_t forceId, uint16_t operationIndex, Operation* operation) const {
  static constexpr int8_t neighborOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
  uint16_t currentIndex = 0;

  for (uint8_t cellIndex = 0; cellIndex < CELL_COUNT; cellIndex++)
  {
    const Cell& sourceCell = board[cellIndex];
    if (sourceCell.owner != forceId || sourceCell.weight == 0)
    {
      continue;
    }

    if (currentIndex == operationIndex)
    {
      operation->type = OperationType::Reinforce;
      operation->source = cellIndex;
      operation->target = cellIndex;
      return true;
    }
    currentIndex++;

    Point sourcePoint = CellPoint(cellIndex);
    for (uint8_t i = 0; i < 4; i++)
    {
      int16_t x = sourcePoint.x + neighborOffsets[i][0];
      int16_t y = sourcePoint.y + neighborOffsets[i][1];
      if (!IsValidCell(x, y))
      {
        continue;
      }

      uint8_t targetIndex = CellIndex(x, y);
      const Cell& targetCell = board[targetIndex];
      if (targetCell.owner == forceId)
      {
        continue;
      }

      if (targetCell.owner == NO_FORCE || targetCell.weight == 0)
      {
        if (currentIndex == operationIndex)
        {
          operation->type = OperationType::Capture;
          operation->source = cellIndex;
          operation->target = targetIndex;
          return true;
        }
        currentIndex++;
      }
      else
      {
        if (currentIndex == operationIndex)
        {
          operation->type = OperationType::Attack;
          operation->source = cellIndex;
          operation->target = targetIndex;
          return true;
        }
        currentIndex++;
      }
    }
  }

  return false;
}

void Influence::ExecuteOperation(uint8_t forceId, const Operation& operation) {
  Cell& targetCell = board[operation.target];

  switch (operation.type)
  {
  case OperationType::Attack:
  {
    uint8_t previousOwner = targetCell.owner;
    if (previousOwner != NO_FORCE && previousOwner != forceId && targetCell.weight > 0)
    {
      targetCell.weight--;
      if (targetCell.weight == 0)
      {
        targetCell.owner = NO_FORCE;
        RemoveForceIfEmpty(previousOwner);
      }
    }
    break;
  }
  case OperationType::Capture:
    if (targetCell.owner == NO_FORCE || targetCell.weight == 0)
    {
      targetCell.owner = forceId;
      targetCell.weight = 1;
    }
    break;
  case OperationType::Reinforce:
    if (targetCell.owner == forceId && targetCell.weight < MAX_WEIGHT)
    {
      targetCell.weight++;
    }
    break;
  }
}

void Influence::Render() {
  Render(true);
}

void Influence::Render(bool fade) {
  if (fade)
  {
    MatrixOS::LED::Fade(FADE_DURATION_MS);
  }

  for (uint8_t cellIndex = 0; cellIndex < CELL_COUNT; cellIndex++)
  {
    MatrixOS::LED::SetColor(CellPoint(cellIndex), GetCellColor(board[cellIndex]), 0);
  }

  MatrixOS::LED::FillPartition("Underglow", GetDominantForceColor(), 0);
  MatrixOS::LED::Update(0);
}

Color Influence::GetCellColor(const Cell& cell) const {
  if (cell.owner == NO_FORCE || cell.weight == 0 || cell.owner >= MAX_FORCES || !forces[cell.owner].active)
  {
    return Color::Black;
  }

  uint8_t brightness = cell.weight * 255 / MAX_WEIGHT;
  return forces[cell.owner].color.Scale(brightness);
}

Color Influence::GetDominantForceColor() const {
  uint16_t bestWeight = 0;
  uint8_t bestForce = NO_FORCE;

  for (uint8_t queueIndex = 0; queueIndex < forceCount; queueIndex++)
  {
    uint8_t forceId = forceQueue[queueIndex];
    uint16_t forceWeight = 0;

    for (const Cell& cell : board)
    {
      if (cell.owner == forceId)
      {
        forceWeight += cell.weight;
      }
    }

    if (forceWeight > bestWeight)
    {
      bestWeight = forceWeight;
      bestForce = forceId;
    }
  }

  if (bestForce == NO_FORCE)
  {
    return Color::Black;
  }

  return forces[bestForce].color;
}

Color Influence::RandomForceColor() const {
  float hue = RandomUnit();
  float saturation = 0.55f + RandomUnit() * 0.20f;
  return Color::HsvToRgb(hue, saturation, 1.0f);
}

uint8_t Influence::RandomSpawnOrigin() const {
  return CellIndex(rand() % (BOARD_WIDTH - 1), rand() % (BOARD_HEIGHT - 1));
}

uint8_t Influence::RandomEmptySpawnOrigin() const {
  uint8_t emptyCount = 0;
  for (uint8_t y = 0; y < BOARD_HEIGHT - 1; y++)
  {
    for (uint8_t x = 0; x < BOARD_WIDTH - 1; x++)
    {
      if (CanSpawnAt(CellIndex(x, y), true))
      {
        emptyCount++;
      }
    }
  }

  if (emptyCount == 0)
  {
    return RandomSpawnOrigin();
  }

  uint8_t selectedEmpty = rand() % emptyCount;
  for (uint8_t y = 0; y < BOARD_HEIGHT - 1; y++)
  {
    for (uint8_t x = 0; x < BOARD_WIDTH - 1; x++)
    {
      uint8_t cellIndex = CellIndex(x, y);
      if (!CanSpawnAt(cellIndex, true))
      {
        continue;
      }

      if (selectedEmpty == 0)
      {
        return cellIndex;
      }
      selectedEmpty--;
    }
  }

  return RandomSpawnOrigin();
}

bool Influence::CanSpawnAt(uint8_t cellIndex, bool requireEmpty) const {
  if (cellIndex >= CELL_COUNT)
  {
    return false;
  }

  Point origin = CellPoint(cellIndex);
  if (!IsValidCell(origin.x + 1, origin.y + 1))
  {
    return false;
  }

  if (!requireEmpty)
  {
    return true;
  }

  for (uint8_t y = 0; y < 2; y++)
  {
    for (uint8_t x = 0; x < 2; x++)
    {
      const Cell& cell = board[CellIndex(origin.x + x, origin.y + y)];
      if (cell.owner != NO_FORCE && cell.weight > 0)
      {
        return false;
      }
    }
  }

  return true;
}

float Influence::RandomUnit() const {
  return (rand() % 10000) / 10000.0f;
}

bool Influence::IsValidCell(int16_t x, int16_t y) const {
  return x >= 0 && y >= 0 && x < BOARD_WIDTH && y < BOARD_HEIGHT;
}

uint8_t Influence::CellIndex(int16_t x, int16_t y) const {
  return y * BOARD_WIDTH + x;
}

Point Influence::CellPoint(uint8_t cellIndex) const {
  return Point(cellIndex % BOARD_WIDTH, cellIndex / BOARD_WIDTH);
}
