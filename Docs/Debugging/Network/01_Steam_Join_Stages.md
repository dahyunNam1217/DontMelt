# Steam 로비 참가 실패를 단계별로 분리해 추적한 과정

| 항목 | 내용 |
|---|---|
| 문제 | Steam 로비는 검색되지만 클라이언트가 방에 들어가지 못했다 |
| 접근 | "방에 못 들어간다"는 하나의 증상을 OSS 초기화 → 검색 → 참가 → 주소 해석 → 접속 단계로 나눠 단계마다 로그로 확인했다 |
| 결과 | 단계별로 서로 다른 원인 5개를 찾아 각각 수정했고, 이후 남은 접속 실패는 별도 문서 4건으로 이어서 해결했다 |
| 관련 코드 | `UDMOnlineSessionSubsystem`, `ADMFrontendPlayerController`, `Config/DefaultEngine.ini` |

## 현상

호스트가 만든 방이 클라이언트 목록에 보이는데도 참가 버튼을 누르면 메인 메뉴로 돌아왔다. 처음에는 원인이 하나라고 생각했지만, 로그를 단계별로 나눠 보니 실패 지점이 매번 달랐다.

```text
Create ─▶ Find ─▶ Join ─▶ ResolveConnectString ─▶ ClientTravel ─▶ 로비 맵 도착
  ①        ②       ③            ④                     ⑤
```

화살표 하나가 통과됐다고 다음 단계도 성공하는 것은 아니다. 그래서 각 단계의 성공 로그를 따로 남기고, 실패한 단계만 조사했다.

## 단계별 원인과 수정

### ① OnlineSubsystem 초기화 — PIE와 Standalone 분리

```text
Unable to create OnlineSubsystem instance Steam
frontend is not standalone
```

PIE에서는 Steam OSS가 정상적으로 올라오지 않고, 세션 요청도 Frontend가 Standalone이 아니라는 이유로 막혔다. 세션 코드의 결함이 아니라 **실행 환경의 차이**였다. 이후 온라인 테스트는 Standalone 두 개(또는 두 PC)로만 진행하고, PIE는 게임플레이 테스트에만 썼다. 이 구분 뒤로 Steam 세션 생성과 `SteamNetDriver` 시작 로그가 정상으로 찍혔다.

### ② 검색 — 프로젝트 빌드 번호와 엔진 BuildUniqueId 혼동

```text
Removed incompatible build: ServerBuildUniqueId ... GetBuildUniqueId()
```

검색 결과를 거르는 코드가 프로젝트에서 직접 광고하는 `DM_NETWORK_BUILD` 값과 엔진의 `BuildUniqueId`를 같은 개념으로 다루고 있었다. 엔진 값은 OSS가 이미 검사하므로 게임 코드에서는 프로젝트 키만 확인하도록 역할을 나눴다.

```cpp
// 엔진 BuildUniqueId의 호환성은 Steam OnlineSubsystem에 맡긴다.
// 게임 코드는 DM_PROJECT, DM_LOBBY_STATUS, DM_NETWORK_BUILD만 검사한다.
```

두 PC가 서로 다른 패키지를 쓰면 엔진 값이 달라 검색에서 서로 보이지 않는다. 그래서 테스트 체크리스트에 "두 PC가 같은 패키지인지 확인"을 넣었다.

### ③ 참가 — Presence와 Lobby 플래그 불일치

```text
JoinSession: bUsesPresence and bUseLobbiesIfAvailable ... have to match
Session operation completed. Success=0 ... Result=5
```

검색 결과의 SessionSettings에는 `Presence=1, UseLobbies=0`이 담겨 있었다. Steam OSS는 로비 참가 시 두 값이 같기를 요구한다. 검색 결과를 그대로 신뢰하지 않고 참가 직전에 값을 맞췄다.

```cpp
// Steam 로비 참가 시 Presence와 Lobby 플래그가 일치해야 한다.
// 검색 결과에서 누락된 Lobby 플래그를 보정한다.
SearchResult.Session.SessionSettings.bUsesPresence = true;
SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

UE_LOG(LogDMOnlineSession, Log, TEXT("Steam join settings normalized. Presence=%d, UseLobbies=%d"), ...);
```

