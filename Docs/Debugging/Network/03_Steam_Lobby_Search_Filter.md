# 공유 AppID 480에서 내 로비가 검색 목록에 안 보일 수 있는 문제

| 항목 | 내용 |
|---|---|
| 문제 | 호스트가 방을 만들었는데 클라이언트 목록이 비어 있거나 가끔만 보였다 |
| 원인 | Steam이 잘라서 돌려준 결과 100개 안에서만 클라이언트가 Don't Melt 로비를 걸렀다 |
| 해결 | 생성 때 광고하는 프로젝트·로비 상태 키를 Steam 서버 쪽 검색 조건에도 넣었다 |
| 관련 코드 | `UDMOnlineSessionSubsystem::FindLobbySessions`, `IsCompatibleOpenLobby` |

## 현상

개발 중에는 Steam 테스트용 AppID 480(Spacewar)을 쓴다. 전 세계 개발자가 같은 ID를 공유한다.

- 다른 로비가 많은 시간대에 증상이 심해졌다.
- 검색 요약 로그 `Steam lobby search summary`에서 Raw(받은 수)는 큰데 ProjectMatched(우리 로비 수)는 0이었다.

## 원인 추적

```cpp
// 기존: 로비 여부만 서버 조건, 나머지는 결과를 받은 뒤 클라이언트에서 거름
SessionSearch->MaxSearchResults = 100;
SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
// HandleFindComplete → IsCompatibleOpenLobby(DM_PROJECT, DM_LOBBY_STATUS, ...)
```

Steam 로비 검색은 조건에 맞는 로비를 **최대 개수만큼 잘라서** 돌려준다. 클라이언트 필터는 이 "자르기" 이후에 적용되므로, 잘린 결과에 우리 로비가 없으면 찾을 방법이 없다. Raw/ProjectMatched 로그 수치가 이 해석과 맞았다.

## 해결

| 안 | 내용 | 판단 |
|---|---|---|
| A. MaxSearchResults 증가 | 더 많이 받아서 거름 | 공유 로비 수가 늘면 다시 밀린다 |
| B. 전용 AppID 사용 | 출시용 AppID 발급 | 최종 해결책이지만 개발 단계에서는 바로 쓸 수 없다 |
| **C. 서버 쪽 필터 추가** | 생성 때 광고하는 키를 검색 조건에도 넣음 | **채택.** Steam OSS는 커스텀 `QuerySettings`를 Steam 로비 필터로 변환한다 |

```cpp
SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

// 공유 App ID(480)에서는 다른 게임 로비가 결과 상한을 채울 수 있으므로
// Steam 서버 쪽에서 프로젝트와 로비 상태로 먼저 거른다.
// 결과는 HandleFindComplete의 IsCompatibleOpenLobby에서 다시 검사한다.
SessionSearch->QuerySettings.Set(DMProjectKey, DMProjectValue, EOnlineComparisonOp::Equals);
SessionSearch->QuerySettings.Set(DMLobbyStatusKey, DMLobbyOpenValue, EOnlineComparisonOp::Equals);
```

- 생성 시 `ViaOnlineService`로 광고한 키와 **같은 키**를 사용해야 서버 필터가 동작한다.
- 클라이언트 재검사(`IsCompatibleOpenLobby`)는 그대로 둔다. 서버 필터가 예상대로 동작하지 않더라도 잘못된 로비가 목록에 섞이지 않는다.
- 로비 상태 필터 덕분에 이미 경기를 시작한 방(`Starting`)도 서버에서 걸러진다.

**되돌리는 기준도 정해 두었다.** OSS의 변환 방식은 엔진 코드로 확인했지만, 실제 Steam 서버 동작은 패키지 환경에서만 볼 수 있다. 수정 후 방이 아예 보이지 않으면 추가한 두 줄만 빼고 다시 확인하면 된다.

## 검증 방법

| 시나리오 | 확인할 결과 |
|---|---|
| 클라이언트에서 방 검색 | 요약 로그의 Raw와 ProjectMatched가 거의 같음 |
| 호스트가 경기를 시작한 뒤 검색 | 해당 방이 목록에 나오지 않음 |

## 정리

- 클라이언트 필터는 **서버가 잘라서 준 결과 안에서만** 동작한다. 공유 자원을 쓸 때는 서버 쪽 필터가 필요하다.
- 외부 서비스 동작에 기대는 수정은, 기대대로 동작하지 않을 때 되돌리는 방법까지 같이 정한다.

---

[Debugging 목차](../README.md)
