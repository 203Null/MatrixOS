#include "Influence.h"
#include <cstdlib>

void Influence::Setup(const vector<string>& args) {
  (void)args;
  MLOGI(TAG, "Influence started");
  srand((unsigned int)MatrixOS::SYS::Micros());
  ResetSimulation();
  Render();
  renderPending = false;
}

void Influence::Loop() {
  ProcessInputEvents();

  if (!operationTimer.Tick(OPERATION_INTERVAL_MS))
  {
    if (renderPending)
    {
      Render();
      renderPending = false;
    }
    return;
  }

  StepSimulation();
  if (renderPending)
  {
    if (fadePending)
    {
      MatrixOS::LED::Fade(OPERATION_FADE_MS);
      fadePending = false;
    }
    Render();
    renderPending = false;
  }
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

  renderPending = true;
  fadePending = true;

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
  if (forceCount >= 4)
  {
    return;
  }

  uint8_t originalForceCount = forceCount;
  array<uint8_t, MAX_FORCES> originalQueue = forceQueue;
  for (uint8_t i = 0; i < originalForceCount && forceCount < MAX_FORCES; i++)
  {
    uint8_t forceId = originalQueue[i];
    if (forceId >= MAX_FORCES || !forces[forceId].active || CountOwnedCells(forceId) == 0)
    {
      continue;
    }

    if ((rand() % 2) == 0)
    {
      SplitForce(forceId);
    }
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
  renderPending = true;
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

  float hue = RandomForceHue();
  forces[forceId].active = true;
  forces[forceId].hue = hue;
  forces[forceId].color = RandomForceColor(hue);

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

bool Influence::SplitForce(uint8_t sourceForceId) {
  if (forceCount >= MAX_FORCES || sourceForceId >= MAX_FORCES || !forces[sourceForceId].active)
  {
    return false;
  }

  uint8_t targetCount = CountOwnedCells(sourceForceId) / 2;
  if (targetCount == 0)
  {
    return false;
  }

  int16_t forceSlot = FindFreeForceSlot();
  if (forceSlot < 0)
  {
    return false;
  }

  array<uint8_t, CELL_COUNT> selectedCells;
  uint8_t selectedCount = 0;
  if (!SelectSplitCells(sourceForceId, targetCount, &selectedCells, &selectedCount))
  {
    return false;
  }

  uint8_t newForceId = (uint8_t)forceSlot;
  float hue = RandomForceHue();
  forces[newForceId].active = true;
  forces[newForceId].hue = hue;
  forces[newForceId].color = RandomForceColor(hue);

  for (uint8_t i = 0; i < selectedCount; i++)
  {
    board[selectedCells[i]].owner = newForceId;
  }

  forceQueue[forceCount] = newForceId;
  forceCount++;

  RemoveForceIfEmpty(sourceForceId);
  return true;
}

bool Influence::SelectSplitCells(uint8_t sourceForceId, uint8_t targetCount, array<uint8_t, CELL_COUNT>* selectedCells,
                                 uint8_t* selectedCount) const {
  static constexpr int8_t neighborOffsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

  if (selectedCells == nullptr || selectedCount == nullptr || targetCount == 0)
  {
    return false;
  }

  for (uint8_t attempt = 0; attempt < 32; attempt++)
  {
    uint8_t ownedCount = 0;
    for (const Cell& cell : board)
    {
      if (cell.owner == sourceForceId && cell.weight > 0)
      {
        ownedCount++;
      }
    }

    if (ownedCount < targetCount)
    {
      return false;
    }

    uint8_t selectedOwned = rand() % ownedCount;
    uint8_t startCell = 0;
    for (uint8_t cellIndex = 0; cellIndex < CELL_COUNT; cellIndex++)
    {
      const Cell& cell = board[cellIndex];
      if (cell.owner != sourceForceId || cell.weight == 0)
      {
        continue;
      }

      if (selectedOwned == 0)
      {
        startCell = cellIndex;
        break;
      }
      selectedOwned--;
    }

    (*selectedCells)[0] = startCell;
    *selectedCount = 1;

    while (*selectedCount < targetCount)
    {
      array<uint8_t, CELL_COUNT> candidates;
      uint8_t candidateCount = 0;

      for (uint8_t selectedIndex = 0; selectedIndex < *selectedCount; selectedIndex++)
      {
        Point point = CellPoint((*selectedCells)[selectedIndex]);
        for (uint8_t offsetIndex = 0; offsetIndex < 4; offsetIndex++)
        {
          int16_t x = point.x + neighborOffsets[offsetIndex][0];
          int16_t y = point.y + neighborOffsets[offsetIndex][1];
          if (!IsValidCell(x, y))
          {
            continue;
          }

          uint8_t candidate = CellIndex(x, y);
          const Cell& cell = board[candidate];
          if (cell.owner != sourceForceId || cell.weight == 0 || IsCellSelected(candidate, *selectedCells, *selectedCount) ||
              IsCellSelected(candidate, candidates, candidateCount))
          {
            continue;
          }

          candidates[candidateCount] = candidate;
          candidateCount++;
        }
      }

      if (candidateCount == 0)
      {
        break;
      }

      (*selectedCells)[*selectedCount] = candidates[rand() % candidateCount];
      (*selectedCount)++;
    }

    if (*selectedCount == targetCount)
    {
      return true;
    }
  }

  *selectedCount = 0;
  return false;
}

bool Influence::IsCellSelected(uint8_t cellIndex, const array<uint8_t, CELL_COUNT>& selectedCells, uint8_t selectedCount) const {
  for (uint8_t i = 0; i < selectedCount; i++)
  {
    if (selectedCells[i] == cellIndex)
    {
      return true;
    }
  }

  return false;
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
  for (uint8_t cellIndex = 0; cellIndex < CELL_COUNT; cellIndex++)
  {
    MatrixOS::LED::SetColor(CellPoint(cellIndex), GetCellColor(board[cellIndex]), 0);
  }

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

Color Influence::RandomForceColor(float hue) const {
  float saturation = MIN_SATURATION + RandomUnit() * (MAX_SATURATION - MIN_SATURATION);
  return Color::HsvToRgb(hue, saturation, 1.0f);
}

float Influence::RandomForceHue() const {
  for (uint8_t i = 0; i < 64; i++)
  {
    float hue = RandomUnit();
    if (IsHueAvailable(hue))
    {
      return hue;
    }
  }

  float bestHue = 0.0f;
  float bestDistance = -1.0f;

  for (uint16_t step = 0; step < 360; step++)
  {
    float hue = step / 360.0f;
    float nearestDistance = 1.0f;

    for (const Force& force : forces)
    {
      if (!force.active)
      {
        continue;
      }

      float distance = HueDistance(hue, force.hue);
      if (distance < nearestDistance)
      {
        nearestDistance = distance;
      }
    }

    if (nearestDistance > bestDistance)
    {
      bestDistance = nearestDistance;
      bestHue = hue;
    }
  }

  return bestHue;
}

bool Influence::IsHueAvailable(float hue) const {
  for (const Force& force : forces)
  {
    if (force.active && HueDistance(hue, force.hue) < MIN_HUE_DISTANCE)
    {
      return false;
    }
  }

  return true;
}

float Influence::HueDistance(float a, float b) const {
  float distance = a > b ? a - b : b - a;
  return distance > 0.5f ? 1.0f - distance : distance;
}

uint8_t Influence::RandomSpawnOrigin() const {
  uint8_t x = rand() % (BOARD_WIDTH - 1);
  uint8_t y = rand() % (BOARD_HEIGHT - 1);
  return y * BOARD_WIDTH + x;
}

uint8_t Influence::RandomEmptySpawnOrigin() const {
  uint8_t emptyCount = 0;
  for (uint8_t y = 0; y < BOARD_HEIGHT - 1; y++)
  {
    for (uint8_t x = 0; x < BOARD_WIDTH - 1; x++)
    {
      uint8_t cellIndex = y * BOARD_WIDTH + x;
      if (CanSpawnAt(cellIndex, true))
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
      uint8_t cellIndex = y * BOARD_WIDTH + x;
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
  return x >= BOARD_ORIGIN_X && y >= BOARD_ORIGIN_Y &&
         x < BOARD_ORIGIN_X + BOARD_WIDTH && y < BOARD_ORIGIN_Y + BOARD_HEIGHT;
}

uint8_t Influence::CellIndex(int16_t x, int16_t y) const {
  return (y - BOARD_ORIGIN_Y) * BOARD_WIDTH + (x - BOARD_ORIGIN_X);
}

Point Influence::CellPoint(uint8_t cellIndex) const {
  return Point((cellIndex % BOARD_WIDTH) + BOARD_ORIGIN_X, (cellIndex / BOARD_WIDTH) + BOARD_ORIGIN_Y);
}
