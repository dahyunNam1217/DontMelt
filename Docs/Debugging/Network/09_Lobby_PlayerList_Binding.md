# 로비에 들어왔는데 플레이어 목록이 비어 있던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 로비 UI는 떴지만 인원 `0 / ?`, `로비 정보 동기화 중...`이 계속 표시됐다 |
| 원인 | GameState 탐색 재시도가 약 10초 뒤 포기한 다음, PlayerState가 도착해도 횟수가 초기화되지 않아 다시 찾지 않았다 |
| 해결 | PlayerState 수신 시 재시도 횟수를 초기화해, 주석에 적힌 의도대로 다시 연결을 시도하게 했다 |
| 관련 코드 | `ADMLobbyPlayerController::OnRep_PlayerState`, `ScheduleLobbyGameStateRetry`, `UDMLobbyWidget::TryBindGameState` |

## 현상

PIE와 패키지 모두에서, Server·Client 양쪽 로비 화면이 비어 있는 경우가 있었다. 화면이 떴다는 사실만으로 로비의 Gameplay Framework 연결이 정상이라고 볼 수는 없었다.

## 원인 추적

### 1차 조사: 범위 좁히기

처음에는 원인 후보가 너무 많았다. C++에만 범위를 두지 않고 에디터 설정까지 후보를 나눴다.

- 로비 맵 `World Settings`의 GameMode Override
- 해당 GameMode의 GameState·PlayerState·PlayerController 클래스 지정
- `PostLogin()`과 MatchRuleData 초기화
- PIE 자동 접속과 타이틀의 수동 Host/Join 혼용 여부
- 데이터는 왔는데 Row만 비어 있는지(`VerticalBox`의 Row Class 누락)

먼저 **데이터가 비었는지, UI만 비었는지**를 가르기 위해 서버와 클라이언트에서 `GameMode`, `GameState`, `LocalPlayerState`의 실제 클래스와 유효성을 로그로 남기도록 했다. 그리고 바인딩 성공·실패 로그(`Lobby GameState binding completed / timed out`)를 추가했다.

### 2차 조사: 재시도 로직의 재개 조건

로그를 보니 느린 접속에서 `Lobby GameState binding timed out`이 한 번 찍힌 뒤 다시는 시도하지 않았다. 컨트롤러는 GameState를 0.25초 간격으로 최대 40회(약 10초) 찾다가 포기한다.

```cpp
void ADMLobbyPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (IsValid(LobbyWidget.Get()))
    {
        LobbyWidget->RefreshLobby();

        // PlayerState 수신 시 GameState 연결도 다시 검사한다.
        ScheduleLobbyGameStateRetry();   // ← 내부에서 Attempts >= 40이면 바로 포기
    }
}
```

주석의 의도는 "PlayerState가 오면 다시 시도"였다. 하지만 재시도 함수가 누적 횟수를 먼저 검사하므로, 이미 시간을 다 쓴 경우에는 아무것도 하지 않았다. **주석과 실제 동작이 어긋난** 경우였다. Steam 접속이 느리거나 맵 로딩이 오래 걸린 클라이언트일수록 이 조건에 걸렸다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 재시도 횟수 상한 증가 | 40 → 120 | 시간만 늘릴 뿐 같은 구조가 남는다 |
| B. GameState BeginPlay에서 위젯에 직접 통보 | GameState가 도착을 알림 | 정확하지만 GameState가 UI를 알아야 해 의존 방향이 나빠진다 |
| **C. 새 이벤트(PlayerState 도착) 시 횟수 초기화** | 주석 의도대로 동작하게 함 | **채택.** 한 줄로 의도를 복구한다 |

```cpp
LobbyWidget->RefreshLobby();

// 접속이 느려 초기 재시도(약 10초)를 모두 쓴 뒤에도 다시 연결을 시도할 수 있게 횟수를 초기화한다.
// 이미 재시도 타이머가 돌고 있으면 ScheduleLobbyGameStateRetry가 중복 등록하지 않는다.
LobbyGameStateRetryAttempts = 0;

// PlayerState 수신 시 GameState 연결도 다시 검사한다.
ScheduleLobbyGameStateRetry();
```

재시도 함수에 `IsTimerActive` 검사가 이미 있어, 타이머가 돌고 있는 중이라면 중복 등록 없이 남은 횟수만 40회로 늘어난다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 일반 로비 진입 | `Lobby GameState binding completed` |
| 네트워크 지연 에뮬레이션 중 진입 | 타임아웃 로그 뒤에도 PlayerState 도착 시 목록이 채워짐 |

## 정리

- 원인 후보가 많을 때는 **데이터 계층과 표시 계층 중 어디가 비었는지**부터 가른다.
- 주석은 의도, 코드는 동작이다. 검수할 때 둘이 같은지 대조한다.
- 재시도 로직에는 "언제 다시 처음부터 시도할 수 있는가"라는 **재개 조건**이 있어야 한다.

---

[Debugging 목차](../README.md)