수정 뒤에는 `Result=5` 없이 다음 단계로 넘어갔다.

### ④ 접속 주소 — NamedSession 실패 시 검색 원본으로 재시도

```text
Invalid session info for session GameSession in GetResolvedConnectString()
```

참가는 성공했는데 접속 주소를 얻지 못했다. 로그를 읽다가 `HostIP: INVALID`도 함께 봤는데, Steam P2P에서는 IP 대신 `steam.<SteamID>` 주소를 쓰므로 이 값만으로 실패라고 판단하면 안 된다는 점을 먼저 정리했다. 그다음 주소를 얻는 경로를 두 개로 나눴다.

```cpp
// 1차: 참가한 NamedSession에서 주소를 얻는다.
const bool bNamedResolved =
    Sessions->GetResolvedConnectString(DMSessionName, ResolvedAddress, NAME_GamePort) && !ResolvedAddress.IsEmpty();

if (!bNamedResolved)
{
    // 2차: NamedSession의 SessionInfo가 아직 비어 있으면 검색 원본으로 주소를 만든다.
    Sessions->GetResolvedConnectString(*OriginalResult, NAME_GamePort, ResolvedAddress);
}
```

어느 경로로 주소를 얻었는지 로그(`resolved from NamedSession` / `resolved from SearchResult fallback`)로 남겨, 다음 실패 때 바로 구분할 수 있게 했다.

### ⑤ 접속 — Steam NetDriver 설정 상속

`DefaultEngine.ini`에 `GameNetDriver`를 Steam으로 정의만 추가하면, 상위 설정에 있던 같은 이름의 IP 드라이버 정의가 배열에 남아 먼저 선택될 수 있다. 호스트가 IP 드라이버로 열리면 클라이언트의 `steam.` 주소 접속은 실패한다. 배열을 비운 뒤 다시 정의했다.

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")
```

호스트 로그에서 `GameNetDriver`가 `SteamNetDriver`로 생성되는지를 패키징 체크리스트의 첫 항목으로 넣었다.

## 이어서 해결한 문제

위 다섯 단계를 정리한 뒤 소스 전체를 다시 검수하면서, "방을 못 만든다 / 로비에 못 들어간다"로 보이는 나머지 원인을 찾았다.

| 증상 | 원인 | 문서 |
|---|---|---|
| 참가 실패 뒤 방 만들기가 계속 실패 | 실패 경로에 남은 Steam 세션 | [잔여 세션 정리](02_Steam_Stale_Session.md) |
| 방이 목록에 가끔만 보임 | 공유 AppID 480에서 클라이언트 쪽 필터만 사용 | [서버 쪽 검색 필터](03_Steam_Lobby_Search_Filter.md) |
| "다른 온라인 작업 진행 중"이 풀리지 않음 | 오지 않는 Steam 콜백 | [세션 작업 타임아웃](04_Steam_Session_Timeout.md) |
| 로비→경기 이동 중 일부 클라이언트만 튕김 | 정상 이동의 연결 종료를 장애로 판단 | [ServerTravel 오탐 방어](05_ServerTravel_ConnectionLost.md) |

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 서로 다른 Steam 계정 2대, 같은 패키지 | 로그가 `Created` → `Find` → `join settings normalized` → `resolved from ...` → 로비 도착 순으로 찍힘 |
| 한쪽 PC만 다시 패키징 | 검색 결과에서 제외됨 (같은 패키지 조건 확인) |
| 호스트 로그 | `GameNetDriver`가 `SteamNetDriver`로 생성됨 |

## 정리

- 증상이 하나여도 원인은 여러 단계에 흩어져 있을 수 있다. **단계별 성공 로그**를 먼저 만들면 다음 실패가 어디서 났는지 바로 보인다.
- 엔진이 이미 검사하는 값(BuildUniqueId)을 게임 코드에서 다시 검사하지 않는다. 책임을 겹치면 원인이 두 군데로 흩어진다.
- 실행 환경(PIE / Standalone)이 만든 실패를 코드 결함으로 분류하지 않도록 테스트 환경부터 고정했다.

---

[Debugging 목차](../README.md)
