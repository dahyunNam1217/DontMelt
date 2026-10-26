# ResultId 검사를 추가한 뒤 정상 종료가 거부된 회귀

| 항목 | 내용 |
|---|---|
| 문제 | 유물·전멸 승리가 확정되지 않고, 연장전 종료는 `InternalError` 중단으로 빠질 수 있었다 |
| 원인 | `SetMatchResult()`가 유효하지 않은 ID를 거부하도록 바뀌었는데, 정상 종료 경로(`TryFinishMatch`)는 ID를 발급하지 않았다 |
| 해결 | 정상 종료 결과를 만들 때도 `FGuid::NewGuid()`로 ID를 발급했다 |
| 관련 코드 | `ADMMatchGameMode::TryFinishMatch`, `ADMMatchGameState::SetMatchResult` |

## 현상

[결과 ACK 설계](07_MatchResult_Travel_Race.md)를 위해 결과마다 `ResultId`를 두고, GameState가 ID 없는 결과를 거부하게 했다. 실행 전 검수에서 이 검사가 정상 종료 경로를 막는다는 것을 발견했다.

## 원인 추적

```cpp
// ADMMatchGameState::SetMatchResult
if (!NewMatchResult.ResultId.IsValid())
{
    UE_LOG(LogDMMatchGameState, Warning, TEXT("SetMatchResult rejected: ResultId is invalid."));
    return;
}
```

결과를 만드는 경로를 모두 역추적했다.

| 경로 | ID 발급 | 결과 |
|---|---|---|
| `TryAbortMatch()` | `FGuid::NewGuid()` 있음 | 정상 적용 |
| `TryFinishMatch()` | **없음** | 거부됨 |
| 연장전 종료 | `TryFinishMatch()` 실패를 내부 오류로 해석 | `InternalError` 중단 |

검사 하나를 추가하면서, 그 값을 만드는 모든 생성 경로를 확인하지 않은 것이 원인이었다.

## 해결

```cpp
// ADMMatchGameMode::TryFinishMatch
FinalResult.EndReason = EndReason;
FinalResult.WinningBaseSlot = WinningBaseSlot;
FinalResult.ResultServerTime = MatchGameState->GetServerWorldTimeSeconds();
FinalResult.ResultId = FGuid::NewGuid();
```

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| `DMTestFinishMatch 0` | 결과 화면 진입, 경고 로그 없음 |
| 실제 유물 승리 | 같음 |
| 연장전 종료 | `InternalError`가 아닌 정상 종료 사유 |

## 정리

- 필수 필드 검사를 추가할 때는 Setter만 보지 말고 **그 값을 만드는 모든 경로**를 검색한다.
- 실패를 다른 실패로 다시 해석하는 코드(연장전 → 내부 오류)는 원인을 가린다. 회귀를 찾을 때 이런 연쇄부터 끊어서 본다.

---

[Debugging 목차](../README.md)
