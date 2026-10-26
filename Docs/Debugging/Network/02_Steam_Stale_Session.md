# 참가 실패 뒤 남은 Steam 세션 때문에 방 만들기가 계속 실패한 문제

| 항목 | 내용 |
|---|---|
| 문제 | 방 참가에 한 번 실패하면, 게임을 재시작할 때까지 방 만들기가 계속 실패했다 |
| 원인 | `JoinSession`은 성공하고 이후 접속만 실패한 경우, 로컬 `GameSession`이 지워지지 않고 남았다 |
| 해결 | "메인 메뉴에는 정상 세션이 있을 수 없다"는 불변 조건을 세우고, 메인 메뉴 진입·복구 완료·Host/Join 직전에 남은 세션을 정리했다 |
| 관련 코드 | `UDMOnlineSessionSubsystem::HasLocalLobbySession`, `ADMFrontendPlayerController::CleanupStaleLobbySession` |

## 현상

개발 내내 겪던 **"Steam 세션 생성이 안 되고 클라이언트가 로비에 못 들어간다"** 는 문제의 원인 중 하나였다. 재현 순서는 다음과 같다.

1. 방 목록에서 참가를 누른다. Steam `JoinSession`은 성공해 로컬 세션이 생긴다.
2. 이어지는 실제 접속(`ClientTravel`)이 실패한다. NetDriver 설정, 호스트 종료 등 원인은 다양하다.
3. "서버에 연결할 수 없습니다" 안내 후 메인 메뉴로 돌아온다.
4. **방 만들기**를 누르면 "기존 세션을 먼저 종료해야 합니다"가 뜨고, 몇 번을 눌러도 같다.

플레이어 입장에서는 "방 생성이 안 된다"로 보이지만 원인은 그 앞의 참가 실패에 있었다. 증상이 나는 기능(방 만들기)만 보고 있으면 찾기 어려운 문제였다.

## 원인 추적

방 만들기 실패 메시지에서 출발해 세션이 만들어진 시점까지 거슬러 올라갔다.

```text
JoinSteamLobby → JoinSession 성공 (로컬 GameSession 생성)
  └ HandleSteamJoinCompleted → ClientTravel(steam.7656...)
       └ 접속 실패 → GEngine::OnNetworkFailure (World 없음 = PendingNetGame 단계)
            └ UDMGameInstance::HandlePendingConnectionFailure
                 └ StartLocalFailureRecovery → 5초 안내
                      └ 이미 메인 메뉴 → FinishLocalFailureRecovery
                            ✗ DestroyLobbySession 호출 없음
이후 HostLobby → CreateLobbySession
  └ GetNamedSession(GameSession) != nullptr → "기존 세션을 먼저 종료해야 합니다"
       └ 실패 메시지만 표시, 정리 없음 → 재시작 전까지 반복
```

- **참가 쪽은 우연히 회복**했다. 다시 참가하면 같은 이유로 실패하지만, 그때 Frontend의 참가 실패 정리 루틴이 세션을 지웠다.
- **방 만들기 쪽에는 정리 경로가 없었다.**
- 부작용도 있었다. 실패한 클라이언트가 **호스트의 Steam 로비 멤버로 남아 슬롯을 차지**했다.

같은 상태는 두 경로에서 더 생겼다. 호스트가 방을 만든 뒤 로비 맵 이동(`OpenLevel`)에 실패한 경우, 방 퇴장 중 세션 해제가 8초 타임아웃에 걸린 경우다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. 실패 복구 경로마다 Destroy 추가 | `FinishLocalFailureRecovery` 등에 해제 호출 | 경로가 세 개 이상이라 하나를 빠뜨리기 쉽다 |
| B. Create가 기존 세션을 지우고 재시도 | 세션 시스템 안에서 처리 | Destroy 완료를 기다린 뒤 Create하는 비동기 2단계를 묶어야 해 상태 머신이 복잡해진다 |
| **C. 메인 메뉴의 세션은 잔여물로 보고 정리** | 메인 메뉴 진입·복구 완료·Host/Join 직전에 검사 | **채택.** 불변 조건 하나로 모든 실패 경로를 덮는다 |

