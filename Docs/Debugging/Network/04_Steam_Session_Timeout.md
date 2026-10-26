# Steam 응답이 오지 않으면 세션 기능 전체가 잠기는 문제

| 항목 | 내용 |
|---|---|
| 문제 | "다른 온라인 작업이 진행 중입니다"가 계속 뜨고 게임을 재시작해야 풀렸다 |
| 원인 | 작업 상태를 푸는 경로가 Steam 완료 콜백 하나뿐이었다 |
| 해결 | 세션 시스템 안에 작업별 30초 타임아웃을 두고, 늦게 온 콜백은 Delegate 해제로 무시했다 |
| 관련 코드 | `UDMOnlineSessionSubsystem::StartOperationTimeout`, `HandleOperationTimeout`, `CompleteOperation` |

## 현상

세션 시스템은 비동기 작업을 **한 번에 하나만** 진행한다(`CurrentOperation`). 생성·검색·참가·해제·갱신 중 하나라도 Steam 완료 콜백이 오지 않으면 `IsBusy()`가 계속 true로 남고, 이후 모든 세션 요청이 거절된다.

- Steam 클라이언트가 응답하지 않거나 네트워크가 잠시 끊긴 상태에서 요청을 보내면 발생했다.
- GameInstance의 방 퇴장 흐름에는 8초 타임아웃이 있었다. 하지만 그건 **화면 이동만** 풀어 줄 뿐, 세션 시스템의 작업 상태는 그대로 남았다.

## 원인 추적

```text
CreateLobbySession
  ├ CurrentOperation = Create
  ├ AddOnCreateSessionCompleteDelegate
  └ Sessions->CreateSession(...)   → true (요청 시작)
        … 콜백이 끝내 오지 않음
  CurrentOperation이 None으로 돌아갈 경로 없음 → IsBusy() 계속 true
```

작업 상태를 되돌리는 곳은 `CompleteOperation` 하나뿐이고, 그 호출은 Steam 콜백에만 의존했다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 호출하는 쪽(UI)마다 타임아웃 | Frontend, Lobby, GameInstance 각자 | 중복이 많고, 세션 시스템의 상태 자체는 풀리지 않는다 |
| **B. 세션 시스템 안에 공통 타임아웃** | 작업 시작 시 타이머, 완료 시 해제 | **채택.** 상태를 소유한 곳에서 상태를 푼다 |

맵 이동 중에도 작업이 이어질 수 있어서 World 타이머 대신 **GameInstance 타이머**(`UGameInstance::GetTimerManager()`)를 썼다.

```cpp
// 세션 작업 하나를 기다리는 최대 시간(초)이다.
constexpr float DMSessionOperationTimeoutSeconds = 30.0f;

// 다섯 가지 작업 모두 CurrentOperation 설정 직후, 요청 호출 전에 시작한다.
CurrentOperation = EOperation::Create;
StartOperationTimeout();

void UDMOnlineSessionSubsystem::CompleteOperation(const bool bWasSuccessful, const FString& Detail)
{
    const EOperation FinishedOperation = CurrentOperation;

    // 이벤트 호출 전에 현재 작업과 Delegate, 응답 대기 타이머를 정리한다.
    ClearOperationTimeout();
    ClearActiveDelegate();
    CurrentOperation = EOperation::None;
    // ...
}

void UDMOnlineSessionSubsystem::HandleOperationTimeout()
{
    if (!IsBusy())
    {
        return;
    }

    if (CurrentOperation == EOperation::Join)
    {
        PendingJoinSourceIndex = INDEX_NONE;
    }

    // Delegate를 먼저 해제하므로 늦게 도착한 Steam 응답은 무시된다.
    CompleteOperation(false, TEXT("Steam 응답 시간이 초과되었습니다. 잠시 후 다시 시도하세요."));
}
```

**설계에서 고려한 점**

| 고려 사항 | 처리 |
|---|---|
| Steam이 요청 함수 안에서 콜백을 바로 호출하는 경우 | 타이머를 요청 **전**에 시작하므로 `CompleteOperation`이 정상적으로 해제한다 |
| 타임아웃 뒤 늦게 도착한 응답 | Delegate를 먼저 제거해 다음 작업의 결과로 잘못 처리되지 않는다 |
| 타임아웃 뒤 Steam에서 세션이 실제로 생긴 경우 | [잔여 세션 정리](02_Steam_Stale_Session.md)가 다음 Host/Join 때 처리한다. 두 수정이 서로를 보완한다 |
| 로비 상태 갱신(Update) 타임아웃 | 로비의 기존 재시도 로직이 실패 결과를 받아 처리한다 |

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 정상 사용 | 로그에 `Session operation timed out`이 나오지 않음 |
| Steam 오프라인 상태에서 검색 | 30초 후 실패 메시지, 이후 재시도 가능 |

## 정리

- 외부 콜백에만 의존하는 상태는 **콜백이 오지 않는 경우**를 기준으로 설계해야 한다.
- 타임아웃은 상태를 **소유한 객체 안**에 둬야 그 상태가 실제로 풀린다.

---

[Debugging 목차](../README.md)
