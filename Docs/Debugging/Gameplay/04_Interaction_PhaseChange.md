# 연장전 전환 때 등록 중이던 유물이 떨어지는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 정규 시간 종료 직전에 유물을 등록하고 있으면, `OvertimeIntro` 진입 직후 등록 실패 페널티로 유물이 떨어졌다 |
| 원인 | 상호작용 허용 Phase와 실패 처리 예외 Phase를 서로 다른 조건식으로 판단했다 |
| 해결 | 허용 Phase 밖에서 끝난 상호작용은 플레이어의 실패가 아니라 시스템 취소로 처리했다 |
| 관련 코드 | `UDMInteractionComponent::ServerValidateActiveInteraction`, `EndServerInteractionAsFailure` |

## 원인 추적

진행 중인 상호작용은 0.1초마다 재검증한다. 등록은 `Playing`과 `Overtime`에서만 허용하는데, 실패 처리에서 페널티를 건너뛰는 예외는 `Finished`와 `Aborted`뿐이었다.

```text
OvertimeIntro 진입
  → 재검증 실패 (허용 Phase가 아님)
  → EndServerInteractionAsFailure()
  → Finished / Aborted가 아니므로 DepositFailure Drop
```

같은 Phase 집합을 두 곳에서 서로 다르게 적어 둔 것이 원인이었다. 플레이어는 거리나 시야 조건을 어기지 않았는데 페널티를 받았다.

## 해결

예외 목록에 `OvertimeIntro`를 하나 더 넣는 대신, **허용 Phase 자체**를 조건으로 사용했다. 앞으로 Phase가 추가돼도 두 조건이 다시 어긋나지 않는다.

```cpp
// DMInteractionComponent.cpp - EndServerInteractionAsFailure()
if (Phase != EDMMatchPhase::Playing && Phase != EDMMatchPhase::Overtime)
{
    EndServerInteraction(false, TEXT("Interaction cancelled by MatchPhase change."));
    return;
}
```

Playing·Overtime 중의 거리 이탈이나 시야 차단 같은 실제 실패, 사망·접속 종료의 별도 Drop 경로는 그대로 둔다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 정규 시간 종료 2초 전에 등록 시작 | 연장전 소개 진입 후 유물을 계속 들고 있음, 진행 UI 닫힘 |
| Playing 중 등록하다 거리 이탈 | 기존처럼 실패 Drop |

## 정리

- 플레이어의 **실패**와 시스템에 의한 **취소**는 결과가 달라야 한다.
- 같은 규칙을 두 곳에 적으면 언젠가 어긋난다. 예외 목록 대신 원래 규칙을 조건으로 쓴다.

---

[Debugging 목차](../README.md)
