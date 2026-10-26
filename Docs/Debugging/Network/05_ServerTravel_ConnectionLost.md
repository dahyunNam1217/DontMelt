# 서버 이동 중 정상적인 연결 종료를 "호스트 연결 끊김"으로 처리하던 문제

| 항목 | 내용 |
|---|---|
| 문제 | 로비에서 경기를 시작하면 일부 클라이언트만 가끔 "호스트의 연결이 끊겼습니다"를 띄우고 메인 메뉴로 갔다 |
| 원인 | non-seamless ServerTravel이 기존 연결을 닫을 때 생기는 `ConnectionLost`를 실제 장애로 처리했다 |
| 해결 | 엔진이 이미 들고 있는 이동 상태(`FWorldContext::TravelURL`, `PendingNetGame`)로 정상 이동 중인지 판별했다 |
| 관련 코드 | `UDMGameInstance::HandleNetworkFailure` |

## 현상

로비→경기, 경기→로비 이동은 **non-seamless ServerTravel**이라 클라이언트가 기존 연결을 닫고 새로 접속한다.

- 매번 나지는 않고, 패킷 순서와 맵 로딩 시간에 따라 가끔 났다.
- 튕긴 참가자는 경기 맵에 도착하지 않으므로, 경기 쪽은 [Waiting 단계에서 멈추는 문제](10_Match_Waiting_Timeout.md)로 이어졌다.

## 원인 추적

엔진의 클라이언트 이동 순서를 따라가며 실패 이벤트가 끼어드는 지점을 찾았다.

```text
호스트 ServerTravel(non-seamless)
  └ 각 클라이언트에 ClientTravel(상대 URL) 전송
클라이언트
  └ SetClientTravel → WorldContext.TravelURL 설정
  └ 다음 틱 TickWorldTravel → Browse → 기존 NetDriver 종료, PendingNetGame 생성
  (이 사이에 기존 연결 종료가 ConnectionLost로 보고될 수 있음)
UDMGameInstance::HandleNetworkFailure(World 있음, NM_Client)
  └ HandleConnectedNetworkFailure → StartLocalFailureRecovery → 메인 메뉴
```

기존 처리는 "클라이언트 월드의 기본 NetDriver에서 난 오류인가"만 봤다. **지금 서버가 지시한 이동 중인가**는 확인하지 않았다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. Seamless Travel로 전환 | 연결을 유지한 채 이동 | 근본적이지만 GameMode·PlayerState 이전 처리 전체를 다시 설계해야 한다. 패키징 직전에는 위험하다 |
| B. 이동 시작 시 게임 쪽 플래그 | ClientTravel 수신 시 표시 | 이동 명령을 받는 지점이 엔진 내부라 정확히 끼우기 어렵다 |
| **C. 엔진의 이동 상태를 직접 확인** | `FWorldContext::TravelURL` / `PendingNetGame` | **채택.** 엔진이 이미 들고 있는 정보라 새 상태가 필요 없다 |

```cpp
// 비컨·데모 등 다른 NetDriver의 오류는 방 연결 종료로 보지 않는다.
if (NetDriver != nullptr && NetDriver != World->GetNetDriver())
{
    return;
}

// 서버가 지시한 레벨 이동(ClientTravel) 중에 기존 연결이 닫히는 것은 정상 흐름이다.
// 새 연결이 실패하면 World가 없는 PendingNetGame 경로에서 따로 처리된다.
if (const FWorldContext* Context = GetWorldContext())
{
    if (!Context->TravelURL.IsEmpty() || Context->PendingNetGame != nullptr)
    {
        UE_LOG(LogDMGameInstance, Log, TEXT("Network failure ignored during client travel. Type=%s, Message=%s"),
            GetNetworkFailureName(FailureType), *ErrorString);
        return;
    }
}

HandleConnectedNetworkFailure(World, NetDriver, FailureType, ErrorString);
```

**두 조건을 모두 보는 이유:** 엔진은 `TickWorldTravel`에서 `TravelURL`을 비운 **직후** `Browse`로 `PendingNetGame`을 만든다. 그래서 이동 중인 상태는 "TravelURL이 남아 있다"와 "PendingNetGame이 있다" 중 하나로만 보인다. 한쪽만 보면 그 사이 구간을 놓친다.

**실제 장애를 놓치지 않는 근거:** 새 서버 접속이 실패하면 엔진은 World 없이(`PendingConnectionFailure`) 알린다. 이 경우는 [PendingNetGame 실패 처리](06_PendingNetGame_Failure.md)가 그대로 받는다. 이동 중 오탐만 걸러내고 진짜 실패 경로는 건드리지 않는다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 로비→경기→결과→로비 10회 반복 | 클라이언트가 메인 메뉴로 튕기지 않음 |
| 위 반복 중 로그 | `Network failure ignored during client travel`이 찍히면 이 수정이 오탐을 막은 경우 |
| 경기 중 호스트 강제 종료 | 기존대로 "호스트 연결 끊김" 안내 후 메인 메뉴 |

## 정리

- 같은 이벤트(연결 종료)라도 **정상 흐름에서 나는 것과 장애에서 나는 것**이 섞일 수 있다. 판단 기준에 "지금 무엇을 하는 중인가"를 넣어야 한다.
- 새 상태 변수를 만들기 전에 **엔진이 이미 들고 있는 상태**로 판단할 수 있는지 먼저 본다.

---

[Debugging 목차](../README.md)