C의 근거는 세션 수명이다. 호스트는 세션을 만든 직후 로비로 이동하고, 클라이언트는 참가 직후 접속한다. 따라서 **진행 중인 요청이 없는 메인 메뉴에 세션이 있다면 그것은 항상 이전 실패의 잔여물**이다.

```cpp
// UDMOnlineSessionSubsystem
bool UDMOnlineSessionSubsystem::HasLocalLobbySession() const
{
    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    // Steam 인터페이스가 없으면 남은 세션도 없다고 본다.
    return Sessions.IsValid() && Sessions->GetNamedSession(DMSessionName) != nullptr;
}
```

```cpp
// ADMFrontendPlayerController
bool ADMFrontendPlayerController::CleanupStaleLobbySession()
{
    UWorld* World = GetWorld();

    // 메인 메뉴(Standalone)에는 유효한 방이 없으므로 남은 GameSession은 이전 실패의 잔여물이다.
    if (!IsLocalController() || !IsValid(World) || World->GetNetMode() != NM_Standalone)
    {
        return false;
    }

    // 방금 시작한 생성·검색·참가·정리 요청의 세션은 지우지 않는다.
    if (bHostSessionRequestPending || SteamJoinOperation != ESteamJoinOperation::None)
    {
        return false;
    }

    // ... Session 획득, 세션이 없으면 return false

    // 다른 작업(예: 퇴장 중인 Destroy)이 끝나지 않았으면 이번에는 기다린다.
    if (Session->IsBusy())
    {
        return true;
    }

    // 세션 해제가 끝나면 검색 결과 캐시도 비워지므로 화면의 방 목록도 함께 비운다.
    MainMenuWidget->SetSteamSessionOptions(TArray<FString>());

    Session->DestroyLobbySession();
    return true;
}
```

**호출 위치**

| 위치 | 이유 |
|---|---|
| `BeginPlay` | 메인 메뉴 진입 시점 |
| `RestoreMainMenuAfterNotice` | 접속 실패 안내가 끝난 직후 |
| `HostLobby`, `JoinSteamLobby` | 요청 직전. 세션이 남아 있으면 정리를 요청하고 "이전 세션을 정리하는 중입니다. 잠시 후 다시 시도하세요"를 띄운 뒤 이번 요청은 중단한다 |

**수정이 만든 2차 문제도 같이 처리했다.** 세션을 해제하면 세션 시스템의 검색 결과 캐시도 비워진다. 그런데 화면에는 예전 방 목록이 남아 있어서, 그 항목을 누르면 "참가할 방을 선택해주세요"라는 엉뚱한 메시지가 나왔다. 정리 시점에 화면의 방 목록도 함께 비우도록 했다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 존재하지 않는 방 참가 → 실패 → 메인 메뉴 | 로그 `Stale Steam session found on main menu` 후 세션 해제 |
| 이어서 방 만들기 | 바로 성공 |
| 호스트 로비 이동 실패 후 메인 메뉴 | 같은 경로로 정리 |
| 정상 방 만들기 / 참가 | 정리 코드가 개입하지 않음 |

## 정리

- "A가 안 된다"는 문제는 **A 직전에 무엇을 했는지**까지 거슬러 올라가야 보일 때가 있다.
- 실패 경로가 여러 개면 경로마다 고치는 대신, **"이 상태에서는 절대 있으면 안 되는 것"** 을 찾아 한 곳에서 지키는 편이 빠뜨림이 적다.
- 수정 자체가 만든 새 문제(낡은 방 목록)까지 확인해야 수정이 끝난다.

---

[Debugging 목차](../README.md)
